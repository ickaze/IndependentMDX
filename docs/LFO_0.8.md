> 0.23: MH/MP/MAスイッチはbit7で形式を、bit0でOFF/ONを判定します。MP/MA波形設定はbit0～2、MHのPMS/AMSは0x73のビットを使用します。0x80/0x81は標準的な表記であり、未使用ビット付きの同値表現も受け付けます。

> 0.21: MP矩形波は格納変移D ×（WAVE+4なら256）÷256をデチューン単位として使用します。矩形波固有の追加256倍は撤去しました。WAVE1/D=256とWAVE5/D=1はいずれも中心から±1/64半音です。負数は極性を反転します。MAの計算は今回変更していません。

> 0.20: MP/MAはキーオン中だけ位相とMD遅延カウントを進めます。

> 0.12: MDはMP/MAのソフトLFO専用です。OPMLFOの消去・復元には使用しません。PCMトラックのMH命令はOPMへ送信しません。

> 0.9でハードウェアスイッチの説明を訂正しました。下記のソフトウェアLFO周期の説明は継続します。

# LFO interpretation in IndependentMDX 0.8 / LFOの扱い

このアプリはMMLをコンパイルせず、MDX内のEC/EB命令の16bit値を読みます。Pは格納された周期ワード、Dは符号付き変移ワードです。MMLのMP2,6,48という文字列の6/48をそのままP/Dとは断定できません。ご提示資料の「設定値×4/×2」と脚注の「1周期/半周期」は、MMLからの変換と格納値を区別して読む必要があります。

0.7ではWAVE2の全周期は2Pクロックでしたが、変移は0〜D*P/256と上側だけでした。0.8は全周期2Pを維持し、初期値0から正方向へ進み、+D*P/512と-D*P/512の間を往復します。P=6,D=48のMDXなら全周期12クロック、最大振幅±0.5625デチューン単位です。MMLコンパイラーが周期ワード12を出す場合は24クロックです。1デチューン単位は1/64半音です。奇数周期は半クロックの頂点を離散サンプルで評価します。

1クロックの時間は `(256 - TimerB値) × 16 / 62500` 秒です。テンポ変更で秒単位のLFO周期も変わります。

WAVE0は格納Pクロックごとに0へ戻る鋸波、WAVE1はPクロックごとに符号を変える矩形波です。WAVE0の表の倍率を再度格納値に掛ける変更はしていません。WAVE3は引き続き独自の決定的なsample-and-hold近似です。4〜7は対応する0〜3の変移を256倍にします。

【0.11訂正】EC80=MPOF、EC81=MPON、EB80=MAOF、EB81=MAONです。【0.9訂正】EA80はMHOF、EA81はMHONです。MHOFではそのチャンネルのPMS/AMSを0にします。共有のPMD/AMDを0にして他チャンネルを巻き込むことはしません。MDディレイは実際の再キーオンでのみ再開し、タイでは継続します。MHOF中は遅延終了でも再有効化しません。

Tests cover stored WAVE2 P=6,D=256 at every tick, ON/OFF register traces, tied versus continuous notes with MD delay, and MHOF across the delay threshold. These establish this implementation's behavior; no supplied failing MDX or original-driver audio was available for byte/recording comparison. Original MXDRV overflow and random-wave behavior are not claimed to be bit-exact.

Reference: user-supplied data-format excerpt (EC/EB/EA/E9). No external MXDRV implementation was copied.
