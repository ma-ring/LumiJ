# LumiJ システム仕様書（UART通信 & UI設計）

## 概要

本システムは3つのデバイスで構成される。

- Stamp1：4x4キーマトリクス入力 + KeyButton専用
- Dial：UI・状態管理・制御・BPM選択
- Stamp2：LED出力（K-595D12W）専用

---

## 全体フロー

1. Stamp1 がキー押下を Dial に通知
2. Dial が押されたキーIDを編集対象として選択
3. Dial のダイアル操作で beat を変更
4. Dial のタッチ操作で wave を変更
5. 値が変わるたびに Dial が Stamp2 に送信
6. Stamp2 が設定を保持し、LEDを発光

---

## ID対応

キーとLEDは位置ベースで1対1対応する。

```
0  1  2  3
4  5  6  7
8  9 10 11
12 13 14 15
```

---

## UI仕様

### 編集開始
- キー押下で即編集モードに入る
- 常に最後に押されたキーが対象

### 操作
- ダイアル：beat変更
- タッチ：wave変更

### 更新タイミング
- 値変更のたびに即送信

---

## パラメータ仕様

### beat
1小節内の発光回数

```
beat ∈ {1, 2, 4, 8, 16}
```

### wave
発光パターン

```
wave ∈ {SQUARE, TRIANGLE, SINE, SAW}
```

### BPM
テンポ設定

```
BPM ∈ {40, 41, ..., 240}
```

- BPM選択モードでタップ間隔から自動計算
- 直近3回のタップ間隔の平均を使用

---

## 各デバイス責務

### Stamp1
- キースキャン
- 押下イベント送信
- KeyButton検出
- BPMモード切替要求

### Dial
- 編集対象管理
- UI操作
- パラメータ変更
- BPMモード管理
- タップ間隔検出とBPM計算
- Stamp2への送信

### Stamp2
- LED制御（K-595D12Wシフトレジスタ使用）
- パラメータ保持
- 発光処理
- BPM値によるテンポ制御

---

## 通信仕様

### 共通ルール

- UART通信
- 1行1コマンド
- 改行区切り（\n）
- 文字列ベース
- ボーレート：115200

---

### Stamp1 → Dial

```
KEY_DOWN:<id>
KEY_BUTTON:PRESSED
```

例:

```
KEY_DOWN:3
KEY_BUTTON:PRESSED
```

---

### Dial → Stamp2

#### beat設定

```
SET_BEAT:<id>,<value>
```

例:

```
SET_BEAT:3,8
```

#### wave設定

```
SET_WAVE:<id>,<type>
```

例:

```
SET_WAVE:3,SINE
```

#### BPM設定

```
SET_BPM:<value>
```

例:

```
SET_BPM:120
```

---

## Dial内部状態

```
selected_id : 0〜15
led_params[16]
bpm_mode : bool
current_bpm : int
tap_times[3] : unsigned long
tap_count : int
```

構造体イメージ:

```c
struct LedParam {
    int beat;
    WaveType wave;
};

// BPM関連状態
bool bpm_mode;           // BPM選択モードフラグ
int current_bpm;         // 現在のBPM値
unsigned long tap_times[3]; // 直近3回のタップ時間
int tap_count;           // タップ回数
```

---

## Stamp2内部状態

```
led_params[16]
global_bpm : int
```

- 各IDの設定を保持
- 一定周期で発光更新
- BPM値による全体テンポ制御

---

## 初期値

```
beat = 4
wave = SQUARE
bpm = 120
bpm_mode = false
```

---

## 動作例

### 通常編集モード

```
Stamp1 → Dial : KEY_DOWN:3
Dial : selected_id = 3

Dial : beat変更（8）
Dial → Stamp2 : SET_BEAT:3,8

Dial : wave変更（SINE）
Dial → Stamp2 : SET_WAVE:3,SINE
```

### BPM選択モード

```
Stamp1 → Dial : KEY_BUTTON:PRESSED
Dial : bpm_mode = true

Dial : タップ検出（3回）
Dial : BPM計算（120）
Dial → Stamp2 : SET_BPM:120

Stamp1 → Dial : KEY_BUTTON:PRESSED
Dial : bpm_mode = false
```

---

## エラー処理

- 不正IDは無視
- 不正コマンドは無視
- ACK応答は初期実装では省略

---

## 実装手順

1. Stamp1：キー入力取得
2. Stamp2：LED制御（K-595D12W使用）
3. Dial：UIとUART送信
4. 全体統合

---

## 配線仕様

### Stamp1 (4x4キーマトリクス + KeyButton)
```
行ピン (出力):
- Row 0: GPIO3
- Row 1: GPIO4  
- Row 2: GPIO5
- Row 3: GPIO6

列ピン (入力・プルアップ):
- Col 0: GPIO7
- Col 1: GPIO8
- Col 2: GPIO9
- Col 3: GPIO10

KeyButton (独立ボタン):
- GPIO11 (入力・プルアップ)

UART接続:
- RX: GPIO1 (DialのTXに接続)
- TX: GPIO2 (DialのRXに接続)
```

### Dial (M5Dial)
```
UART接続:
Stamp1との通信:
- RX: GPIO13 (Stamp1のTXに接続)
- TX: GPIO15 (Stamp1のRXに接続)

Stamp2との通信:
- RX2: GPIO2 (Stamp2のTXに接続)
- TX2: GPIO1 (Stamp2のRXに接続)

内蔵コントローラ:
- エンコーダー: 内蔵
- タッチボタン: 内蔵
- LCDディスプレイ: 内蔵
```

### Stamp2 (LED制御 with K-595D12W)
```
K-595D12W シフトレジスタベースキット接続:
- DIO (Data Input): GPIO11
- RCLK (Register Clock): GPIO12
- SCLK (Shift Clock): GPIO13
- VCC: 3.3V or 5V
- GND: GND

UART接続:
- RX: GPIO1 (DialのTX2に接続)
- TX: GPIO2 (DialのRX2に接続)

LED接続:
K-595D12Wの出力ピンQ0-Q15を各LEDに接続（16個のLED）
注: K-595D12Wは74HC595を2個直列接続で16ビット出力に対応
```

---

## 発光アルゴリズム詳細

### タイミング計算
- 1小節 = 2秒
- beat間隔 = 2000ms / beat値
- 最小beat値で全体のタイミングを同期

### 波形生成

#### SQUARE
```c
return beatPosition < (beat / 2);
```

#### TRIANGLE
```c
float phase = (float)beatPosition / beat;
return phase < 0.5 ? (phase * 2) : (2 - phase * 2) > 0.5;
```

#### SINE
```c
float phase = (float)beatPosition / beat;
return sin(phase * 2 * PI) > 0;
```

#### SAW
```c
float phase = (float)beatPosition / beat;
return phase < 0.5;
```

---

## ファイル構成

```
LumiJ/
├── SPECIFICATION.md      # 本仕様書
├── README.md             # セットアップ手順
├── Stamp1/
│   └── Stamp1.ino        # キーマトリクス入力
├── Dial/
│   └── Dial.ino          # UI・制御
└── Stamp2/
    └── Stamp2.ino        # LED制御
```

---

## 拡張案

### 短期的拡張
- EEPROMによる設定保存
- シリアルモニタでのデバッグ表示強化
- 新しいwaveパターン追加

### 長期的拡張
- MIDI同期機能
- PCからの制御インターフェース
- 複数デバイス連携
- Web UIでの制御
