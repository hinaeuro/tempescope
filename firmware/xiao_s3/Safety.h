#pragma once
#include <stdint.h>

namespace tempescope {
enum class Scene : uint8_t { Clear, Cloud, Rain, Storm };
enum class Fault : uint8_t { None, Stop, LowWater, RunLimit, Supervisor };
struct Inputs {
  bool enabled = false, network = false, fresh = false, validated = false;
  bool pumpWater = false, mistWater = false, stopClosed = false;
  bool supervisorAlive = false, reset = false, day = true;
  Scene scene = Scene::Clear;
};
struct Outputs {
  bool pump = false, mist = false, fan = false;
  uint8_t red = 0, green = 0, blue = 0;
};
// Limits are conservative INITIAL commissioning settings, not pump ratings.
constexpr uint32_t kPumpOnMs = 10000, kPumpCycleMs = 60000;
constexpr uint32_t kMistOnMs = 20000, kMistCycleMs = 60000;
constexpr uint32_t kWaterStableMs = 2000;
constexpr uint32_t kSessionLimitMs = 30 * 60 * 1000;
class Safety {
 public:
  Fault fault = Fault::None;
  Outputs tick(uint32_t now, const Inputs &i) {
    // Unsigned differences remain correct across the 32-bit millis rollover.
    if (!i.pumpWater || !i.mistWater) { waterTiming_ = false; }
    else if (!waterTiming_) { waterTiming_ = true; waterSince_ = now; }
    const bool waterReady = waterTiming_ && uint32_t(now-waterSince_) >= kWaterStableMs;
    if (!i.stopClosed) fault = Fault::Stop;
    else if (!i.supervisorAlive) fault = Fault::Supervisor;
    if (i.reset && !i.enabled && i.stopClosed && i.supervisorAlive && waterReady)
      fault = Fault::None;
    if (i.enabled && (!i.pumpWater || !i.mistWater)) fault = Fault::LowWater;
    if (!i.enabled || !i.network || !i.fresh || !i.validated || fault != Fault::None) {
      active_ = false;
      return {};
    }
    if (!waterReady) return {};
    if (!active_) { active_ = true; started_ = now; }
    const uint32_t elapsed = now - started_;
    if (elapsed >= kSessionLimitMs) {
      fault = Fault::RunLimit; active_ = false; return {};
    }
    Outputs o;
    if (i.scene == Scene::Rain || i.scene == Scene::Storm)
      o.pump = elapsed % kPumpCycleMs < kPumpOnMs;
    else if (i.scene == Scene::Cloud) {
      o.mist = elapsed % kMistCycleMs < kMistOnMs;
      o.fan = true;
    }
    if (i.day) { o.red = 150; o.green = 110; o.blue = 60; }
    else { o.red = 0; o.green = 0; o.blue = 15; }
    if (i.scene == Scene::Cloud) { o.red /= 3; o.green /= 3; o.blue = 90; }
    if (i.scene == Scene::Rain || i.scene == Scene::Storm) {
      o.red = 0; o.green = 20; o.blue = 90;
    }
    // Sparse non-blocking storm flash, no random long delays.
    if (i.scene == Scene::Storm && elapsed % 11000 < 80)
      o.red = o.green = o.blue = 220;
    return o;
  }
 private:
  bool active_ = false, waterTiming_ = false;
  uint32_t started_ = 0, waterSince_ = 0;
};
inline bool decodeWeather(int code, Scene &scene) {
  if (code == 0 || code == 1) scene = Scene::Clear;
  else if (code == 2 || code == 3 || code == 45 || code == 48) scene = Scene::Cloud;
  else if (code == 51 || code == 53 || code == 55 || code == 56 || code == 57 ||
           code == 61 || code == 63 || code == 65 || code == 66 || code == 67 ||
           code == 80 || code == 81 || code == 82) scene = Scene::Rain;
  else if (code == 95 || code == 96 || code == 99) scene = Scene::Storm;
  // No snow actuator: blue/cloud approximation is documented, never pump snow.
  else if (code == 71 || code == 73 || code == 75 || code == 77 ||
           code == 85 || code == 86) scene = Scene::Cloud;
  else return false;
  return true;
}
} // namespace tempescope
