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
// As-built image(3).png: default to local, supervised commissioning.
#ifndef TEMPESCOPE_BENCH_MODE
#define TEMPESCOPE_BENCH_MODE 1
#endif
#ifndef TEMPESCOPE_BENCH_PUMP_ENABLED
#define TEMPESCOPE_BENCH_PUMP_ENABLED 0
#endif
#ifndef TEMPESCOPE_BENCH_MIST_ENABLED
#define TEMPESCOPE_BENCH_MIST_ENABLED 0
#endif
// Raw SoC GPIO numbers, NOT Arduino Uno pin numbers.
constexpr int kPumpPin = 1;   // D0
constexpr int kMistPin = 2;   // D1
constexpr int kFanPin = 3;    // D2 -> TD62083 I8/O8; GPIO3 is a strapping pin
constexpr int kRedPin = 9;    // D10 -> TD62083 I4/O4
constexpr int kGreenPin = 8;  // D9 -> TD62083 I5/O5
constexpr int kBluePin = 7;   // D8 -> TD62083 I6/O6
constexpr int kPumpWaterPin = 43; // D6, contact CLOSED to GND when water is safe
constexpr int kMistWaterPin = 44; // D7, same convention; may share one contact
constexpr int kStopPin = 5;       // D4, RESERVED NC stop loop; absent on current board
constexpr int kResetPin = 6;      // D5, momentary NO to GND; fault reset only
