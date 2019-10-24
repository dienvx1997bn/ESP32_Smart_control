#include "Relay.h"
#include "Arduino.h"

extern Relay relay[MAX_RELAY];

Relay::Relay()
{
}

Relay::~Relay()
{
}

bool Relay::setRelay(byte index, byte pinIO, byte state)
{
    if (relay[index].oldStatus != state) {  //Nếu có sư thay đổi trạng thái 
        digitalWrite(pinIO, state);         //Thay đổi trang thái relay
        relay[index].oldStatus = state;     //Lưu thay đổi trạng thái cũ
        return 1;
    }
    return 0;
}


