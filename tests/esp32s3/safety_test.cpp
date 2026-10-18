#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include "../../firmware/xiao_s3/Safety.h"
using namespace tempescope;
Inputs good() {
  Inputs i;
  i.enabled=i.network=i.fresh=i.validated=true;
  i.pumpWater=i.mistWater=i.stopClosed=i.supervisorAlive=true;
  i.scene=Scene::Rain;
  return i;
}
bool off(Outputs o) { return !o.pump&&!o.mist&&!o.fan&&!o.red&&!o.green&&!o.blue; }
int main() {
  {
    Safety s; auto i=good();
    assert(off(s.tick(0,i))); assert(off(s.tick(1999,i)));
    assert(s.tick(2000,i).pump);
    assert(!s.tick(12000,i).pump); assert(s.tick(62000,i).pump);
    i.scene=Scene::Cloud;
    auto o=s.tick(62010,i); assert(!o.pump&&o.mist&&o.fan);
    i.scene=Scene::Clear; o=s.tick(62020,i); assert(!o.pump&&!o.mist&&!o.fan&&o.red);
  }
  {
    Safety s; auto i=good(); s.tick(0,i); s.tick(2000,i);
    i.pumpWater=false; assert(off(s.tick(2001,i))); assert(s.fault==Fault::LowWater);
    i.pumpWater=true; assert(off(s.tick(3000,i))); assert(off(s.tick(5000,i)));
    i.reset=true; assert(off(s.tick(5001,i))); // ON command cannot reset latch
    i.enabled=false; s.tick(5002,i); assert(s.fault==Fault::None);
    i.reset=false; i.enabled=true; assert(s.tick(5003,i).pump);
  }
  for (int field=0; field<7; ++field) {
    Safety s; auto i=good(); s.tick(0,i); s.tick(2000,i);
    switch(field) {
      case 0: i.enabled=false; break; case 1:i.network=false;break;
      case 2:i.fresh=false;break;case 3:i.validated=false;break;
      case 4:i.stopClosed=false;break;case 5:i.supervisorAlive=false;break;
      case 6:i.mistWater=false;break;
    }
    assert(off(s.tick(2001,i)));
  }
  {
    Safety s; auto i=good(); s.tick(0,i); s.tick(2000,i);
    assert(off(s.tick(2000+kSessionLimitMs,i))); assert(s.fault==Fault::RunLimit);
    assert(off(s.tick(2001+kSessionLimitMs,i)));
    i.enabled=false;i.reset=true;s.tick(2002+kSessionLimitMs,i);
    assert(s.fault==Fault::None);
  }
  {
    Safety s; auto i=good(); uint32_t start=UINT_MAX-1000;
    s.tick(start,i); assert(s.tick(start+2000,i).pump);
    assert(!s.tick(start+12000,i).pump);
    assert(off(s.tick(start+2000+kSessionLimitMs,i)));
  }
  {
    Safety s; auto i=good(); i.enabled=false; s.tick(0,i); i.pumpWater=false;s.tick(1999,i);
    i.pumpWater=true; assert(off(s.tick(2000,i))); assert(off(s.tick(3999,i)));
    i.enabled=true; assert(s.tick(4000,i).pump); // stable recovery required
  }
  Scene scene;
  assert(decodeWeather(0,scene)&&scene==Scene::Clear);
  assert(decodeWeather(45,scene)&&scene==Scene::Cloud);
  assert(decodeWeather(61,scene)&&scene==Scene::Rain);
  assert(decodeWeather(99,scene)&&scene==Scene::Storm);
  assert(decodeWeather(75,scene)&&scene==Scene::Cloud);
  assert(!decodeWeather(-1,scene)&&!decodeWeather(4,scene)&&!decodeWeather(100,scene));
  // Full 30 min pulse schedule: mutually exclusive actuators and pulse bounds.
  Safety s; auto i=good(); s.tick(0,i);
  for(uint32_t t=2000;t<2000+kSessionLimitMs;t+=10) {
    auto o=s.tick(t,i); assert(!o.mist);
    assert(o.pump==((t-2000)%60000<10000));
  }
  puts("PASS: safety gates, latches, reset, pulses, session limit, rollover, weather mapping");
}
