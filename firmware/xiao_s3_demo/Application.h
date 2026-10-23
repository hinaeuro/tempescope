#pragma once
#include <Arduino.h>
#include "Demo.h"
#include "../common/drivers/BoardDriver.h"
#include "../common/drivers/ConsoleDriver.h"

namespace demo {
namespace application {
// 数値はXIAOのD番号ではなくESP32-S3のGPIO番号。
constexpr tempescope::drivers::OutputPins kOutputPins = {1, 2, 3, 9, 8, 7};
inline Controller controller;
inline tempescope::drivers::BoardDriver board;
inline tempescope::drivers::TaskWatchdog watchdog;
inline tempescope::drivers::ConsoleDriver console;
inline char commandLine[96] = {};
inline size_t commandLength = 0;
inline bool droppingLine = false;
inline bool announced = false;

/**
 * 関数名: applyOutputs
 * 内容: デモの出力状態を共通の値型へ変換し、ボードドライバーへ渡す。
 * 出力: なし。注意: GPIO番号・PWM APIの知識はドライバーに閉じ込める。
 */
inline void applyOutputs() {
  tempescope::Outputs output;
  output.pump = controller.pump.on;
  output.mist = controller.mist.on;
  output.fan = controller.fan.on;
  output.red = controller.led.on ? controller.r : 0;
  output.green = controller.led.on ? controller.g : 0;
  output.blue = controller.led.on ? controller.b : 0;
  board.apply(output);
}

/** 関数名: printHelp / 内容: コマンド・時間上限をUSBシリアルへ表示する。入力/出力: なし。 */
inline void printHelp() {
  console.println("デモ: USBシリアル・115200bps・改行あり");
  console.println("led red|green|blue|white [ms] / led rgb R G B [ms] / led off");
  console.println("fan on [ms] / fan off / pump on [ms] / pump off / mist on [ms] / mist off");
  console.println("off または stop: 全停止 / status: 状態 / help: 一覧");
  console.println("arm pump / arm mist: 個別に試運転許可 / disarm: 全停止して許可解除");
  console.println("上限: LED・ファン30秒（既定5秒）、ポンプ1秒、ミスト2秒。水位検知なし。");
}

/** 関数名: handleCommand / 内容: 完成したデモコマンドを実行・表示する。入力: line=受信行。 */
inline void handleCommand(const char* line) {
  if (!strcmp(line, "help")) {
    printHelp();
  } else if (!strcmp(line, "status")) {
    controller.tick(millis());
    console.printf(
        "初期化=%d LED=%d RGB=%u,%u,%u ファン=%d ポンプ=%d ミスト=%d ポンプ許可=%d ミスト許可=%d\n",
        controller.driverOK, controller.led.on, controller.r, controller.g, controller.b,
        controller.fan.on, controller.pump.on, controller.mist.on, controller.pumpAllowed,
        controller.mistAllowed);
  } else {
    console.println(controller.command(line, millis()));
  }
}

/**
 * 関数名: pollConsole
 * 内容: 最大64文字を取り込み、完成行を処理する。
 * 注意: バッファ超過・制御文字で即停止しLFまで破棄する既存デモ仕様を維持する。
 */
inline void pollConsole() {
  int value = 0;
  char character = 0;
  for (unsigned count = 0; count < 64 && console.available(); ++count) {
    value = console.read();
    if (value < 0) {
      break;
    }
    character = static_cast<char>(value);
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      if (!droppingLine) {
        commandLine[commandLength] = '\0';
        handleCommand(commandLine);
      }
      commandLength = 0;
      droppingLine = false;
    } else if (!droppingLine) {
      if ((character < 32 && character != '\t') || commandLength == sizeof(commandLine) - 1) {
        controller.stop();
        droppingLine = true;
        console.println("入力不正または長すぎるため全停止。改行まで破棄");
      } else {
        commandLine[commandLength++] = character;
      }
    }
    controller.tick(millis());
    applyOutputs();
  }
}

/** 関数名: begin / 内容: 出力を停止してPWM・USB・監視を初期化する。失敗時は動作を許可しない。 */
inline void begin() {
  bool pwmOK = false;
  bool watchdogOK = false;
  board.begin(kOutputPins);
  console.begin();
  pwmOK = board.beginPwm();
  watchdogOK = watchdog.begin();
  controller.driverOK = pwmOK && watchdogOK;
  applyOutputs();
}

/** 関数名: update / 内容: 時間上限・USB入力・出力反映を進め、監視を更新する。入力/出力: なし。 */
inline void update() {
  controller.tick(millis());
  applyOutputs();
  if (!announced && console.connected()) {
    printHelp();
    announced = true;
  }
  pollConsole();
  if (controller.driverOK) {
    watchdog.feed();
  }
  delay(2);
}
}  // namespace application
}  // namespace demo
