# IndependentMDX 0.32 — 操作と検証

## 操作

- 検索中の再生位置ラベルは `再生位置（検索50%）` / `Position (seeking 50%)`。別の進捗行は削除し、日英の最大文字幅を確保しています。
- MDXと同じベース名のDOC/TXT/HED/MUS/PDLをこの順で検索します。アイコンは優先する拡張子名です。Spaceはそのファイルを開閉、右クリックでは存在する種類から選べます。メニューで選ぶと、同じ文書が既に表示されていても閉じずに表示します。
- 「すべて」で表示したHED/PDLもEnterでビューアーを開閉できます。通常フォルダーとZIP内の仮想フォルダーに共通です。再生は停止しません。既存の16MiBテキスト上限と文字コード処理は維持しています。
- 設定ファイル形式、既存の保存値、DLL、MRL 1.2の形式は変更していません。

## 検証範囲

- Linux native CTest: 5/5成功。MP矩形/三角波とMHのバンク指定の回帰検査を含みます。
- Windows x64 GUI/CLIとx86ブリッジをZig 0.13.0でクロスビルド。GUIテストコードもWindows向けにコンパイルしました。
- 元の検証用MDX80個と追加17個のレジスタログ出力に成功。受領した比較対象76ログとの結果は [LFO比較](LFO_COMPARISON_0.32.md) を参照。
- Windows実行環境がないため、今回の右クリックメニュー、アイコン描画、日英表示を含むGUI操作と音声出力は実機では未検証です。GUIテストの実行成功を意味するものではありません。
- DLLは従来と同じIndependent OPM 0.14で、音源ソースの同梱・静的組み込みは行っていません。

Historical release documents retain their original scope. This release does not claim random-LFO compatibility or full audio equivalence.
