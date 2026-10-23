#pragma once
#include <stdint.h>
#include "Types.h"

namespace tempescope {
// 試運転時の仮上限。ポンプ等のメーカー定格を表す値ではない。
constexpr uint32_t kPumpOnMs = 10000;
constexpr uint32_t kPumpCycleMs = 60000;
constexpr uint32_t kMistOnMs = 20000;
constexpr uint32_t kMistCycleMs = 60000;
constexpr uint32_t kWaterStableMs = 2000;
constexpr uint32_t kSessionLimitMs = 30 * 60 * 1000;
class Safety {
 public:
  Fault fault = Fault::None;
  /**
   * 関数名: tick
   * 内容: 安全入力と経過時間から今回の演出出力を算出する。
   * 入力: now=millis、inputs=最新の入力。出力: 各負荷の出力値。異常はfaultに保持。
   * 注意: 単一制御タスクから周期的に呼ぶ。GPIO操作や通信は行わない。
   */
  Outputs tick(uint32_t now, const Inputs& inputs) {
    bool waterReady = false;
    uint32_t elapsed = 0;
    Outputs outputs;

    // 符号なし差分でmillisの32bit周回を扱う。
    if (!inputs.pumpWater || !inputs.mistWater) {
      waterTiming_ = false;
    } else if (!waterTiming_) {
      waterTiming_ = true;
      waterSince_ = now;
    }
    waterReady = waterTiming_ && uint32_t(now - waterSince_) >= kWaterStableMs;
    if (!inputs.stopClosed) {
      fault = Fault::Stop;
    } else if (!inputs.supervisorAlive) {
      fault = Fault::Supervisor;
    }
    if (inputs.reset && !inputs.enabled && inputs.stopClosed && inputs.supervisorAlive &&
        waterReady) {
      fault = Fault::None;
    }
    if (inputs.enabled && (!inputs.pumpWater || !inputs.mistWater)) {
      fault = Fault::LowWater;
    }
    if (!inputs.enabled || !inputs.network || !inputs.fresh || !inputs.validated ||
        fault != Fault::None) {
      active_ = false;
      return {};
    }
    if (!waterReady) {
      return {};
    }
    if (!active_) {
      active_ = true;
      started_ = now;
    }
    elapsed = now - started_;
    if (elapsed >= kSessionLimitMs) {
      fault = Fault::RunLimit;
      active_ = false;
      return {};
    }
    if (inputs.scene == Scene::Rain || inputs.scene == Scene::Storm) {
      outputs.pump = elapsed % kPumpCycleMs < kPumpOnMs;
    } else if (inputs.scene == Scene::Cloud) {
      outputs.mist = elapsed % kMistCycleMs < kMistOnMs;
      outputs.fan = true;
    }
    if (inputs.day) {
      outputs.red = 150;
      outputs.green = 110;
      outputs.blue = 60;
    } else {
      outputs.red = 0;
      outputs.green = 0;
      outputs.blue = 15;
    }
    if (inputs.scene == Scene::Cloud) {
      outputs.red /= 3;
      outputs.green /= 3;
      outputs.blue = 90;
    }
    if (inputs.scene == Scene::Rain || inputs.scene == Scene::Storm) {
      outputs.red = 0;
      outputs.green = 20;
      outputs.blue = 90;
    }
    // 長い待ち時間を入れず、雷雨の短い点滅だけを加える。
    if (inputs.scene == Scene::Storm && elapsed % 11000 < 80) {
      outputs.red = outputs.green = outputs.blue = 220;
    }
    return outputs;
  }

 private:
  bool active_ = false;
  bool waterTiming_ = false;
  uint32_t started_ = 0;
  uint32_t waterSince_ = 0;
};
/**
 * 関数名: decodeWeather
 * 内容: 天気コードを装置の演出に変換する。
 * 入力: code=WMOコード。出力: 対応時trueとscene、未知コード時falseでscene維持。
 * 注意: 雪は曇りとして近似し、ポンプを動かさない。
 */
inline bool decodeWeather(int code, Scene& scene) {
  if (code == 0 || code == 1) {
    scene = Scene::Clear;
  } else if (code == 2 || code == 3 || code == 45 || code == 48) {
    scene = Scene::Cloud;
  } else if (code == 51 || code == 53 || code == 55 || code == 56 || code == 57 || code == 61 ||
             code == 63 || code == 65 || code == 66 || code == 67 || code == 80 || code == 81 ||
             code == 82) {
    scene = Scene::Rain;
  } else if (code == 95 || code == 96 || code == 99) {
    scene = Scene::Storm;
  }
  // 雪用の機構がないため曇りに近似し、降雪でポンプを駆動しない。
  else if (code == 71 || code == 73 || code == 75 || code == 77 || code == 85 || code == 86) {
    scene = Scene::Cloud;
  } else {
    return false;
  }
  return true;
}
}  // namespace tempescope
