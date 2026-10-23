#pragma once
#include <stdint.h>

namespace tempescope {
// ライブラリ層の値型。Arduino・ドライバー・アプリケーションに依存しない。
enum class Scene : uint8_t { Clear, Cloud, Rain, Storm };
enum class Fault : uint8_t { None, Stop, LowWater, RunLimit, Supervisor };

struct Inputs {
  bool enabled = false;
  bool network = false;
  bool fresh = false;
  bool validated = false;
  bool pumpWater = false;
  bool mistWater = false;
  bool stopClosed = false;
  bool supervisorAlive = false;
  bool reset = false;
  bool day = true;
  Scene scene = Scene::Clear;
};

struct Outputs {
  bool pump = false;
  bool mist = false;
  bool fan = false;
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;
};

struct WeatherSample {
  Scene scene = Scene::Clear;
  bool day = true;
  uint32_t received = 0;
  int64_t observed = 0;
};
}  // namespace tempescope
