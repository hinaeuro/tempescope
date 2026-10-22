// SPDX-License-Identifier: LGPL-2.1-or-later
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "Demo.h"

// 現在の実装配線。数値はD番号ではなくGPIO番号。
constexpr int pumpPin = 1, mistPin = 2, fanPin = 3; // D0, D1, D2
constexpr int redPin = 9, greenPin = 8, bluePin = 7; // D10, D9, D8
demo::Controller controller;

void applyOutputs() {
  digitalWrite(pumpPin, controller.pump.on);
  digitalWrite(mistPin, controller.mist.on);
  digitalWrite(fanPin, controller.fan.on);
  ledcWrite(redPin, controller.led.on ? controller.r : 0);
  ledcWrite(greenPin, controller.led.on ? controller.g : 0);
  ledcWrite(bluePin, controller.led.on ? controller.b : 0);
}
void help() {
  Serial.println("デモ: USBシリアル・115200bps・改行あり");
  Serial.println("led red|green|blue|white [ms] / led rgb R G B [ms] / led off");
  Serial.println("fan on [ms] / fan off / pump on [ms] / pump off / mist on [ms] / mist off");
  Serial.println("off または stop: 全停止 / status: 状態 / help: 一覧");
  Serial.println("arm pump / arm mist: 個別に試運転許可 / disarm: 全停止して許可解除");
  Serial.println("上限: LED・ファン30秒（既定5秒）、ポンプ1秒、ミスト2秒。水位検知なし。");
}
void setup() {
  const int pins[] = {pumpPin, mistPin, fanPin, redPin, greenPin, bluePin};
  for (int pin : pins) { digitalWrite(pin, LOW); pinMode(pin, OUTPUT); }
  Serial.begin(115200);
  bool ok = ledcAttach(redPin, 2000, 8);
  ok = ledcAttach(greenPin, 2000, 8) && ok;
  ok = ledcAttach(bluePin, 2000, 8) && ok;
  esp_task_wdt_config_t config = { .timeout_ms = 2000, .idle_core_mask = 0, .trigger_panic = true };
  esp_err_t result = esp_task_wdt_add(nullptr);
  if (result == ESP_ERR_INVALID_STATE && esp_task_wdt_init(&config) == ESP_OK)
    result = esp_task_wdt_add(nullptr);
  if (result == ESP_OK && esp_task_wdt_reconfigure(&config) != ESP_OK) result = ESP_FAIL;
  controller.driverOK = ok && result == ESP_OK;
  applyOutputs();
}
void loop() {
  static char line[96];
  static size_t used = 0;
  static bool dropping = false, announced = false;
  controller.tick(millis());
  applyOutputs();
  if (!announced && Serial) { help(); announced = true; }
  // 入力が連続しても期限確認とWDT更新を止めない。
  for (unsigned n = 0; n < 64 && Serial.available(); ++n) {
    const char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (!dropping) {
        line[used] = 0;
        if (!strcmp(line, "help")) help();
        else if (!strcmp(line, "status")) {
          controller.tick(millis());
          Serial.printf("初期化=%d LED=%d RGB=%u,%u,%u ファン=%d ポンプ=%d ミスト=%d ポンプ許可=%d ミスト許可=%d\n",
            controller.driverOK, controller.led.on, controller.r, controller.g, controller.b,
            controller.fan.on, controller.pump.on, controller.mist.on,
            controller.pumpAllowed, controller.mistAllowed);
        } else Serial.println(controller.command(line, millis()));
      }
      used = 0; dropping = false;
    } else if (!dropping) {
      // NUL等を混ぜて、見えている行と解釈するコマンドがずれるのを防ぐ。
      if ((c < 32 && c != '\t') || used == sizeof(line)-1) {
        controller.stop(); dropping = true;
        Serial.println("入力不正または長すぎるため全停止。改行まで破棄");
      } else line[used++] = c;
    }
    controller.tick(millis());
    applyOutputs();
  }
  if (controller.driverOK) esp_task_wdt_reset();
  delay(2);
}
