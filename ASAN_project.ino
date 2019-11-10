
#include <vfs_api.h>
#include <FSImpl.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "RTClib.h"

#include <SHT1x.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <MQTT.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include "WebOTA.h"
#include <ShiftRegister74HC595.h>


#include "FileIO.h"
#include "RTCTime.h"
#include "Config.h"
#include "Relay.h"
#include "AnalogINPUT.h"
#include "DigitalInput.h"
#include "Timmer.h"

// Mutil task
TaskHandle_t Task1;
TaskHandle_t Task2;

#define pinWifi 12

WiFiManager wifiManager;

HTTPClient http;
WiFiClient wifiClient;
bool isOnline = false;

MQTTClient client;

WebServer server(80);


//how many clients should be able to telnet to this ESP32
#define MAX_SRV_CLIENTS 4

WiFiServer local_server(1234);
WiFiClient local_serverClients[MAX_SRV_CLIENTS];

#define BUFFER_SIZE 512
byte buff[BUFFER_SIZE];

/* Put IP Address details */
//IPAddress local_ip(192, 168, 1, 1);
//IPAddress gateway(192, 168, 1, 1);
//IPAddress subnet(255, 255, 255, 0);

DateTime dateTime;
long timeNow;
#define TIME_UPDATE 5000

Relay relay[MAX_RELAY];
ShiftRegister74HC595 relayStatus(4, 13, 14, 15);
// Creates a MUX74HC4067 instance
// 1st argument is the Arduino PIN to which the EN pin connects
// 2nd-5th arguments are the Arduino PINs to which the S0-S3 pins connect
MUX74HC4067 mux(33, 27, 2, 25, 26);

Timmer timmer[MAX_TIMMER];
AnalogINPUT analogInput[MAX_ANALOG];
DigitalInput digitalInput[MAX_DIGITAL];
// Specify data and clock connections and instantiate SHT1x object
#define dataPin  21
#define clockPin 22
SHT1x sht1x(dataPin, clockPin);

struct SHTSensor {
    float temp_c;
    float temp_f;
    float humidity;
}Sht10;

void readSensorSHT(long timeCycle) { //second
    static long previousSecond = 0;

    if (timeNow - previousSecond >= timeCycle) {
        previousSecond = timeNow;
        // Read values from the sensor
        // Save old value

        // Update current value
        Sht10.temp_c = sht1x.readTemperatureC();
        Sht10.humidity = sht1x.readHumidity();
        //#ifdef DEBUG
        Serial.print("temp:  ");
        Serial.print(Sht10.temp_c);
        Serial.print("\thum: ");
        Serial.println(Sht10.humidity);
        //#endif // DEBUG
        //updateAnallog();
    }
}


void OTA_update() {
    String host = "esp";
    host += String(ESP_getChipId());

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    delay(1);
    /*use mdns for host name resolution*/
    if (!MDNS.begin(host.c_str())) {            //  http://esp3381529988.local
        Serial.println("Error setting up MDNS responder!");
    }
    else {
        host += ".local";
        Serial.println("mDNS responder started:  " + host);
    }

    /*return index page which is stored in serverIndex */
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", loginIndex);
    });
    server.on("/serverIndex", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", serverIndex);
    });
    /*handling uploading firmware file */
    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("Update: %s\n", upload.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_WRITE) {
            /* flashing firmware to ESP*/
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { //true to set the size to the current progress
                Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            }
            else {
                Update.printError(Serial);
            }
        }
    });
    server.begin();

}

void MQTT_sendTimmerConfig(int index) {
    Serial.println("send timmer config");
    if (index < 0 || index > MAX_TIMMER) {
        return;
    }

    String filename = "/timmer";
    filename += index;
    filename += ".txt";
    String msg;
    if (readFileString(SPIFFS, filename.c_str(), msg)) {
        //msg = "{\"type\":\"timmer\", \"id\" : 0, \"relayID\" : 0, \"rcn\" : 1, \"ts\" : 0, \"te\" : 0, \"tc\" : 0, \"ti\" : 0}";
        client.publish(mqtt_topic_config, msg.c_str());
    }
    else {
        Serial.println("No timmer");
    }
    delay(10);
}

void MQTT_sendWifiIP() {
    Serial.println("send wifi config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    if (!root.success()) {
        Serial.println("JSON parse failed");
        return;
    }
    root["ssid"] = WiFi.SSID();
    root["ip"] = WiFi.localIP().toString();

    char msg[200];
    root.printTo(msg, sizeof(msg));

    client.publish(mqtt_topic_config, msg);
    delay(10);
}

