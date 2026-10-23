# ESP32S3ファームの構成・実装方針

対象: `esp32s3/xiao-weather-matter` ブランチの `firmware/xiao_s3`（通常/BENCH）と
`firmware/xiao_s3_demo`（USBコマンドデモ）。旧ATmega328P版と旧iOSアプリは対象外。
2026-10-24に構成を整理した。

## 層と依存方向

自作コードの依存は **アプリケーション → ドライバー → ライブラリ** の下向きとする。
アプリはハードウェアに依存しない制御ライブラリを直接使用してもよい。
ライブラリ同士・ドライバー同士の下位機能呼出しは許可するが、上位のアプリは参照しない。

|層|場所|責務|呼び出してよい相手|
|---|---|---|---|
|アプリケーション|各スケッチの `.ino`、`Application.h`|起動、モード選択、タスク周期、入力と出力の接続|ドライバー、制御ライブラリ、基盤SDKの時計・タスクAPI|
|ドライバー|`firmware/common/drivers/`|GPIO/PWM、USB、監視タイマー、Matter/HTTPSのSDK適合|同層のドライバー、下位ライブラリ、外部SDK|
|自作ライブラリ|`firmware/common/library/`|値型、安全判定、単発試験、コマンド解析、固定領域管理|同層のライブラリ、標準C/C++、ArduinoJson|
|外部ライブラリ・基盤|Arduino-ESP32、ESP-IDF、FreeRTOS、Matter、Wi-Fi、TLS、ArduinoJson|周辺機能・OS・プロトコルの実装|外部製品の内部構造による|

ドライバーは「ポンプをいつ動かすか」を判断しない。上位から渡された出力値を適用する。
制御ライブラリは `digitalWrite`、`ledcWrite`、`WiFi`、`Matter`、USBを呼ばず、
入力値と時刻から出力値を算出する。そのためPC上でも同じ制御コードを検証できる。

## ファイルごとの役割

|ファイル|内容|
|---|---|
|`xiao_s3/xiao_s3.ino`|Arduinoのsetup/loop入口だけ|
|`xiao_s3/Application.h`|通常/BENCHのタスク・状態・コマンド処理の接続|
|`xiao_s3/Config.h`、`LocalConfig.h`|GPIO割当と利用者設定。ドライバーに設定を渡すのはアプリの責任|
|`xiao_s3_demo/xiao_s3_demo.ino`|デモのsetup/loop入口だけ|
|`xiao_s3_demo/Application.h`|デモの受信・表示・制御結果と出力の接続|
|`common/drivers/BoardDriver.h`|GPIO初期化、RGB PWM、入力読出し、出力反映、タスクWDT|
|`common/drivers/ConsoleDriver.h`|USBシリアルの読出し・文字列送信|
|`common/drivers/NetworkDriver.h`|Wi-Fi状態・時計取得、Matterの受信保持、HTTPS受信|
|`common/library/Types.h`|入力・出力・天気・異常の共有値型|
|`common/library/Safety.h`|自動モードの安全停止と演出判定|
|`common/library/Bench.h`|通常ファームのBENCH単発制御|
|`common/library/Demo.h`|別デモのコマンド・個別出力・単発期限管理|
|`common/library/WeatherData.h`|天気JSONの型・時刻・コード検証|
|`common/library/FixedJsonArena.h`|ArduinoJson用の固定16KiB領域|
|`common/library/LineBuffer.h`|通常/BENCHの固定長受信行|

既存の `xiao_s3/Safety.h`、`Bench.h`、`WeatherData.h`、`xiao_s3_demo/Demo.h` は、
過去のincludeパスを維持するための互換入口。実装は `common/library` のみを編集する。
同じGPIO/PWM/WDT実装を通常版とデモで共有し、複製しない。

ヘッダー内実装は `inline` またはクラス内定義とし、複数翻訳単位でも定義が衝突しないようにする。
`.ino` を入口だけにすることでArduinoの自動プロトタイプ生成に依存しない。

## 受信イベントで上位を呼ばない仕組み

Matter受信は、SDKが `MatterDriver::receiveChange()` を呼ぶ。
この関数はドライバー内の最新要求メールボックスへ値を保存するだけで、
アプリケーション関数・利用者コールバックを呼ばない。
アプリの周期処理が `takeRequest()` を呼び、取り出した値を `requestPower()` で検証する。
ドライバーが許可状態を読むための値も、アプリから `setOnAllowed()` で下向きに渡す。
メールボックスは「最新値だけ保持」で、全操作の履歴キューではない。

HTTPSのイベントも `WeatherClient::receiveData()` 内で受信領域を更新するだけ。
取得結果を関数の戻り値と値型で上位へ返す。
SDKのコールバックをなくすのではなく、SDKとの境界で処理を完結させる。

