#include <vfs_api.h>
#include <FSImpl.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "RTClib.h"

#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <MQTT.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include "BluetoothSerial.h"

#include "WebOTA.h"
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

WiFiClient wifiClient;
MQTTClient client;

WebServer server(80);

DateTime dateTime;
long timeNow;
Relay relay[MAX_RELAY];
Timmer timmer[MAX_RELAY];
AnalogINPUT analogInput[MAX_ANALOG];
DigitalInput digitalInput[MAX_DIGITAL];

void relayProcessing();
void TimmerHandler();
void digitalHandler();
void analogHandler();


void OTA_update() {     
    String host = "esp";
    host += String(ESP_getChipId());
    host += ".local";

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    /*use mdns for host name resolution*/
    if (!MDNS.begin(host.c_str())) {            //  http://esp193858948.local
        Serial.println("Error setting up MDNS responder!");
    }
    else {
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
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonArray& data = root.createNestedArray("relayStatus");

    for (int i = 0; i < MAX_RELAY; i++) {
        /*filename = "/timmer";
        filename += i;
        filename += ".txt";
        msg = readFileString(SPIFFS, filename.c_str());
        if (msg != NULL) {
            client.publish(mqtt_topic_config, msg);
            delay(100);
        }*/
        if (relay[i].status == RELAY_ON) {
            data.add("ON");
        }
        else {
            data.add("OFF");
        }
        
    }
    root.printTo(msg);
    client.publish(mqtt_topic_config, msg);
}

void connect() {
    Serial.print("connecting to wifi...");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }

    //MQTT connect client
    Serial.print("\nconnecting mqtt...");
    client.begin(mqtt_server.c_str(), mqtt_port, wifiClient);
    client.onMessage(messageReceived);

    while (!client.connect(mqtt_clientID.c_str(), mqtt_user.c_str(), mqtt_pwd.c_str())) {
        Serial.print(".");
        delay(1000);
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
        handleEventControl(payload);
    }
    else if (topic == mqtt_topic_reponse) {

    }
}


void setup_PinMode() {
    for (byte i = 0; i < MAX_RELAY; i++) {
        pinMode(relay[i].pinIO, OUTPUT);
        /* digitalWrite(relayPin[i], LOW);
         delay(200);*/
        digitalWrite(relay[i].pinIO, HIGH);
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
    wifiManager.autoConnect();

    //Connect wifi and mqtt
    connect();

    //Send config 
    MQTT_sendConfig();

    //RTC setup
    setupRTC();

    OTA_update();

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

    for (;;) {
        //update time
        dateTime = getRTC().now();
        timeNow = dateTime.unixtime();
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= 2000) {
            previousMillis = currentMillis;
            updateRTC();
            Serial.println(timeNow);
        }

        // mqtt
        if (!client.connected()) {
            connect();
        }
        client.loop();

        //OTA
        server.handleClient();
        delay(1);
    }
}

//Task2code:  xử lý điều khiển
void Task2code(void * pvParameters) {

    for (;;) {
        readSensorSHT(5000);

        TimmerHandler();
        digitalHandler();
        analogHandler();

        relayProcessing();
    }
}

void loop() {

    // Not nedded, Do nothing here
}

void relayProcessing()
{
    byte i, j;
    byte sum = 0;
    for (i = 0; i < MAX_RELAY; i++) {
        if (relay[i].numCondition < 1) {
            continue;   // Nếu không đc gắn cho bất kỳ tác động nào
        }
        else {
            for (j = 0; j < relay[i].numCondition; j++) {
                sum += relay[i].listCondition[j];
            }
            if (sum == RELAY_ON) {                 //Tất cả đều thỏa mãn là ON thì mới ON
                relay[i].status = RELAY_ON;
            }
            else {
                relay[i].status = RELAY_OFF;
            }

            if (relay[i].setRelay(i, relay[i].pinIO, relay[i].status) == 1) {   //nếu có sự thay đổi trạng thái relay thì cập nhật vào file
                updateRelayConfig(SPIFFS, i);
            }
        }

    }
}

void TimmerHandler()
{
    byte i;
    int relayid, relayConditionNumber, timmerInfluence;

    for (i = 0; i < MAX_RELAY; i++) {    // Duyệt timmer
        if (timmer[i].relayID != -1) {  // Nếu timmer đã đc gắn cho relay nào đó
            relayid = timmer[i].relayID;
            relayConditionNumber = timmer[i].relayConditionNumber;
            timmerInfluence = timmer[i].timmerInfluence;

            //Serial.print("relay id:   ");
            //Serial.println(timmer[i].relayID);
            relay[relayid].numCondition = relayConditionNumber;
            relayConditionNumber = relayConditionNumber - 1; //chỉ số thấp hơn giá trị 1 đơn vị

            if (timmer[i].timmerStart == 0 && timmer[i].timmerEnd == 0) { // Nếu là bật hoặc tắt hoặc trong thời gian hẹn giờ
                relay[relayid].listCondition[relayConditionNumber] = timmerInfluence;
            }
            else if (timeNow < timmer[i].timmerStart) { // Nếu chưa đến thời gian hẹn giờ thì đảo ngược trạng thái cài đặt
                relay[relayid].listCondition[relayConditionNumber] = !timmerInfluence;
            }
            else if (timeNow > timmer[i].timmerEnd) { // Nếu quá thời gian hẹn giờ thì đảo ngược trạng thái cài đặt, cộng thêm chu kỳ chuẩn bị cho tiếp theo
                relay[relayid].listCondition[relayConditionNumber] = !timmerInfluence;
                timmer[i].timmerStart += timmer[i].timmerCycle;
                timmer[i].timmerEnd += timmer[i].timmerCycle;
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

void digitalHandler()
{

}

void handleEventControl(String & payload)
{
    Serial.println(payload);

    String fileName = "";
    byte id;

    StaticJsonBuffer<500>jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);
    if (!root.success()) {
        Serial.println("JSON parse failed");
        return;
    }
    else {
        Serial.println("JSON parse OK!");
    }

    if (root["type"] == "remove") {
        String fileName = root["file"];
        fileName += ".txt";
        deleteFile(SPIFFS, fileName.c_str());
    }
    else {
        //{ "type": "Timmer", "id" : 0, "relayID" : 0, "rcn" : 1, "ts" : 0, "te" : 0, "tc" : 100, "ti" : 0 }
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
        else if (root["type"] == "Analog") {
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
        }

        fileName += id;
        fileName += ".txt";
        writeFile(SPIFFS, fileName.c_str(), payload.c_str());
    }
}


/*

{ "type": "Timmer", "id": 0, "relayID": 0, "relayConditionNumber": 1, "timmerStart": 0, "timmerEnd": 0, "timmerCycle": 100, "timmerInfluence": 0 }
{ "type": "Timmer", "id": 1, "relayID": 1, "relayConditionNumber": 1, "timmerStart": 1568759150, "timmerEnd": 1568759160, "timmerCycle": 30, "timmerInfluence": 0 }
{ "type": "Timmer", "id": 2, "relayID": 2, "relayConditionNumber": 1, "timmerStart": 1568759290, "timmerEnd": 1568759350, "timmerCycle": 0, "timmerInfluence": 0 }
{ "type": "Timmer", "id": 3, "relayID": 2, "relayConditionNumber": 2, "timmerStart": 1568759300, "timmerEnd": 1568759330, "timmerCycle": 0, "timmerInfluence": 0 }


{ "type": "Timmer", "id": 0, "relayID": 0, "rcn": 1, "ts": 0, "te": 0, "tc": 0, "ti": 0 }

//ESP2646569276

*/
