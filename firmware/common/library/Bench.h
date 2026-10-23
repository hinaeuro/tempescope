#pragma once
#include <string.h>
#include "Types.h"

namespace tempescope {
// 立ち会いで使う単発試運転。自動モードの安全入力を無効化しない。
class Bench {
 public:
  bool driverOK = false;
  /** 関数名: active / 内容: 時間上限内の動作中か判定する。入力: now=millis。出力: 動作中true。 */
  bool active(uint32_t now) const {
    return running_ && uint32_t(now - start_) < duration_;
  }
  /** 関数名: stop / 内容: 全出力を停止する。入力: 現在時刻（予約）。注意: 60秒間隔は解除しない。 */
  void stop(uint32_t) {
    running_ = false;
    output_ = {};
  }
  /** 関数名: tick / 内容: 期限またはドライバー異常で停止する。入力: now。出力: 今回の出力値。 */
  Outputs tick(uint32_t now) {
    if (!driverOK || !active(now)) {
      stop(now);
    }
    return output_;
  }
  /**
   * 関数名: command
   * 内容: BENCHの単発コマンドを判定し、動作を開始または停止する。
   * 入力: name=コマンド、now=millis、allowPump/allowMist=個別許可。
   * 出力: 受理ならtrue。注意: offは常に受理。液体負荷は共通60秒間隔を維持する。
   */
  bool command(const char* name, uint32_t now, bool allowPump, bool allowMist) {
    tick(now);
    if (!strcmp(name, "off")) {
      stop(now);
      return true;
    }
    if (!driverOK || active(now)) {
      return false;  // 再指令で動作時間を延長しない
    }
    Outputs next;
    uint32_t duration = 5000;
    const bool pump = !strcmp(name, "pump");
    const bool mist = !strcmp(name, "mist");
    if (pump || mist) {
      if ((pump && !allowPump) || (mist && !allowMist)) {
        return false;
      }
      if (liquidUsed_ && uint32_t(now - liquidSince_) < 60000) {
        return false;
      }
      next.pump = pump;
      next.mist = mist;
      duration = pump ? 1000 : 2000;
      liquidUsed_ = true;
      liquidSince_ = now;
    } else if (!strcmp(name, "fan")) {
      next.fan = true;
    } else if (!strcmp(name, "red")) {
      next.red = 64;
    } else if (!strcmp(name, "green")) {
      next.green = 64;
    } else if (!strcmp(name, "blue")) {
      next.blue = 64;
    } else if (!strcmp(name, "white")) {
      next.red = next.green = next.blue = 64;
    } else {
      return false;
    }
    output_ = next;
    start_ = now;
    duration_ = duration;
    running_ = true;
    return true;
  }

 private:
  Outputs output_{};
  bool running_ = false;
  bool liquidUsed_ = false;
  uint32_t start_ = 0;
  uint32_t duration_ = 0;
  uint32_t liquidSince_ = 0;
};
}  // namespace tempescope
