#include "Relay.h"
#include "Arduino.h"

//#define HC595               //To use HC595         
#define NOMAL               //To use nomal io pin

extern Relay relay[MAX_RELAY];

int hc595_latchPin = 15;     //STCP
int hc595_clockPin = 14;    //SHCP
int hc595_dataPin = 13;     //DS

byte leds = 0;  // Variable to hold the pattern of which LEDs are currently turned on or off


Relay::Relay()
{
    pinMode(hc595_latchPin, OUTPUT);
    pinMode(hc595_dataPin, OUTPUT);
    pinMode(hc595_clockPin, OUTPUT);
}

Relay::~Relay()
{
}

/*
 * updateShiftRegister() - This function sets the latchPin to low, then calls the Arduino function 'shiftOut' 
 to shift out contents of variable 'leds' in the shift register before putting the 'latchPin' high again.
 */
void updateShiftRegister()
{
    digitalWrite(hc595_latchPin, LOW);
    shiftOut(hc595_dataPin, hc595_clockPin, LSBFIRST, leds);
    digitalWrite(hc595_latchPin, HIGH);
}


bool Relay::setRelay(byte index, byte pinIO, byte status)
{
    if (relay[index].oldStatus != status) {  //Nếu có sư thay đổi trạng thái 
#ifdef HC595
        if (status == RELAY_ON) {
            leds += pow(2, index);
        }
        else if (status == RELAY_OFF) {
            leds -= pow(2, index);
        }

        setRelay595();
#endif // HC595

#ifdef NOMAL
        digitalWrite(pinIO, status);         //Thay đổi trang thái relay
#endif // HC595
        
        relay[index].oldStatus = status;     //Lưu thay đổi trạng thái cũ
        return 1;
    }
    return 0;
}

void Relay::setRelay595()
{
    updateShiftRegister();
    for (int i = 0; i < 8; i++) // Turn all the LEDs ON one by one.
    {
        bitSet(leds, i);        // Set the bit that controls that LED in the variable 'leds'
        updateShiftRegister();
    }
}