Arduinoがsetup/loopを呼び、FreeRTOSが登録したタスク入口を呼ぶ部分は、
実行基盤として必要な起動境界。自作ライブラリやドライバーからアプリへ処理を委譲する仕組みではない。
外部SDK自身のソースに、このプロジェクトのアプリへの依存を追加していない。

## 静的領域と並行動作

動作中にサイズが増える自作コンテナー・new/malloc・タスクの追加生成は使用しない。
自作のタスクとキューは `xTaskCreateStatic` / `xQueueCreateStatic` を使用する。

|領域|確保方法・容量|所有者|
|---|---|---|
|安全制御タスク|静的TCB＋4096バイトのスタック|通常版の初期化で1回だけ作成|
|天気取得タスク|静的TCB＋12288バイトのスタック|同上|
|天気キュー|静的管理領域＋WeatherSample 1要素|天気タスクが上書き、安全タスクが受信|
|HTTP応答・URL|WeatherClientの固定メンバー、2048 / 300バイト|天気タスクだけ|
|JSON解析領域|固定16384バイト、解析開始ごとに再利用|天気タスクだけ。非再入|
|USB受信|通常32バイト、デモ96バイト|Arduinoのloopだけ|
|安全・BENCH・デモ状態|起動前宣言した固定オブジェクト|それぞれの制御実行元だけ|
|要求・異常・ハートビート|固定atomic変数|loopと安全タスクが共有|
|Matter最新受信要求|固定atomicメールボックス|SDKが保存、loopが消費|

ESP-IDFのFreeRTOSはタスクスタックサイズをバイト単位で受け取るため、
配列要素数ではなく `sizeof(stack)` を渡す。自動モードのGPIO所有者は安全タスクだけ。
BENCHと別デモではloopだけがGPIOを更新する。両者は同時実行しない。

ローカルの小さな一時変数は固定サイズの自動変数とする。
再入性・並行性を損なうため、すべてのローカル変数を一律にstaticにはしない。
関数内の作業変数は原則として先頭に宣言・初期化し、寿命が明確なループ変数はfor内に宣言する。
JSONオブジェクトは領域をresetした後に構築し、関数を抜ける前に破棄する必要があるため例外。

### 完全なヒープ不使用ではない部分

Matter・Wi-Fi・BLE・TLS/HTTP・ArduinoのUSB送信・標準ライブラリの内部には動的確保がある。
HTTPクライアントのSDKハンドルもSDKが確保・解放する。
Matterのペアリング文字列はSDKのStringを使用する。これらまで静的化したとは扱わない。
自作JSON処理はArduinoJson 7.3.1のAllocator APIに固定領域を渡し、既定ヒープを使用しない。
領域不足はfalseとして扱い、前回の出力用サンプルを変更しない。
固定16KiB領域をJSON解析に追加したため、その分の静的RAM増加がある。

## コメントと整形

- 自作関数の宣言・定義直前に、関数名・内容・必要な入力/出力・注意事項を書く。
- 1行に複数の処理を書かず、条件分岐には波括弧を付ける。
- インデントは2スペース、目安100桁。clang-formatのプロジェクト設定で揃える。
- ハードウェアの判定と制御の判定を同じ関数に混ぜない。
- GPIO割当、コマンド、動作上限、ミスト許可設定、60秒待ち時間は今回変更しない。

## ビルドとソース取得時の注意

Arduino IDEで開くファイル名はこれまでどおり。
ただし共通ソースを参照するため、**スケッチフォルダだけでなく、リポジトリをフォルダごと取得**する。
`firmware/common` と各スケッチを同じ `firmware` の下に保持する。
追加の自作ライブラリをスケッチブックへインストールする作業は不要。
Arduino-ESP32 3.3.8のコンパイル設定が元スケッチのパスをinclude検索に含めることを前提とする。
別のビルド環境に移す場合は同じincludeパスを設定する。

## 確認方法

```sh
python3 scripts/check-architecture.py
bash scripts/test-demo.sh
ARDUINOJSON_INCLUDE=/path/to/ArduinoJson/src bash scripts/test-esp32s3.sh
```

依存チェックは自作ソースのローカルincludeの層逆転、GPIO/WDT APIの配置、
静的タスク・キューAPIの使用を確認する。C++の全呼出しグラフや外部SDK内部を証明するものではない。
ホストテストは安全停止・単発制御・天気検証の既存項目に加え、
固定JSON領域の整列・容量超過・2000回解析後の回収、解析中のmalloc/realloc不使用、受信行の境界を検証する。
ESP32用ビルドと実機での電気的・時間的な検証は別に記録する。

参考: [Arduinoのビルド手順](https://docs.arduino.cc/arduino-cli/sketch-build-process/)、
[Arduino-ESP32 3.3.8](https://github.com/espressif/arduino-esp32/tree/3.3.8)、
[ArduinoJson 7.3.1 Allocator](https://github.com/bblanchon/ArduinoJson/blob/v7.3.1/src/ArduinoJson/Memory/Allocator.hpp)。
