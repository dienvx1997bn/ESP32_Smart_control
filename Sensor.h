#pragma once
#include <Arduino.h>
#include "AnalogINPUT.h"

// Specify data and clock connections and instantiate SHT1x object
extern AnalogINPUT analogInput[MAX_ANALOG];


//
//
//void updateAnallog() {
//    analogInput[0].value = Sht10.temp_c * analogInput[0].gain;
//    analogInput[1].value = Sht10.humidity * analogInput[1].gain;
//}
//
//int sensorError() {
//    if (Sht10.temp_c < -40 || Sht10.temp_c > 125 || Sht10.humidity < 0 || Sht10.humidity > 100)
//        return 1;
//    return 0;
//}

