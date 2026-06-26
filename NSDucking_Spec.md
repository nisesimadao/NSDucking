# NSDucking 仕様書

## 1. 概要

**NSDucking** は、MIDI入力でトリガーできるシンプルな無料ダッキングプラグインである。

目的は、複雑なサイドチェインコンプレッサーではなく、KickStart2 系のように **カーブ・Depth・Length を中心に直感的に音量ダッキングを作れるプラグイン** を作ること。

ただし、既存製品のUIを完全にコピーするのではなく、操作思想だけを参考にし、NSDucking独自の見た目・構造・名前で実装する。

---

## 2. コンセプト

NSDuckingは、以下を重視する。

- MIDIノートでダッキングカーブを再生できる
- UIは1画面で完結する
- 難しいコンプレッサー設定を使わない
- カーブを見ながら音量変化を調整できる
- 無料プラグインとして軽く、扱いやすく、導入しやすい
- DAW上で小さく表示しても操作できる
- 将来的にカーブ編集・プリセット管理・Host Syncにも拡張できる

---

## 3. 対応形式

MVPでは以下を目標にする。

- Windows: VST3
- macOS: VST3 / AU
- フレームワーク: JUCE
- 言語: C++17 以上
- ビルド方式: CMake推奨

---

## 4. 基本機能

### 4.1 MIDIトリガー

MIDI Note On を受け取ると、ダッキングエンベロープを先頭から再生する。

基本動作:

1. MIDI Note On を受信
2. 現在のカーブ再生位置を 0 に戻す
3. 指定された Length に従ってカーブを再生
4. カーブ値と Depth に応じて音量を下げる
5. カーブ終了後、音量を元に戻す

Note Off はMVPでは無視する。

---

### 4.2 音量ダッキング

内部処理はシンプルなゲイン制御とする。

概念:

```txt
output = input * gain
```

gain はカーブとDepthから決定する。

```txt
gain = 1.0 - curveValue * depth
```

- `curveValue = 0.0` のとき、音量は下がらない
- `curveValue = 1.0` のとき、Depthぶん音量が下がる
- `depth = 1.0` のとき、最大で無音付近まで下げられる
- `depth = 0.5` のとき、最大で半分程度まで下げる

---

### 4.3 カーブ

MVPでは、手描きカーブ編集は必須ではない。

最初はプリセットカーブを用意し、ユーザーはプリセットを選ぶだけでも使えるようにする。

将来的には、カーブ上のポイント編集、ドラッグ、スナップ、カーブ補間を追加する。

---

## 5. パラメータ

### 5.1 必須パラメータ

| パラメータ | 範囲 | 説明 |
|---|---:|---|
| Depth | 0%〜100% | ダッキングの強さ |
| Length | 1/64〜1 bar | カーブ全体の長さ |
| Smooth | 0ms〜20ms | クリックノイズ防止用のスムージング |
| Curve / Preset | enum | ダッキングカーブの種類 |
| Output Gain | -12dB〜+12dB | 出力音量補正 |

---

### 5.2 将来的な追加パラメータ

| パラメータ | 説明 |
|---|---|
| Trigger Mode | MIDI / Host Sync / Audio Sidechain |
| Velocity Sensitivity | MIDIベロシティでDepthを変化させる |
| Retrigger Mode | 再トリガー時の挙動 |
| Note Filter | 特定のMIDIノートだけに反応する |
| Mix | Dry/Wet 的にダッキング量を混ぜる |
| Curve Tension | カーブのなめらかさを調整する |

---

## 6. UI設計

### 6.1 基本方針

UIは、SVGデザインの雰囲気を参考にしつつ、仕様書上では細かい絶対座標を指定しない。

実装者には、以下のような大枠の構造だけを伝える。

```txt
┌────────────────────────────────────┐
│ Header                             │
├───────────────┬────────────────────┤
│ Controls      │ Curve Editor        │
├───────────────┴────────────────────┤
│ Preset Grid / Bottom Controls       │
└────────────────────────────────────┘
```

