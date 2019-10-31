
#include <vfs_api.h>
#include <FSImpl.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "RTClib.h"


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
#include "Sensor.h"

//#define DEBUG
//
//#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
//#error Bluetooth is not enabled!
//#endif
//BluetoothSerial SerialBT;

// Mutil task
TaskHandle_t Task1;
TaskHandle_t Task2;

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

/* Put your SSID & Password */
String APssid = "ESP32";  // Enter SSID here
const char* APpassword = "12345678";  //Enter Password here

/* Put IP Address details */
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

DateTime dateTime;
long timeNow;
#define TIME_UPDATE 2000

Relay relay[MAX_RELAY];
ShiftRegister74HC595 sr(1, 13, 14, 15);

Timmer timmer[MAX_RELAY];
AnalogINPUT analogInput[MAX_ANALOG];
DigitalInput digitalInput[MAX_DIGITAL];

void OTA_update() {
    String host = "esp";
    host += String(ESP_getChipId());

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    delay(1);
    /*use mdns for host name resolution*/
    if (!MDNS.begin(host.c_str())) {            //  http://esp193858948.local
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

void MQTT_sendConfig() {
    String msg;
    String filename;
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonArray& data = root.createNestedArray("relayStatus");

    for (int i = 0; i < MAX_RELAY; i++) {
        filename = "/timmer";
        filename += i;
        filename += ".txt";
        msg = readFileString(SPIFFS, filename.c_str());
        if (msg != NULL) {
            client.publish(mqtt_topic_config, msg);
            delay(100);
        }
        if (relay[i].status == RELAY_ON) {
            data.add("ON");
        }
        else {
            data.add("OFF");
        }

    }
    root.printTo(msg);
    client.publish(mqtt_topic_config, msg);

    client.publish(mqtt_topic_config, WiFi.localIP().toString());
}

void connectMQTT() {
    byte numWait = 0;

    //Serial.print("connecting to wifi");
    //while (WiFi.status() != WL_CONNECTED && numWait < 10) {
    //    Serial.print(".");
    //    delay(1000);
    //    numWait++;
    //}

    //MQTT connect client
    Serial.print("\nconnecting mqtt...");
    client.begin(mqtt_server.c_str(), mqtt_port, wifiClient);
    client.onMessage(messageReceived);
    numWait = 0;
    while (numWait < 10) {
        if (client.connect(mqtt_clientID.c_str(), mqtt_user.c_str(), mqtt_pwd.c_str())) {
            break;
        }
        Serial.print(".");
        delay(1000);
        numWait++;
    }

    //Sub topic
    client.subscribe(mqtt_topic_control.c_str());
    client.subscribe(mqtt_topic_reponse.c_str());

    Serial.println();
    Serial.print("topic control:  ");
    Serial.println(mqtt_topic_control.c_str());
}

void messageReceived(String &topic, String &payload) {
    //Serial.println("incoming: " + topic + " - " + payload);
#ifdef DEBUG
    Serial.println("incoming topic: " + topic);
#endif // DEBUG
    if (topic == mqtt_topic_control) {
        handleEventControl(payload.c_str());
    }
    else if (topic == mqtt_topic_reponse) {

    }
}

void setup_PinMode() {

    pinMode(15, OUTPUT);
    pinMode(13, OUTPUT);
    pinMode(14, OUTPUT);

    //Relay::setRelay595(255);
    for (int i = 0; i < MAX_RELAY; i++) {
        /*Relay::setRelay(i, relay[i].status);*/
        //if (relay[i].numCondition > 0) {
        if (relay[i].status == RELAY_ON) {
            //relay.setRelayOn(i);
            sr.setNoUpdate(i, RELAY_ON);
        }
        else if (relay[i].status == RELAY_OFF) {
            //relay.setRelayOff(i);
            sr.setNoUpdate(i, RELAY_OFF);
        }
        relay[i].oldStatus = relay[i].status;
        updateRelayConfig(SPIFFS, i);   //cập nhật vào file
    //}
    }
    sr.updateRegisters();
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
    //String bluetoothName = "ESP";
    //bluetoothName += String(ESP_getChipId());
    //SerialBT.begin(bluetoothName); //Bluetooth device name
    //Serial.println("The device started, now you can pair it with bluetooth!");

    //Doc cau hinh
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    listDir(SPIFFS, "/", 0);

    readDeviceInfor(SPIFFS);
    delay(10);
    readRelayConfig(SPIFFS);
    delay(10);
    /*readAnalogConfig(SPIFFS);
    delay(10);*/
    readTimmerConfig(SPIFFS);
    delay(10);

    Serial.setTimeout(500);

    setup_PinMode();

    ////wifimanager here
    String host = "esp";
    host += String(ESP_getChipId());
    wifiManager.setTimeout(60);

    if (wifiManager.autoConnect(host.c_str())) {
        WiFi.softAPdisconnect(true);

        //OTA
        OTA_update();

        //if (isOnline == true) {
            //RTC setup
        setupRTC();

        //Connect wifi and mqtt
        connectMQTT();

        //Send config 
        MQTT_sendConfig();
        /* }
         else {*/
         //local controler
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
        10000,       /* Stack size of task */
        NULL,        /* parameter of the task */
        1,           /* priority of the task */
        &Task1,      /* Task handle to keep track of created task */
        0);          /* pin task to core 0 */
    delay(500);

    //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
    xTaskCreatePinnedToCore(
        Task2code,   /* Task function. */
        "Task2",     /* name of task. */
        10000,       /* Stack size of task */
        NULL,        /* parameter of the task */
        1,           /* priority of the task */
        &Task2,      /* Task handle to keep track of created task */
        1);          /* pin task to core 1 */
    delay(500);
}

//Task1code:   giao tiếp mqtt, update RTC
void Task1code(void * pvParameters) {
    static unsigned long previousMillis = 0;
    static unsigned long previousMillisConnect = 0;
    int counter = 0;

    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            //checkOnline();
            //if (isOnline) {
            //OTA
            server.handleClient();
            //update time
            dateTime = getRTC().now();
            timeNow = dateTime.unixtime();
            unsigned long currentMillis = millis();
            if (currentMillis - previousMillis >= TIME_UPDATE) {
                previousMillis = currentMillis;
                updateRTC();
                Serial.println(timeNow);
            }

            // mqtt
            if (!client.connected()) {
                connectMQTT();
            }
            client.loop();

            previousMillisConnect = millis();

            /*}
            else {*/
            localConnectionHandler();
            // mqtt
            /*if (!client.connected() && (millis() - previousMillisConnect > 30000)) {
                connect();
                previousMillisConnect = millis();
            }*/
            //}
        }
        else {
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
        //readSensorSHT(5000);
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
                        //Serial.write(dataRecv);
                    }
                    if (dataTransfer[i] != "") {
                        Serial.println("ok");
                        handleEventControl(dataTransfer[i].c_str());
                    }
                }
            }
            else {
                if (local_serverClients[i]) {
                    local_serverClients[i].stop();
                }
            }
        }
        //check UART for data
        if (Serial.available()) {
            size_t len = Serial.available();
            uint8_t sbuf[len];
            Serial.readBytes(sbuf, len);
            //push UART data to all connected telnet clients
            for (i = 0; i < MAX_SRV_CLIENTS; i++) {
                if (local_serverClients[i] && local_serverClients[i].connected()) {
                    local_serverClients[i].write(sbuf, len);
                    delay(1);
                }
            }
        }
    }
    else {
        Serial.println("WiFi not connected!");
        for (i = 0; i < MAX_SRV_CLIENTS; i++) {
            if (local_serverClients[i]) local_serverClients[i].stop();
        }
        delay(10);
    }

    //while (clientlocal.connected()) {
    //    int size = 0;
    //    // read data from wifi client and send to serial
    //    while ((size = clientlocal.available())) {
    //        size = (size >= BUFFER_SIZE ? BUFFER_SIZE : size);
    //        clientlocal.read(buff, size);
    //        Serial.write(buff, size);
    //        Serial.flush();
    //        String msg = String((char*)buff);
    //        handleEventControl(msg);
    //        Serial.println("OK!");
    //    }
    //    // read data from serial and send to wifi client
    //    while ((size = Serial.available())) {
    //        size = (size >= BUFFER_SIZE ? BUFFER_SIZE : size);
    //        Serial.readBytes(buff, size);
    //        clientlocal.write(buff, size);
    //        clientlocal.flush();
    //    }
    //}
    //Serial.println("client disconnected");

    //clientlocal.stop();

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
                    sr.setNoUpdate(i, RELAY_ON);

                else if (relay[i].status == RELAY_OFF)
                    //relay.setRelayOff(i);
                    sr.setNoUpdate(i, RELAY_OFF);

                relay[i].oldStatus = relay[i].status;
                updateRelayConfig(SPIFFS, i);   //cập nhật vào file

                //Serial.print("relay change:  ");
                //Serial.println(i);
            }

        }
    }
    sr.updateRegisters();
    /*Serial.println();
    Serial.println(*(sr.getAll()), BIN);*/
}

