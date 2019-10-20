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

#include "WebOTA.h"
#include "FileIO.h"
#include "RTCTime.h"
#include "Config.h"
#include "Relay.h"
#include "AnalogINPUT.h"
#include "DigitalInput.h"
#include "Timmer.h"
#include "Sensor.h"


#define DEBUG

// Mutil task
TaskHandle_t Task1;
TaskHandle_t Task2;

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

//
//void OTA_update() {
//    //host += ".local";
//    /*use mdns for host name resolution*/
//    if (!MDNS.begin("http://esp32.local")) { //http://esp32.local
//        Serial.println("Error setting up MDNS responder!");
//    }
//    else Serial.println("mDNS responder started");
//
//    /*return index page which is stored in serverIndex */
//    server.on("/", HTTP_GET, []() {
//        server.sendHeader("Connection", "close");
//        server.send(200, "text/html", loginIndex);
//    });
//    server.on("/serverIndex", HTTP_GET, []() {
//        server.sendHeader("Connection", "close");
//        server.send(200, "text/html", serverIndex);
//    });
//    /*handling uploading firmware file */
//    server.on("/update", HTTP_POST, []() {
//        server.sendHeader("Connection", "close");
//        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
//        ESP.restart();
//    }, []() {
//        HTTPUpload& upload = server.upload();
//        if (upload.status == UPLOAD_FILE_START) {
//            Serial.printf("Update: %s\n", upload.filename.c_str());
//            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
//                Update.printError(Serial);
//            }
//        }
//        else if (upload.status == UPLOAD_FILE_WRITE) {
//            /* flashing firmware to ESP*/
//            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
//                Update.printError(Serial);
//            }
//        }
//        else if (upload.status == UPLOAD_FILE_END) {
//            if (Update.end(true)) { //true to set the size to the current progress
//                Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
//            }
//            else {
//                Update.printError(Serial);
//            }
//        }
//    });
//    server.begin();
//}

//int setup_wifi() {
//    unsigned long previousMillis = 0;  // will store last time LED was updated
//    const long interval = 30000;  // interval at which to blink (milliseconds)
//    previousMillis = millis();
//
//    delay(10);
//    // We start by connecting to a WiFi network
//    Serial.println();
//    Serial.print("Connecting to ");
//    Serial.println(ssid);
//    WiFi.begin(ssid.c_str(), password.c_str());
//    while (WiFi.status() != WL_CONNECTED) {
//        if (millis() - previousMillis >= interval) {
//            return 0;
//        }
//
//        delay(500);
//        Serial.print(".");
//    }
//    randomSeed(micros());
//    Serial.println("");
//    Serial.println("WiFi connected");
//    Serial.println("IP address: ");
//    Serial.println(WiFi.localIP());
//
//    return -1;
//}

void connect() {
    Serial.print("connecting to wifi...");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }

    Serial.print("\nconnecting...");
    while (!client.connect(mqtt_clientID.c_str(), mqtt_user.c_str(), mqtt_pwd.c_str())) {
        Serial.print(".");
        delay(1000);
    }

    Serial.println("\nconnected!");

    client.subscribe(mqtt_topic_sub.c_str());
    Serial.print("topic control:  ");
    Serial.println(mqtt_topic_sub.c_str());
    // client.unsubscribe("/hello");
}

void messageReceived(String &topic, String &payload) {
    //Serial.println("incoming: " + topic + " - " + payload);
    //payload[length] = 0;
#ifdef DEBUG
    Serial.println("incoming topic: " + topic);
#endif // DEBUG

    handleEvent(payload);
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
    readAnalogConfig(SPIFFS);
    delay(10);

    Serial.setTimeout(500);
    
    setup_PinMode();
    
      ////wifimanager here
    WiFiManager wifiManager;
    wifiManager.autoConnect();

    //MQTT connect

    client.begin(mqtt_server.c_str(), mqtt_port, wifiClient);
    client.onMessage(messageReceived);

    connect();

    //RTC setup
    setupRTC();

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


        /*Serial.print("Task1 running on core ");
        Serial.println(xPortGetCoreID());*/
    }
}

//Task2code:  xử lý điều khiển
void Task2code(void * pvParameters) {
    /*Serial.print("Task2 running on core ");
    Serial.println(xPortGetCoreID());*/

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
            //Serial.print("relay id: ");
            //Serial.println(i);
            //Serial.println("relay numCondition");
            //Serial.println(relay[i].numCondition);

            for (j = 0; j < relay[i].numCondition; j++) {
                sum += relay[i].listCondition[j];
            }
            if (sum == 0) {
                relay[i].action = RELAY_ON;
            }
            else {
                relay[i].action = RELAY_OFF;
            }

            relay[i].setRelay(relay[i].pinIO, relay[i].action);
        }

    }
}

void TimmerHandler()
{
    byte i;
    int id, relayConditionNumber, timmerInfluence;

    for (i = 0; i < MAX_RELAY; i++) {    // Duyệt timmer
        if (timmer[i].relayID != -1) {  // Nếu timmer đã đc gắn cho relay nào đó
            id = (int)timmer[i].relayID;
            relayConditionNumber = (int)timmer[i].relayConditionNumber;
            timmerInfluence = (int)timmer[i].timmerInfluence;

            //Serial.print("relay id:   ");
            //Serial.println(timmer[i].relayID);
            relay[id].numCondition = relayConditionNumber;
            relayConditionNumber--;

            if (timmer[i].timmerEnd == 0 || (timeNow >= timmer[i].timmerStart && timeNow < timmer[i].timmerEnd)) { // Nếu là bật hoặc tắt hoặc trong thời gian hẹn giờ
                relay[id].listCondition[relayConditionNumber] = timmerInfluence;
            }
            else if (timeNow < timmer[i].timmerStart) { // Nếu chưa đến thời gian hẹn giờ thì đảo ngược trạng thái cài đặt
                relay[id].listCondition[relayConditionNumber] = !timmerInfluence;
            }
            else if (timeNow > timmer[i].timmerEnd) { // Nếu quá thời gian hẹn giờ thì đảo ngược trạng thái cài đặt, cộng thêm chu kỳ chuẩn bị cho tiếp theo
                relay[id].listCondition[relayConditionNumber] = !timmerInfluence;
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

void handleEvent(String & payload)
{
    Serial.println(payload);

    String fileName = "";
    byte id;
    
    DynamicJsonBuffer jsonBuffer;
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

*/
