# MDX → VGZ 一括変換 / Batch export

```bat
independent-mdx.exe --vgz-folder "D:\Music\MDX" --loops 2
```

GUI版でも同じ引数を使えます。コマンドプロンプトで終了を待つ場合:

```bat
start /wait "" IndependentMDX.exe --vgz-folder "D:\Music\MDX" --loops 2
```

ウインドウ、オーディオデバイス、MIDIデバイスを開かず、INI設定も読み書きしません。別途起動しているGUIにも影響しません。

例えばカレントフォルダーが `C:\Converted`、入力が `D:\Music\MDX` の場合、`D:\Music\MDX\Album\song.mdx` は `C:\Converted\MDX\Album\song.vgz` になります。大文字・小文字にかかわらず `.mdx` を再帰検索します。シンボリックリンクは変換対象外です。通常ファイルを検索した後、フォルダー名と変換中のファイル名をコンソールへ表示します。

- `--loops N`: 1～1000、初期値2。MDXのループを指定回数まで演奏します。ループしない曲は通常どおり最後まで記録します。無限ループは指定できません。VGMのループメタデータは付けず、有限の演奏として展開します。フェード時間は0です。
- `--no-pcm`: PCMを記録せずFMのみ出力します。PDXの検索・読み込み・デコード・ミキシングを省略するため、このモードではPDXに関する警告・エラーも発生しません。PCMトラックの命令と同期は引き続き解釈します。省略時はPCMを記録します。
- `--seconds N`: 1ファイルの記録上限、1～86400秒、初期値3600秒。超過時はその曲をエラーとし、切れたVGZを出力しません。
- 既存の同名VGZはスキップします。再変換する場合は別のカレントフォルダーで実行するか、対象VGZを先に移動してください。
- PDXはMDXのあるフォルダーから通常再生と同じ規則で探します。見つからない場合は警告し、FM部分を変換します。この警告はエラーログに記録しません。壊れたPDXなど他のエラーは変換失敗です。
- 解析・再生エラーのある曲はスキップし、出力ルートの `vgz-errors.csv` にフルパス、曲名、内容をUTF-8で追記します。次のファイルの変換を続けます。
- Ctrl+Cで中断します。完成済みVGZは残り、処理中の一時ファイルは削除します。強制終了時に残った `.vgz.work-*` は、処理が動いていないことを確認して削除できます。
- 終了コード: 0=成功（既存出力のスキップを含む）、1=一部変換失敗、2=引数・検索・ログ等のエラー、130=中断。

PCMを除外する例:

```bat
independent-mdx.exe --vgz-folder "D:\Music\MDX" --loops 2 --no-pcm
```

## GD3タグ

| 表示名 | 記録値 |
| --- | --- |
| Track Title / TITLE_E | MDXの曲名 |
| TITLE_J | MDXの曲名（TITLE_Eと同じ内容） |
| RIPPER / 変換者 | Independent MDX |
| Comment / Notes | Exported from Independent MDX |

標準GD3 1.00の該当欄へUTF-16LEで記録します。表示名はプレイヤーにより異なります。曲名が空のMDXはタイトル欄も空にします。

## 記録方式と制限

FMは4 MHz YM2151のレジスタ列、PCMは44.1 kHzステレオに合成して**12bitに量子化したPWMデータ**としてVGM 1.71へ記録し、gzip圧縮します。PCMには量子化誤差があり、8chを個別には保持しません。YM2151とPWMに対応するVGMプレイヤーが必要です。実機PWMのプログラムやROMイメージではありません。GUIの音量、キー、音色編集、マスク、実機出力、再生速度は適用せず、元MDXを標準速度で記録します。

作業用の非圧縮VGMとVGZを出力先で一時的に保持します。PCMの長い曲ではディスク使用量が増えます。VGMの32bitファイルサイズ・時間上限を超えた場合も不完全な出力は公開しません。IndependentMDX自身のVGZ読み込みには非圧縮128 MiB上限があるため、それより大きい出力は対応する別プレイヤーが必要です。

## 変換処理

レジスタの生成に必要なMDXシーケンサーと、PCM記録に必要なPCM演算のみを実行します。FM合成・音声デバイス出力・波形表示は行いません。圧縮時のCRC32はテーブル方式です。時間とイベント列は通常再生のシーケンサーに従います。

## English

Run either executable with `--vgz-folder DIRECTORY --loops N [--seconds N]` to convert recursively without opening a GUI or devices. Output is `CURRENT_DIRECTORY / INPUT_DIRECTORY_NAME / relative_path.vgz`. Loops default to 2 (1–1000); per-file limit defaults to 3600 seconds (1–86400). Non-looping songs end normally. Loops are materialized, with no VGM loop pointer or fade.

Existing outputs are never overwritten. Missing PDX produces a console warning and an FM-only export. Other failures are appended to `vgz-errors.csv` (full path, title, error), then conversion continues. Ctrl+C cancels the active conversion without publishing partial output. Settings and a running GUI are untouched.

FM remains YM2151 register data; mixed stereo PCM uses 44.1 kHz **12-bit quantized direct PWM**. This is lossy for PCM and requires a player supporting both chips. This application's decoder supports the exported direct-PWM profile, not arbitrary PWM or DAC streaming. Large exports may exceed this application's 128 MiB decompressed input limit. Temporary uncompressed and compressed files require free disk space.

Use `--no-pcm` to export FM only; this skips PDX lookup, decoding and mixing while retaining PCM track command/synchronization interpretation. PCM is included by default. Export skips unused FM synthesis and uses table-based CRC32. GD3 English/Japanese title fields both contain the MDX title; converter/ripper is `Independent MDX`, and notes/comment is `Exported from Independent MDX`.
