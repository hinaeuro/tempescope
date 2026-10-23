#pragma once
#include <ArduinoJson.h>
#include "Safety.h"
#include "FixedJsonArena.h"

namespace tempescope {
/**
 * 関数名: parseWeather
 * 内容: Open-MeteoのJSONを解析し、型・天気コード・観測時刻を検証する。
 * 入力: body/length=JSON、epoch=現在UTC秒、received=受信millis。
 * 出力: 有効ならtrueとsample。無効・領域不足時はfalse、sampleは変更しない。
 * 注意: 固定16KiB領域を再利用するため非再入。weatherタスクだけから呼ぶ。
 */
inline bool parseWeather(const char* body, size_t length, int64_t epoch, uint32_t received,
                         WeatherSample& sample) {
  static FixedJsonArena arena;
  int day = 0;
  int64_t observed = 0;
  Scene scene = Scene::Clear;

  // 前回のdocumentは破棄済み。JSON既定ヒープアロケーターを使わない。
  arena.reset();
  JsonDocument document(&arena);
  if (deserializeJson(document, body, length)) {
    return false;
  }
  JsonVariant current = document["current"];
  if (!current["weather_code"].is<int>() || !current["is_day"].is<int>() ||
      !current["time"].is<int64_t>()) {
    return false;
  }
  day = current["is_day"].as<int>();
  if (day != 0 && day != 1) {
    return false;
  }
  if (!decodeWeather(current["weather_code"].as<int>(), scene)) {
    return false;
  }
  observed = current["time"].as<int64_t>();
  if (epoch < 1700000000 || observed > epoch || observed <= epoch - 1800) {
    return false;
  }
  sample = {scene, day == 1, received, observed};
  return true;
}
}  // namespace tempescope
