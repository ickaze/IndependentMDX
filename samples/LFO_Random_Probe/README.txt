IndependentMDX random-LFO follow-up probes (17 inputs)

Original synthetic MDX data, Copyright (C) 2026 by I.C.KaZe, 0BSD.
Use the project LICENSE.txt. No external driver implementation is used.

Please acquire reference register logs for this entire folder twice, in
separate output folders, using one loop and no fade. Preserve both recordings.
IndependentMDX: independent-mdx.exe --reglog-folder LFO_Random_Probe --loops 1 --mrz

WARNING: these are measurement inputs and can make large pitch jumps.
Capture register logs without listening, or at a low playback volume.

Each file has identical MP7, period 1, raw depth 1 and 384 ticks.
Only signed detune differs (-32768 through +32767). Note 48 is used.
Combining unclipped KC/KF states across the detune offsets is intended to
recover 16-bit sample values obscured by clipping and 1/256 quantization
in the first probes. This is a measurement hypothesis, not an asserted
reference RNG formula. Results must be cross-checked against the earlier
MP3 depths +/-8, multi-channel and delay tests before changing the RNG.
