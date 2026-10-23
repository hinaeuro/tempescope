#pragma once
#include <Arduino.h>
#include <stdarg.h>

namespace tempescope {
namespace drivers {
class ConsoleDriver {
 public:
  /** 関数名: begin / 内容: USBシリアルを115200bpsで開始する。 */
  void begin() {
    Serial.begin(115200);
  }

  /** 関数名: connected / 内容: USBシリアルの接続状態を読む。出力: 接続時true。 */
  bool connected() const {
    return bool(Serial);
  }

  /** 関数名: available / 内容: 受信済みデータの有無を読む。出力: 読出可能ならtrue。 */
  bool available() const {
    return Serial.available() > 0;
  }

  /** 関数名: read / 内容: 1文字受信する。出力: 文字または-1。available()確認後に使用する。 */
  int read() {
    return Serial.read();
  }

  /** 関数名: println / 内容: 文字列と改行を送る。入力: text=UTF-8またはASCII文字列。 */
  void println(const char* text) {
    Serial.println(text);
  }

  /** 関数名: printf / 内容: 書式付き文字列を送る。入力: formatと対応する引数。注意: 送信はSDK依存。
   */
  void printf(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    Serial.vprintf(format, arguments);
    va_end(arguments);
  }
};
}  // namespace drivers
}  // namespace tempescope
