# IndependentMDX 0.33 — アーカイブ再生

## PDX探索

MDXのPDX名に拡張子がなければ `.PDX` を補います。同じフォルダーの名前を大文字小文字を区別せず照合し、従来の数字付きコピー名への対応も維持します。見つからない場合に限り、MDXを収めたアーカイブ内全体の同名ファイルを探します。候補が1つなら使用し、複数なら曖昧として警告します。別のアーカイブや外部フォルダーへは広げません。明示的なPDX指定が最優先です。

Windowsのファイル検索は大文字小文字を区別しない一方、従来の展開キャッシュは区別していました。未展開の空ファイルが存在する状態で、異なる表記の名前を開くと展開が行われない経路を修正しました。キャッシュの検索も区切り文字と大文字小文字を正規化して照合します。

## LZH/LHA

`.lzh` / `.lha` をセレクターやファイル選択から開くとフォルダーとして表示します。Enterで移動／再生、上の階層へ戻る操作、ブックマーク、曲名、MDXのテキストアイコン、右クリック文書選択、コピーをZIPと共通の処理で扱います。LZHの曲名欄は拡張子付きアーカイブ名です。

- 読み込みを許可する方式: LH0、LH1、LH4、LH5、LH6、LH7、LZ4、LZ5、LZS。LHDはディレクトリーです。LH2/LH3/LHX/LK7/PMarcはこのアプリでは対象外です。
- ヘッダーはLhasaのlevel 0〜3解析を使用します。従来名の文字コードはCP932として扱います。任意のコードページや特殊な機種依存拡張を保証するものではありません。
- 単一ボリュームの通常アーカイブが対象です。SFX、暗号化、分割は未対応です。リンクを拒否し、展開先を一時キャッシュ内に制限します。
- 目次は保持しますが、全ファイルの圧縮データや展開データをメモリへ読み込みません。選択したファイルのみ64KiBの出力バッファとデコーダーの辞書を使用して展開します。曲名取得は必要な先頭部分だけを読みます。
- 全展開時はCRC16とサイズを検査します。先頭部分だけの取得ではファイル全体のCRCは検証できません。
- 共通の制限は100,000エントリー、入れ子4段、メンバー宣言サイズ1TiB以下、展開先の空きディスク容量です。MDX/PDX等の従来の読込上限は別途適用されます。
- 一時キャッシュは通常終了で削除します。元のアーカイブを変更しません。日時・属性は展開用の仮ファイル側の情報です。

```bat
independent-mdx.exe "C:\Music\collection.lzh\Music\DEMO.MDX" --wav output.wav
```

## ビルド・依存関係

Lhasa 0.6.0の必要ソースを同梱し、CMakeとZigのビルドで静的リンクします。別の解凍DLLや外部アプリ、ビルド時のダウンロードは不要です。ISCの著作権・ライセンス全文は `third_party/lhasa/COPYING.md` と `THIRD_PARTY_NOTICES.txt` に記載しています。音源のIndependent OPM 0.14 DLLは従来どおり差し替え可能な構成を維持し、同じバイナリを同梱しています。

## 検証

独自のDEMO.MDX/PDXと生成したアーカイブで、ZIPの同階層・別階層PDX、重複候補の拒否、LZH LH0/LH5/LH6/LH7の展開結果のバイト一致、level 0/2、曲名用先頭読込、CRC不一致・壊れたヘッダー・level 0の親階層指定の拒否を確認しました。その他の許可方式・level 1/3には今回の独自テスト入力はありません。

Linux native CTestは5/5成功。Windows x64 GUI/CLIとx86ブリッジをクロスビルドし、デバッグディレクトリー・制作環境パス・非システムのランタイム依存を検査しました。Windows固有の大文字小文字テストはテストソースへ追加していますが、この環境ではWindowsで実行していません。GUI操作、実機音源での再生は未検証です。

設定ファイルのキーと値は変更していません。LFO・MRLの仕様変更はありません。

## English

Browse LZH/LHA as virtual folders. Methods allowed: LH0/LH1/LH4/LH5/LH6/LH7/LZ4/LZ5/LZS; LHD directories. Lhasa 0.6.0 is statically linked under ISC. Regular single-volume archives only, CP932 legacy names; no SFX, encryption or split archives. Extract members on demand with size/CRC16 verification, and decode only a prefix for titles. Limits: 100,000 entries, four nesting levels, one TiB declared member size plus disk space and individual format limits. Prefer same-directory PDX, then a unique match within the containing archive. Settings and the replaceable OPM DLL are preserved. Native regression tests execute on Linux; Windows GUI and case-insensitive path behavior are build-only checks here.
