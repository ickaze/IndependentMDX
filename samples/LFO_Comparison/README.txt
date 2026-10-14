IndependentMDX LFO comparison inputs

Original synthetic MDX data, Copyright (C) 2026 by I.C.KaZe, 0BSD (project LICENSE.txt).
No external driver implementation or music data is included.

Each file isolates an MP, MA or MH condition. MDX note 48 is the base note;
software depth operands are raw signed 16-bit values. PDX is not used.
All scores terminate, so use one loop without fade when acquiring traces.

IndependentMDX command:
independent-mdx.exe --reglog-folder "LFO_Comparison" --loops 1 --mrl-v1

Record each random test at least three times with the reference tool.
Compare KC/KF for MP, TL for MA, and 0x18/0x19/0x1b/0x38..0x3f for MH.
Compare both identical simultaneous channels and staggered starts.
This separates shared random state from per-channel state and note resets.
Waves 4..7 probe the expanded-depth variant. Extreme operands can clip pitch/TL.
CASES.csv lists raw conditions. No expected reference trace is asserted.
