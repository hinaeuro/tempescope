#include "../../firmware/xiao_s3_demo/Demo.h"
#include <assert.h>
#include <iostream>

int main() {
  demo::Controller c;
  c.command("fan on", 0); assert(!c.fan.on); // 初期化失敗時は動作しない
  c.driverOK = true;
  c.command("led rgb 12 34 255 1000", 100);
  c.command("fan on 2000", 100);
  assert(c.led.on && c.fan.on && c.r == 12 && c.g == 34 && c.b == 255);
  c.command("led blue 30000", 500); // 色変更で元の期限は延長しない
  assert(c.b == 64 && c.led.start == 100 && c.led.duration == 1000);
  c.tick(1100); assert(!c.led.on && c.fan.on);
  c.command("fan on 30000", 1500);
  c.tick(2100); assert(!c.fan.on);
  const char *invalid[] = {"led rgb 256 0 0", "led rgb -1 0 0", "led rgb 1 2", "fan on 0",
    "fan on 30001", "fan on 99999999999999999999999", "pump on 1001", "mist on 2001",
    "fan on 1 extra", "fan off extra", "led red 1 garbage", "led rgb 0 0 0 -1"};
  for (const char *s : invalid) { c.command(s, 2200); assert(!c.led.on && !c.fan.on && !c.pump.on && !c.mist.on); }
  c.command("pump on", 3000); assert(!c.pump.on);
  c.command("arm pump", 3000); c.command("arm mist", 3000);
  c.command("pump on", 3000); assert(c.pump.on);
  c.command("mist on", 3000); assert(!c.mist.on);
  c.command("pump off", 3100); assert(!c.pump.on);
  c.command("mist on", 3100); assert(!c.mist.on);
  c.command("off", 3200); c.command("mist on", 62999); assert(!c.mist.on);
  c.command("mist on", 63000); assert(c.mist.on);
  c.tick(65000); assert(!c.mist.on);
  c.command("fan on", 65000); c.command("led white", 65000);
  c.command("led off", 65001); assert(!c.led.on && c.fan.on);
  c.command("disarm", 65002); assert(!c.fan.on && !c.pumpAllowed && !c.mistAllowed);
  c.command("fan on", 70000); c.driverOK = false; c.tick(70001); assert(!c.fan.on);
  demo::Controller wrap; wrap.driverOK = true;
  wrap.command("fan on 100", UINT32_MAX-50); wrap.tick(48); assert(wrap.fan.on);
  wrap.tick(49); assert(!wrap.fan.on);
  wrap.command("arm pump", 0); wrap.command("pump on", UINT32_MAX-100);
  wrap.tick(1000); assert(!wrap.pump.on);
  wrap.command("pump on", 59898); assert(!wrap.pump.on);
  wrap.command("pump on", 59899); assert(wrap.pump.on);
  wrap.command("stop", 60000); assert(!wrap.pump.on);
  char longLine[100]; memset(longLine, 'x', 99); longLine[99] = 0;
  c.driverOK = true; c.command("fan on", 80000); c.command(longLine, 80001); assert(!c.fan.on);
  std::cout << "demo_test: PASS\n";
}
