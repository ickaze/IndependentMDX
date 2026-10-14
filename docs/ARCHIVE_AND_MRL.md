# ZIP内の選曲とMRL/MRZ再生 — IndependentMDX 0.33.1

ZIP/LZH/LHAをセレクター上のフォルダーとして開き、Enter／ダブルクリックで移動・選曲します。上へ／Backspaceで親階層へ戻り、元の選択とスクロール位置を復元します。ブックマークと前回選曲は `C:\Music\archive.zip\Album\song.mrl` のような仮想パスで保存します。元ZIPは変更しません。

MDXとPDXには個別フィルター、VGM/VGZ/MRL/MRZには共通の「VGM / MRL」フィルターを使用します。ZIP/LZH/LHAは常に表示します。MRZは曲として扱い、フォルダー表示にはしません。自動次曲は現在の階層だけを対象にします。

LZH/LHAの対応方式、PDX探索順、制限は [0.33アーカイブ仕様](RELEASE_0.33.md) を参照してください。以下のZIP固有の圧縮方式説明はLZHには適用しません。

## 大容量ZIP

- ZIP32 / ZIP64、Stored / Deflateに対応。アーカイブ全体の128 MiB制限・合計展開量512 MiB制限はありません。
- 目次を読み、選択したメンバーだけを一時ディスクへ展開します。ZIP全体の圧縮データ・展開データはメモリへ読み込みません。
- MRL曲名は先頭のメタデータだけを展開して取得し、曲全体の展開を待ちません。この先頭取得では全体CRCを検査できないため、再生用の展開時にCRCとサイズを検査します。
- ZIPの目次情報と空のファイル一覧を保持するので、必要メモリと初回列挙時間はエントリー数に依存します。上限100,000エントリー、入れ子4段です。
- UTF-8名、UTF-8指定なしの場合はCP932として解釈。分割ZIP、暗号化、他の圧縮方式、リンク、危険なパス、重複名は未対応／拒否します。
- 選択したファイルの非圧縮サイズ分の空きディスクが必要です。個別MDX/PDX/VGM/VGZの128 MiB読込制限は残りますが、ファイルから読むMRLはイベント列をストリーム処理します。各メンバーの宣言サイズは1 TiB以下です。
- 一時キャッシュは通常終了時に削除します。異常終了後の `IndependentMDX-zip-*` はアプリ終了後に削除できます。セレクターのサイズはZIP目次の非圧縮サイズ、日時・属性は一時ファイル側の情報です。

```bat
independent-mdx.exe "C:\Music\archive.zip\Album\song.mrl" --wav song.wav
```

## MRL / MRZ

MRL 1.0はFMのみ、MRL 1.1/1.2は曲名・PCM8ch操作・PDX参照情報と任意の内蔵PDXに対応します。1.1/1.2の曲名をセレクターに表示します。1.0には曲名データがないため、セレクターの曲名欄は空欄です。

MRZはZIP内の1つのMRLを再生する形式です。エクスポーターは `record.mrl` だけを格納します。MRLが複数あるMRZは拒否します。一般の複数曲ZIPは `.zip` として開いて選曲してください。

PDX内蔵時はそのデータを優先します。外部参照時は同じフォルダーから探し、サイズ・CRCが記録されていれば照合します。見つからない／不一致の場合は警告を残してFMのみ再生します。PCM8chは個別の再生位置・ゲイン・パン・レートを持ち、チャンネルマスク、波形／鍵盤、移調モード、全体／PCM音量を反映します。シークは先頭からFM/PCM状態を再構築するため、長い曲の後方へ移動すると時間がかかる場合があります。

イベント読込バッファは64 KiBです。ZIPの目次、PDX本体、再生中サンプルのデコード、音源エミュレーター等には別途メモリを使います。ファイル全体・全イベント数に比例するイベント配列は作りません。PDXは128 MiB以下です。入力のbyte配列を受け取る開発用コンストラクターは小さなテスト向けで、GUI/CLIはファイルストリーム経路を使います。