void MQTT_sendRelayConfig() {
    Serial.println("send relay config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    if (!root.success()) {
        Serial.println("JSON parse failed");
        return;
    }
    root["ON"] = RELAY_ON;
    root["OFF"] = RELAY_OFF;

    JsonArray& data = root.createNestedArray("relayStatus");
    for (int i = 0; i < MAX_RELAY; i++) {
        /*if (relay[i].status == RELAY_ON) {
            data.add("ON");
        }
        else {
            data.add("OFF");
        }*/
        data.add(relay[i].status);
    }
    char msg[512];
    root.printTo(msg, sizeof(msg));
    client.publish(mqtt_topic_config, msg);
    delay(10);
}

void connectMQTT() {
    byte numWait = 0;

    //MQTT connect client
    Serial.print("\nconnecting mqtt");
    client.begin(mqtt_server.c_str(), mqtt_port, wifiClient);
    client.onMessage(messageReceived);
    numWait = 0;
    client.connect(mqtt_clientID.c_str(), mqtt_user.c_str(), mqtt_pwd.c_str());
    while (numWait < 10) {
        if (client.connected()) {
            break;
        }
        Serial.print(".");
        delay(500);
        numWait++;
    }

    if (numWait < 10) {
        Serial.println("connected");
        //Sub topic
        client.subscribe(mqtt_topic_request.c_str());
        client.subscribe(mqtt_topic_control.c_str());

        Serial.print("topic control:  ");
        Serial.println(mqtt_topic_control.c_str());

        //send wifiIP
        MQTT_sendWifiIP();
    }
    else {
        Serial.println("connect fail");
    }
}

void messageReceived(String &topic, String &payload) {

    //Serial.println("incoming: " + topic + " - " + payload);
#ifdef DEBUG
    Serial.println("incoming topic: " + topic);
#endif // DEBUG
    if (topic == mqtt_topic_control) {
        handleEventControl(payload.c_str());
    }
    else if (topic == mqtt_topic_request) {
        handleEventResquest(payload.c_str());
    }
    else {
        Serial.println("Message not available");
    }
}

void setup_PinMode() {
    relayStatus.setAllLow();

    pinMode(pinWifi, OUTPUT);

    //relayStatus.setAllHigh();

    //Relay::setRelay595(255);
    for (int i = 0; i < MAX_RELAY; i++) {
        /*Relay::setRelay(i, relay[i].status);*/
        //if (relay[i].numCondition > 0) {
        if (relay[i].status == RELAY_ON) {
            //relay.setRelayOn(i);
            relayStatus.setNoUpdate(i, RELAY_ON);
        }
        else if (relay[i].status == RELAY_OFF) {
            //relay.setRelayOff(i);
            relayStatus.setNoUpdate(i, RELAY_OFF);
        }
        relay[i].oldStatus = relay[i].status;
        updateRelayConfig(SPIFFS, i);   //cập nhật vào file
    //}
    }
    relayStatus.updateRegisters();
}

void checkOnline() {
    http.begin("http://google.com.vn");
    int httpCode = http.GET();                                        //Make the request
    http.end();

    if (httpCode > 0) { //Check for the returning code
        isOnline = true;
    }
    else {
        isOnline = false;
        Serial.println("No internet connection");
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);

    //Doc cau hinh
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    //listDir(SPIFFS, "/", 0);

    readDeviceInfor(SPIFFS);
    delay(10);
    readRelayConfig(SPIFFS);
    delay(10);
    /*readAnalogConfig(SPIFFS);
    delay(10);*/
    readTimmerConfig(SPIFFS);
    delay(10);
    readDigitalInput(SPIFFS);
    delay(10);

    setup_PinMode();

    //digital Input
    mux.signalPin(4, INPUT, DIGITAL);

    ////wifimanager here
    String host = "esp";
    host += String(ESP_getChipId());
    wifiManager.setTimeout(120);


    if (wifiManager.autoConnect(host.c_str())) {
        //WiFi.softAPdisconnect(true);

        //OTA
        OTA_update();

        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
            //Connect wifi and mqtt
            connectMQTT();
            delay(50);
        }
        setupRTC();

        local_server.begin();
        local_server.setNoDelay(true);
        Serial.print("Ready! Use 'telnet ");
        Serial.print(WiFi.localIP());
        Serial.println(" 1234' to connect");
        //}
    }

    xTaskCreatePinnedToCore(
        Task1code,   /* Task function. */
        "Task1",     /* name of task. */
        12000,       /* Stack size of task */
        NULL,        /* parameter of the task */
        1,           /* priority of the task */
        &Task1,      /* Task handle to keep track of created task */
        0);          /* pin task to core 0 */
    delay(500);

    //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
    xTaskCreatePinnedToCore(
        Task2code,   /* Task function. */
        "Task2",     /* name of task. */
        12000,       /* Stack size of task */
        NULL,        /* parameter of the task */
        1,           /* priority of the task */
        &Task2,      /* Task handle to keep track of created task */
        1);          /* pin task to core 1 */
    delay(100);
}

