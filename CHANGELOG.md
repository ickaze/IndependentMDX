# 0.33.5

Replace approximate software-LFO RNG with a capture-derived recurrence; fix signed 16-bit pitch overflow. All 202 available probe cases match their target register states. See docs/RANDOM_LFO_0.33.5.md for exclusions and limits.

# 0.33.4

Correct random LFO depth arithmetic, update timing and song-wide draws. The random sequence generator remains approximate; see docs/RANDOM_LFO_0.33.4.md for measured limits. Existing recordings require re-export.

# 0.33.3

Correct signed MA saw/square attenuation and MD-dependent phase/delay handling. Signed-probe comparison improves from 42 to 48 matches out of 58 captures. Ten random-wave cases remain unresolved. Add 68 original random-depth probes. See docs/LFO_SIGNED_0.33.3.md.

# 0.33.2

Negative MA triangle start attenuation and MD-delay baseline fixed. Added 70 signed-LFO probes; see docs/LFO_SIGNED_0.33.2.md. Existing recordings require re-export.

# 0.33.1

- Enable the vertical scrollbar in the bookmark dropdown. Preserve stored bookmarks, ordering and settings.

# 0.33

- Fix Windows archive cache lookup with case differences, preventing reads of unexpanded PDX placeholders.
- Preserve same-folder PDX priority and add unique archive-wide filename lookup with ambiguity errors.
- Add LZH/LHA virtual-folder browsing, lazy streaming extraction, prefix metadata reads, bookmarks, text sidecars and copy integration using statically linked Lhasa 0.6.0 (ISC).
- Add original archive/PDX fixtures, byte/CRC regression tests and documented format limits.

# 0.32

- Correct MP square first reversal and retrigger boundary, bipolar MP triangle phase, and preservation of raw MH PMD/AMD bank bits using independently captured register evidence.
- Include reproducible LFO state comparison, CSV/HTML results and 17 follow-up random-state probes. Keep the unresolved approximate software RNG explicitly documented.
- Move seek progress into the Position label, reserving width in both languages.
- Add HED/PDL viewing and DOC/TXT/HED/MUS/PDL sidecar precedence, extension-specific icons and right-click document selection.

# 0.31

- Add MRL 1.2 loop time/event bounds, repeat playback and seek progress. Preserve legacy 1.0/1.1 reads and the requested capture loop count.
- Bound WAV output independently of the GUI infinite-repeat switch (CLI --loops 0 uses two loops for WAV).
- Fall back to PDX name/PCM operation records when an embedded bank is missing.
- Add ZIP filename titles, All files filtering, DOC/TXT sidecar icons, a modeless plain-text viewer and Explorer copy drags.
- Add 80 original software/hardware LFO diagnostic MDX inputs. Document observed random-wave differences and reference export failures for missing voices; do not alter either behavior without evidence.

# 0.30.1

- Remove the obsolete Independent OPM 0.13/0.14 KON bit 4/5 host adapter. Pass raw key-on bits unchanged for audio and channel monitoring.
- Add immediate/timestamped isolated-operator regression coverage and correct the partial-operator test fixture.
- Preserve the MRL 1.1 format, existing logs, settings and bundled Independent OPM 0.14 DLL.

# 0.30

- Correct observed MP/MA deterministic waveform centering, initial displacement and square startup phase; keep modulation active through key-off/rests until disabled.
- Apply tempo changes after the pending timer interval, prepare delayed-note pitch at the note boundary, preserve delayed key-on at a simultaneous gate boundary, and defer FM voice programming until a note.
- Preserve y-written same-note pitch and MH saved sensitivity; retain the meaningful PMD/AMD bank bit.
- Capture logical FM log writes before output retrigger adaptation.
- Add streaming ZIP32/ZIP64 member extraction and a 64 KiB MRL event reader; retrieve MRL titles without full member extraction.
- Add MDXRLOG 1.1 UTF-8 titles, PCM8 control records and PDX identity/optional content, with legacy 1.0 read/write compatibility.
- Add --embed-pdx / --no-embed-pdx, --mrl-v1 and --mrz (single-member ZIP64) batch options.
- Replay eight PCM channels with masks, gains, keyboard/waveform monitoring and seek reconstruction.
- Include the register comparison report and updated format/export documentation. Random-wave integer behavior remains a known limitation.

