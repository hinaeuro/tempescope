#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Install separately: arduino-cli core install esp32:esp32@3.3.8
# and arduino-cli lib install ArduinoJson@7.3.1
arduino-cli compile \
  --fqbn esp32:esp32:XIAO_ESP32S3:PartitionScheme=max_app_8MB,PSRAM=opi,CDCOnBoot=cdc \
  --warnings all --output-dir "${BUILD_OUTPUT_DIR:-/tmp/tempescope-esp32s3}" \
  "$@" firmware/xiao_s3
