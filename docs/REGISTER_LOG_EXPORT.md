# MRL / MRZログ出力 — IndependentMDX 0.31

GUIを開かず、指定フォルダー以下のMDXを検索し、カレントフォルダー下の「入力フォルダー名」に同じ階層で出力します。Windows版はGUI EXE、CLI EXEのどちらでも同じ引数を使用できます。大量処理時はコンソール版 `independent-mdx.exe` が進捗を確認しやすい形式です。

```bat
independent-mdx.exe --reglog-folder "D:\Music" --loops 2 --seconds 3600
independent-mdx.exe --reglog-folder "D:\Music" --loops 2 --no-embed-pdx
independent-mdx.exe --reglog-folder "D:\Music" --loops 2 --embed-pdx --mrz
independent-mdx.exe --reglog-folder "D:\Music" --loops 2 --mrl-v1
```

| オプション | 動作 |
| --- | --- |
| `--reglog-folder DIRECTORY` | 再帰的なMDX検索。ZIP内の一括検索は対象外 |
| `--loops N` | 1～1000、既定2。0／無限は禁止 |
| `--seconds N` | 1～86400秒、既定3600。上限到達は失敗扱いで部分ログを残さない |
| `--embed-pdx` | MRL 1.2へ見つかったPDX全体を内蔵。既定 |
| `--no-embed-pdx` | PDX本体を含めず、名前と取得できたサイズ・CRCだけ記録。PCM操作は記録する |
| `--mrz` | `.mrl`の代わりにZIP圧縮した`.mrz`を出力 |
| `--mrl-v1` | 従来のMRL 1.0。FMレジスタのみ。曲名・PDX・PCM操作なし。PDX指定オプションとは併用不可。MRZとの併用は可能 |

既定のMRL 1.2にはMDX曲名（UTF-8）、PDX参照名、PDX識別情報、FMとPCM8chの論理操作が入ります。PDXが見つからない場合も名前とPCM操作を記録しますが、再生には対応する外部PDXが必要です。内蔵されないPDXは、再生時にMRL/MRZと同じフォルダー（ZIP内を含む）から探します。`--pdx`またはGUIのPDX指定も使用できます。記録済みサイズ・CRCが違うPDXは使用せず、警告とFM再生を継続します。

MRZは `record.mrl` 1ファイルをDeflateしたZIP64です。汎用ZIPソフトでも展開できます。MRL再生時には内部イベントを再計算せず、記録された順序と時刻で再生します。MRL 1.0の比較ツールには`--mrl-v1`を使うか、[1.2仕様](REGISTER_LOG_FORMAT.md)に従ってヘッダー長とchip番号を扱うよう更新してください。

音源合成・PCM音声レンダリングはログ出力に不要です。MDXの全トラックの命令、同期、待機、ループ判定は実行します。FMは出力先固有の補正前、PCMはデコーダー／ミキサーへ渡す論理操作を記録します。ホストのGUI音量・移調・マスク・MIDI入力は記録しません。PDX内蔵時もPCMのデコードは行いません。

既存ファイルは上書きしません。設定を変えて出力し直す場合は、別のカレントフォルダーを使ってください。`reglog-errors.csv`は追記型です。フォルダー移動、変換対象、結果件数はコンソールへ表示します。Ctrl+C、時間超過、データエラーでは一時ファイルを削除します。既知のソフトウェアLFO精度警告だけは継続します。埋め込みフェード近似等の他の診断は引き続き失敗扱いです。

MRZ変換とZIPからの再生は一時ディスク領域を使います。圧縮前のMRLを置ける空き容量が必要です。出力の持ち運びではPDX内蔵によるサイズ増加に注意してください。

## English

Run `--reglog-folder DIR` before any GUI/INI initialization. Mirror recursive MDX paths under `cwd/input-folder-name`. Defaults: two loops, 3600-second limit, MRL 1.2 with embedded PDX when available. Use `--no-embed-pdx` for external-bank references, `--mrz` for a single-member ZIP64 file, and `--mrl-v1` for FM-only 1.0 comparison logs. Explicit PDX flags cannot be combined with `--mrl-v1`. The PCM event stream is retained even without embedded PDX.

Titles are UTF-8; the original PDX reference is CP932. External banks are looked up beside the MRL/MRZ, including virtual ZIP directories, or through `--pdx`/Choose PDX. When recorded, size and CRC must match. Missing/mismatched banks leave FM playback available with a warning. Existing outputs are skipped; errors append to `reglog-errors.csv`. Cancellation, timeout and data errors never publish partial outputs. Only the known software-LFO fidelity warning is tolerated. No other MXDRV implementation source is used.

## ループ情報とWAV保存（0.31）

既定出力はMRL 1.2です。指定した`--loops`分の全イベントを記録する仕様は継続します。加えて、初回の曲ループ開始／終了の時刻とイベント番号をヘッダーへ保存します。自然ループがない曲は記録された全区間を反復区間にします。再生側はその反復区間を使い、GUIの無限ループをON/OFFできます。

WAV保存時はGUIの無限ループ設定を無視し、周回数欄の回数後にフェードアウトします。CLIで`--wav`と`--loops 0`を併用した場合は2周へ置き換えます。GUIの設定値自体は変更しません。CLI WAV保存に既定の時間打ち切りはなく、明示した`--seconds`の上限だけが周回数より優先します。旧1.0/1.1には区間情報がないため、記録済みの全演奏が1区間です。

`--embed-pdx`で名前に対応するPDXが見つからない場合も、PDX名とPCM操作を保存して出力を完了します。内蔵フラグは0、未取得サイズ・CRCは0になります。破損したPDXや曖昧な参照など、単なる未発見以外のエラーは隠しません。

MRL 1.2 stores loop time/index bounds without truncating the requested capture. Missing PDX falls back to an external reference even with --embed-pdx. WAV output is finite: the GUI uses its loop-count field; CLI --loops 0 becomes two loops. Legacy 1.0/1.1 files lack section boundaries and are treated as one recorded performance.
