# IndependentMDX 0.10

## 日本語

- MHの通常設定（EAの直後が80/81以外）は、周波数・PMD・AMDとともにPMS/AMSを書き込み、その場でハードLFOを有効にします。以前はMDの未経過状態によりPMS/AMSが0のままになる経路がありました。次の再発音に対するMD遅延は維持します。EA80=MHOF、EA81=MHONは変更していません。
- シークの空白クリックは、トラックバーの溝の座標から絶対位置へ変換します。以前の「再描画なしで両端へ移動し、つまみ矩形を取得する」方法は、更新されていない矩形に依存していました。マウスを離した時に移動を確定します。
- 停止／一時停止中のMIDIは初期値Polyphonicです。Monophonicとの切り替えが可能です。切り替え時は押鍵中の音を消し、次のNote Onから新しい方式になります。再生中は従来どおり入力チャンネルと再生チャンネルの1対1対応です。
- PolyphonicではFM8ch／PCM8chを別々に割り当て、入力元MIDIチャンネルの保持音色／選択プログラム／PCMバンク・形式を使用します。空のFM割り当て先にも入力元音色を一時的に設定できます。割り当て先の保持音色は書き換えません。Note Off、サステイン、ベンドは入力元に対応付けます。一時停止を解除すると試奏を消音し、元の曲の音色・レジスタ・PCM設定を復元します。
- Channel PressureとPolyphonic Key Pressureを音量に反映します（0=無音、127=元の音量）。受信前は127相当です。前者は同じ入力チャンネルから割り当てた全発音、後者は一致するノートに適用します。FM/PCM両方に有効で、マスク中もMIDIには適用します。
- X68SoundのOpmWait指定を0から1へ変更。確認した公開実装では0指定により内部サンプルごとに65,535回のキュー処理を繰り返し、空のキューでもループを続けます。1指定では160回となります。DLL内のこの負荷は波形表示を止めても残るため、今回の遅さの有力な原因です。ユーザー環境のDLLでの実測は未実施です。
- 32bit補助プロセスとの受け渡しをパイプライン化し、音声生成と本体の処理を重ねます。転送遅延は2048内部サンプル（約32.8ms）です。ソフトPCMも同じ時間だけ遅延して合わせます。チャンネル波形を復帰しました。FM波形は独立した可視化用OPMコアによる参考表示であり、X68Sound DLL内部から取得した波形ではありません。
- C86CTLにはFM／PCM独立の遅延欄を追加しました。各0～2000ms、初期値0、次回の曲再生時から適用します。早い側の値を増やして合わせてください。FMはレジスタ送信予定時刻、PCMは音声リングバッファで遅らせます。MIDI試奏は低遅延の直接出力です。
- FM gainの横にPCM gain（0～400%）を配置し、どちらも再生中に反映します。全体音量スライダーは0～400%（初期値40%）です。MIDI試奏にも反映します。C86CTLのFM音量はキャリアTLで調整するため、増幅側は元のTLが0に達すると頭打ちとなり、ソフトウェアの線形増幅と完全には一致しません。0%ではパンを閉じて消音します。
- 表示選択から「チャンネル別鍵盤」／「縦ピアノロール」を切り替えます。後者は共通の96音鍵盤と直近8秒の縦スクロール表示で、FM A～HとPCM P～Wを16色で区別します。同じ音程の複数チャンネルは分割表示します。A～W見出しのクリックで個別マスク、マスク中は見出しを暗くします。PCM音階は従来どおりPDXサンプル番号の対応表示です。
- 添付画像から16/24/32/48/64/128/256pxのアイコンを作成し、実行ファイルとウィンドウに設定しました。
- 新設定も変更2秒後にEXEと同じ場所のINIへ保存します。

## English

Normal MH settings now immediately enable PMS/AMS, even behind a pending MD delay. The next retrigger still observes MD. Hardware EA80=OFF / EA81=ON is unchanged. Blank seek clicks map directly from channel coordinates; releasing the mouse commits the seek.

Idle/paused MIDI defaults to Polyphonic, with an optional Monophonic mode. Pooled voices borrow the input channel's instrument or PCM bank/format, including otherwise undefined destination slots. Borrowing does not overwrite retained destination instruments. Resume silences trial notes and restores song state. Mode changes silence held notes. Normal song playback retains one-to-one MIDI channel mapping. Channel Pressure and Polyphonic Key Pressure control FM/PCM amplitude: 0=silence, 127=original level; before receiving pressure the level is unchanged.

X68Sound OpmWait(0) has been replaced with OpmWait(1) to avoid the classic DLL's excessive empty-queue polling. IPC is pipelined with 2048 native frames (~32.8ms) of transfer latency; software PCM receives matching delay. Waveforms are restored, using the separate visualization OPM core, not taps from the X68Sound DLL. Actual user-DLL performance remains unmeasured.

C86CTL FM and PCM delays are independently configurable from 0 to 2000ms, default 0, applied on the next song start. Increase the earlier side to align outputs. MIDI audition bypasses these song delays. PCM gain sits beside FM gain and updates live. Master volume is 0–400%, default 40%, also affecting MIDI. Hardware FM gain uses carrier TL and saturates at TL=0 on amplification; zero closes channel pan. Software output uses linear gain.

The view selector switches between channel keyboards and one shared vertical piano roll. Pitch runs left-to-right; the last eight seconds scroll upwards. Sixteen channel colors and A–W clickable mute headers are provided. PCM positions represent sample indices. The supplied image is embedded as a multi-resolution EXE/window icon. New settings use the existing two-second INI save beside the EXE.

## Sources checked for the adapter diagnosis

- Public X68Sound C API implementation: https://github.com/rururutan/X68Sound/blob/master/X68Sound/X68Sound.cpp
- `Opm::CalcCmndRate`, `Opm::ExecuteCmnd`, `Opm::GetPcm`: https://github.com/rururutan/X68Sound/blob/master/X68Sound/opm.h
- Trackbar channel coordinates: https://learn.microsoft.com/en-us/windows/win32/controls/tbm-getchannelrect

Only adapter behavior was inspected. No X68Sound implementation was copied into the application or Independent OPM core. The supplied Independent OPM DLL and upstream sources are unchanged.
