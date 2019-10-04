#pragma once
#include "Relay.h"


#define MAX_ANALOG 2

class AnalogINPUT
{
public:
    int relayID[MAX_RELAY];
    byte relayConditionNumber[MAX_RELAY];  // Điều kiện thứ i của relay.listCondigion

    byte id;
    String name;
    float value;
    float upper;
    float lower;
    float gain;

    int analogInfluence;

    AnalogINPUT();
    ~AnalogINPUT();

private:

};