この構造を維持したまま、プラグインウィンドウのサイズに応じて各領域をレスポンシブに再配置する。

---

### 6.2 レスポンシブレイアウト

NSDuckingは固定座標UIにしない。

基準デザインサイズは存在してよいが、実装では現在のウィンドウサイズから各領域を再計算する。

基本構造:

- 上部: Header
- 中央: Controls + Curve Editor
- 下部: Preset Grid / Bottom Controls

横方向:

- 左側に Controls
- 右側に Curve Editor
- Controlsは狭すぎないように最小幅を持つ
- Curve Editorは可能な限り広く取る

縦方向:

- Headerは常に上部に固定
- Main領域には Controls と Curve Editor を置く
- Bottom領域にはプリセットや補助操作を置く

---

### 6.3 Header

Headerには以下を配置する。

- 左側: `NSDucking` ロゴ
- 右側: `Main / MIDI / About` タブ
- 必要に応じて MIDI入力インジケーター

Headerは高くしすぎない。  
視認性を保ちながら、メインのカーブ表示を邪魔しない高さにする。

画面幅が狭い場合は、タブ表示を短縮してよい。

例:

```txt
Main / MIDI / About
↓
Main / MIDI
↓
M / I
```

優先度は以下とする。

```txt
Main > MIDI > About
```

---

### 6.4 Controls

Controlsには、主要操作ノブを配置する。

推奨構成:

- 大きいノブ: Depth
- 小さいノブ: Smooth

理由:

- ダッキングプラグインで最も重要なのはDepth
- Smoothはクリックノイズ防止の補助設定として扱いやすい
- 元SVGにある `Volume` は、NSDuckingでは `Depth` に置き換える方が分かりやすい
- 元SVGにある `Low` は、NSDuckingでは `Smooth` または `Tone` として再解釈する

MVPでは以下を採用する。

```txt
Small knob: Smooth
Large knob: Depth
```

Controls領域が狭い場合でも、Depthは必ず表示する。  
Smoothは小型表示、スライダー化、省略のいずれかを許容する。

---

### 6.5 Curve Editor

Curve EditorはNSDuckingの中心的なUIである。

表示する内容:

- ダッキングカーブ
- 背景グリッド
- 現在の再生位置
- トリガー反応
- 選択中プリセットの形状

カーブの意味:

```txt
上 = 強くダックする
下 = ダックしない
左 = カーブ開始
右 = カーブ終了
```

カーブは、単なる線だけでなく、半透明の塗りやグロー表現を使って視認性を高める。

---

### 6.6 Curve Editorのグリッド

グリッドは必須とする。

基本:

```txt
横方向: 8分割
縦方向: 4分割
```

ただし、画面サイズによって変えてよい。

- 小型UI: 横4分割 / 縦4分割
- 通常UI: 横8分割 / 縦4分割
- 大型UI: 横16分割 / 縦4分割

グリッドは主張しすぎず、カーブを見やすくするための補助として薄く描画する。

---

### 6.7 Bottom Area

Bottom Areaには、プリセットグリッドと補助操作を配置する。

MVPでは、プリセットグリッドを中心にする。

表示例:

```txt
Classic | Tight | Pump | Soft
Hard    | Long  | Side | Gate
```

将来的には、Bottom Areaに以下も配置する。

- Length selector
- Trigger mode
- Output gain
- Sync toggle
- Preset save / load

---

### 6.8 Preset Grid

プリセットグリッドはレスポンシブに列数を変える。

目安:

- 小型: 3列
- 通常: 4列
- 大型: 6列

プリセットボタンは、押しやすさと視認性を優先する。  
小さすぎるボタンは避ける。

MVPで用意するプリセット:

| 名前 | 用途 |
|---|---|
| Classic | 標準的なポンピング |
| Tight | ベース用の短めダック |
| Pump | 強めのEDM系 |
| Soft | 自然なサイドチェイン風 |
| Hard | ほぼゲートに近い強いダック |
| Long | ゆっくり戻るカーブ |
| Side | 汎用サイドチェイン |
| Gate | 急激に切るカーブ |

---

## 7. UIページ

NSDuckingには、上部タブで切り替えるページを用意する。

### 7.1 Main

通常操作画面。

表示内容:

- Depth
- Smooth
- Curve Editor
- Preset Grid
- Length
- Trigger Mode
- Output Gain

MVPではMainページだけ実装してもよい。

---

### 7.2 MIDI

MIDI設定画面。

表示内容:

- MIDI input activity
- Trigger note
- All notes mode
- Velocity sensitivity
- Retrigger mode
- Note filter

MVPでは、MIDIページはプレースホルダーでもよい。

---

### 7.3 About

情報画面。

表示内容:

- NSDucking
- Version
- Author / Credits
- License
- Website / GitHub

MVPでは、Aboutページは簡易表示でよい。

---

## 8. DSP仕様

### 8.1 処理単位

処理は `processBlock` 内で行う。

やること:

1. MIDIイベントを確認
2. Note On があればエンベロープをトリガー
3. 各サンプルごとに現在のgainを計算
4. 全チャンネルに同じgainを適用
5. Smoothで急激な変化を抑える

---

### 8.2 サンプル精度

MVPではブロック単位のトリガーでもよい。

ただし、実用性を上げるため、v1ではMIDIイベントのsample positionを使い、ブロック内の正しい位置でトリガーする。

---

### 8.3 スムージング

ゲイン変化によるクリックノイズを防ぐため、必ずスムージングを入れる。

Smoothはユーザーが調整できる。

目安:

- 0ms: 最も鋭いがクリックが出る可能性あり
- 1〜3ms: 標準
- 5〜20ms: なめらかで自然

---

## 9. トリガーモード

### 9.1 MIDI Trigger

MVPの基本モード。

- MIDI Note Onでカーブを再生
- Note Offは無視
- すべてのノートに反応する
- 将来的にはノート指定に対応

---

### 9.2 Host Sync

将来的に追加する。

DAWの再生位置とテンポに同期して、自動的に一定間隔でカーブを再生する。

例:

- 1/4
- 1/8
- 1/16
- 1 bar

---

### 9.3 Audio Sidechain

後回しでよい。

外部サイドチェイン入力の音量がしきい値を超えたときにカーブをトリガーする。

無料版MVPでは必須ではない。

---

## 10. プリセットカーブ仕様

MVPでは、各プリセットは固定カーブとして持つ。

### Classic

標準的な4つ打ち用。  
最初に強く下げて、なめらかに戻る。

### Tight

短く鋭いダック。  
ベースやキックのアタックを避けたいときに使う。

### Pump

大きくうねるEDM向け。  
Depthを高めにした時に派手に動く。

### Soft

自然なサイドチェイン風。  
ミックス内で目立ちすぎない。

### Hard

急激に下げて急激に戻る。  
ゲート的な効果。

### Long

ゆっくり戻る。  
パッドや長いベース向け。

### Side

汎用的な中間カーブ。

### Gate

ほぼミュートに近い強いダック。  
リズムゲート的に使える。

---

## 11. 状態保存

プラグインは以下をDAWプロジェクト内に保存する。

- Depth
- Length
- Smooth
- Output Gain
- Selected Preset
- Trigger Mode
- MIDI設定
- UIタブ状態
- 将来的なカスタムカーブ

---

## 12. MIDIルーティング互換性

DAWによって、オーディオエフェクトへのMIDI入力の扱いが異なる。

そのため、必要に応じて以下の2種類のビルドを検討する。

### Audio Effect版

通常のオーディオエフェクトとして動作する。  
MIDI入力を受け取れるDAWではこの形式で使える。

### Instrument版