//Task1code:   giao tiếp mqtt, update RTC
void Task1code(void * pvParameters) {
    static unsigned long previousMillis = 0;
    int counter = 0;


    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            digitalWrite(pinWifi, HIGH);

            //checkOnline();
            //if (isOnline) {

            //update time
            dateTime = getRTC().now();
            timeNow = dateTime.unixtime();
            unsigned long currentMillis = millis();
            if (currentMillis - previousMillis >= TIME_UPDATE) {
                previousMillis = currentMillis;
                updateRTC();
                Serial.println(timeNow);
            }

            client.loop();
            delay(10);  // <- fixes some issues with WiFi stability
            // mqtt
            if (!client.connected()) {
                connectMQTT();
            }
            //Serial.println(ESP.getFreeHeap());
        }
        else {
            digitalWrite(pinWifi, LOW);

            /*if (!client.connected() && (millis() - previousMillisConnect > 60000)) {
                wifiManager.autoConnect(host.c_str());
                previousMillisConnect = millis();
            }*/

            Serial.println("WiFi not connected!");
            for (int i = 0; i < MAX_SRV_CLIENTS; i++) {
                if (local_serverClients[i]) local_serverClients[i].stop();
            }
            delay(1000);
        }
    }
}

//Task2code:  xử lý điều khiển
void Task2code(void * pvParameters) {

    for (;;) {
        server.handleClient();

        //OTA
        localConnectionHandler();

        //readSensorSHT(10);
        //digitalHandler();
        TimmerHandler();

        relayProcessing();
    }
}

void loop() {

    // Not nedded, Do nothing here
}

void localConnectionHandler() {
    uint8_t i;
    String dataTransfer[MAX_SRV_CLIENTS];
    if (WiFi.status() == WL_CONNECTED) {
        //check if there are any new clients
        if (local_server.hasClient()) {
            for (i = 0; i < MAX_SRV_CLIENTS; i++) {
                //find free/disconnected spot
                if (!local_serverClients[i] || !local_serverClients[i].connected()) {
                    if (local_serverClients[i]) local_serverClients[i].stop();
                    local_serverClients[i] = local_server.available();
                    if (!local_serverClients[i]) Serial.println("available broken");
                    Serial.print("New client: ");
                    Serial.print(i); Serial.print(' ');
                    Serial.println(local_serverClients[i].remoteIP());
                    break;
                }
            }
            if (i >= MAX_SRV_CLIENTS) {
                //no free/disconnected spot so reject
                local_server.available().stop();
            }
        }
        //check clients for data
        for (i = 0; i < MAX_SRV_CLIENTS; i++) {
            if (local_serverClients[i] && local_serverClients[i].connected()) {
                if (local_serverClients[i].available()) {
                    //get data from the telnet client and push it to the UART
                    dataTransfer[i] = "";
                    while (local_serverClients[i].available()) {
                        char dataRecv = (char)local_serverClients[i].read();
                        dataTransfer[i] += dataRecv;
                        Serial.write(dataRecv);
                    }
                    if (dataTransfer[i] != "") {
                        //Serial.println("ok");
                        handleEventControl(dataTransfer[i].c_str());
                        delay(10);
                    }
                }
            }
            else {
                if (local_serverClients[i]) {
                    local_serverClients[i].stop();
                }
            }
        }
        ////check UART for data
        //if (Serial.available()) {
        //    size_t len = Serial.available();
        //    uint8_t sbuf[len];
        //    Serial.readBytes(sbuf, len);
        //    //push UART data to all connected telnet clients
        //    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
        //        if (local_serverClients[i] && local_serverClients[i].connected()) {
        //            local_serverClients[i].write(sbuf, len);
        //            delay(1);
        //        }
        //    }
        //}
    }
    /*else {
        Serial.println("WiFi not connected!");
        for (i = 0; i < MAX_SRV_CLIENTS; i++) {
            if (local_serverClients[i]) local_serverClients[i].stop();
        }
        delay(10);
    }*/

}

