#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Arduino-ESP32 3.3.8を事前にインストールする。追加ライブラリは不要。
arduino-cli compile \
  --fqbn esp32:esp32:XIAO_ESP32S3:PartitionScheme=max_app_8MB,PSRAM=opi,CDCOnBoot=cdc \
  --warnings all --output-dir "${BUILD_OUTPUT_DIR:-/tmp/tempescope-demo}" \
  "$@" firmware/xiao_s3_demo
