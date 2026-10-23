#pragma once
#include <Matter.h>
#include <WiFi.h>
#include <atomic>
#include <time.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include "../library/WeatherData.h"
#include "ConsoleDriver.h"

namespace tempescope {
namespace drivers {
/** 関数名: networkConnected / 内容: Wi-Fi接続状態を読む。出力: 接続済みならtrue。 */
inline bool networkConnected() {
  return WiFi.status() == WL_CONNECTED;
}

/** 関数名: startClock / 内容: SNTPでUTC時刻の取得を開始する。通信完了までは待たない。 */
inline void startClock() {
  configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");
}

/** 関数名: epochNow / 内容: 現在のUTC秒を返す。注意: 時計未設定時は有効な日時ではない。 */
inline int64_t epochNow() {
  return time(nullptr);
}

// SDKコールバックはこのドライバー内で完結し、上位関数を登録・呼出ししない。
// 関数ポインターの代わりに、上位層がtakeRequest()で要求値を読み出す。
class MatterDriver {
 public:
  /** 関数名: begin / 内容: 初期OFFのMatter機器を登録する。出力: 初期化成功ならtrue。 */
  bool begin() {
    if (!endpoint_.begin(false)) {
      return false;
    }
    endpoint_.onChange(receiveChange);
    Matter.begin();
    return true;
  }

  /** 関数名: setOnAllowed / 内容: 次の外部ON要求を受理できるか設定する。入力: allowed=許可。 */
  void setOnAllowed(bool allowed) {
    onAllowed_.store(allowed);
  }

  /** 関数名: takeRequest / 内容: 最新の未読要求を取得する。出力: 有無とon。要求は取得時に消費。 */
  bool takeRequest(bool& on) {
    const int value = pending_.exchange(-1);
    if (value < 0) {
      return false;
    }
    on = value != 0;
    return true;
  }

  /** 関数名: synchronize / 内容: アプリ側のON/OFFをMatterへ報告する。入力: on=報告値。 */
  void synchronize(bool on) {
    if (endpoint_.getOnOff() != on) {
      endpoint_.setOnOff(on);
    }
  }

  /** 関数名: commissioned / 内容: Matter登録状態を返す。出力: 登録済みならtrue。 */
  bool commissioned() const {
    return Matter.isDeviceCommissioned();
  }

  /** 関数名: printPairing / 内容: ペアリング情報をシリアルドライバーへ渡す。注意:
   * コードは公開しない。 */
  void printPairing(ConsoleDriver& console) {
    console.printf("Pairing: %s\nQR: %s\n", Matter.getManualPairingCode().c_str(),
                   Matter.getOnboardingQRCodeUrl().c_str());
  }

  /** 関数名: decommission / 内容: Matterとネットワークの登録を解除する。先に上位層で停止する。 */
  void decommission() {
    Matter.decommission();
  }

 private:
  /**
   * 関数名: receiveChange
   * 内容: SDKから来た要求をドライバー内のメールボックスへ保存する。
   * 入力: on=要求。出力: 受理ならtrue。上位アプリの関数は呼び出さない。
   * 注意: このドライバーは1機器専用。複数機器へ拡張する場合はメールボックスを分離する。
   */
  static bool receiveChange(bool on) {
    if (on && !onAllowed_.load()) {
      return false;
    }
    pending_.store(on ? 1 : 0);
    return true;
  }

  MatterOnOffPlugin endpoint_;
  inline static std::atomic<bool> onAllowed_{false};
  inline static std::atomic<int> pending_{-1};
};

class WeatherClient {
 public:
  /**
   * 関数名: fetch
   * 内容: Open-MeteoのJSONをHTTPSで取得し、下位の解析ライブラリで検証する。
   * 入力: latitude/longitude=座標。出力: 成功ならtrueとsample、失敗時sampleは変更しない。
   * 注意: 単一タスク専用。応答領域は固定長。HTTP/TLSの内部確保はSDKに依存する。
   */
  bool fetch(double latitude, double longitude, WeatherSample& sample) {
    esp_http_client_config_t config = {};
    esp_http_client_handle_t client = nullptr;
    esp_err_t result = ESP_FAIL;
    int status = 0;
    int length = 0;

    response_.used = 0;
    response_.overflow = false;
    response_.body[0] = '\0';
    length = snprintf(url_, sizeof(url_),
                      "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                      "&current=weather_code,is_day&timeformat=unixtime&timezone=GMT",
                      latitude, longitude);
    if (length < 0 || static_cast<size_t>(length) >= sizeof(url_)) {
      return false;
    }
    config.url = url_;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 8000;
    config.event_handler = receiveData;
    config.user_data = &response_;
    config.disable_auto_redirect = true;
    client = esp_http_client_init(&config);
    if (client == nullptr) {
      return false;
    }
    esp_http_client_set_header(client, "User-Agent", "OpenTempescope-XIAO/0.1");
    result = esp_http_client_perform(client);
    status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (result != ESP_OK || status != 200 || response_.overflow) {
      return false;
    }
    return parseWeather(response_.body, response_.used, epochNow(), millis(), sample);
  }

 private:
  struct Response {
    char body[2048] = {};
    size_t used = 0;
    bool overflow = false;
  };

  /**
   * 関数名: receiveData
   * 内容: HTTPデータを固定長受信領域へ追記するSDKコールバック。
   * 入力: event=SDKイベント。出力: 正常ESP_OK、容量超過ESP_FAIL。
   * 注意: アプリケーション関数を呼ばず、ドライバー所有のデータだけを更新する。
   */
  static esp_err_t receiveData(esp_http_client_event_t* event) {
    Response* response = nullptr;
    size_t length = 0;
    if (event->event_id != HTTP_EVENT_ON_DATA) {
      return ESP_OK;
    }
    response = static_cast<Response*>(event->user_data);
    if (response == nullptr || event->data_len < 0) {
      return ESP_FAIL;
    }
    length = static_cast<size_t>(event->data_len);
    if (length > sizeof(response->body) - 1 - response->used) {
      response->overflow = true;
      return ESP_FAIL;
    }
    memcpy(response->body + response->used, event->data, length);
    response->used += length;
    response->body[response->used] = '\0';
    return ESP_OK;
  }

  char url_[300] = {};
  Response response_;
};
}  // namespace drivers
}  // namespace tempescope
