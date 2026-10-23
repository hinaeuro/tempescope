# 検証記録

最新の層分離・静的領域対応の検証は末尾の「2026-10-24」を参照。

実施日:2026-10-18。実機は接続していない。
元master:`eef729041b61eb9d43cc1621082516c81ce04cbc`。
作業ブランチ:`esp32s3/xiao-weather-matter`。

## 実施した検証

|項目|結果|
|---|---|
|添付回路PDFとrepo版のSHA-256照合|一致。ハード資料の対象を確認|
|元ファームとEAGLE pinref照合|成功。霧の物理18/19不一致、RGB G/B逆転を検出|
|ホスト安全制御テスト|成功。ASan/UBSan有効、LeakSanitizerは実行環境のptrace制約により無効|
|天気JSONパーサのホストテスト|成功。型／欠落／null／未知コード／不正JSON／未来／30分期限／未設定時計を拒否|
|ESP32-S3出力禁止設定の実コンパイル・リンク|成功（exit 0）。Matter、BLE commissioning、証明書bundle、HTTPSを含む|
|ESP32-S3出力有効設定の実コンパイル・リンク|成功（exit 0）。実機で有効化してよいことを意味しない|
|既存AVR／hardwareファイルへの変更|なし|
|git差分の空白チェック|成功|

使用環境:Linux x86_64、Arduino CLI 1.4.1、Espressif Arduino-ESP32 3.3.8、ArduinoJson 7.3.1。
FQBN:`esp32:esp32:XIAO_ESP32S3:PartitionScheme=max_app_8MB,PSRAM=opi`。
実際のESP32-S3 Xtensaコンパイラと同梱Matterライブラリでコンパイルし、モックだけの検証ではない。

|ビルド|プログラム領域|静的RAM|アプリ.bin SHA-256|
|---|---:|---:|---|
|初期設定（出力禁止）|2,104,445 bytes / 8,257,536|111,504 bytes / 327,680|`68864c592005afc69cb9b89fd0f2f47d18325d26944a393ad4e2821e613021cb`|
|出力有効設定|2,104,985 bytes / 8,257,536|111,504 bytes / 327,680|`1fe6e10a6434556a9e2dc577ff1626eddf4325302b0ec14147261df1eb75e1e4`|

再ビルドではビルド時刻・パス等によりバイナリのハッシュが異なる可能性がある。上記は今回生成したものの識別値。
初回の全SDKコンパイルではEspressif同梱HAL/TinyUSBヘッダにmissing-field-initializers警告が出た。プロジェクト自身のエラーは解消済み。最終増分ビルドログはエラー／警告なし。
フラッシュ使用率と静的RAMの表示は起動後のMatter/BLE/TLSの動的メモリ余裕を保証しない。

再現コマンド:

```sh
python3 scripts/audit-original-schematic.py
bash scripts/test-esp32s3.sh
bash scripts/build-esp32s3.sh
BUILD_OUTPUT_DIR=/tmp/tempescope-esp32s3-enabled bash scripts/build-esp32s3.sh \
  --build-property compiler.cpp.extra_flags=-DTEMPESCOPE_HARDWARE_VALIDATED=1
```

安全制御テストには、7種の停止条件、低水位ラッチ、OFF中のみの解除、2秒安定、水位の揺れ、30分上限、millisの32bit周回、30分間の全ポンプ周期、ミストとの排他、天気コード対応を含めた。

## 確認できていないこと

- 現物のU$1型番・12V出力・R3実装値。資料の不整合は検出したが、実基板の修正履歴は不明。
- RGB LEDの正確なメーカー資料と実ピン配列、Vf、電流・抵抗値の最終選定。
- ポンプ／ファン／ミストの銘板と定常・突入・拘束電流、電源容量、FET放熱とSOA、リレー接点の負荷適合。
- 起動パルス、停止応答の実測、水位接点の機械的信頼性、電源／USB逆流、ブラウンアウト。
- 実ネットワークでのDNS、SNTP、Open-MeteoへのTLS接続・取得、長時間再接続。テストはスキーマ処理とビルドまで。
- MatterのBLE登録、Alexa/Echoへの追加、音声応答、未認証機器の受入条件、実際の状態反映。
- HTTPSとBLE commissioningが重なるときのヒープ余裕、Task WDTの実応答、長期運転、結露、カビ／コケ、漏水。

