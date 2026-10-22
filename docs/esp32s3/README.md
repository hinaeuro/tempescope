# XIAO ESP32S3移植

**2026-10-22更新:** 今回の実装回路は[実装配線と試運転](as-built.md)を先に読むこと。既定はUSBシリアルのベンチモード。以下のWi-Fi/Matter説明は自動モードのもの。

元master `eef729041b61eb9d43cc1621082516c81ce04cbc` を保持した追加実装。
Arduino-ESP32 **3.3.8**、ArduinoJson **7.3.1**、通常版Seeed XIAO ESP32S3を対象とする。
旧AVRファームとBLEアプリは変更していない。新ファームは `firmware/xiao_s3/`。

## USBコマンドで動かすデモ

LEDの色・ファン・ポンプ・ミストを個別に操作する場合は、[デモの操作・書き込み手順](demo-ja.md)を参照。
ソースは `firmware/xiao_s3_demo/xiao_s3_demo.ino`。通常ファームとは別に書き込む。

## はじめに

ソースの取得・Arduino IDEの準備・USB書き込みは、[Windows向け日本語書き込み手順](flashing-ja.md)から進める。
現在の配線での試運転は[実装配線と試運転](as-built.md)を参照する。
コンパイル済みファームは公開していない。10月22日の変更後はフルビルド未完了・実機未検証。

## 読む順序

1. [配線・電気的検討](hardware.md): 元端子の照合、追加部品、変更箇所、未確定点。
2. [ビルド・実機確認](commissioning.md): 書込み、Alexa登録、安全停止試験。
3. [検証記録](validation.md): 実際に通したビルド・ホストテストと残課題。

## 実装範囲

- Matter over Wi-FiのOn/Off Plugin Unitを1台公開。Alexaで命名後「テンプスコープをつけて／消して」。ONで現在の天気を自動再現。天気ごとの個別音声コマンドは今回の範囲外。
- ESP32-S3内蔵BLEはMatterの初期登録に使用。外付けBLESerialとそのUART配線は不要。
- Open-Meteoのcurrent weather_code/is_dayをHTTPSで10分ごとに取得。既定座標は浜松。現在値は天気モデルの現在時刻の値で、現地の実況センサー値とは限らない。
- 晴れ:照明、曇り・霧:ミスト＋ファン、雨:ポンプ、雷雨:ポンプ＋点滅。雪はミスト／青系照明で近似し、水を雨として流さない。風・気温・積雪そのものは再現しない。
- 水位・停止入力は10ms周期の別タスク。HTTP待ち中も監視する。異常はラッチし、OFF＋安全条件復帰＋resetで解除。
- 再起動は常にOFF。Wi-Fi切断、天気期限切れでOFF指令に戻し、復旧後は新しいON操作が必要。

**自動モードの初期設定は `TEMPESCOPE_HARDWARE_VALIDATED=0` で全出力を禁止する。ベンチモードではLED・ファンのみ試験可能。**
回路の未確認点があるため、実機チェック完了後にだけローカル設定で1にする。
ビルド成功はポンプ電流、LED型番、リレーの突入電流適合、Alexa登録の成功を意味しない。

## 制御の優先順位

停止ループ開放／監視異常／水不足／運転上限 > 無効設定／Wi-Fiなし／古い天気 > OFF > 天気演出。

|条件|挙動|復帰|
|---|---|---|
|起動・ブラウンアウト|全負荷OFF、ONを保存しない|新規ON|
|水位接点開放・断線|全負荷OFF。運転中は水不足ラッチ|両水位が2秒安定、OFF後reset|
|停止NCループ開放|全負荷OFF、停止ラッチ|ループ復帰、OFF後reset|
|loopの更新が1秒停止|全負荷OFF、監視異常ラッチ|loop復帰、OFF後reset|
|安全タスク停止|2秒のTask WDTでパニック／リセットを要求|再起動後OFF。実時間は実測する|
|Wi-Fi切断|全負荷OFF、ON指令を取消|接続・データ復帰後に新規ON|
|HTTPS失敗|60秒後再試行。前回の有効な天気は観測時刻から30分以内だけ利用|期限切れでOFF、更新後に新規ON|
|連続セッション30分|全負荷OFF、上限ラッチ|OFF後reset、新規ON|

ポンプ10秒ON/60秒周期、ミスト20秒ON/60秒周期。いずれも**仮の試運転上限**であり、メーカー保証値ではない。現物の流量・水戻り・最低水位に基づいて変更する。元の演出時間をそのまま移植してはいない。

単一の水位接点では接点固着・対GND短絡・閉塞・漏水・ポンプ故障までは判別できない。今回の実装は水位低下／断線を検出する方式。数か月無人運転の認定や耐久検証は未実施。

## 参照

- [Espressif Matter実装（固定版）](https://github.com/espressif/arduino-esp32/tree/3.3.8/libraries/Matter)
- [AlexaのMatter対応](https://developer.amazon.com/en-US/docs/alexa/smarthome/matter-support.html)
- [Open-Meteo API](https://open-meteo.com/en/docs) / [利用条件](https://open-meteo.com/en/terms)

天気データ: Open-Meteo.com。非商用公開APIを想定し、商用利用は利用条件・契約を別途確認する。
