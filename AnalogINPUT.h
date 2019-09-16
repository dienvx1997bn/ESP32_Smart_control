#pragma once
#include "Relay.h"


#define MAX_ANALOG 4

class AnalogINPUT
{
public:
    int relayID = -1;
    byte relayConditionNumber;  // Điều kiện thứ i của relay.listCondigion

    int pinIO;
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


