// SPDX-License-Identifier: LGPL-2.1-or-later
#include <Arduino.h>
#include <Matter.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <atomic>
#include <time.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_task_wdt.h>
#include "Config.h"
#include "Safety.h"
#include "WeatherData.h"

#if !CONFIG_IDF_TARGET_ESP32S3
#error "This port requires ESP32-S3"
#endif
#if !CONFIG_ENABLE_CHIPOBLE
#error "Enable the ESP32-S3 Matter BLE commissioning build"
#endif
#if !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#error "Enable the ESP-IDF trusted certificate bundle; insecure HTTPS is not supported"
#endif

using namespace tempescope;
MatterOnOffPlugin weatherPower;
std::atomic<bool> requested{false}, resetRequested{false}, ready{false};
std::atomic<uint32_t> loopBeat{0};
std::atomic<Fault> reportedFault{Fault::None};
QueueHandle_t weatherQueue;
constexpr uint32_t kFreshMs = 30 * 60 * 1000;
constexpr uint32_t kPollMs = 10 * 60 * 1000;

void allOff() {
  digitalWrite(kPumpPin, LOW); digitalWrite(kMistPin, LOW); digitalWrite(kFanPin, LOW);
  digitalWrite(kRedPin, LOW); digitalWrite(kGreenPin, LOW); digitalWrite(kBluePin, LOW);
}

void controlTask(void *) {
  Safety safety;
  WeatherSample weather{};
  bool haveWeather = false;
  bool pwmOK = ledcAttach(kRedPin, 2000, 8);
  pwmOK = ledcAttach(kGreenPin, 2000, 8) && pwmOK;
  pwmOK = ledcAttach(kBluePin, 2000, 8) && pwmOK;
  esp_task_wdt_config_t wdt = { .timeout_ms = 2000, .idle_core_mask = 0, .trigger_panic = true };
  esp_err_t wdtResult = esp_task_wdt_add(nullptr);
  if (wdtResult == ESP_ERR_INVALID_STATE) {
    if (esp_task_wdt_init(&wdt) == ESP_OK) wdtResult = esp_task_wdt_add(nullptr);
  }
  if (wdtResult == ESP_OK && esp_task_wdt_reconfigure(&wdt) != ESP_OK)
    wdtResult = ESP_FAIL;
  // Do not energize loads if the safety task watchdog could not be registered.
  const bool driverOK = pwmOK && wdtResult == ESP_OK;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    const uint32_t now = millis();
    if (xQueueReceive(weatherQueue, &weather, 0) == pdTRUE) haveWeather = true;
    Inputs in;
    in.enabled = requested.load();
    in.network = WiFi.status() == WL_CONNECTED;
    const int64_t epoch = time(nullptr);
    in.fresh = haveWeather && uint32_t(now - weather.received) < kFreshMs &&
               epoch >= weather.observed && epoch - weather.observed < 1800;
    in.validated = TEMPESCOPE_HARDWARE_VALIDATED && driverOK;
    in.pumpWater = digitalRead(kPumpWaterPin) == LOW;
    in.mistWater = digitalRead(kMistWaterPin) == LOW;
    in.stopClosed = digitalRead(kStopPin) == LOW;
    in.supervisorAlive = !ready.load() || uint32_t(now-loopBeat.load()) < 1000;
    in.reset = resetRequested.exchange(false) || digitalRead(kResetPin) == LOW;
    in.day = weather.day;
    in.scene = weather.scene;
    const Outputs out = safety.tick(now, in);
    reportedFault.store(safety.fault);
    // Loss of network/data/health cancels the command. Recovery needs a new ON.
    if (in.enabled && (!in.network || !in.fresh || !in.validated || safety.fault != Fault::None))
      requested.store(false);
    digitalWrite(kPumpPin, out.pump); digitalWrite(kMistPin, out.mist);
    digitalWrite(kFanPin, out.fan);
    ledcWrite(kRedPin, out.red); ledcWrite(kGreenPin, out.green); ledcWrite(kBluePin, out.blue);
    if (wdtResult == ESP_OK) esp_task_wdt_reset();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
  }
}

struct Response { char body[2048]; size_t used = 0; bool overflow = false; };
esp_err_t httpEvent(esp_http_client_event_t *event) {
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  auto *response = static_cast<Response *>(event->user_data);
  const size_t length = static_cast<size_t>(event->data_len);
  if (length > sizeof(response->body)-1-response->used) {
    response->overflow = true; return ESP_FAIL;
  }
  memcpy(response->body+response->used, event->data, length);
  response->used += length;
  response->body[response->used] = 0;
  return ESP_OK;
}

