#pragma once

#if ARDUINO < 100
#include <WProgram.h>
#else
#include <Arduino.h>
#endif
#include <ArduinoJson.h>

#define MAX_RELAY 8
#define MAX_CONDITION 4


#define RELAY_ON    0
#define RELAY_OFF   1

static byte leds = 0B11111111;


struct Relay
{

    String name;
    byte listCondition[MAX_CONDITION];
    byte numCondition = 0;
    byte status = RELAY_OFF;
    byte oldStatus = 2;

};


