#pragma once
#include "Arduino.h"
#include "Relay.h"
#include "MUX74HC4067.h"

#define MAX_DIGITAL 16
#define TIME_PRESS 5

struct DigitalInput
{
    int isActive = 0;
    int status;     // Giá trị đo
    int old_status = 0;     // Giá trị đo
};
