# XIAO ESP32S3への書き込み（Windows・Arduino IDE）

対象は通常版Seeed XIAO ESP32S3。まずはUSBだけで書き込みとコマンド応答を確認する。
「ソース」は人が編集するプログラム、「ビルド」は機器が実行できる形へ変換する処理、
「ファームウェア」はその実行プログラム、「書き込み」は機器のフラッシュへ転送する操作。

## 1. 正しいブランチをダウンロード

1. [ESP32S3ブランチ](https://github.com/hinaeuro/tempescope/tree/esp32s3/xiao-weather-matter)を開く。
2. ブランチ名が `esp32s3/xiao-weather-matter` になっていることを確認する。
3. 緑色の「Code」→「Download ZIP」で取得し、ZIPを展開する。
4. 展開先に `firmware/xiao_s3/xiao_s3.ino` と共有ソースの `firmware/common/` があることを確認する。
   スケッチだけを取り出さず、リポジトリ全体のフォルダ構成を維持する。

`master` のZIPにはESP32S3版がない。旧版の `OpenTempescope.ino` ではなく `xiao_s3.ino` を開く。

## 2. Arduino IDEを準備

1. [Arduino公式サイト](https://www.arduino.cc/en/software/)からArduino IDE 2系を導入する。
2. 「ファイル → 基本設定」の「追加のボードマネージャのURL」に以下を追加する。

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

3. 「ツール → ボード → ボードマネージャ」で `esp32` を検索し、**esp32 by Espressif Systems の3.3.8**をインストールする。
4. 「スケッチ → ライブラリをインクルード → ライブラリを管理」で **ArduinoJson by Benoit Blanchon の7.3.1**をインストールする。
5. 「ファイル → 開く」で `firmware/xiao_s3/xiao_s3.ino` を開く。同じフォルダの `.h` も必要なので、inoだけ別の場所にコピーしない。

MatterはESP32コアに同梱される。名前の似た別のMatterライブラリを追加する必要はない。
最新版へ自動で置き換えず、まず上記の固定バージョンを使う。

## 3. ボード設定

「ツール」で以下を選択する。表示名はIDEの言語により多少異なる。

|項目|選ぶ値|
|---|---|
|ボード（Board）|`XIAO_ESP32S3`|
|PSRAM|`OPI PSRAM`|
|USB CDC On Boot（起動時USBシリアル）|`Enabled`（有効）|
|Partition Scheme（フラッシュの区画構成）|`Maximum APP (7.9MB APP No OTA/No FS)`|
|ポート（Port）|XIAOを接続すると現れる `COM番号`|

その他は選択したボードの既定値を使う。この区画構成ではOTA（無線でのファーム更新）は使わない。

## 4. 初回は既定設定でビルド

そのままなら `Config.h` の既定値でUSBシリアル試運転モードになる。
最初は設定ファイルの作成やWi-FiのSSID・パスワード入力は不要。

|設定|既定値|意味|
|---|---:|---|
|`TEMPESCOPE_BENCH_MODE`|1|USBシリアルから単発試運転。Wi-Fi・Matterは起動しない|
|`TEMPESCOPE_BENCH_PUMP_ENABLED`|0|ポンプ試運転を禁止|
|`TEMPESCOPE_BENCH_MIST_ENABLED`|1|ミストの2秒試運転を許可|
|`TEMPESCOPE_HARDWARE_VALIDATED`|0|自動モードの物理出力を禁止。ベンチのLED・ファン試験は可能|

IDE左上の「✓」（検証）でコンパイルする。初回は時間がかかる。
失敗した場合は書き込みへ進まず、出力欄のエラーを確認する。
**10月24日: 配線・BENCH変更と層分離後のソースについて、ESP32S3向けコンパイル・リンクを確認済み。実機の電気的動作は別途確認する。**

## 5. USBで書き込み

1. 外部24V/12V電源と負荷を外し、XIAOをデータ通信対応のUSBケーブルでPCに接続する。
2. 「ツール → ポート」で該当COMポートを選ぶ。抜き差ししたときに増減するポートで判別できる。
3. 左上の「→」（マイコンボードに書き込む）を押す。コンパイルと書き込みが順番に行われる。
4. 書き込み完了後に動き始めない場合は、BOOTを押さずにRESETを押す。
5. 起動後にCOM番号が変わった場合は、ポートを選び直す。

ポートが見えない／接続待ちで失敗する場合:

1. シリアルモニタを閉じる。
2. USBを抜き、XIAOの **BOOTを押したままUSBを接続**する。
3. BOOTを離し、現れたCOMポートを選んで再度書き込む。
4. 完了したらRESETを押し、必要ならCOMポートを選び直す。

BOOTは書き込みモードに入るためのボタン、RESETは再起動ボタン。
D2/GPIO3は起動設定に関係するため、外付け回路接続時だけ失敗する場合は、外付け回路を外したUSB単体でも確認する。

## 6. USBシリアルで応答確認

「ツール → シリアルモニタ」を開き、**115200 baud、改行あり**に設定する。
起動案内に `BENCH` と出ることを確認し、入力欄から `status` を送る。

```text
bench=1 driver=1 pump_allowed=0 mist_allowed=1 active=0
```

これは正常時の表示例。実際の値を確認する。

|表示|意味|
|---|---|
|`bench=1`|USBシリアル試運転モード|
|`driver=1`|PWMと監視タイマーの初期化成功。外部回路の正常判定ではない|
|`pump_allowed=0`|ポンプ試運転は禁止|
|`mist_allowed=1`|ミスト試運転を許可|
|`active=0`|現在は停止中|

USB単体ではコマンド応答だけを確認し、外部回路接続後の試験は[実装配線と試運転](as-built.md)に従う。

|入力するコマンド|動作|
|---|---|
|`red` / `green` / `blue` / `white`|赤・緑・青・白を最大5秒点灯（PWM 64/255）|
|`fan`|ファンを最大5秒運転|
|`pump`|ポンプを最大1秒運転。既定では拒否|
|`mist`|ミストを最大2秒運転。既定で許可|
|`off`|全出力を停止|
|`status`|状態を表示|

入力するコマンドは表の英字をそのまま使う。
`OK` は受け付けたという意味。
`Rejected: locked, busy, cooldown, driver failure or unknown command` は、
「許可なし・動作中・待ち時間中・初期化失敗・不明なコマンドのいずれかで拒否」という意味。
ポンプとミストには共通で60秒の開始間隔があり、動作中の追加指令で時間は延長しない。

## 7. 設定を変える場所

`firmware/xiao_s3/LocalConfig.example.h` を同じフォルダへコピーし、名前を `LocalConfig.h` にする。
Windowsで拡張子を表示し、`LocalConfig.h.txt` にならないようにする。
変更後はもう一度ビルドして書き込む。設定変更だけでは本体へ反映されない。
`LocalConfig.h` はgit管理対象外。

現状のポンプ駆動回路の課題はソフトでは解決しないので、
ポンプ許可を1にする前に[実装配線の注意点](as-built.md)を確認する。
自動天気・Alexaへ進む場合は水位・停止入力を追加し、
[自動モードの確認手順](commissioning.md)に従う。ベンチモードのままではAlexaに現れない。

## ソースと生成ファームの場所

|ファイル・場所|役割|
|---|---|
|`firmware/xiao_s3/xiao_s3.ino`|Arduinoのsetup/loop入口|
|`firmware/xiao_s3/Application.h`|起動・USBコマンド・タスク周期の接続|
|`firmware/common/drivers/`|GPIO/PWM・USB・監視・Wi-Fi/Matter/HTTPSの適合処理|
|`firmware/common/library/`|安全・BENCH・デモ・JSON解析などの共通実装|
|`firmware/xiao_s3/Config.h`|GPIO割り当てと既定設定|
|`firmware/xiao_s3/LocalConfig.example.h`|個人設定のひな形|
|`firmware/xiao_s3/Bench.h`|単発試運転の制御|
|`firmware/xiao_s3/Safety.h`|自動モードの停止・水位・時間制限|
|`firmware/xiao_s3/WeatherData.h`|天気データの解析|
|`docs/esp32s3/`|日本語の説明|
|`scripts/build-esp32s3.sh`|LinuxなどでArduino CLIを使う開発用スクリプト|

**配布済みの完成ファームはない。** Arduino IDEの「→」で書き込む場合、binを自分で探す必要はない。
binを保存したい場合は、ボード設定後に「スケッチ → コンパイル済みバイナリをエクスポート」を使い、
スケッチフォルダ内の `build` 以下を確認する。出力場所はIDEのビルドログでも確認できる。
アプリ単体binと、ブートローダ・パーティションなどを含む書き込み一式は別物なので、
単体binを任意のアドレスへ書き込まない。

開発用スクリプトの場合、既定出力先はリポジトリ外の `/tmp/tempescope-esp32s3/`。
一時フォルダなので過去のビルド記録にbinのハッシュがあっても、GitHubにファイルがあるという意味ではない。
Windowsで初めて使う場合は、このページのArduino IDE手順で進めればよい。

参考: [Seeed公式・日本語の導入ガイド](https://wiki.seeedstudio.com/ja/xiao_esp32s3_getting_started/)

## 2026-10-23 ファン・ミスト試運転設定

ファンは従来から許可済み。ミストの既定値を1に変更した。
既に `LocalConfig.h` がある場合、その `TEMPESCOPE_BENCH_MIST_ENABLED` も1へ変更して再ビルド・再書き込みする。
ポンプの許可は0のまま。GPIO対応は上記の配線資料を参照。


ソースの層構成・静的領域の方針は[構成資料](architecture-ja.md)を参照。
