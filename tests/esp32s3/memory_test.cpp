#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include "../../firmware/common/library/LineBuffer.h"
#include "../../firmware/common/library/FixedJsonArena.h"
#include "../../firmware/xiao_s3/WeatherData.h"

namespace {
size_t heapAllocations = 0;
}

// リンカーのwrapでJSON解析中のヒープ確保を検出する（テスト専用）。
/** 関数名: __real_malloc / 内容: リンカーが提供する実mallocの宣言。 */
extern "C" void* __real_malloc(size_t size);
/** 関数名: __real_realloc / 内容: リンカーが提供する実reallocの宣言。 */
extern "C" void* __real_realloc(void* pointer, size_t size);
/** 関数名: __wrap_malloc / 内容: 確保回数を数えて実mallocへ渡す。入力: size。出力: 確保領域。 */
extern "C" void* __wrap_malloc(size_t size) {
  ++heapAllocations;
  return __real_malloc(size);
}
/** 関数名: __wrap_realloc / 内容: 再確保回数を数えて実reallocへ渡す。入力: pointer/size。出力:
 * 領域。 */
extern "C" void* __wrap_realloc(void* pointer, size_t size) {
  ++heapAllocations;
  return __real_realloc(pointer, size);
}

/** 関数名: main / 内容: 受信行と固定JSON領域の境界・繰返し回収を検証する。出力: 成功0。 */
int main() {
  tempescope::LineBuffer<4> line;
  using Result = tempescope::LineBuffer<4>::Result;
  assert(line.push('o') == Result::Pending);
  assert(line.push('f') == Result::Pending);
  assert(line.push('f') == Result::Pending);
  assert(line.push('\r') == Result::Pending);
  assert(line.push('\n') == Result::Complete);
  assert(std::strcmp(line.text(), "off") == 0);
  for (char value : std::string("abcd")) {
    assert(line.push(value) == Result::Pending);
  }
  assert(line.push('\n') == Result::Overflow);
  assert(line.push('x') == Result::Pending);
  assert(line.push('\n') == Result::Complete);
  assert(std::strcmp(line.text(), "x") == 0);

  tempescope::FixedJsonArena arena;
  void* first = arena.allocate(3);
  assert(first != nullptr);
  assert(reinterpret_cast<uintptr_t>(first) % alignof(std::max_align_t) == 0);
  std::memcpy(first, "abc", 3);
  void* expanded = arena.reallocate(first, 64);
  assert(expanded != nullptr && std::memcmp(expanded, "abc", 3) == 0);
  assert(arena.reallocate(expanded, 2) == expanded);
  assert(arena.allocate(SIZE_MAX) == nullptr);
  assert(arena.reallocate(expanded, SIZE_MAX) == nullptr);
  assert(std::memcmp(expanded, "abc", 3) == 0);
  arena.reset();
  assert(arena.allocate(3) == first);

  const char* valid = R"({"current":{"time":1799999999,"weather_code":61,"is_day":1}})";
  tempescope::WeatherSample sample;
  const size_t allocationsBefore = heapAllocations;
  for (int iteration = 0; iteration < 2000; ++iteration) {
    assert(tempescope::parseWeather(valid, std::strlen(valid), 1800000000, 42, sample));
  }
  assert(heapAllocations == allocationsBefore);

  // ホスト側だけで大きな入力を生成する。実機HTTP応答は2048バイトに制限される。
  std::string excessive = "[0";
  for (int index = 0; index < 10000; ++index) {
    excessive += ",0";
  }
  excessive += "]";
  const tempescope::WeatherSample previous = sample;
  assert(!tempescope::parseWeather(excessive.c_str(), excessive.size(), 1800000000, 99, sample));
  assert(sample.received == previous.received && sample.observed == previous.observed);
  assert(tempescope::parseWeather(valid, std::strlen(valid), 1800000000, 100, sample));
  assert(sample.received == 100);
}
