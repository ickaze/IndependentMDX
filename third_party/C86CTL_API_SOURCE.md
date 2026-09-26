# C86CTL interface provenance

`src/c86_output.h` contains the subset of COM-compatible interface declarations used by this application, based on the official header:
https://github.com/honet/c86ctl/blob/development/c86ctl/src/c86ctl.h

Header copyright: Copyright (c) 2009–2012 honet. Repository license: BSD-3-Clause, reproduced in C86CTL_LICENSE.txt. Interface GUIDs, inheritance and method order are retained. No C86CTL implementation or binary is bundled.

Official distribution: https://www.c86box.com/software.html
Use an x64 c86ctl.dll beside IndependentMDX.exe. OPM/YM2151 only. GIMIC PLL is set to 4 MHz when that interface is exposed; other hardware must already operate at 4 MHz. The first identified OPM device is selected.
