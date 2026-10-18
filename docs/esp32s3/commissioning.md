# ビルド・実機確認手順

## ビルド

Arduino CLIを導入し、リポジトリルートで実行する。3.3.8に固定し、更新時は再評価する。

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.8 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install ArduinoJson@7.3.1
bash scripts/test-esp32s3.sh
bash scripts/build-esp32s3.sh
```

Arduino IDEの場合:ボードXIAO_ESP32S3、PSRAM=OPI、USB CDC On Boot=Enabled、Partition Scheme=Maximum APP (7.9MB APP No OTA/No FS)。`firmware/xiao_s3/xiao_s3.ino` を開く。
ArduinoJson 7.3.1をインストール。Wi-Fi資格情報はファイルへ書かず、Matter初期登録で渡す。
このパーティションにOTA更新機能はない。アプリの実体は約2.1MBだが、同梱のパーティション構成で書き込む。

最初は全負荷を外した状態でUSBから書き込む。

```sh
arduino-cli board list
arduino-cli upload -b esp32:esp32:XIAO_ESP32S3:PartitionScheme=max_app_8MB,PSRAM=opi \
  -p /dev/ttyACM0 --input-dir /tmp/tempescope-esp32s3
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=115200
```

ポート名は実際の環境に合わせる。初回／パーティション変更時のみ全消去を行うと、過去のMatter fabricやWi-Fi設定の影響を除ける。通常更新で毎回消去すると再登録が必要。
起動ログにDRY RUNが出ることと、onを送っても出力しないことを確認する。

## 電気・機構チェック（電源OFFから）

1. 原図の霧D12/D13、RGB G/B逆転、抵抗値、U$1型番不整合を実物で解消する。AVRを外し、6系統の制御先の導通を記録する。LEDは実物の共通アノードと色ごとの端子を確認。
2. 無負荷で24V、ポンプ用12V、既存5V、新規XIAO用5Vを測定。U$1が12Vを出す根拠が得られない場合は先へ進めず、必要なら12V buckに交換する。R1変更での過電圧調整は行わない。
3. Qp/Qmとダイオード、TD9/10、ゲートプルダウン、各TD入力プルダウンを確認。ポンプD1はカソード=12V、アノード=PUMP_GND。リレー追加ダイオードはカソード=5V、アノード=コイル1。
4. 水位接点は水が十分ある時だけ閉じる。水抜き、コネクタ抜き、電線断線でHIGHとなることを確認。GPIO43/44の1k直列を省略しない。水位は揺れ、循環中の低下、最低浸漬深さに余裕を持たせる。
5. 停止NC入力は開くとHIGH。電源を直接切る別極／スイッチを用意する。GPIO入力だけの停止は、FET短絡やリレー溶着を止められない。
6. XIAOのみ通電し、3.3Vと5V、USB／外部電源の逆流を確認。Wi-Fi送信時の3.3V低下と発熱を観測。5V系にはXIAOの3.3V出力を流用しない。

記録が揃った後、`LocalConfig.example.h` を同じフォルダの `LocalConfig.h` にコピーして座標を設定し、`TEMPESCOPE_HARDWARE_VALIDATED=1` にする。このファイルはgit管理しない。
電源を切って変更し、再ビルド・書込み。以下は最初に負荷の代わりにテスター／ロジアナ／試験LEDで確認する。

## 段階的な通電試験

|試験|操作|期待結果・合格条件|
|---|---|---|
|リセット／起動／書込み|電源再投入、RESET、BOOT書込みを繰返す|6出力に負荷を動かすパルスが出ない|
|水位待ち|安全水位接点を閉じる|2秒安定するまで動かない|
|NC停止|ON中にNC停止線を開く|GPIOは次の10ms周期を目安にOFF。実際の停止時間を測定し、リレー・水流の残留も記録|
|空運転防止|水位接点を片方ずつ開く／配線を抜く|全出力OFF、fault=2。復帰だけでは再開しない|
|故障解除|OFF、水位2秒安定、停止ループ閉→reset|fault=0、新たなONまでOFF。ON中にresetだけでは解除しない|
|Wi-Fi切断|AP停止／圏外|切断がOSへ通知された後にOFF。復帰後は自動再開しない。AP障害検出時間も実測|
|API停止|DNS不通・HTTPS失敗・応答異常|再試行は60秒間隔。古いデータが観測時刻から30分に達したらOFF。TLS無効化しない|
|主loop停止|デバッグでloopを止め、安全タスクは動作させる|約1秒で監視異常fault=4、全OFF|
|安全タスク停止|デバッグで安全タスクだけ停止|Task WDTで約2秒後リセット。再起動はOFF|
|運転上限|有効データを更新しつつ30分ON|30分でfault=3、全OFF。HTTP更新で上限が延びない|
|ポンプ波形|雨の天気時、出力を観測|10秒ON/60秒周期、ミスト同時ONなし|
|ミスト波形|曇り時、出力を観測|20秒ON/60秒周期。ファンON、ポンプOFF|

fault番号:0=なし、1=停止、2=水不足、3=連続セッション上限、4=主loop監視異常。
Serialコマンドは改行付き `on`、`off`、`reset`、`status`、`decommission`。
天気取得前、通信切断中、無効設定ではONを維持しない。Matterは要求ON/OFFを報告し、水位待ちの間は負荷がまだOFFの場合がある。

次にRGBのみを接続して単色を確認し、必要ならG/B配線を訂正する。現行の天気色だけでは純色の同定は難しいため、デバッガで出力値を固定するか、一時的な単色試験コードを別ビルドして確認する。
RGBのVf／許容電流が不明な状態で抵抗を小さくしない。
ファン、リレーコイル、最後に十分な水を入れたポンプ／ミストを個別に接続し、定常・始動電流、電源降下、温度、逆起電力を測る。
ポンプは拘束して破損させる測定をせず、メーカーの拘束電流値または電流制限下の許容された試験を使う。
リレーの24V負荷は容量性突入がDC定格内か確認。接点溶着やミスト側からの回り込みがないことも検査する。

## Alexaへの登録

1. Matter対応のEcho／Alexa側Matterコントローラと、最新Alexaアプリを用意する。特定の所有Echoの対応は未確認。XIAOはThreadボーダールータを必要としない。
2. スマートフォンのBluetoothを有効にし、XIAOと同じ運用先2.4GHz Wi-Fiへ登録できる状態にする。AP隔離を避け、IPv6とmDNSのローカル通信を許可する。
3. Serialに10秒ごとに表示される `Pairing` のコードを使い、Alexaアプリでデバイス追加→Matterデバイスの登録へ進む。UI名はアプリ版で異なる。QR URLを使う場合もコードを第三者へ公開しない。
4. 機器名を「テンプスコープ」にする。「アレクサ、テンプスコープをつけて」「アレクサ、テンプスコープを消して」を試す。ONは天気自動再現、OFFは全出力停止。
5. 再起動、AP切断・復帰、低水位後に、Alexa表示がOFFへ戻ることと新規ONの必要性を確認。
6. 登録解除はまずOFFにし、USB Serialから `decommission`。fabricとネットワーク設定が消えるので再登録する。

開発用Matter実装のため、Alexaの未認証デバイスの警告・受入条件は実アカウント／コントローラで確認する。量産向け認証済み製品としての登録保証はしていない。
Alexa連携に独自Alexaスキル、AWS Lambda、クラウドAPIキーは不要。

## 未検証・長時間試験

実機、水位スイッチ、Echoはこの環境にない。登録成功、流量、結露、電源過渡、BLE＋TLSピーク時のRAM、実停止時間、接点寿命は未検証。
30分セッションを繰返す8時間、24時間、数日という順で監視付き試験を行い、水減り・戻り時間・漏れ・温度・コケ／カビを記録する。
常時無制限運転へ変更する場合は本実装の運転上限を単に削除せず、タンク容量・漏水検出・必要な独立遮断を設計し直す。
