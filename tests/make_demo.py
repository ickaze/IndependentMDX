"""Generate original synthetic test music and samples. No third-party music data."""
import math
import struct
from pathlib import Path
root = Path(__file__).resolve().parents[1]

def adpcm(pcm):
    predictor = 0
    index = 0
    codes = []
    for target in pcm:
        step = math.floor(16 * 1.1 ** index)
        choices = []
        for code in range(16):
            delta = step // 8
            if code & 1: delta += step // 4
            if code & 2: delta += step // 2
            if code & 4: delta += step
            decoded = max(-2048, min(2047, predictor + (-delta if code & 8 else delta)))
            choices.append((abs(decoded - target), code, decoded))
        _, code, predictor = min(choices)
        magnitude = code & 7
        index = max(0, min(48, index + (-1 if magnitude < 4 else 2 * (magnitude - 3))))
        codes.append(code)
    if len(codes) % 2: codes.append(0)
    return bytes(codes[i] | (codes[i + 1] << 4) for i in range(0, len(codes), 2))

kick = [int(1500 * math.exp(-i / 950) * math.sin(2 * math.pi * (65 * i / 15625 + .14 * (1-math.exp(-i/100))))) for i in range(4000)]
click = [int(1100 * math.exp(-i / 250) * (math.sin(i * 1.74) + math.sin(i * 2.63)) / 2) for i in range(1600)]
samples = [adpcm(kick), adpcm(click)]
pdx = bytearray(768)
for i, sample in enumerate(samples):
    struct.pack_into('>II', pdx, i * 8, len(pdx), len(sample))
    pdx.extend(sample)
(root / 'samples' / 'DEMO.PDX').write_bytes(pdx)
# Eight bars at 48 clocks per quarter. Melody and bass are original test phrases.
melody = [45,49,52,56,54,52,49,47, 45,52,57,56,54,49,47,45]
tracks = [bytearray([0xf1,0]) for _ in range(9)]
tracks[0] = bytearray([0xff,220, 0xfd,0, 0xfb,0x88, 0xfc,1, 0xf8,7])
tracks[1] = bytearray([0xfd,1,0xfb,0x94,0xfc,2,0xf8,6])
tracks[8] = bytearray([0xfb,0x86,0xed,4])
for bar in range(8):
    for beat in range(4):
        tracks[0].extend([128 + melody[(bar*2+beat) % len(melody)], 47])
        tracks[1].extend([128 + [21,26,24,28][bar % 4], 47])
        tracks[8].extend([128 + (beat % 2), 47])
for i in [0,1,8]: tracks[i].extend([241,0])
# New instrument definitions, deliberately simple and reproducible.
voice0 = bytes([0,7,1, 1,1,1,1, 0,127,127,127, 31,31,31,31, 8,0,0,0, 3,0,0,0, 0x48,15,15,15])
voice1 = bytes([1,4,5, 1,1,2,1, 24,127,0,127, 31,31,31,31, 9,0,6,0, 3,0,3,0, 0x58,15,0x58,15])
header = bytearray(b'Original FM + PCM test sequence\r\n\x1aDEMO\0')
base = len(header)
header.extend(bytes(20))
for i, track in enumerate(tracks):
    struct.pack_into('>H',header,base+2+i*2,len(header)-base)
    header.extend(track)
struct.pack_into('>H',header,base,len(header)-base)
header.extend(voice0 + voice1)
(root/'samples'/'DEMO.MDX').write_bytes(header)
print('Created original DEMO.MDX and DEMO.PDX')