void TimmerHandler()
{
    byte i;
    int relayid, relayConditionNumber, timmerInfluence;

    for (i = 0; i < MAX_RELAY; i++) {       // Duyệt timmer
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
            //TODO: DEBUG
           /* Serial.print("timmer id ");
            Serial.println(i);

            Serial.print("relay id ");
            Serial.println(relayid);

            Serial.print("relay numCondition ");
            Serial.println(relay[relayid].numCondition);

            Serial.print("relay action ");
            Serial.println(timmerInfluence);*/
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

void digitalHandler()
{

}

void handleEventControl(const char * payload)
{

    Serial.println(payload);

    String fileName = "";
    int id;

    StaticJsonBuffer<500>jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);
    if (!root.success()) {
        Serial.println("JSON parse failed");
        return;
    }
    else {
        //Serial.println("JSON parse OK!");
    }

    if (root["type"] == "remove") {
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
    else {
        //{ "type": "Timmer", "id" : 0 "relayID" : 0, "rcn" : 1, "ts" : 0, "te" : 0, "tc" : 100, "ti" : 0 }
        if (root["type"] == "Timmer") {
            //Serial.print("Timmer action");
            id = root["id"];
            timmer[id].relayID = root["relayID"];
            timmer[id].relayConditionNumber = root["rcn"];
            timmer[id].timmerStart = root["ts"];
            timmer[id].timmerEnd = root["te"];
            timmer[id].timmerCycle = root["tc"];
            timmer[id].timmerInfluence = root["ti"];

            fileName = "/timmer";
        }
        /*else if (root["type"] == "Analog") {
            id = root["id"];
            byte relayid = (byte)root["relayID"];
            analogInput[id].relayID[relayid] = relayid;
            analogInput[id].relayConditionNumber[relayid] = root["rcn"];
            analogInput[id].name = root["name"].asString();
            analogInput[id].upper = root["u"];
            analogInput[id].lower = root["l"];
            analogInput[id].gain = root["g"];
            analogInput[id].analogInfluence = root["ai"];

            fileName = "/analog";
        }*/

        fileName += id;
        fileName += ".txt";
        writeFile(SPIFFS, fileName.c_str(), payload);
    }

}


/*

{ "type": "Timmer", "id": 0,  "relayID": 0, "rcn": 1, "ts": 0, "te": 0, "tc": 0, "ti": 0 }


*/
