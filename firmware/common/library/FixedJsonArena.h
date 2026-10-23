#pragma once
#include <ArduinoJson.h>
#include <cstddef>
#include <cstring>

namespace tempescope {
// 1回のJSON解析に使う固定領域。解放は解析完了後に領域全体で行う。
class FixedJsonArena : public ArduinoJson::Allocator {
 public:
  static constexpr size_t kCapacity = 16384;

  /** 関数名: reset / 内容: 使用位置を戻す。注意: JsonDocument破棄後にだけ呼ぶ。 */
  void reset() {
    used_ = 0;
  }

  /**
   * 関数名: allocate
   * 内容: 整列済みの固定領域から指定サイズを確保する。
   * 入力: size=バイト数。出力: 領域先頭、容量不足ならnullptr。ヒープは使用しない。
   */
  void* allocate(size_t size) override {
    const size_t padding = (kAlignment - used_ % kAlignment) % kAlignment;
    size_t payload = 0;
    if (size == 0 || padding > kCapacity - used_) {
      return nullptr;
    }
    payload = used_ + padding;
    if (kHeaderSize > kCapacity - payload) {
      return nullptr;
    }
    payload += kHeaderSize;
    if (size > kCapacity - payload) {
      return nullptr;
    }
    std::memcpy(storage_ + payload - kHeaderSize, &size, sizeof(size));
    used_ = payload + size;
    return storage_ + payload;
  }

  /** 関数名: deallocate / 内容: 個別解放しない。入力: ptr。resetで一括回収する。 */
  void deallocate(void* ptr) override {
    (void)ptr;
  }

  /**
   * 関数名: reallocate
   * 内容: 拡張時は別の固定領域へコピーし、縮小時は同じ領域を返す。
   * 入力: ptr=自身が確保した領域、size=新サイズ。出力: 新領域または容量不足のnullptr。
   */
  void* reallocate(void* ptr, size_t size) override {
    size_t previousSize = 0;
    void* replacement = nullptr;
    if (ptr == nullptr) {
      return allocate(size);
    }
    if (size == 0) {
      return nullptr;
    }
    std::memcpy(&previousSize, static_cast<unsigned char*>(ptr) - kHeaderSize,
                sizeof(previousSize));
    if (size <= previousSize) {
      return ptr;
    }
    replacement = allocate(size);
    if (replacement != nullptr) {
      std::memcpy(replacement, ptr, previousSize);
    }
    return replacement;
  }

 private:
  static constexpr size_t kAlignment = alignof(std::max_align_t);
  static constexpr size_t kHeaderSize =
      ((sizeof(size_t) + kAlignment - 1) / kAlignment) * kAlignment;
  alignas(std::max_align_t) unsigned char storage_[kCapacity] = {};
  size_t used_ = 0;
};
}  // namespace tempescope