void relayProcessing()
{
    //Serial.println("---relay process---");
    int i, j;
    int sum = 0;
    for (i = 0; i < MAX_RELAY; i++) {
        //Serial.print(relay[i].status);

        if (relay[i].numCondition < 1) {
            //continue;   // Nếu không đc gắn cho bất kỳ tác động nào
        }
        else {
            //Serial.print("relay ");
            //Serial.print(i);
            //Serial.print(" status:  ");
            sum = 0;

            for (j = 0; j < relay[i].numCondition; j++) {
                sum += relay[i].listCondition[j];
            }

            if (sum == 0) {                 //Tất cả đều thỏa mãn là ON thì mới ON
                relay[i].status = RELAY_ON;
            }
            else {
                relay[i].status = RELAY_OFF;
            }

            if (relay[i].oldStatus != relay[i].status) {  //Nếu có sư thay đổi trạng thái 
                if (relay[i].status == RELAY_ON)
                    //relay.setRelayOn(i);
                    relayStatus.setNoUpdate(i, RELAY_ON);

                else if (relay[i].status == RELAY_OFF)
                    //relay.setRelayOff(i);
                    relayStatus.setNoUpdate(i, RELAY_OFF);

                relay[i].oldStatus = relay[i].status;
                updateRelayConfig(SPIFFS, i);   //cập nhật vào file
                MQTT_sendRelayConfig();
                //Serial.print("relay change:  ");
                //Serial.println(i);
                delay(10);
            }

        }
    }
    relayStatus.updateRegisters();
    delay(10);

    //Serial.println();
}

void TimmerHandler()
{
    byte i;
    int relayid, relayConditionNumber, timmerInfluence;

    for (i = 0; i < MAX_TIMMER; i++) {       // Duyệt timmer
        if (timmer[i].relayID != -1) {      // Nếu timmer đã đc gắn cho relay nào đó
            relayid = timmer[i].relayID;
            relayConditionNumber = timmer[i].relayConditionNumber;
            timmerInfluence = timmer[i].timmerInfluence;

            if (relay[relayid].numCondition < relayConditionNumber) {
                relay[relayid].numCondition = relayConditionNumber;
            }

            relayConditionNumber = relayConditionNumber - 1; //chỉ số thấp hơn giá trị 1 đơn vị

            if (timmer[i].timmerStart == 0 && timmer[i].timmerEnd == 0) { // Nếu là bật hoặc tắt 
                relay[relayid].listCondition[relayConditionNumber] = timmerInfluence;
            }
            else if (timeNow < timmer[i].timmerStart) { // Nếu chưa đến thời gian hẹn giờ hoặc hết giờ thì đảo ngược trạng thái cài đặt
                relay[relayid].listCondition[relayConditionNumber] = !timmerInfluence;
            }
            else if (timeNow > timmer[i].timmerEnd) { // Nếu quá thời gian hẹn giờ thì đảo ngược trạng thái cài đặt, cộng thêm chu kỳ chuẩn bị cho tiếp theo
                relay[relayid].listCondition[relayConditionNumber] = !timmerInfluence;
                timmer[i].timmerStart += timmer[i].timmerCycle;
                timmer[i].timmerEnd += timmer[i].timmerCycle;
            }
            else if (timeNow >= timmer[i].timmerStart && timeNow <= timmer[i].timmerEnd) {      //Nếu là trong thời gian hẹn giờ
                relay[relayid].listCondition[relayConditionNumber] = timmerInfluence;
            }
        }
    }
}


void analogHandler()
{
    byte i, j;
    int id, relayConditionNumber, analogInfluence;

    for (i = 0; i < MAX_ANALOG; i++) {
        for (j = 0; j < MAX_RELAY; j++) {

            if (analogInput[i].relayID[j] != -1) {
                id = analogInput[i].relayID[j];
                relayConditionNumber = analogInput[i].relayConditionNumber[j];
                analogInfluence = analogInput[i].analogInfluence;

                relay[id].numCondition = relayConditionNumber;
                relayConditionNumber--;

                if (analogInput[i].value > analogInput[i].upper) {
                    relay[id].listCondition[relayConditionNumber] = !analogInfluence;
                }
                else if (analogInput[i].value < analogInput[i].lower) {
                    relay[id].listCondition[relayConditionNumber] = analogInfluence;
                }
            }
        }

    }

}

void writeFileInputs() {
    DynamicJsonBuffer jsonBuffer_2;
    JsonObject& root2 = jsonBuffer_2.createObject();
    JsonArray& status = root2.createNestedArray("status");
    JsonArray& active = root2.createNestedArray("active");

    for (byte i = 8; i < 16; ++i) {
        status.add(digitalInput[i].status);
    }
    for (byte i = 8; i < 16; ++i) {
        status.add(digitalInput[i].isActive);
    }

    const char *fileNameInput = "/inputs.txt";
    root2.printTo(fileData, sizeof(fileData));
    writeFile(SPIFFS, fileNameInput, fileData);
}

