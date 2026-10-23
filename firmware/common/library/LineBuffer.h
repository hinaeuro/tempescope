#pragma once
#include <stddef.h>

namespace tempescope {
// 既存のLF終端・CR無視・超過行破棄を共通化。編集キー対応等の仕様追加は行わない。
template <size_t Capacity>
class LineBuffer {
 public:
  enum class Result { Pending, Complete, Overflow };

  /**
   * 関数名: push
   * 内容: 1文字を固定長バッファへ取り込み、LFで行の完成を通知する。
   * 入力: value=受信文字。出力: Completeならtext()が有効。
   * 注意: 完成行は次のpushまで有効。超過時はLFまで破棄する。単一呼出元専用。
   */
  Result push(char value) {
    if (completed_) {
      length_ = 0;
      overflow_ = false;
      completed_ = false;
    }
    if (value == '\r') {
      return Result::Pending;
    }
    if (value == '\n') {
      buffer_[length_] = '\0';
      completed_ = true;
      return overflow_ ? Result::Overflow : Result::Complete;
    }
    if (length_ < Capacity - 1) {
      buffer_[length_++] = value;
    } else {
      overflow_ = true;
    }
    return Result::Pending;
  }

  /** 関数名: text / 内容: 完成行を参照する。出力: 所有権を移さない文字列ポインター。 */
  const char* text() const {
    return buffer_;
  }

 private:
  static_assert(Capacity >= 2, "LineBuffer needs room for data and NUL");
  char buffer_[Capacity] = {};
  size_t length_ = 0;
  bool overflow_ = false;
  bool completed_ = false;
};
}  // namespace tempescope
