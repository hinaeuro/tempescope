#pragma once
#include <ArduinoJson.h>
#include "Safety.h"
namespace tempescope {
struct WeatherSample {
  Scene scene;
  bool day;
  uint32_t received;
  int64_t observed;
};
inline bool parseWeather(const char *body, size_t length, int64_t epoch,
                         uint32_t received, WeatherSample &sample) {
  JsonDocument doc;
  if (deserializeJson(doc, body, length)) return false;
  JsonVariant current = doc["current"];
  if (!current["weather_code"].is<int>() || !current["is_day"].is<int>() ||
      !current["time"].is<int64_t>()) return false;
  const int day = current["is_day"].as<int>();
  if (day != 0 && day != 1) return false;
  Scene scene;
  if (!decodeWeather(current["weather_code"].as<int>(), scene)) return false;
  const int64_t observed = current["time"].as<int64_t>();
  if (epoch < 1700000000 || observed > epoch || observed <= epoch-1800) return false;
  sample = {scene, day == 1, received, observed};
  return true;
}
} // namespace tempescope
