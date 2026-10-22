# Tempescope（テンプスコープ）

箱の中で雨・雲・雷などの天気を再現する装置。OpenTempescopeは、そのオープンソース版だよ。

## XIAO ESP32S3版を使う場合

**ESP32S3版は `master` ではなく、`esp32s3/xiao-weather-matter` ブランチにある。**

- [LED・ファン・ポンプ・ミストをUSBコマンドで操作するデモ](https://github.com/hinaeuro/tempescope/blob/esp32s3/xiao-weather-matter/docs/esp32s3/demo-ja.md)
- [ESP32S3版のソースを開く](https://github.com/hinaeuro/tempescope/tree/esp32s3/xiao-weather-matter/firmware/xiao_s3)
- [日本語の書き込み手順（Windows・Arduino IDE）](https://github.com/hinaeuro/tempescope/blob/esp32s3/xiao-weather-matter/docs/esp32s3/flashing-ja.md)
- [現在の実装配線と試運転コマンド](https://github.com/hinaeuro/tempescope/blob/esp32s3/xiao-weather-matter/docs/esp32s3/as-built.md)
- [ESP32S3版の全体説明](https://github.com/hinaeuro/tempescope/blob/esp32s3/xiao-weather-matter/docs/esp32s3/README.md)

GitHub左上のブランチ選択が `master` なら、`esp32s3/xiao-weather-matter` に切り替えてから「Code → Download ZIP」で取得する。
先にmasterをダウンロードしても、ESP32S3版は含まれない。

現在はソースからビルド（コンパイル）してUSBで書き込む方式。コンパイル済みの `.bin` やGitHub Releasesは公開していない。
既定はUSBシリアルの単発試運転モードで、Wi-Fi・Alexaは起動しない。
2026-10-22の変更後はESP32S3向けフルビルド未完了・実機未検証。詳細は検証記録を参照。

## ファイルの場所

|場所|内容|
|---|---|
|`firmware/xiao_s3/`（ESP32S3ブランチのみ）|XIAO ESP32S3版のソース。開くファイルは `xiao_s3.ino`|
|`docs/esp32s3/`（ESP32S3ブランチのみ）|日本語の配線・書き込み・試運転・検証記録|
|`scripts/`、`tests/`（ESP32S3ブランチのみ）|開発用のビルドスクリプト・PC上のテスト|
|`OpenTempescope/OpenTempescope/`|旧ATmega328P版のソース。ESP32S3への書き込み対象ではない|
|`OpenTempescope/TempescopeController/`|旧iOS向けBLE操作アプリ|
|`hardware/`|元の回路図・CADデータ・部品表|
|`images/`|元の構造図・回路図の画像|

## 元のATmega328P版

`OpenTempescope/OpenTempescope/OpenTempescope.ino` をATmega328Pへ書き込み、本体に取り付ける。
旧iOSアプリはBLEで本体を操作する。元の配布版には天気サービスとの連携機能は含まれない。

- [元プロジェクトの製作Wiki](https://github.com/kenkawakenkenke/tempescope/wiki)
- 元プロジェクトのWebサイト: http://tempescope.com
- 原作者: Ken Kawamoto（ken@kawamoto.co.uk）
- ライセンス: [LICENSE.txt](LICENSE.txt)
