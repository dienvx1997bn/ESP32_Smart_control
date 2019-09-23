#pragma once
#include <SHT1x.h>
#include <Arduino.h>
#include "AnalogINPUT.h"

// Specify data and clock connections and instantiate SHT1x object
extern AnalogINPUT analogInput[MAX_ANALOG];

#define dataPin  2
#define clockPin 4
SHT1x sht1x(dataPin, clockPin);

struct SHTSensor {
    float temp_c;
    float temp_f;
    float humidity;
}Sht10;


void updateAnallog() {
    analogInput[0].value = Sht10.temp_c * analogInput[0].gain;
    analogInput[1].value = Sht10.humidity * analogInput[1].gain;
}

int sensorError() {
    if (Sht10.temp_c < -40 || Sht10.temp_c > 125 || Sht10.humidity < 0 || Sht10.humidity > 100)
        return 1;
    return 0;
}

void readSensorSHT(long timeCycle) {
    unsigned long currentMillis = millis();
    static unsigned long previousMillis = 0;

    if (currentMillis - previousMillis >= timeCycle) {
        previousMillis = currentMillis;
        // Read values from the sensor
        // Save old value

        // Update current value
        Sht10.temp_c = sht1x.readTemperatureC();
        Sht10.humidity = sht1x.readHumidity();
        //#ifdef DEBUG
        Serial.print("temp:  ");
        Serial.print(Sht10.temp_c);
        Serial.print("\thum: ");
        Serial.println(Sht10.humidity);
        //#endif // DEBUG
        updateAnallog();
    }
}