#pragma once
// このファイルを同じフォルダの LocalConfig.h へコピーして使う。
// 自動モードの電気的確認（docs/esp32s3/commissioning.md）完了後にだけ1にする。
#define TEMPESCOPE_HARDWARE_VALIDATED 0
// 天気を取得する地点の緯度・経度（既定: 浜松）。
#define TEMPESCOPE_LATITUDE 34.7108
#define TEMPESCOPE_LONGITUDE 137.7261
// Wi-Fi情報はソースに書かず、自動モードのMatter初期登録で渡す。

// 1: USBシリアルの単発試運転。Wi-Fi・Matterは起動しない。
// 0: 自動天気・Alexaモード。水位・停止入力の追加と確認が必要。
#define TEMPESCOPE_BENCH_MODE 1
// ポンプの駆動回路・電流・給水を確認した後にだけ1にする。
#define TEMPESCOPE_BENCH_PUMP_ENABLED 0
// リレー駆動・5V電源・ミスト発生器の水深を確認した後にだけ1にする。
#define TEMPESCOPE_BENCH_MIST_ENABLED 0
