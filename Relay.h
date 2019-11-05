#pragma once

#if ARDUINO < 100
#include <WProgram.h>
#else
#include <Arduino.h>
#endif
#include <ArduinoJson.h>

#define MAX_RELAY 16
#define MAX_CONDITION 3


#define RELAY_ON    0
#define RELAY_OFF   1


struct Relay
{

    String name;
    int listCondition[MAX_CONDITION];
    int numCondition = 0;
    int status = RELAY_OFF;
    int oldStatus = -1;

};


