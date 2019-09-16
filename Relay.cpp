#include "Relay.h"
#include "Arduino.h"

Relay::Relay()
{
}

Relay::~Relay()
{
}

void Relay::setRelay(byte pinIO, byte state)
{
    digitalWrite(pinIO, state);
}
