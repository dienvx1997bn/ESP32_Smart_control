#pragma once
#include <TimeLib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#include "RTClib.h"

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

void setupRTC();
RTC_DS3231 getRTC();
void updateRTC();
