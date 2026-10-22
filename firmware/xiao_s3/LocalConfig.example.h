#pragma once
// Set to 1 ONLY after completing docs/esp32s3/commissioning.md electrical checks.
#define TEMPESCOPE_HARDWARE_VALIDATED 0
#define TEMPESCOPE_LATITUDE 34.7108
#define TEMPESCOPE_LONGITUDE 137.7261
// Network credentials are provisioned by Matter BLE commissioning, not in source.

// Current as-built board: local serial pulses, without water/stop inputs.
#define TEMPESCOPE_BENCH_MODE 1
// Enable ONLY after pump driver/current and real water supply are checked.
#define TEMPESCOPE_BENCH_PUMP_ENABLED 0
// Enable after coil drive/5V rail and mist water depth are checked.
#define TEMPESCOPE_BENCH_MIST_ENABLED 0
