﻿
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "RTClib.h"

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
PubSubClient client(wifiClient);

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

void handleEvent(byte *payload);


void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    randomSeed(micros());
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");

        // Attempt to connect
        if (client.connect(mqtt_clientID.c_str(), mqtt_user.c_str(), mqtt_pwd.c_str())) {
            Serial.println("connected");
            //Once connected, publish an announcement...
            //client.publish(mqtt_topic_pub, "hello world");
            // ... and resubscribe
            client.subscribe(mqtt_topic_sub.c_str(), 0);
        }
        else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}


void callback(char* topic, byte *payload, unsigned int length) {
    //print recevied messages on the serial console
    //Serial.println("-------new message from broker-----");
    //Serial.print("channel:");
    //Serial.println(topic);
    //Serial.print("data:");
    //Serial.write(payload, length);
    //Serial.println();

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

    //Doc cau hinh
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    listDir(SPIFFS, "/", 0);
    readWifiConfig(SPIFFS);
    delay(10);
    readDeviceInfor(SPIFFS);
    delay(10);
    readRelayConfig(SPIFFS);
    delay(10);
    readAnalogConfig(SPIFFS);
    delay(10);
    //Serial.println();
    //readFile(SPIFFS, "/units.txt");

    Serial.setTimeout(500);// Set time out for 
    setup_PinMode();
    setup_wifi();
    client.setServer(mqtt_server.c_str(), mqtt_port);
    client.setCallback(callback);

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
        if (currentMillis - previousMillis >= 1000) {
            previousMillis = currentMillis;
            updateRTC();
            Serial.println(timeNow);
        }

        // mqtt
        if (!client.connected()) {
            reconnect();
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
    byte i;
    int id, relayConditionNumber, analogInfluence;

    for (i = 0; i < MAX_ANALOG; i++) {
        if (analogInput[i].relayID != -1) {
            id = analogInput[i].relayID;
            relayConditionNumber = analogInput[i].relayConditionNumber;
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

void digitalHandler()
{

}

void handleEvent(byte * payload)
{
    String fileName = "";
    byte id;

    StaticJsonBuffer<500> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject((char *)payload);
    if (!root.success()) {
        Serial.println("JSON parse failed");
        return;
    }

    if (root["type"] == "Timmer") {
        //Serial.print("Timmer action");
        id = root["id"];
        timmer[id].relayID = root["relayID"];
        timmer[id].relayConditionNumber = root["relayConditionNumber"];
        timmer[id].timmerStart = root["timmerStart"];
        timmer[id].timmerEnd = root["timmerEnd"];
        timmer[id].timmerCycle = root["timmerCycle"];
        timmer[id].timmerInfluence = root["timmerInfluence"];

        fileName = "/timmer";
        

    }
    else if (root["type"] == "Analog") {
        id = root["id"];
        analogInput[id].relayID = root["relayID"];
        analogInput[id].relayConditionNumber = root["relayConditionNumber"];
        analogInput[id].name = root["name"].asString();
        analogInput[id].upper = root["upper"];
        analogInput[id].lower = root["lower"];
        analogInput[id].gain = root["gain"];
        analogInput[id].analogInfluence = root["analogInfluence"];

        fileName = "/analog";
    }

    fileName += id;
    fileName += ".txt";
    writeFile(SPIFFS, fileName.c_str(), (char *)payload);
}


/*
{ "type": "Timmer", "id": 0, "relayID": 0, "relayConditionNumber": 1, "timmerStart": 0, "timmerEnd": 0, "timmerCycle": 100, "timmerInfluence": 0 }
{ "type": "Timmer", "id": 1, "relayID": 1, "relayConditionNumber": 1, "timmerStart": 1568759150, "timmerEnd": 1568759160, "timmerCycle": 30, "timmerInfluence": 0 }
{ "type": "Timmer", "id": 2, "relayID": 2, "relayConditionNumber": 1, "timmerStart": 1568759290, "timmerEnd": 1568759350, "timmerCycle": 0, "timmerInfluence": 0 }
{ "type": "Timmer", "id": 3, "relayID": 2, "relayConditionNumber": 2, "timmerStart": 1568759300, "timmerEnd": 1568759330, "timmerCycle": 0, "timmerInfluence": 0 }


*/