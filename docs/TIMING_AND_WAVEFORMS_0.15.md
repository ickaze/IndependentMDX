> Historical release note. These details describe the named earlier version; see README.md and docs/VALIDATION.md for the current distribution.

# 実測出力タイミングと波形表示 / Timing and waveform display

## スイッチの適用範囲

| 出力・操作 | 実際の音への反映 |
|---|---|
| Independent OPM DLLでMDX再生 | ALG5/7で反映。従来からライブ更新あり |
| Independent OPM DLLでMIDI／音色画面の試奏 | 0.14まではON固定。0.15でライブ更新を修正 |
| X68Sound DLL | このIndependent OPM専用設定は適用されない |
| C86CTL | 実チップの演算はこのソフトウェア設定で変化しない |

0.15では後ろ2種類の出力でスイッチを無効表示にする。保存値は保持し、Independent OPM DLLへ戻った時に使用する。

モデルは4MHz／64分周の1内部サンプル（16μs）単位で、ALG5/7のキャリア出力に因果的な時差を付ける。音量や音程を大きく変えるエフェクトではない。単一キャリア、低い音、モノラル再生などでは聴感差が小さいことがある。他のアルゴリズムにはこの自動モデルを適用しない。別スイッチの「YM3012時差モデル R +7.00μs」とは別の処理。

調査でMDX Player::renderはliveMeasuredOutputTimingを読み、iopm_set_measured_output_timingへ送っていた。一方MidiSynthは初期化でtrueを指定したまま、LiveSessionから更新していなかった。0.15は停止／一時停止／曲再生中の試奏を含め、各音声ブロックで保存値またはライブ値を適用する。

## 波形表示の経路

MDX/MIDIレジスタ書き込み → 表示用Independent OPMミラー → 各チャンネルの出力タップ → 鍵盤上の波形。

C86CTLから録音波形を読み戻しているわけではない。実機のFM信号を8chへ分離する処理もない。C86CTL使用中も表示用FM計算はソフトウェアで行う。PCMはソフトウェア側の各チャンネルのミックス前信号を表示する。FMタップは最終出力フィルターより前なので、DLL出力の波形とも完全一致する測定表示ではない。

@11で波形がほぼゼロになる原因は、キーオンのOP順M1/C1/M2/C2を、パラメータ格納順M1/M2/C1/C2へ変換していなかったこと。OP=3はM1とC1を鳴らすが、旧ミラーはM1と無音設定のM2をキーオンしていた。修正版ソースとミラーでは対応を訂正した。

## 確認結果と限界

- 提示音色の表示用コアの0.1秒プローブ（4MHz、キーコード0x4a）で、旧版ピーク3.42302e-11、修正版0.0342302。表示用の正規化値であり実機電圧ではない。
- MDXのタイミングOFF/ON/OFFが固定OFF/ONの参照レンダーに一致する試験を実施。新コアに合わせ、試験音色もC1のキーオンビットとパラメータ位置を正しく対応させた。
- MIDI LiveSessionの同一状態比較で、ON同士は一致、片方をOFFにすると差分発生、ONへ戻して出力フィルターが落ち着いた後は一致することを確認。
- C86CTL相当のexternalFmモードで、@11の表示波形が生成され、表示用計算が実際の音声バッファへ混入しないことを確認。
- 当該版のDLLはx64で、必要なC ABIエクスポートを確認。DLL本体の実行、Windowsのドラッグ描画、実機C86CTLの聴感確認は未実施。Linux試験は同梱の更新版C APIソースを実行している。

## English

The measured timing switch affects Independent OPM's ALG5/7 audio model. The MDX renderer already forwarded live changes; MIDI/viewer audition was fixed ON until 0.15 and now forwards the setting each block. X68Sound and physical C86CTL do not use this software audio model, so the switch is disabled for those outputs.

The model changes carrier framing on a 16-microsecond native-sample scale at 4MHz; perceived differences can be subtle. It is separate from the 7-microsecond right-channel DAC-delay option.

All FM channel waveforms are simulated from register writes in a private monitor core, not captured from C86 hardware or separated from a mixed recording. The @11 failure came from swapped C1/M2 key-on routing and is corrected. Tests use the updated source on Linux; the supplied Windows DLL and physical devices have not been executed here.