bool fetchWeather(WeatherSample &sample) {
  char url[300];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=weather_code,is_day&timeformat=unixtime&timezone=GMT",
    double(TEMPESCOPE_LATITUDE), double(TEMPESCOPE_LONGITUDE));
  Response response{};
  esp_http_client_config_t config{};
  config.url = url;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.timeout_ms = 8000;
  config.event_handler = httpEvent;
  config.user_data = &response;
  config.disable_auto_redirect = true;
  auto client = esp_http_client_init(&config);
  if (!client) return false;
  esp_http_client_set_header(client, "User-Agent", "OpenTempescope-XIAO/0.1");
  const esp_err_t err = esp_http_client_perform(client);
  const int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  if (err != ESP_OK || status != 200 || response.overflow) return false;
  return parseWeather(response.body, response.used, time(nullptr), millis(), sample);
}

void weatherTask(void *) {
  bool clockStarted = false;
  uint32_t lastAttempt = 0, interval = 0;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!clockStarted) {
        configTime(0, 0, "time.cloudflare.com", "pool.ntp.org"); clockStarted = true;
      }
      const uint32_t now = millis();
      if (time(nullptr) > 1700000000 && (interval == 0 || uint32_t(now-lastAttempt) >= interval)) {
        lastAttempt = now;
        WeatherSample sample;
        if (fetchWeather(sample)) {
          xQueueOverwrite(weatherQueue, &sample); interval = kPollMs;
          Serial.println("Weather updated (Open-Meteo)");
        } else { interval = 60000; Serial.println("Weather failed; retry in 60 s"); }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

bool powerChanged(bool on) {
  if (on && (!ready.load() || !TEMPESCOPE_HARDWARE_VALIDATED || reportedFault.load() != Fault::None)) return false;
  requested.store(on);
  return true;
}

void setup() {
  const int outputs[] = {kPumpPin,kMistPin,kFanPin,kRedPin,kGreenPin,kBluePin};
  for (int pin : outputs) { digitalWrite(pin, LOW); pinMode(pin, OUTPUT); }
  allOff();
  for (int pin : {kPumpWaterPin,kMistWaterPin,kStopPin,kResetPin}) pinMode(pin, INPUT_PULLUP);
  Serial.begin(115200);
  weatherQueue = xQueueCreate(1, sizeof(WeatherSample));
  if (!weatherQueue) return; // outputs remain LOW; no Matter or network startup
  if (xTaskCreate(controlTask, "safety", 4096, nullptr, 5, nullptr) != pdPASS) return;
  if (xTaskCreate(weatherTask, "weather", 12288, nullptr, 1, nullptr) != pdPASS) return;
  if (!weatherPower.begin(false)) return; // Never restore ON after reboot/init failure.
  weatherPower.onChange(powerChanged);
  Matter.begin(); // S3: built-in BLE commissioning, operational transport Wi-Fi
  loopBeat.store(millis()); ready.store(true);
  Serial.println("OpenTempescope: off. Commands: on, off, reset, status, decommission");
  if (!TEMPESCOPE_HARDWARE_VALIDATED) Serial.println("DRY RUN: physical outputs disabled by configuration");
}

void loop() {
  loopBeat.store(millis());
  // Fixed buffer, never block the supervisor waiting for a newline.
  static char command[32]; static size_t length = 0; static bool overflow = false;
  for (int count=0; count<64 && Serial.available(); ++count) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      command[length] = 0;
      if (!overflow) {
        if (!strcmp(command,"off")) requested.store(false);
        else if (!strcmp(command,"on")) powerChanged(true);
        else if (!strcmp(command,"reset")) { requested.store(false); resetRequested.store(true); }
        else if (!strcmp(command,"status"))
          Serial.printf("requested=%d fault=%u wifi=%d validated=%d\n",requested.load(),
            unsigned(reportedFault.load()),WiFi.status()==WL_CONNECTED,TEMPESCOPE_HARDWARE_VALIDATED);
        else if (!strcmp(command,"decommission")) { requested.store(false); delay(30); Matter.decommission(); }
      }
      length=0; overflow=false;
    } else if (length < sizeof(command)-1) command[length++] = c;
    else overflow = true;
  }
  if (ready.load() && weatherPower.getOnOff() != requested.load())
    weatherPower.setOnOff(requested.load());
  static uint32_t lastCode = 0;
  if (ready.load() && !Matter.isDeviceCommissioned() && millis()-lastCode >= 10000) {
    lastCode=millis();
    Serial.printf("Pairing: %s\nQR: %s\n",Matter.getManualPairingCode().c_str(),
                  Matter.getOnboardingQRCodeUrl().c_str());
  }
  delay(10);
}
