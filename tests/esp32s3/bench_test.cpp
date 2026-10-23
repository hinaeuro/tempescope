#include <cassert>
#include "../../firmware/xiao_s3/Bench.h"
using namespace tempescope;
/** 関数名: main / 内容: 対象制御の回帰テストを実行する。出力: 成功0、失敗時assertで停止。 */
int main() {
  Bench b;
  assert(!b.command("fan", 0, true, true));
  b.driverOK = true;
  assert(!b.command("pump", 0, false, false));
  assert(!b.command("mist", 0, false, false));
  assert(b.command("fan", 0, false, false));
  assert(b.tick(4999).fan);
  assert(!b.command("fan", 4999, false, false));
  assert(!b.tick(5000).fan);
  assert(b.command("pump", 5000, true, true));
  assert(b.tick(5999).pump);
  assert(!b.tick(6000).pump);
  assert(!b.command("mist", 6000, true, true));
  assert(b.command("red", 6000, false, false));
  assert(b.tick(6000).red == 64);
  assert(b.command("off", 6001, false, false));
  assert(!b.tick(6001).red);
  assert(b.command("mist", 65000, true, true));
  assert(b.command("off", 65001, true, true));
  assert(!b.command("pump", 65002, true, true));
  assert(!b.command("invalid", 65002, true, true));
  Bench r;
  r.driverOK = true;
  assert(r.command("blue", UINT32_MAX - 100, false, false));
  assert(r.tick(0).blue == 64);
  assert(!r.tick(4899).blue);
  assert(r.command("fan", 5000, false, false));
  r.driverOK = false;
  assert(!r.tick(5001).fan);
}
