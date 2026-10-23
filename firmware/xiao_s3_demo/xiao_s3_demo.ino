// SPDX-License-Identifier: LGPL-2.1-or-later
#include "Application.h"

/** 関数名: setup / 内容: Arduinoの初期化入口。デモアプリを起動する。入力/出力: なし。 */
void setup() {
  demo::application::begin();
}

/** 関数名: loop / 内容: Arduinoの周期入口。デモアプリの1周期を実行する。入力/出力: なし。 */
void loop() {
  demo::application::update();
}