void digitalHandler()
{
    String fileTimmer;
    char fileData[512];
    byte isSomethingChanged = 0;

    for (byte i = 8; i < 16; i++)
    {
        if (digitalInput[i].isActive == 1) {

            digitalInput[i].status = mux.read(i);       //đọc nút nhấn

            if (digitalInput[i].status != digitalInput[i].old_status) { //Nếu có sự thay đổi
                delay(100); //delay 100
                digitalInput[i].status = mux.read(i);       //đọc lại để kiểm tra
                if (digitalInput[i].status != digitalInput[i].old_status) { //Nếu vẫn là có sự thay đổi
                    
                    isSomethingChanged++;
                    byte timmerID = i - 8;

                    timmer[timmerID].relayID = timmerID;
                    timmer[timmerID].relayConditionNumber = 1;
                    timmer[timmerID].timmerStart = 0;
                    timmer[timmerID].timmerEnd = 0;
                    timmer[timmerID].timmerCycle = 0;
                    timmer[timmerID].timmerInfluence = !timmer[timmerID].timmerInfluence;

                    DynamicJsonBuffer jsonBuffer;
                    JsonObject& root = jsonBuffer.createObject();
                    root["relayID"] = timmerID;
                    root["rcn"] = 1;
                    root["ts"] = 0;
                    root["te"] = 0;
                    root["tc"] = 0;
                    root["ti"] = !timmer[timmerID].timmerInfluence;
                    root.printTo(fileData, sizeof(fileData));
                    fileTimmer = "/timmer";
                    fileTimmer += (timmerID);
                    fileTimmer += ".txt";
                    writeFile(SPIFFS, fileTimmer.c_str(), fileData);


                }

            }
        }

    }

    if (isSomethingChanged > 0) {
        writeFileInputs();
    }
}

void handleEventResquest(const char * payload) {
    StaticJsonBuffer<500> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);
    if (!root.success()) {
        Serial.println("JSON parse failed");
        return;
    }

    String type = root[F("type")];

    if (type == "relays") {
        MQTT_sendRelayConfig();
    }
    else if (type == "wifi") {
        MQTT_sendWifiIP();
    }
    else if (type == "timmer") {
        MQTT_sendTimmerConfig(root["id"]);
    }
}

void handleEventControl(const char * payload)
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);
    if (!root.success()) {
        Serial.println("JSON parse failed");
        return;
    }


    if (root["type"].asString() == "remove") {
        String fileName = root["file"];
        fileName += ".txt";
        deleteFile(SPIFFS, fileName.c_str());
    }
    else if (root["type"] == "reset") {
        WiFi.disconnect(true);   // still not erasing the ssid/pw. Will happily reconnect on next start
        WiFi.begin(" ", " ");       // adding this effectively seems to erase the previous stored SSID/PW

        //reset
        digitalWrite(0, HIGH);//
        delay(100);
        ESP.restart();
    }
    else if (root["type"] == "digital") {
        int index = root["id"];
        int isActive = root["active"];

        digitalInput[index].isActive = isActive;

        writeFileInputs();
    }
    else if (root["type"] == "timmer") {
        //Serial.println("timmer control");
        String fileName;
        byte id;

        //{"type":"timmer","id":1,"relayID":1,"rcn":1,"ts":"0","te":"0","ti":0}
        if (root["type"] == "timmer") {
            //Serial.print("Timmer action");
            id = root["id"];
            if (id >= MAX_TIMMER) {
                return;
            }
            timmer[id].relayID = root["relayID"];
            timmer[id].relayConditionNumber = root["rcn"];
            timmer[id].timmerStart = root["ts"];
            timmer[id].timmerEnd = root["te"];
            timmer[id].timmerCycle = root["tc"];
            timmer[id].timmerInfluence = root["ti"];

            fileName = "/timmer";
            fileName += id;
            fileName += ".txt";
            writeFile(SPIFFS, fileName.c_str(), payload);
        }
        else {
            return;
        }
    }

}


/*
cmd/215883381529988

{"type":"timmer","id": 0,"relayID":0,"rcn":1,"ts":0,"te":0,"tc":0,"ti":0}

{ "type": "timmer","id":0}
{ "type": "relays"}
{ "type": "wifi"}
{ "type": reset}

*/
