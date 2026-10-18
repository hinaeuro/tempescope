# 検証記録

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
