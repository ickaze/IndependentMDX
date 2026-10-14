# LFO comparison — IndependentMDX 0.31

## ランダム音程LFOの観測

元MDX CRC32 `055F8493` の同一入力から得た2つのMRLを比較しました。これはログのレジスタ観測であり、他のMXDRVソースや逆アセンブルは参照していません。E/Fチャンネルの設定はともに`EC 03 00 18 05 55`（WAVE3、周期24、変移1365）です。

| 観測 | IndependentMDX | 比較対象 |
| --- | --- | --- |
| Eの音程範囲（KC/KFを1/64半音へ換算） | 3525～3780 | 0～6143 |
| Fの音程範囲 | 3525～3780 | 0～6143 |
| E/Fの系列 | 同一 | 異なる |
| 開始時 | 既にランダム変位を適用 | 基準値3653 |
| 初回更新の時刻 | 次の値変化は574464クロック | 57344クロック |

同時刻の書き込み順は保持し、KC/KFの両方が更新された後の状態を比較しています。IndependentMDXはphase/periodから独立ハッシュを作り、正規化値にdepth×periodを掛けます。そのため同じ位相のチャンネルで同じ値になり、ここでは約4半音の範囲しか動きません。比較対象では上下限まで達する大幅な変位があり、開始位相にも差があります。

この1入力だけでは、共有乱数状態かチャンネル別シードか、整数幅・符号・掛け算の順序、乱数の再初期化条件を一意に決められません。波形を合わせるための根拠のない定数や特定曲専用処理は追加していません。WAVE3の既存近似は継続課題です。MP/MA/MHを分離した80本のオリジナル比較用MDXを`samples/LFO_Comparison`に収録しました。各条件と取得手順は同フォルダーのREADME.txt / CASES.csvを参照してください。

## 未定義音色

MDXの実行経路上で未定義FM音色が見つかった85入力を照合しました。該当入力に対応する正常な比較対象MRLは0件でした。比較対象ツールのエラーログには80件の`voice number not found`と5件の`voice table offset out of range`が対応していました。

従って、未定義音色を無視する・前の音色を使う・別の番号へ置き換える、といった挙動を観測結果から正当化できません。0.31ではこのエラー判定を維持しています。エラー文を削除して成功ログとして扱う変更は行っていません。

## English

The random pitch-LFO operands on two channels are identical, but the reference produces different channel sequences, a much larger range and a different first update. The current phase-hash approximation cannot reproduce these observations. The data does not uniquely identify RNG state, seeding, signed arithmetic or reset rules. Eighty original isolated MP/MA/MH inputs are provided for repeatable acquisition; no speculative random-wave change was applied.

All 85 missing-voice inputs lacked successful paired reference logs. The reference error list reports 80 missing voice numbers and five invalid voice-table offsets. Missing-voice errors remain; no undocumented replacement voice is selected.
