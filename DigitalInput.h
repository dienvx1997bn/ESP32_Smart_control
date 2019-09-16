#pragma once
#include "Arduino.h"
#include "Relay.h"

#define MAX_DIGITAL 4

class DigitalInput
{
public:
    int relayID = -1;
    byte relayConditionNumber;  // Điều kiện thứ i của relay.listCondigion

    byte state;     // Giá trị đo
    byte digitalInfluence;  // Tác động kết quả lên relay
    String name;
    byte pinIO;


    DigitalInput();
    ~DigitalInput();

private:

};

DigitalInput::DigitalInput()
{
}

DigitalInput::~DigitalInput()
{
}