MIDI入力を受け取りやすくするための互換版。  
音源トラックとして挿し、外部入力音声を処理する設計が必要になる場合がある。

MVPではAudio Effect版を優先する。  
ただし、配布時にはDAW別の使い方説明を用意する。

---

## 13. デザイン方針

### 13.1 見た目

NSDuckingの見た目は、以下を目指す。

- 暗めの背景
- グレー系のパネル
- 視認性の高いカーブ
- 立体感のあるノブ
- 小さくても読める文字
- 派手すぎないが、音楽プラグインらしい質感

---

### 13.2 カラー

推奨カラーの方向性:

- 背景: ダークグレー / 黒寄り
- パネル: 少し明るいグレー
- メインアクセント: 青〜水色
- サブアクセント: 赤〜オレンジ
- テキスト: 白 / 薄いグレー
- 非アクティブ: 暗いグレー

---

### 13.3 コピー回避

KickStart2風の操作感は参考にしてよいが、以下は避ける。

- 既存製品の画面をそのまま再現する
- ロゴやアイコンを似せすぎる
- 独自性のない完全クローンにする
- 既存製品名をUI内に出す

NSDuckingは、あくまで独自UIの無料ダッキングプラグインとして設計する。

---

## 14. MVP範囲

最初の完成目標は以下。

- VST3として読み込める
- オーディオ入力を処理できる
- MIDI Note Onでダッキングする
- Depthを調整できる
- Lengthを調整できる
- Smoothを調整できる
- プリセットカーブを選べる
- レスポンシブUIで表示できる
- Curve Editorにカーブとグリッドを表示できる
- DAWプロジェクトに状態保存できる

---

## 15. MVPでやらないこと

以下は後回し。

- 手描きカーブ編集
- Audio Sidechain Trigger
- 複雑なプリセットブラウザ
- クラウドプリセット
- A/B比較
- Undo/Redo
- スキン変更
- 3D的な重いUI表現
- 過剰なアニメーション

---

## 16. 開発フェーズ

### Phase 1: DSP最小実装

- JUCEプロジェクト作成
- VST3出力
- MIDI Note On検出
- 固定カーブでゲイン制御
- Depth / Length / Smooth実装

---

### Phase 2: UI実装

- Header
- Controls
- Curve Editor
- Preset Grid
- レスポンシブレイアウト
- MIDI入力インジケーター

---

### Phase 3: 実用化

- sample-accurate MIDI trigger
- パラメータ保存
- プリセット切替
- Output Gain
- DAW別動作確認

---

### Phase 4: 拡張

- Host Sync
- MIDIページ
- Aboutページ
- カーブ編集
- カスタムプリセット保存
- Audio Sidechain

---

## 17. 実装時の注意

- レイアウトは絶対座標で固定しない
- SVGの座標をそのまま写すのではなく、領域構造だけを参考にする
- 画面サイズ変更時に必ず各領域を再計算する
- DSP処理とUI描画を分離する
- Audio threadで重い処理やロックを行わない
- UIアニメーションは軽量にする
- MIDIが来ているか分かる表示を必ず入れる
- 小さい画面でもDepthとCurveは見えるようにする

---

## 18. 完成イメージ

NSDuckingは、以下のようなプラグインを目指す。

```txt
┌────────────────────────────────────┐
│ NSDucking                 Main MIDI │
├───────────────┬────────────────────┤
│ Smooth        │  Curve Editor       │
│        Depth  │  Grid + Duck Curve  │
├───────────────┴────────────────────┤
│ Classic Tight Pump Soft Hard Long   │
│ Length  Trigger  Output             │
└────────────────────────────────────┘
```

ユーザーは、ベースやパッドのトラックにNSDuckingを挿し、MIDIノートを送るだけで、テンポに合ったダッキングを直感的に作れる。

---

## 19. 一文まとめ

**NSDuckingは、MIDIノートでカーブを再生し、音量をなめらかに下げて戻す、レスポンシブUIの無料ダッキングプラグインである。**
