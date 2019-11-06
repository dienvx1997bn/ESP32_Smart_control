#pragma once
#include <SPIFFS.h>
#include "Relay.h"
#include "AnalogINPUT.h"
#include "ArduinoJson.h"
#include "Config.h"
#include "Timmer.h"
#include "Arduino.h"
#include "DigitalInput.h"

char fileData[512];
int counter = 0;
extern Relay relay[MAX_RELAY];
extern Timmer timmer[MAX_TIMMER];
extern AnalogINPUT analogInput[MAX_ANALOG];
extern DigitalInput digitalInput[MAX_DIGITAL];


#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())


void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("- failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels) {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

int readFileString(fs::FS &fs, const char * path, String &str) {
    //String str = "";
    byte size = 0;
    File file = fs.open(path);
    while (file.available()) {
        char data = file.read();
        str += data;
        size++;
    }
    return size;
}

int readFile(fs::FS &fs, const char * path) {
    Serial.printf("Reading file: %s\r\n", path);
    counter = 0;
    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        //Serial.println("- failed to open file for reading");
        return 0;
    }

    //Serial.println("- read from file:");
    while (file.available()) {
        char data = file.read();
        fileData[counter] = data;
        counter++;
    }
    return counter;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("- failed to open file for writing");
        return;
    }
    if (file.print(message)) {
        Serial.println("- file written");
    }
    else {
        Serial.println("- write failed");
    }
}

void deleteFile(fs::FS &fs, const char * path) {
    Serial.printf("Deleting file: %s\r\n", path);
    if (fs.remove(path)) {
        Serial.println("- file deleted");
    }
    else {
        Serial.println("- delete failed");
    }
}

int readDeviceInfor(fs::FS &fs) {
    String relayFileName = "/deviceinfor.txt";
    memset(fileData, 0, 512);
    readFile(fs, relayFileName.c_str());

    String data = String(fileData);
    //Serial.println(data);

    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(data);
    if (!root.success()) {
        Serial.println("deviceinfor Not success!");
        return -1;
    }

    device_id = String(ESP_getChipId());
    mqtt_topic_request += device_id;
    mqtt_topic_config += device_id;
    mqtt_topic_control += device_id;
    mqtt_user = root["mqtt_user"].asString();
    mqtt_pwd = root["mqtt_pwd"].asString();
    ssid = root["ssid"].asString();
    password = root["password"].asString();
    mqtt_server = root["mqtt_server"].asString();
    mqtt_port = root["mqtt_port"];

}

void readDigitalInput(fs::FS &fs) {
    String relayFileName = "/inputs.txt";
    memset(fileData, 0, 512);
    if (!readFile(fs, relayFileName.c_str())) {
        return;
    }
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(fileData);
    if (!root.success()) {
        Serial.println("relay Not success!");
        return;
    }
    for (byte i = 0; i < MAX_DIGITAL; i++) {
        digitalInput[i].old_status = root["status"][i];
        //digitalInput[i].status = digitalInput[i].old_status;
    }

}


int readRelayConfig(fs::FS &fs) {
    String relayFileName = "/relay";
    for (byte i = 0; i < MAX_RELAY; i++) {
        relayFileName = "/relay";
        memset(fileData, 0, 512);
        relayFileName += i;
        relayFileName += ".txt";
        if (!readFile(fs, relayFileName.c_str())) {
            continue;
        }
        //Serial.println(fileData);
        // update data relay
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(fileData);
        if (!root.success()) {
            Serial.println("relay Not success!");
            continue;
        }

        relay[i].name = root["name"].asString();
        relay[i].numCondition = root["numCondition"];
        for (byte j = 0; j < relay[i].numCondition; j++) {
            relay[i].listCondition[j] = root["listCondition"][j];
        }
        relay[i].status = root["action"];
        relay[i].oldStatus = relay[i].status;

        counter = 0;
    }
}

int readAnalogConfig(fs::FS &fs) {
    String relayFileName = "/analog";
    for (byte i = 0; i < MAX_ANALOG; i++) {
        memset(fileData, 0, 512);
        relayFileName += i;
        relayFileName += ".txt";
        if (!readFile(fs, relayFileName.c_str())) {
            continue;
        }        
        //Serial.println(fileData);
        // update data relay
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(fileData);
        if (!root.success()) {
            Serial.println("analog Not success!");
            continue;
        }

        byte relayID = root["relayID"];
        analogInput[i].relayID[relayID] = relayID;
        analogInput[i].id = root["id"];
        analogInput[i].name = root["name"].asString();
        analogInput[i].relayConditionNumber[relayID] = root["relayConditionNumber"];
        
        analogInput[i].upper = root["upper"];
        analogInput[i].lower = root["lower"];
        analogInput[i].gain = root["gain"];

        analogInput[i].analogInfluence = root["analogInfluence"];

        counter = 0;
        relayFileName = "/analog";
        //Serial.println(analogInput[i].name);
    }
}

//
int readTimmerConfig(fs::FS &fs) {
    String timmerFileName = "/timmer";
    for (byte i = 0; i < MAX_TIMMER; i++) {
        memset(fileData, 0, 512);
        timmerFileName = "/timmer";
        timmerFileName += i;
        timmerFileName += ".txt";
        if (!readFile(fs, timmerFileName.c_str())) {
            continue;
        }
        //Serial.println(fileData);
        // update data relay
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(fileData);
        if (!root.success()) {
            Serial.println("timmer Not success!");
            continue;
        }

        timmer[i].relayID = root["relayID"];
        timmer[i].relayConditionNumber = root["rcn"];
        timmer[i].timmerStart = root["ts"];
        timmer[i].timmerEnd = root["te"];
        timmer[i].timmerCycle = root["tc"];
        timmer[i].timmerInfluence = root["ti"];

    }
}

void updateRelayConfig(fs::FS &fs, byte index)
{
    String relayData = "";
    String fileName = "/relay";
    fileName += index;
    fileName += ".txt";
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonArray& listCondition = root.createNestedArray("listCondition");

    root["name"] = relay[index].name;
    root["numCondition"] = relay[index].numCondition;
    for (byte j = 0; j < relay[index].numCondition; j++) {
        listCondition.add(relay[index].listCondition[j]);
    }
    root["action"] = relay[index].status;

    root.printTo(relayData);
    writeFile(fs, fileName.c_str(), relayData.c_str());
}