したがって「ビルド可能な移植実装・配線設計・試験手順」の段階であり、「実機で動作確認済みの完成機」ではない。
実機投入の順序と合格条件は [commissioning.md](commissioning.md) を参照する。

## 2026-10-22 実装回路対応

- image(3).pngの配線を目視照合し、ポンプD0、ミストD1、ファンD2、RGB D10/D9/D8に更新。
- USBシリアルのベンチモードを追加。試験時の水位/停止入力の偽装はしない。
- 新規ベンチテスト・従来安全制御・天気JSONテストをC++17、ASan/UBSan（detect_leaks=0）で実行し合格。
  出力期限、追加指令拒否、液体負荷の許可/共通60秒間隔、off、時計周回、ドライバ異常時停止を検証。
- Arduino CLIは取得できたが、ESP32コア/ライブラリのインデックス取得がDNS/network unreachableで失敗。
  この変更後のESP32-S3向けフルビルドは未完了。以前のビルド成功を今回の変更の検証とは扱わない。
- 実機書込み・RGBの実際の色・起動ストラップ・WDT・電流/温度/流量は未検証。


## 2026-10-24 層分離・関数コメント・静的領域対応

対象: ESP32S3通常/BENCH版と別USBデモ。旧AVR/iOSのコードは変更していない。
構成とSDK依存部分の扱いは[構成資料](architecture-ja.md)を参照。

- アプリ、共通ドライバー、ハードウェアに依存しないライブラリを分離。
- Matter受信はドライバー内のatomicメールボックスへ保存し、アプリからポーリング。
- GPIO/PWM/WDTを共通化し、コマンドと時間制限・ミスト許可設定は維持。
- 自作タスクのTCB/スタック、1要素キュー、HTTP応答/URL/受信行を固定領域へ配置。
- ArduinoJsonの確保先を固定16KiB領域へ変更。SDK内部の動的確保は残る。
- 各自作関数に関数名・内容・必要な入出力と注意事項を追加し、整形方針を統一。

ホストテスト: C++17、ASan/UBSan有効（detect_leaks=0）。
安全判定、BENCH、デモ、JSON解析の回帰テストはすべて成功。
固定JSON領域の整列・容量超過・次回復帰・2000回反復とmalloc/realloc不使用、
受信行の境界を追加検証。依存層と静的タスク/キューAPIの配置チェックも成功。

実コンパイル環境: Linux x86_64、Arduino CLI 1.5.2-rc.1、Arduino-ESP32 3.3.8、
ArduinoJson 7.3.1、Xtensa GCC 14.2.0。
FQBN: `esp32:esp32:XIAO_ESP32S3:PartitionScheme=max_app_8MB,PSRAM=opi,CDCOnBoot=cdc`。
自動モードの2設定は、一時LocalConfig.hでBENCH_MODE=0、HARDWARE_VALIDATED=0/1を指定した。
検証後に一時ファイルを削除し、リポジトリの既定BENCH設定を維持する。

|構成|結果|プログラム領域|静的RAM|
|---|---|---:|---:|
|通常ファームの既定BENCH|成功|1,863,069 bytes|102,472 bytes|
|自動モード・出力禁止|成功|2,126,765 bytes|147,424 bytes|
|自動モード・出力有効|成功|2,127,225 bytes|147,424 bytes|
|別USBデモ|成功|306,268 bytes|22,284 bytes|

外部SDKのHAL/TinyUSBヘッダーにmissing-field-initializers警告がある。
自作ソースに由来する警告・エラーは確認されなかった。
メモリ使用量はリンク時の値で、Matter/BLE/HTTPSの実行時ヒープ余裕や
各タスクのスタック高水位を保証するものではない。

実機の書き込み・GPIO波形・水位/停止・USB抜去・停止時間・電流/温度・
Alexa登録と天気APIの実通信は今回も未検証。実機確認手順に従って段階的に検証する。
