from pathlib import Path
import csv,struct
root=Path(__file__).resolve().parents[1];out=root/'samples/LFO_Signed_Probe';out.mkdir(exist_ok=True)
voice=bytes([0,7,1,1,1,1,1,32,127,127,127,31,31,31,31,0,0,0,0,0,0,0,0,15,15,15,15])
rows=[]
def emit(kind,wave,period,depth,scenario='tie',delay=0):
 name=f'{kind}{wave}_P{period:03}_D{depth:+06}_{scenario}_MD{delay}'
 cmd=bytearray([255,200,253,0,251,128,233,delay,236 if kind=='MP' else 235,wave])+struct.pack('>Hh',period,depth)
 for n in range(8):
  if scenario=='tie' and n<7:cmd.append(247)
  cmd+=bytes([176,63])
 cmd+=bytes([236 if kind=='MP' else 235,128,176,31,241,0])
 b=bytearray(('Signed LFO probe '+name).encode()+b'\r\n\x1a\0');base=len(b);b+=bytes(20)
 for ch in range(9):
  struct.pack_into('>H',b,base+2+ch*2,len(b)-base)
  b+=cmd if ch==0 else bytes([241,0])
 struct.pack_into('>H',b,base,len(b)-base);b+=voice
 (out/(name+'.MDX')).write_bytes(b);rows.append([name+'.MDX',kind,wave,period,depth,scenario,delay,544])
for kind in ['MP','MA']:
 for wave in range(8):
  for depth in ([-256,-93,0,93,256] if wave<4 else [-1,0,1]):
   emit(kind,wave,8,depth)
for depth in [-93,93]:
 for scenario,delay in [('tie',0),('retrigger',0),('retrigger',4)]:emit('MA',2,192,depth,scenario,delay)
with (out/'CASES.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['file','command','wave','period','raw_signed_depth','notes','MD','ticks']);w.writerows(rows)
(out/'README_JA.txt').write_text('''符号付きソフトウェアLFO検証用MDX（70個）

比較対象ツールでフォルダー全体を1ループ・フェードなしで記録し、MRL/MRZログをご提供ください。
ランダム波形(MP3/7、MA3/7)の比較のため、全体を2回記録し別のフォルダーに保存してください。
PDXは不要です。独自生成データで、他者のドライバー実装を使用していません。

MP/MAそれぞれ全8波形の正負・ゼロを比較します。
波形0～3は生の深さ -256/-93/0/93/256、波形4～7は -1/0/1、周期8です。
MA2の周期192・深さ±93は、タイ継続/通常再発音/MD4の3条件も比較します。
通常データは64クロック音符を8個タイで接続し、最後にLFO OFFで32クロック鳴らします。
retriggerはタイなしで64クロックごとに再発音します。CASES.csvに全条件を記載しています。
原始MDXパラメータを直接格納しているため、MMLコンパイラの深さ変換の影響はありません。

符号による開始位置・向き・端数処理・クリップ、MDと再発音の位相を確認するための入力です。
未取得の比較結果は未確定です。特にMA0/1の負値とランダム波形は推測で修正していません。
音程・音量が変動するため、ログだけを取得するか小さい音量で使用してください。

IndependentMDXでの取得例:
independent-mdx.exe --reglog-folder samples/LFO_Signed_Probe --loops 1 --mrz

Copyright (C) 2026 by I.C.KaZe / 0BSD（プロジェクトのLICENSE.txt）。
''')
(out/'README.txt').write_text('''Signed software LFO probes (70 original MDX inputs)
Capture this entire folder twice with the reference tool, one loop, no fade,
in separate output folders. Supply both MRL/MRZ captures. No PDX is needed.
CASES.csv lists raw signed MDX depths, periods, MD delay and note patterns.
64 cases cover MP/MA waves 0..7, negative/zero/positive depths; six cases
cover MA2 period 192 depth +/-93 with tied notes, retriggers and MD4.
Each input lasts 544 ticks at tempo 200: 512 ticks modulation, 32 ticks OFF.
Use quiet playback or capture only. These are measurement inputs, not an
assertion of reference behavior. Random waves and negative MA0/1 are unresolved.
Copyright (C) 2026 by I.C.KaZe / 0BSD; see project LICENSE.txt.
''')
print('generated',len(rows))
