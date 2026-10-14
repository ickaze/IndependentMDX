#!/usr/bin/env python3
"""Create original raw-MDX probes; no external compiler or driver is used."""
from pathlib import Path
import csv
import struct

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'samples/LFO_Random_Depth_Probe'
OUT.mkdir(exist_ok=True)
VOICE = bytes([0,7,1,1,1,1,1,32,127,127,127,31,31,31,31,0,0,0,0,0,0,0,0,15,15,15,15])
rows = []

def emit(kind, depth, detune, group):
    name = f'{group}_{kind}3_D{depth:+06}_DT{detune:+06}'
    cmd = bytearray([255,200,253,0,251,128,233,0,243])
    cmd += struct.pack('>h', detune)
    cmd += bytes([236 if kind == 'MP' else 235,3,0,1])
    cmd += struct.pack('>h', depth)
    for n in range(4):
        if n < 3:
            cmd.append(247)
        cmd += bytes([176,127])
    cmd += bytes([236 if kind == 'MP' else 235,128,176,31,241,0])
    data = bytearray(('Random depth probe ' + name).encode() + b'\r\n\x1a\0')
    base = len(data)
    data += bytes(20)
    for channel in range(9):
        struct.pack_into('>H', data, base + 2 + channel * 2, len(data) - base)
        data += cmd if channel == 0 else bytes([241,0])
    struct.pack_into('>H', data, base, len(data) - base)
    data += VOICE
    (OUT / (name + '.MDX')).write_bytes(data)
    rows.append([name + '.MDX', kind, 3, 1, depth, detune, 544])

for depth in [93,95]:
    for detune in list(range(-32768,32768,4096)) + [32767]:
        emit('MP', depth, detune, 'RNG')
for kind in ['MP','MA']:
    for depth in [-257,-256,-255,-129,-128,-127,-93,-1,0,1,93,127,128,129,255,256,257]:
        emit(kind, depth, 0, 'DEPTH')
with (OUT / 'CASES.csv').open('w', newline='') as stream:
    writer = csv.writer(stream)
    writer.writerow(['file','command','wave','period','raw_signed_depth','detune','ticks'])
    writer.writerows(rows)
(OUT / 'README_JA.txt').write_text('''ランダムLFOの深さ・乱数値の切り分け検証（68個）

比較対象ツールでこのフォルダー全体を1ループ・フェードなしで2回記録し、
別々のフォルダーにMRL/MRZを保存してください。PDXは不要です。
既存のLFO_Random_Probe / LFO_Signed_Probeの再取得は不要です。

RNG_で始まる34個：MP3の生の深さ93/95、周期1、17段階のデチューン。
深さ93は既存の比較ログで音程変化が確認された値です。95は量子化の
境界が異なる補助条件です。クリップされない区間を複数のデチューンで
補い、可能なら乱数出力を絞り込みます。完全復元は未確認です。
DEPTH_で始まる34個：MP3/MA3の生の深さの正負・128/256付近を比較し、
深さを演算するビット幅と符号の扱いを調べます。

全入力はテンポ200、MD0、512クロックのタイ継続後にOFFで32クロック。
CASES.csvに全条件を記載しています。MMLコンパイラを介さない独自MDXです。
音程が大きく跳ぶため、ログだけを取得するか、小さい音量で使用してください。

IndependentMDX側の取得例:
independent-mdx.exe --reglog-folder samples/LFO_Random_Depth_Probe --loops 1 --mrz

Copyright (C) 2026 by I.C.KaZe / 0BSD（プロジェクトのLICENSE.txt）。
''')
(OUT / 'README.txt').write_text('''Random LFO depth probes: 68 original MDX inputs
Capture the entire folder twice with the reference tool, one loop, no fade,
in separate MRL/MRZ folders. No PDX is required. Earlier probes need not be repeated.
34 RNG inputs use MP3 raw depth 93/95, period 1 and 17 detune offsets. Depth 93
has produced nonzero reference modulation; depth 95 adds quantization boundaries.
34 DEPTH inputs cover signed MP3/MA3 depths near 128 and 256. CASES.csv lists all
conditions. 512 tied ticks plus 32 OFF ticks, tempo 200, MD0. Capture silently
or at low volume. Exact RNG recovery is not established.
Copyright (C) 2026 by I.C.KaZe / 0BSD; see project LICENSE.txt.
''')
print('Generated', len(rows), 'MDX inputs')
