# IndependentMDX 0.30.1 — Independent OPM出力修正

## 原因と変更

ホスト側の `OpmBackend::writeDll()` に、Independent OPM 0.13/0.14のキーON値のbit 4とbit 5を入れ替える処理が残っていました。Independent OPM 0.14は自身でYM2151のキーONビット順をパラメータの格納順へ対応付けるため、この追加変換は誤りでした。

例えばレジスタ0x08の値0x18（チャンネルA、M1/C1）を0x28（M1/M2）へ変えていました。AL4でC1を出力に使用し、もう一方のキャリアを無音設定にしている音色では、必要なキャリアが発音せず無音になります。全オペレーターONの0x78では入れ替え前後の値が同じなので、従来の全ON中心のテストでは検出できませんでした。

0.30.1はキーON値を変更せずDLLへ渡します。即時書き込み、時刻指定書き込み、チャンネル波形、MDX/VGM/VGZ/MRL/MRZ、MIDIの共有出力経路に適用されます。DLL本体、MRL形式、記録済みログ、ユーザー設定は変更しません。旧版で生成したMRL/MRZを再出力する必要はありません。

## 実行検証

8つのMRZを先頭20秒、48kHzでオフライン再生しました。PCM音量は0にし、FMだけを検査しています。ネイティブLinux上の未改変Independent OPM 0.14共有ライブラリを使用しました。旧処理の比較は同じイベント列のキーON bit 4/5だけを入れ替えて再現しています。

ピークは符号付き16bit出力の絶対値最大です。「有信号ch」は20秒間のモニター波形ピークが1/32768を超えたFMチャンネルで、音声の正確な一致を意味しません。曲名・他ユーザー作成のファイル名は記載しません。

| 元MDX CRC32 | 旧処理ピーク | 修正後ピーク | 旧処理の有信号ch | 修正後の有信号ch |
| --- | ---: | ---: | --- | --- |
| `D2DD8078` | 0 | 6255 | — | ABCDEF |
| `0D907231` | 0 | 7314 | — | ABCDEFG |
| `C57EFC62` | 9162 | 9981 | EFH | EFGH |
| `333A75DC` | 0 | 2721 | — | ABCDE |
| `1D22B671` | 5514 | 10740 | H | ABCDEH |
| `DC0917B3` | 6921 | 8468 | ABCDH | ABCDGH |
| `94553122` | 3398 | 4104 | ABCD | ABCDGH |
| `209DAF1A` | 8334 | 8946 | ABCDH | ABCDGH |

3入力で無音から発音へ、残り5入力では欠落チャンネルの出力復帰を確認しました。全曲全区間や実機との音色一致を検証したものではありません。

各オペレーターを単独キャリアにする合成テストを即時／時刻指定の両経路へ追加しました。従来の部分オペレーターテストとオリジナルDEMOフィクスチャも、誤った入れ替えに依存していたキーONマスクを訂正しました。音量や許容値を緩和する変更ではありません。

## 配布・検証範囲

Windows x64 GUI/CLIとx86 X68Soundブリッジをクロスビルドしています。Windows GUI、同梱Windows DLLの実行、C86CTL、X68Sound実機動作は未実施です。Independent OPM DLLは0.30同梱の0.14とバイト単位で同一です。レジスタ比較報告 `MRL_VALIDATION.html` は0.30のシーケンサー比較結果であり、このホスト出力修正で論理ログは変わりません。

## English

The host incorrectly swapped KON bits 4 and 5 for Independent OPM 0.13/0.14. The 0.14 core already maps YM2151 KON bits to operator slots. Removing the extra swap restores partial-operator voices. Existing MRL/MRZ logs and the DLL do not need replacement. Eight 20-second, FM-only native Linux probes reproduced three silent inputs and five partial-channel failures with the old routing; the missing outputs returned with the corrected routing. This is not a full-song or hardware fidelity test. Windows binaries are cross-built; Windows/device runtime checks remain outstanding.
