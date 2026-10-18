#pragma once
// Copy LocalConfig.example.h to LocalConfig.h; never commit credentials.
#if __has_include("LocalConfig.h")
#include "LocalConfig.h"
#endif
#ifndef TEMPESCOPE_HARDWARE_VALIDATED
#define TEMPESCOPE_HARDWARE_VALIDATED 0
#endif
#ifndef TEMPESCOPE_LATITUDE
#define TEMPESCOPE_LATITUDE 34.7108
#endif
#ifndef TEMPESCOPE_LONGITUDE
#define TEMPESCOPE_LONGITUDE 137.7261
#endif
// Raw SoC GPIO numbers, NOT Arduino Uno pin numbers.
constexpr int kPumpPin = 1;   // D0
constexpr int kMistPin = 2;   // D1
constexpr int kFanPin = 4;    // D3; D2/GPIO3 is a strapping pin, unused
constexpr int kRedPin = 5;    // D4
constexpr int kGreenPin = 7;  // D8
constexpr int kBluePin = 8;   // D9
constexpr int kPumpWaterPin = 43; // D6, contact CLOSED to GND when water is safe
constexpr int kMistWaterPin = 44; // D7, same convention; may share one contact
constexpr int kStopPin = 9;       // D10, NC stop loop to GND
constexpr int kResetPin = 6;      // D5, momentary NO to GND; fault reset only
