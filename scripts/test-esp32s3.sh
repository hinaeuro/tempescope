#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  tests/esp32s3/safety_test.cpp -o "$build_dir/safety_test"
ASAN_OPTIONS=detect_leaks=0 "$build_dir/safety_test"
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  tests/esp32s3/bench_test.cpp -o "$build_dir/bench_test"
ASAN_OPTIONS=detect_leaks=0 "$build_dir/bench_test"
json_include="${ARDUINOJSON_INCLUDE:-$HOME/Arduino/libraries/ArduinoJson/src}"
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I "$json_include" tests/esp32s3/weather_test.cpp -o "$build_dir/weather_test"
ASAN_OPTIONS=detect_leaks=0 "$build_dir/weather_test"


bash scripts/test-memory.sh
