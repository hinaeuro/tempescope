#pragma once
#include <string.h>
#include "Safety.h"

namespace tempescope {
// Local supervised pulses only. No sensor inputs are bypassed in automatic mode.
class Bench {
 public:
  bool driverOK = false;
  bool active(uint32_t now) const { return running_ && uint32_t(now-start_) < duration_; }
  void stop(uint32_t) { running_ = false; output_ = {}; }
  Outputs tick(uint32_t now) {
    if (!driverOK || !active(now)) stop(now);
    return output_;
  }
  bool command(const char *name, uint32_t now, bool allowPump, bool allowMist) {
    tick(now);
    if (!strcmp(name, "off")) { stop(now); return true; }
    if (!driverOK || active(now)) return false; // commands never extend a pulse
    Outputs next;
    uint32_t duration = 5000;
    const bool pump = !strcmp(name, "pump"), mist = !strcmp(name, "mist");
    if (pump || mist) {
      if ((pump && !allowPump) || (mist && !allowMist)) return false;
      if (liquidUsed_ && uint32_t(now-liquidSince_) < 60000) return false;
      next.pump = pump; next.mist = mist;
      duration = pump ? 1000 : 2000;
      liquidUsed_ = true; liquidSince_ = now;
    } else if (!strcmp(name, "fan")) next.fan = true;
    else if (!strcmp(name, "red")) next.red = 64;
    else if (!strcmp(name, "green")) next.green = 64;
    else if (!strcmp(name, "blue")) next.blue = 64;
    else if (!strcmp(name, "white")) next.red = next.green = next.blue = 64;
    else return false;
    output_ = next; start_ = now; duration_ = duration; running_ = true;
    return true;
  }
 private:
  Outputs output_{};
  bool running_ = false, liquidUsed_ = false;
  uint32_t start_ = 0, duration_ = 0, liquidSince_ = 0;
};
} // namespace tempescope