各レコードは再生／シークで読み進める際に検査します。ヘッダーだけの読込や短い再生で、未読の末尾まで正常と保証するものではありません。ZIPメンバー全体のCRC検査は展開時に行います。MRL自体の元MDX CRCはログの改変検知用ではありません。

FMは4 MHz YM2151へ記録どおり送ります。ソフトウェアLFOを再計算するものではありません。1.2は指定されたループ範囲を再生側の周回数で反復します。全記録をその回数倍するものではありません。区間情報のない1.0/1.1は記録済みの全体を一度再生し、無限設定では終端の無音後に全体を再演奏します。

## English

Browse ZIP32/ZIP64 Stored/Deflate archives as virtual folders. Read the central directory and extract only requested files to a temporary cache. MRL titles use prefix decoding; CRC is verified on full member extraction. Limits: 100,000 entries, four nesting levels, one TiB declared size per member, and available temporary disk space. Encrypted/multi-volume ZIP, other codecs, unsafe paths, links and duplicate names are rejected. Legacy names are CP932 unless marked UTF-8. Individual MDX/PDX/VGM/VGZ reads still have their 128 MiB limit.

File-backed MRL playback uses a 64 KiB event buffer, not an array of all events. MRL 1.1 adds titles, eight PCM channels and optional PDX content; 1.0 remains FM-only. MRZ contains exactly one MRL and is opened as a track. External PDX lookup uses the containing folder or an explicit override, with recorded size/CRC verification. Missing banks warn and leave FM playback available. PCM supports masks, monitoring, gain and seek reconstruction. PDX/decoded samples/backend memory is additional. Seek replays from the beginning. Event validation occurs when read; merely opening a file does not validate all later events. Cache is removed on normal shutdown.

## セレクター操作（0.32）

ZIPの曲名欄には拡張子を含むZIP名を表示します。「すべて / All files」がONなら拡張子フィルターを無視して全ファイルを列挙します。未知のバイナリファイルのEnterは再生しません。

MDXと同じフォルダーに同じベース名のDOC/TXT/HED/MUS/PDLがあると、その拡張子名のアイコンになります。優先順位はDOC→TXT→HED→MUS→PDLで、拡張子の大文字小文字は区別しません。Spaceで最優先のテキストを表示／非表示にします。MDXを右クリックすると存在するテキストの種類がメニューに並び、選択したファイルを表示します。TXT/DOC/HED/PDL/MD/MML/MUS/INI/LOG/CSV/NFO/CFGは直接Enterでも開けます。ビューアーのEnter/Space/Escで閉じ、カーソル/PageUp/PageDown/ホイールでスクロール、選択してCtrl+Cでコピーできます。ビューアーの開閉は再生を停止しません。

ビューアーは最大16MiBのプレーンテキスト用です。UTF-8（BOM有無）、UTF-16 LE/BE BOM、CP932を扱います。WordのバイナリDOCは対象外です。ZIP内のテキストも同様に扱います。

項目を別のエクスプローラーへドラッグすると、Windowsのコピー操作になります。移動は要求しません。ZIP内のメンバー／フォルダーはコピー前に一時ディスクへ展開します。大きなファイルではドラッグ開始まで待ち時間があります。コピー完了前にアプリを終了しないでください。

MRL/MRZのシークは過去の状態を再構築し、「再生位置（検索50%）」／「Position (seeking 50%)」のようにラベル内へ進捗を表示します。1.2のループ情報があればその範囲を反復します。旧MRLの記録イベント・曲名を変更しません。

All files overrides the extension filters. DOC/TXT/HED/MUS/PDL sidecars use that priority and show their actual extension as an icon. Space toggles the preferred sidecar; right-click an MDX to choose an existing sidecar; Enter toggles supported plain-text files. Viewing never stops playback. Text supports UTF-8, BOM UTF-16 and CP932, up to 16 MiB; binary Word documents are not supported. Dragging to Explorer requests copy only and materializes archive members first. MRL 1.2 carries loop bounds; seek reconstruction reports progress inside the Position label.
