#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
memory_build_dir=$(mktemp -d)
trap 'rm -rf "$memory_build_dir"' EXIT
json_include="${ARDUINOJSON_INCLUDE:-$HOME/Arduino/libraries/ArduinoJson/src}"
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Wl,--wrap=malloc,--wrap=realloc -I "$json_include" tests/esp32s3/memory_test.cpp -o "$memory_build_dir/memory_test"
ASAN_OPTIONS=detect_leaks=0 "$memory_build_dir/memory_test"
echo 'PASS: line buffer, fixed JSON allocation, exhaustion and recovery'
python3 scripts/check-architecture.py
