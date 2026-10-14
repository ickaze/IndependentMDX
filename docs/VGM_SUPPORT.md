# VGM / VGZ support — IndependentMDX 0.25

## 対応範囲

- VGM 1.00～1.71のうち、YM2151を1基だけ使用するデータ。
- `.vgm` と `.vgz`。拡張子に関わらずgzipヘッダーを判定します。VGZは単一gzipメンバー、展開後128 MiBまでです。stored / fixed / dynamic DEFLATEとCRC32検査に対応します。
- YM2151書き込み `54 aa dd`、待機 `61 nn nn` / `62` / `63` / `70`～`7f`、待機長変更 `64`、終了 `66`。`67`のデータブロックは境界を検査して読み飛ばします。音源へのPCMストリーム転送は行いません。
- 待機時間は44,100分の1秒単位です。ヘッダーの総サンプル数を信用せず、命令を走査して曲長・ループ開始位置を計算します。
- GD3の英語曲名（空なら日本語曲名）をUTF-16LEから読み込みます。曲名は既存の非同期セレクター処理で後から表示します。
- ヘッダー指定のクロックと音量補正をIndependent OPM DLLに反映します。対応クロックは100,000～10,000,000 Hzです。
- 通常再生、一時停止、シーク、再生位置表示、有限／無限ループ、再生速度、キー、FM音量、チャンネル消音、チャンネル波形、WAV保存。

ループ指定がある曲は指定位置へ戻ります。無限ループ設定の変更はフェード開始前まで受け付けます。ループ指定のない曲は、通常は終了命令後に0.5秒のリリース時間を設けて終了します。無限ループ時は全チャンネルが静かになってから先頭へ戻ります。

シークは先頭からレジスタを再実行して音源状態を再構築します。長い曲の後方へのシークには時間がかかる場合があります。シーク計算中のレジスタは外部音源へ送らず、再開時に現在状態を送ります。

## 出力と制限

音声・波形は既存のIndependent OPM 0.14 DLL APIを使用します。VGMのレジスタ書き込みは時刻指定APIで内部サンプルの途中にも反映します（4 MHzでは1クロック=0.25μs）。OPMの実装ソースや表示専用の改変音源は追加していません。

C86CTLとX68Soundの接続ではクロックを変更できないため、4 MHz以外のVGMは明示的なエラーとします。これらの出力で表示する波形はDLLによる推定で、外部音源から収録した波形ではありません。

本ツールの直接PWM出力形式を除くYM2151以外のチップ、YM2151を2基使う指定、YM2164、追加チップヘッダー、未対応命令はエラーにします。複数音源の一部を無音として再生する機能ではありません。OPM音源を使う曲であっても、ADPCM等を併用するVGMは対象外です。

VGMにはMDX形式の音色番号一覧がないため「全音色をMML保存」は無効です。現在チャンネルの音色表示はレジスタから更新します。MDX用のPDX選択、PCM設定、音色番号によるプログラムチェンジ、一括検証とエラーMDX収集はVGM向け機能ではありません。

不正なオフセット、切れた引数、不正なループ位置、破損gzipを検出します。コマンドはホストプログラムとして実行しません。待機時間は24時間分まで検査します。レジスタ書き込みの件数に固定上限は設けず、入力データの大きさと利用可能なメモリに応じて保持します。イベントは1件8バイトで、入力サイズから容量を予約して再確保を抑えます。

## English summary

Single YM2151, optionally with the direct-PWM export profile below; VGM 1.00–1.71 and single-member gzip VGZ. Supported commands: 54, 61–64, 66, 67 (bounded skip), 70–7f, and B2 for the direct-PWM profile. Timing uses actual command waits at 44.1 kHz, independent of chip clock. GD3 English title is preferred, falling back to Japanese. Playback, seek, loops, speed, key shift, gain, mute, channel waveforms and WAV export use the existing player controls.

Independent OPM DLL honors the header clock; C86CTL/X68Sound require 4 MHz. Other chips, dual chips, YM2164, extra chip headers and unsupported opcodes are rejected. Whole-song MML voice export and MDX batch validation do not apply to VGM. Seeking reconstructs state from the start. Finite non-looped playback has a 0.5-second release tail; unlimited playback restarts after silence.

## Format reference

[VGM 1.71 specification (VGMPlay upstream)](https://raw.githubusercontent.com/vgmrips/vgmplay-legacy/master/VGMPlay/vgmspec171.txt). The parser and gzip decoder are new host implementation code; no other player's synthesis implementation is bundled.

## 0.27: 一括変換のPCM形式

本ツールのVGZ出力が使用するPWMクロック23011361 Hz、cycle=0、mono=0による初期化／停止、左右への直接12bitデータ書き込みに対応します。一般のPWM制御、DACストリーム、他チップのPCM再生を実装したものではありません。PCM8chはステレオ合成済みで、個々のチャンネルには分離できません。再生時はPCM Pのマスクと波形表示を合成PCMに使用します。
