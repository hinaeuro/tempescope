#pragma once
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "../library/Types.h"

namespace tempescope {
namespace drivers {
struct OutputPins {
  int pump;
  int mist;
  int fan;
  int red;
  int green;
  int blue;
};

// 下位ライブラリの値型だけを使用。Safety/Bench/Demoやアプリを呼び出さない。
class BoardDriver {
 public:
  /** 関数名: begin / 内容: 指定GPIOをLOWの出力にする。入力: pins=GPIO番号。PWM開始前に呼ぶ。 */
  void begin(const OutputPins& pins) {
    pins_ = pins;
    const int outputs[] = {pins.pump, pins.mist, pins.fan, pins.red, pins.green, pins.blue};
    for (int pin : outputs) {
      digitalWrite(pin, LOW);
      pinMode(pin, OUTPUT);
    }
  }

  /**
   * 関数名: beginPwm
   * 内容: RGBの2kHz・8bit PWMを初期化し、消灯する。
   * 出力: 全色成功ならtrue。注意: 失敗時もapply()でデジタル出力を停止できる。
   */
  bool beginPwm() {
    pwmRed_ = ledcAttach(pins_.red, 2000, 8);
    pwmGreen_ = ledcAttach(pins_.green, 2000, 8);
    pwmBlue_ = ledcAttach(pins_.blue, 2000, 8);
    apply(Outputs{});
    return pwmRed_ && pwmGreen_ && pwmBlue_;
  }

  /** 関数名: beginInputs / 内容: 水位2点・停止・解除入力をプルアップする。入力: 各GPIO番号。 */
  void beginInputs(int pumpWater, int mistWater, int stop, int reset) {
    const int inputs[] = {pumpWater, mistWater, stop, reset};
    for (int pin : inputs) {
      pinMode(pin, INPUT_PULLUP);
    }
  }

  /** 関数名: isLow / 内容: 接点のLOWを読む。入力: GPIO番号。出力: LOWならtrue。 */
  bool isLow(int pin) const {
    return digitalRead(pin) == LOW;
  }

  /**
   * 関数名: apply
   * 内容: 決定済みの出力値をGPIO/PWMへ反映する。
   * 入力: output=ON/OFFと0〜255のPWM値。注意: 演出・許可・時間の判断は上位層の責任。
   */
  void apply(const Outputs& output) {
    digitalWrite(pins_.pump, output.pump);
    digitalWrite(pins_.mist, output.mist);
    digitalWrite(pins_.fan, output.fan);
    if (pwmRed_) {
      ledcWrite(pins_.red, output.red);
    } else {
      digitalWrite(pins_.red, LOW);
    }
    if (pwmGreen_) {
      ledcWrite(pins_.green, output.green);
    } else {
      digitalWrite(pins_.green, LOW);
    }
    if (pwmBlue_) {
      ledcWrite(pins_.blue, output.blue);
    } else {
      digitalWrite(pins_.blue, LOW);
    }
  }

 private:
  OutputPins pins_ = {};
  bool pwmRed_ = false;
  bool pwmGreen_ = false;
  bool pwmBlue_ = false;
};

class TaskWatchdog {
 public:
  /**
   * 関数名: begin
   * 内容: 呼び出したタスクを2秒監視へ登録する。
   * 出力: 登録・設定成功ならtrue。注意: feed()も同じタスクから呼ぶ。
   */
  bool begin() {
    esp_task_wdt_config_t config = {};
    esp_err_t result = ESP_OK;
    config.timeout_ms = 2000;
    config.idle_core_mask = 0;
    config.trigger_panic = true;
    result = esp_task_wdt_add(nullptr);
    if (result == ESP_ERR_INVALID_STATE) {
      if (esp_task_wdt_init(&config) == ESP_OK) {
        result = esp_task_wdt_add(nullptr);
      }
    }
    if (result == ESP_OK && esp_task_wdt_reconfigure(&config) != ESP_OK) {
      result = ESP_FAIL;
    }
    ready_ = result == ESP_OK;
    return ready_;
  }

  /** 関数名: feed / 内容: 登録済みの場合だけ呼出タスクの監視期限を更新する。 */
  void feed() const {
    if (ready_) {
      esp_task_wdt_reset();
    }
  }

 private:
  bool ready_ = false;
};
}  // namespace drivers
}  // namespace tempescope
