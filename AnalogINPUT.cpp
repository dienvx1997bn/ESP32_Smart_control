#include "AnalogINPUT.h"

AnalogINPUT::AnalogINPUT()
{
    byte i;
    for (i = 0; i < MAX_RELAY; i++)
    {
        relayID[i] = -1;
    }
}

AnalogINPUT::~AnalogINPUT()
{
}