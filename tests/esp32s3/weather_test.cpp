#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../../firmware/xiao_s3/WeatherData.h"
using namespace tempescope;
bool parse(const char *s, int64_t now=1800000000) {
  WeatherSample sample;
  return parseWeather(s,strlen(s),now,42,sample);
}
int main() {
  const char *valid=R"({"current":{"time":1799999999,"weather_code":61,"is_day":1}})";
  WeatherSample s;
  assert(parseWeather(valid,strlen(valid),1800000000,42,s));
  assert(s.scene==Scene::Rain&&s.day&&s.received==42&&s.observed==1799999999);
  assert(!parse(valid,0)); // uninitialized clock
  assert(!parse(valid,1800001799)); // exactly 30 min old
  assert(!parse(valid,1799999998)); // future observation
  for(const char *bad : {"{}","null","{broken", R"({"error":true})",
    R"({"current":{"time":1799999999,"weather_code":null,"is_day":1}})",
    R"({"current":{"time":1799999999,"weather_code":4,"is_day":1}})",
    R"({"current":{"time":1799999999,"weather_code":61,"is_day":2}})",
    R"({"current":{"time":1799999999,"weather_code":"61","is_day":1}})",
    R"({"current":{"time":1799999999,"weather_code":61}})",
    R"({"current":{"time":"1799999999","weather_code":61,"is_day":1}})"})
    assert(!parse(bad));
  puts("PASS: weather schema, types, null, unsupported codes, stale/future timestamps");
}
