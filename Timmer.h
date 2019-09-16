#pragma once
#include "Relay.h"
#include "Arduino.h"



class Timmer
{
public:
    int relayID = -1;   
    int relayConditionNumber;  // Điều kiện thứ i của relay.listCondigion
    int timmerCycle;    // chu trình vòng lặp 
    long timmerStart = 0;;  // Thời gian bắt đầu
    long timmerEnd = 0;;    // Thời gian kết thúc
    int timmerInfluence = RELAY_OFF;   // Tác động lên relay

    Timmer();
    ~Timmer();



private:

};

Timmer::Timmer()
{
}

Timmer::~Timmer()
{
}


