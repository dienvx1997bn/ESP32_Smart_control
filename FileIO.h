#pragma once
#include <SPIFFS.h>
#include "Relay.h"
#include "AnalogINPUT.h"
#include "ArduinoJson.h"
#include "Config.h"

char fileData[512];
int counter = 0;
extern Relay relay[MAX_RELAY];
extern AnalogINPUT analogInput[MAX_ANALOG];



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

int readFile(fs::FS &fs, const char * path) {
    //Serial.printf("Reading file: %s\r\n", path);
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
    return 1;
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
        Serial.println("- frite failed");
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

    device_id = root["device_id"].asString();
    mqtt_topic_pub += device_id;
    mqtt_topic_config += device_id;
    mqtt_topic_sub += device_id;
    mqtt_user = root["mqtt_user"].asString();
    mqtt_pwd = root["mqtt_pwd"].asString();

    host += device_id;
}

int readWifiConfig(fs::FS &fs) {
    String relayFileName = "/wifiConfig.txt";
    memset(fileData, 0, 512);
    readFile(fs, relayFileName.c_str());

    String data = String(fileData);

    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(data);
    if (!root.success()) {
        Serial.println("wifiConfig Not success!");
        return -1;
    }

    ssid = root["ssid"].asString();
    password = root["password"].asString();
    mqtt_server = root["mqtt_server"].asString();
    mqtt_port = root["mqtt_port"];


}

int readRelayConfig(fs::FS &fs) {
    String relayFileName = "/relay";
    for (byte i = 0; i < MAX_RELAY; i++) {
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

        relay[i].pinIO = root["pinIO"];
        relay[i].name = root["name"].asString();
        relay[i].numCondition = root["numCondition"];
        for (byte j = 0; j < relay[i].numCondition; j++) {
            relay[i].listCondition[j] = root["listCondition"][j];
        }
        relay[i].action = root["action"];

        counter = 0;
        relayFileName = "/relay";
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