# 0.29.1

- Continue MRL and VGZ export when the software-LFO approximation diagnostic is emitted; retain failure handling for other diagnostics and actual data errors.
- Preserve sequencer behavior and the MDXRLOG 1.0 format for output comparisons.

# 0.29

- Play MDXRLOG 1.0 files with fractional event timing and seek support.
- Browse ZIP folders and nested archives; preserve virtual paths in bookmarks and last-file settings.
- Add bounded Stored/Deflate ZIP decoding with CRC checks and unsafe-path rejection.

# 0.28

- Add recursive --reglog-folder export and the public MDXRLOG 1.0 binary specification.
- Preserve native logical timestamps, duplicate writes and event ordering without audio synthesis or device adaptation.

# 0.27.1

- Add `--no-pcm` to headless VGZ export; PDX files are not required in this mode.
- Omit unused FM synthesis without changing register events or PCM samples; accelerate gzip CRC calculation.
- Add GD3 English/Japanese titles, converter and notes; preserve UTF-8 titles in batch error logs without double conversion.

# 0.27

- Add headless recursive `--vgz-folder` conversion with finite loop count, mirrored output paths, cancellation, per-file time limit and CSV errors.
- Preserve YM2151 register events and record PCM as stereo 44.1 kHz / 12-bit direct PWM; decode this export profile on playback.
- Stream VGM and gzip output through private temporary files; never overwrite an existing VGZ or publish incomplete output.

# IndependentMDX 変更履歴

## 0.26.1

音色テキストの変更パラメーターと一覧の変更音色を赤色で表示。9行が表示できる最小・初期サイズを設定。音色ウインドウの表示状態・位置・サイズの保存復元と、表示ボタンでの表示／非表示切り替えを追加。

## 0.26

Independent OPM 0.14へ更新。Workbench形式の音色ノブ編集、再生セッション全音色の初期値保持、即時反映、個別リセット、変更値と初期値方向の表示を追加。

## 0.25

音量LFOの振幅・位相初期化、F1ジャンプと曲全体時間、エラー記録・収集・一括検証、セレクターの曲名遅延読み込み、MIDIデバイス切り替え、コマンドラインファイル引き渡し、無限ループの動的変更を修正・追加。出力変更は再生停止から同位置再開へ変更。Independent OPM 0.13のDLL APIだけでチャンネル波形を取得する構成へ移行。VGM/VGZ再生と高密度イベント列、クロック単位の書き込みに対応。

## 0.24

重複する音色番号は最初の定義を採用。チャンネル波形表示を修正。README_JA.mdをREADME.mdへ改名。

## 0.23〜0.19

MDXのスイッチビット判定、構造検査、ソフトLFO、画面配置・スライダー・MIDI入力更新を改善。

## 0.18〜0.13

機能別レイアウト、音色切り替え時の出力処理、音源状態保存、試奏鍵盤、キャリア音程表示、再生速度、スプリッター描画を追加・改善。

## 0.12〜0.7

MIDI多声試奏・アフタータッチ、ピアノロール、音量と出力別設定、LFO解釈、シーク、X68Sound出力、音色表示と一覧を追加・修正。

## 0.6

Independent OPM DLL、C86CTL出力、再生位置操作、MIDI入力、音色MML出力と表示を追加。この版では出力切り替え時に再生停止していた。

## 0.5〜0.1

独立したIndependentMDXとして公開。表示言語切り替え、PCMマスク、ファイルセレクター、設定保存、鍵盤・波形表示、ブックマーク、キー変更などを整備。
