#pragma once
#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "Config.h"
#include "Safety.h"
#include "Bench.h"
#include "../common/library/LineBuffer.h"
#include "../common/drivers/BoardDriver.h"
#include "../common/drivers/ConsoleDriver.h"
#include "../common/drivers/NetworkDriver.h"

#if !CONFIG_IDF_TARGET_ESP32S3
#error "This port requires ESP32-S3"
#endif
#if !CONFIG_ENABLE_CHIPOBLE
#error "Enable the ESP32-S3 Matter BLE commissioning build"
#endif
#if !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#error "Enable the ESP-IDF trusted certificate bundle"
#endif
#if configSUPPORT_STATIC_ALLOCATION != 1
#error "Static FreeRTOS allocation must be enabled"
#endif

namespace tempescope {
namespace application {
// 固定領域と設定は処理開始前に宣言する。所有タスクはarchitecture-ja.mdを参照。
constexpr uint32_t kFreshMs = 30 * 60 * 1000;
constexpr uint32_t kPollMs = 10 * 60 * 1000;
constexpr uint32_t kRetryMs = 60000;
constexpr uint32_t kSafetyStackBytes = 4096;
constexpr uint32_t kWeatherStackBytes = 12288;
constexpr drivers::OutputPins kOutputs = {kPumpPin, kMistPin,  kFanPin,
                                          kRedPin,  kGreenPin, kBluePin};

inline drivers::BoardDriver board;
inline drivers::ConsoleDriver console;
inline drivers::TaskWatchdog benchWatchdog;
inline drivers::TaskWatchdog safetyWatchdog;
inline drivers::MatterDriver matter;
inline drivers::WeatherClient weatherClient;
inline Bench bench;
inline Safety safety;
inline LineBuffer<32> commandLine;
inline std::atomic<bool> requested{false};
inline std::atomic<bool> resetRequested{false};
inline std::atomic<bool> ready{false};
inline std::atomic<uint32_t> loopBeat{0};
inline std::atomic<Fault> reportedFault{Fault::None};
inline QueueHandle_t weatherQueue = nullptr;
inline StaticQueue_t weatherQueueControl;
alignas(WeatherSample) inline uint8_t weatherQueueStorage[sizeof(WeatherSample)] = {};
inline StaticTask_t safetyTaskControl;
inline StaticTask_t weatherTaskControl;
inline StackType_t safetyStack[kSafetyStackBytes / sizeof(StackType_t)] = {};
inline StackType_t weatherStack[kWeatherStackBytes / sizeof(StackType_t)] = {};
inline WeatherSample latestWeather;
inline bool haveWeather = false;
inline bool benchAnnounced = false;
inline uint32_t lastPairingCode = 0;

/**
 * 関数名: requestPower
 * 内容: ON/OFF要求を検証して制御タスクへ渡す。
 * 入力: on=要求。出力: 受理ならtrue。注意: 起動未完了・未確認回路・ラッチ異常ではONを拒否。
 */
inline bool requestPower(bool on) {
  if (on &&
      (!ready.load() || !TEMPESCOPE_HARDWARE_VALIDATED || reportedFault.load() != Fault::None)) {
    return false;
  }
  requested.store(on);
  return true;
}

/**
 * 関数名: controlTask
 * 内容: 10ms周期で入力を収集し、安全ロジックの結果を出力する。
 * 入力: FreeRTOS予約引数（不使用）。出力: なし。異常状態はreportedFaultに公開。
 * 注意: このタスクだけが自動モードのGPIOを更新する。スタックは静的領域。
 */
inline void controlTask(void*) {
  bool driverOK = false;
  bool pwmOK = false;
  bool watchdogOK = false;
  TickType_t wake = xTaskGetTickCount();
  Inputs inputs;
  Outputs outputs;
  uint32_t now = 0;
  int64_t epoch = 0;

  pwmOK = board.beginPwm();
  watchdogOK = safetyWatchdog.begin();
  driverOK = pwmOK && watchdogOK;
  for (;;) {
    now = millis();
    if (xQueueReceive(weatherQueue, &latestWeather, 0) == pdTRUE) {
      haveWeather = true;
    }
    inputs = {};
    inputs.enabled = requested.load();
    inputs.network = drivers::networkConnected();
    epoch = drivers::epochNow();
    inputs.fresh = haveWeather && uint32_t(now - latestWeather.received) < kFreshMs &&
                   epoch >= latestWeather.observed && epoch - latestWeather.observed < 1800;
    inputs.validated = TEMPESCOPE_HARDWARE_VALIDATED && driverOK;
    inputs.pumpWater = board.isLow(kPumpWaterPin);
    inputs.mistWater = board.isLow(kMistWaterPin);
    inputs.stopClosed = board.isLow(kStopPin);
    inputs.supervisorAlive = !ready.load() || uint32_t(now - loopBeat.load()) < 1000;
    inputs.reset = resetRequested.exchange(false) || board.isLow(kResetPin);
    inputs.day = latestWeather.day;
    inputs.scene = latestWeather.scene;
    outputs = safety.tick(now, inputs);
    reportedFault.store(safety.fault);
    if (inputs.enabled &&
        (!inputs.network || !inputs.fresh || !inputs.validated || safety.fault != Fault::None)) {
      requested.store(false);
    }
    board.apply(outputs);
    safetyWatchdog.feed();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
  }
}

/**
 * 関数名: weatherTask
 * 内容: 時計同期と天気の取得周期を管理し、成功時だけ1要素キューへ書く。
 * 入力: FreeRTOS予約引数（不使用）。出力: 天気サンプル。
 * 注意: HTTP待ちを安全制御タスクから分離する。通常10分、失敗時60秒間隔を維持。
 */
inline void weatherTask(void*) {
  bool clockStarted = false;
  uint32_t lastAttempt = 0;
  uint32_t interval = 0;
  uint32_t now = 0;
  WeatherSample sample;

  for (;;) {
    if (drivers::networkConnected()) {
      if (!clockStarted) {
        drivers::startClock();
        clockStarted = true;
      }
      now = millis();
      if (drivers::epochNow() > 1700000000 &&
          (interval == 0 || uint32_t(now - lastAttempt) >= interval)) {
        lastAttempt = now;
        if (weatherClient.fetch(TEMPESCOPE_LATITUDE, TEMPESCOPE_LONGITUDE, sample)) {
          xQueueOverwrite(weatherQueue, &sample);
          interval = kPollMs;
          console.println("Weather updated (Open-Meteo)");
        } else {
          interval = kRetryMs;
          console.println("Weather failed; retry in 60 s");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/**
 * 関数名: handleCommand
 * 内容: 完成済みの1行を現在のモードで解釈する。
 * 入力: command=LFを除いた文字列。出力: USB応答と要求状態の更新。
 * 注意: コマンド・時間制限・異常解除の条件を従来実装から変更しない。
 */
inline void handleCommand(const char* command) {
  if (TEMPESCOPE_BENCH_MODE) {
    if (!strcmp(command, "status")) {
      console.printf("bench=1 driver=%d pump_allowed=%d mist_allowed=%d active=%d\n",
                     bench.driverOK, bool(TEMPESCOPE_BENCH_PUMP_ENABLED),
                     bool(TEMPESCOPE_BENCH_MIST_ENABLED), bench.active(millis()));
    } else if (!bench.command(command, millis(), TEMPESCOPE_BENCH_PUMP_ENABLED,
                              TEMPESCOPE_BENCH_MIST_ENABLED)) {
      console.println("Rejected: locked, busy, cooldown, driver failure or unknown command");
    } else {
      console.println("OK");
    }
    return;
  }
  if (!strcmp(command, "off")) {
    requested.store(false);
  } else if (!strcmp(command, "on")) {
    requestPower(true);
  } else if (!strcmp(command, "reset")) {
    requested.store(false);
    resetRequested.store(true);
  } else if (!strcmp(command, "status")) {
    console.printf("requested=%d fault=%u wifi=%d validated=%d\n", requested.load(),
                   unsigned(reportedFault.load()), drivers::networkConnected(),
                   TEMPESCOPE_HARDWARE_VALIDATED);
  } else if (!strcmp(command, "decommission")) {
    requested.store(false);
    delay(30);
    matter.decommission();
  }
}

/**
 * 関数名: pollConsole
 * 内容: 1周期最大64文字を処理し、完成行だけをアプリのコマンド処理へ渡す。
 * 出力: なし。注意: 入力待ちで停止せず、超過行は従来どおりBENCHで全停止する。
 */
inline void pollConsole() {
  int value = 0;
  LineBuffer<32>::Result result = LineBuffer<32>::Result::Pending;
  for (unsigned count = 0; count < 64 && console.available(); ++count) {
    value = console.read();
    if (value < 0) {
      break;
    }
    result = commandLine.push(static_cast<char>(value));
    if (result == LineBuffer<32>::Result::Complete) {
      handleCommand(commandLine.text());
    } else if (result == LineBuffer<32>::Result::Overflow && TEMPESCOPE_BENCH_MODE) {
      bench.stop(millis());
      console.println("Command too long; stopped");
    }
  }
}

/**
 * 関数名: begin
 * 内容: GPIOを停止状態で初期化し、選択モードの処理を開始する。
 * 出力: なし。注意: タスク/キュー領域は事前確保。初期化失敗時はreadyを立てない。
 */
inline void begin() {
  bool pwmOK = false;
  bool watchdogOK = false;
  TaskHandle_t safetyHandle = nullptr;
  TaskHandle_t weatherHandle = nullptr;

  board.begin(kOutputs);
  console.begin();
  if (TEMPESCOPE_BENCH_MODE) {
    pwmOK = board.beginPwm();
    watchdogOK = benchWatchdog.begin();
    bench.driverOK = pwmOK && watchdogOK;
    return;
  }
  board.beginInputs(kPumpWaterPin, kMistWaterPin, kStopPin, kResetPin);
  weatherQueue =
      xQueueCreateStatic(1, sizeof(WeatherSample), weatherQueueStorage, &weatherQueueControl);
  if (weatherQueue == nullptr) {
    return;
  }
  // ESP-IDF版FreeRTOSではスタックサイズの単位はバイト。
  safetyHandle = xTaskCreateStatic(controlTask, "safety", sizeof(safetyStack), nullptr, 5,
                                   safetyStack, &safetyTaskControl);
  if (safetyHandle == nullptr) {
    return;
  }
  weatherHandle = xTaskCreateStatic(weatherTask, "weather", sizeof(weatherStack), nullptr, 1,
                                    weatherStack, &weatherTaskControl);
  if (weatherHandle == nullptr || !matter.begin()) {
    return;
  }
  loopBeat.store(millis());
  ready.store(true);
  matter.setOnAllowed(TEMPESCOPE_HARDWARE_VALIDATED && reportedFault.load() == Fault::None);
  console.println("OpenTempescope: off. Commands: on, off, reset, status, decommission");
  if (!TEMPESCOPE_HARDWARE_VALIDATED) {
    console.println("DRY RUN: physical outputs disabled by configuration");
  }
}

/**
 * 関数名: update
 * 内容: USB入力・Matter要求をポーリングし、現在モードの周期処理を進める。
 * 出力: なし。注意: BENCHと自動モードでGPIOの所有者を分ける。
 */
inline void update() {
  bool remoteOn = false;
  if (TEMPESCOPE_BENCH_MODE) {
    if (!benchAnnounced && console.connected()) {
      console.println("BENCH: red green blue white fan (5s), mist (2s), pump (1s), off, status");
      console.println(
          "No water/stop sensors. No Wi-Fi/Matter in bench mode. Check status for output "
          "permissions.");
      benchAnnounced = true;
    }
    pollConsole();
    board.apply(bench.tick(millis()));
    if (bench.driverOK) {
      benchWatchdog.feed();
    }
    delay(10);
    return;
  }
  loopBeat.store(millis());
  matter.setOnAllowed(ready.load() && TEMPESCOPE_HARDWARE_VALIDATED &&
                      reportedFault.load() == Fault::None);
  if (matter.takeRequest(remoteOn)) {
    requestPower(remoteOn);
  }
  pollConsole();
  if (ready.load()) {
    matter.synchronize(requested.load());
    if (!matter.commissioned() && uint32_t(millis() - lastPairingCode) >= 10000) {
      lastPairingCode = millis();
      matter.printPairing(console);
    }
  }
  delay(10);
}
}  // namespace application
}  // namespace tempescope
