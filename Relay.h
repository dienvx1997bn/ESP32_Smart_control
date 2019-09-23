#pragma once
#include <ArduinoJson.h>
#include "Arduino.h"

#define MAX_RELAY 4
#define MAX_CONDITION 4


#define RELAY_ON    0
#define RELAY_OFF   1


class Relay
{
public:
    byte pinIO;
    String name;
    byte listCondition[MAX_CONDITION];
    byte numCondition = 0;
    byte action;


public:
    Relay();
    ~Relay();

    void setRelay(byte pinIO, byte state);

};


