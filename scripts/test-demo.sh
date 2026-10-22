#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
demo_build_dir=$(mktemp -d)
trap 'rm -rf "$demo_build_dir"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  tests/esp32s3/demo_test.cpp -o "$demo_build_dir/demo_test"
ASAN_OPTIONS=detect_leaks=0 "$demo_build_dir/demo_test"
