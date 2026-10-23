#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Arduinoに依存しないコマンド処理。時間はmillis()の周回にも対応。
namespace demo {
struct Timer {
  bool on = false;
  uint32_t start = 0;
  uint32_t duration = 0;
  /** 関数名: tick / 内容: 単発タイマーの期限を確認する。入力: now=millis。期限後onをfalseにする。
   */
  void tick(uint32_t now) {
    if (on && uint32_t(now - start) >= duration) {
      on = false;
    }
  }
  /** 関数名: begin / 内容: 単発タイマーを開始する。入力: now=開始時刻、ms=有効時間。 */
  void begin(uint32_t now, uint32_t ms) {
    on = true;
    start = now;
    duration = ms;
  }
};
class Controller {
 public:
  Timer led;
  Timer fan;
  Timer pump;
  Timer mist;
  unsigned r = 0;
  unsigned g = 0;
  unsigned b = 0;
  bool pumpAllowed = false;
  bool mistAllowed = false;
  bool driverOK = false;
  bool haveLiquidStart = false;
  uint32_t lastLiquidStart = 0;

  /** 関数名: stop / 内容: 全負荷をOFFにする。注意: 許可と液体負荷の60秒間隔は維持する。 */
  void stop() {
    led.on = fan.on = pump.on = mist.on = false;
  }
  /** 関数名: tick / 内容: 各期限とドライバー状態を確認する。入力: now。異常時は全停止。 */
  void tick(uint32_t now) {
    led.tick(now);
    fan.tick(now);
    pump.tick(now);
    mist.tick(now);
    if (!driverOK) {
      stop();
    }
  }
  /**
   * 関数名: number
   * 内容: 符号なし十進整数を上限付きで解析する。
   * 入力: s=文字列、maximum=上限。出力: 成功ならtrueとvalue。失敗時valueは未規定。
   */
  static bool number(const char* s, unsigned& value, unsigned maximum) {
    if (!s || !*s) {
      return false;
    }
    value = 0;
    for (; *s; ++s) {
      if (*s < '0' || *s > '9') {
        return false;
      }
      const unsigned digit = unsigned(*s - '0');
      if (value > maximum / 10 || (value == maximum / 10 && digit > maximum % 10)) {
        return false;
      }
      value = value * 10 + digit;
    }
    return true;
  }
  /**
   * 関数名: command
   * 内容: デモコマンドを解析し、個別出力・許可・時間制限を更新する。
   * 入力: input=コマンド、now=millis。出力: 静的な応答文字列。解放不要。
   * 注意: 同一Controllerへは単一呼出元から使用。バッファと引数数には上限がある。
   */
  const char* command(const char* input, uint32_t now) {
    char line[96] = {};
    char* args[7] = {};
    unsigned count = 0;
    unsigned limit = 30000;
    unsigned duration = 5000;
    unsigned nr = 0;
    unsigned ng = 0;
    unsigned nb = 0;
    unsigned durationIndex = 2;
    char* context = nullptr;
    Timer* timer = nullptr;

    tick(now);
    if (strlen(input) >= sizeof(line)) {
      stop();
      return "入力が長すぎるため全停止";
    }
    strcpy(line, input);
    for (char* s = strtok_r(line, " \t", &context); s; s = strtok_r(nullptr, " \t", &context)) {
      if (count == 7) {
        return "引数が多すぎる";
      }
      args[count++] = s;
    }
    if (!count) {
      return "空のコマンド";
    }
    if (count == 1 && (!strcmp(args[0], "off") || !strcmp(args[0], "stop"))) {
      stop();
      return "全停止";
    }
    if (count == 1 && !strcmp(args[0], "disarm")) {
      stop();
      pumpAllowed = mistAllowed = false;
      return "全停止・ポンプとミストを禁止";
    }
    if (!strcmp(args[0], "arm") && count == 2) {
      if (!strcmp(args[1], "pump")) {
        pumpAllowed = true;
      } else if (!strcmp(args[1], "mist")) {
        mistAllowed = true;
      } else {
        return "arm pump または arm mist を指定";
      }
      return "試運転を許可（再起動またはdisarmで解除）";
    }
    if (!strcmp(args[0], "led")) {
      timer = &led;
    } else if (!strcmp(args[0], "fan")) {
      timer = &fan;
    } else if (!strcmp(args[0], "pump")) {
      timer = &pump;
      limit = duration = 1000;
    } else if (!strcmp(args[0], "mist")) {
      timer = &mist;
      limit = duration = 2000;
    } else {
      return "不明なコマンド（helpで一覧）";
    }
    if (count == 2 && !strcmp(args[1], "off")) {
      timer->on = false;
      return "停止";
    }
    if (timer == &led) {
      if (count < 2) {
        return "led red / green / blue / white / rgb R G B / off";
      }
      if (!strcmp(args[1], "rgb")) {
        if (count < 5 || !number(args[2], nr, 255) || !number(args[3], ng, 255) ||
            !number(args[4], nb, 255)) {
          return "RGBは0〜255の整数で指定";
        }
        durationIndex = 5;
      } else if (!strcmp(args[1], "red")) {
        nr = 64;
      } else if (!strcmp(args[1], "green")) {
        ng = 64;
      } else if (!strcmp(args[1], "blue")) {
        nb = 64;
      } else if (!strcmp(args[1], "white")) {
        nr = ng = nb = 64;
      } else {
        return "不明なLED色";
      }
    } else if (count < 2 || strcmp(args[1], "on")) {
      return "on または off を指定";
    }
    if (count != durationIndex && count != durationIndex + 1) {
      return "引数の数が不正";
    }
    if (count == durationIndex + 1 &&
        (!number(args[durationIndex], duration, limit) || !duration)) {
      return "時間は1以上・上限以内のミリ秒で指定";
    }
    if (!driverOK) {
      return "出力初期化失敗のため動作禁止";
    }
    // LEDは色変更を許すが、開始からの期限を延長しない。
    if (timer->on) {
      if (timer == &led) {
        r = nr;
        g = ng;
        b = nb;
        return "色変更（停止期限は延長しない）";
      }
      return "動作中のため拒否（期限は延長しない）";
    }
    if (timer == &pump || timer == &mist) {
      if ((timer == &pump && !pumpAllowed) || (timer == &mist && !mistAllowed)) {
        return "未許可（armが必要）";
      }
      if (pump.on || mist.on) {
        return "ポンプ・ミストの同時動作は禁止";
      }
      if (haveLiquidStart && uint32_t(now - lastLiquidStart) < 60000) {
        return "ポンプ・ミスト共通の60秒待ち時間中";
      }
      haveLiquidStart = true;
      lastLiquidStart = now;
    }
    if (timer == &led) {
      r = nr;
      g = ng;
      b = nb;
    }
    timer->begin(now, duration);
    return "動作開始";
  }
};
}  // namespace demo
