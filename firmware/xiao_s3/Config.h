#pragma once
// 利用者設定はLocalConfig.hへ。認証情報はコミットしない。
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
// 現在の実装回路: 既定はUSBからの単発試運転。
#ifndef TEMPESCOPE_BENCH_MODE
#define TEMPESCOPE_BENCH_MODE 1
#endif
#ifndef TEMPESCOPE_BENCH_PUMP_ENABLED
#define TEMPESCOPE_BENCH_PUMP_ENABLED 0
#endif
#ifndef TEMPESCOPE_BENCH_MIST_ENABLED
#define TEMPESCOPE_BENCH_MIST_ENABLED 1
#endif
// 数値はESP32-S3のGPIO番号。XIAOのD番号とは区別する。
constexpr int kPumpPin = 1;        // D0
constexpr int kMistPin = 2;        // D1
constexpr int kFanPin = 3;         // D2 -> TD62083 I8/O8; GPIO3は起動設定端子
constexpr int kRedPin = 9;         // D10 -> TD62083 I4/O4
constexpr int kGreenPin = 8;       // D9 -> TD62083 I5/O5
constexpr int kBluePin = 7;        // D8 -> TD62083 I6/O6
constexpr int kPumpWaterPin = 43;  // D6, 水位正常時にGNDと閉じる接点
constexpr int kMistWaterPin = 44;  // D7, 同じ接点方式。共通接点も可
constexpr int kStopPin = 5;        // D4, 停止NC接点の予約端子。現在は未実装
constexpr int kResetPin = 6;       // D5, 押すとGNDとなるNO接点。故障解除専用
