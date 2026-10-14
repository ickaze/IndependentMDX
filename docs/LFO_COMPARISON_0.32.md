# LFO比較結果 — IndependentMDX 0.32

## 方法と対象

他者のMXDRVソース・逆アセンブルは参照していません。独自に生成した `samples/LFO_Comparison` のMDXと、提供された2組のMRL/MRZレジスタログのみを比較し、変更後は同じMDXから再出力しました。共通ログの元MDX CRCが一致することも確認しています。

入力80ケースに対し、IndependentMDXは80ログ、比較対象は76ログです。比較対象のエラーCSVにはMA波形4/5/6/7の4ケースが `invalid amp LFO waveform` として記録されています。この4ケースを一致・不一致の母数に含めていません。

同一時刻の書き込みを全て反映してから、ドライバーの各クロック末の状態を比較します。KCとKFの書き込み順による一時的な組み合わせは比較しません。テンポ200の1クロックは57,344 OPMクロック、クロック源は4 MHz。通常ケースの先頭384クロック、ON/OFF切替ケースの先頭288クロックを対象とし、STAGGERは両チャンネルとも先頭384クロックまでです。初期化専用・終了専用レコードは除外しています。

- MP: KC/KFを1/64半音単位の音程へ換算。
- MA: この検証音色で有効なキャリアOP1のTL。
- MH: 0x01、0x0f、0x18、0x19のAMDとPMDを**別バンク**として保持、0x1b、0x38。
- 多チャンネルケースは全ての対象FMチャンネルを比較。

これは限定した入力の変調状態の比較であり、全レジスタ書き込みの順序・回数、PCM、音声波形、全MDXの互換性を保証するものではありません。既存のMRL/MRZは記録済み値を再生するため、MDX解釈の修正はログ再出力後に反映されます。

## 結果

| 分類 | ケース数 | 変更前の一致 | 0.32の一致 |
|---|---:|---:|---:|
| MP通常波形（0/1/2/4/5/6） | 15 | 5 | 15 |
| MA通常波形（0/1/2） | 12 | 12 | 12 |
| MH（ハードLFO） | 4 | 0 | 4 |
| MP/MAランダム波形 | 45 | 0 | 0 |
| 比較可能な合計 | 76 | 17 | 31 |

詳細は [変更前CSV](LFO_COMPARISON_0.32_BEFORE.csv)、[変更後CSV](LFO_COMPARISON_0.32.csv)、[HTML](LFO_VALIDATION_0.32.html)。CSVの差分数は「クロック×対象チャンネル」の状態差で、書き込みイベント件数ではありません。

## 修正内容

**MP矩形波:** 周期8の入力では初回反転がクロック9ですが、従来は5で反転していました。初期変位と反転時刻を修正し、MD0の再発音境界では前の変位を保持して翌クロックに新しい値を反映します。保持音、再発音、休符、ON/OFF切替で確認しました。

**MP三角波:** 従来は一方向へ大きく振れる形になっていました。周期8・波形6・深さ1の先頭17クロックで観測された変位は `1,2,3,4,5,4,3,2,1,0,-1,-2,-3,-2,-1,0,1`（1/64半音単位）です。中心と初回折り返し位置を修正しました。MA三角波の単方向変位は変更していません。

**MHバンク指定:** MHの最初の深さバイトにbit 7を強制追加していたため、値0x20を比較対象はAMDへ送るのに、こちらは0xa0としてPMDへ送っていました。2つの値はそれぞれ元のbit 7を保持して書き込みます。両方が同じバンクを指定する入力もそのまま扱います。MHON/MHOF、MDとOPMLFOの分離は維持しています。

## ランダムLFOの確定した差と未解決点

乱数生成式は未特定です。既存の近似ハッシュを今回のログの値で置き換えたり、有限の値列を繰り返す実装にはしていません。

- MPの周期8・生の深さ1では、比較対象はクロック1、9、17…で更新します。従来実装はクロック0から変位があり、8、16…を区切りに更新します。
- 比較対象のMPの最初の変位は `7,-65,85,70,-24,-84,56,-86…`（1/64半音）。従来の振幅は極端に小さく、深さに周期を掛けた近似は観測値と合いません。
- 比較対象MAの周期8では初回更新はクロック8です。MD8ではクロック15、MD24では31でした。MPとMAで初回更新位置が異なります。
- 2ch/8chの比較対象MPは、単一チャンネルの乱数列をチャンネル順に分配した結果と一致しました。チェックできた96/376個の値で差はありません。現在の実装は位相から値を作るので、同位相のチャンネルが同じ値になるという違いがあります。
- 周期1・深さ1/8/-8のMAは、同じ条件のMPの変位を下位8bitへ丸め、基準TL45へ加え127で制限する関係と、各383クロックで一致しました。符号・整数の折り返しも現在の浮動小数近似と違います。ただし、この関係だけから乱数内部の式や全てのオーバーフロー規則を断定はできません。
- 今回受領した比較対象ログは各ケース1回分です。別の起動・取り直しでも乱数列が同じになるかは未検証です。

## 追加の検証入力

`samples/LFO_Random_Probe` に17個の独自MDXを追加しました。同じMP7・周期1・深さ1に対しデチューンを変え、音程の上下限で見えなくなる乱数値を複数ログから補うためのものです。比較対象ツールで**フォルダー全体を2回、別フォルダーへ**記録してください。1ループ・フェードなしを使用します。音程が大きく変わるので、音を出さずにログを取得するか再生音量を下げてください。これは乱数式を特定するための測定仮説であり、互換実装そのものではありません。

元の80ケースの比較を再実行するには:

```bat
independent-mdx.exe --reglog-folder LFO_Comparison --loops 1 --mrz
python tools/compare_lfo_logs.py own_logs reference_logs result.csv
```

比較スクリプトはこの80ケース専用です。追加17入力の結果をこの固定条件のスクリプトで判定しないでください。

## English

Clean-room comparison uses only original synthetic MDX inputs and recorded register traces. No third-party MXDRV source or disassembly was consulted. Of 80 inputs, the reference exported 76; four MA extended-wave cases failed. Tick-end modulation states matched in 17 cases before correction and 31 afterward. All 31 deterministic/hardware cases now match within the stated windows; 45 random cases remain different. Whole-file event-order, audio and general compatibility are not asserted. Preserve raw MH bank bits, correct MP square startup/retrigger timing and bipolar triangle turning points. The approximate RNG remains unchanged. Seventeen detune-offset probes are supplied to recover values obscured by clipping; acquire two independent reference runs.
