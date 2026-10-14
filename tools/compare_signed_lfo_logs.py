#!/usr/bin/env python3
"""Compare 544 tick-end states for original LFO_Signed_Probe inputs.
Usage: python compare_signed_lfo_logs.py OWN REFERENCE1 REFERENCE2 OUTPUT.csv
Requires compare_lfo_logs.py from the same directory. Python standard library only.
"""
import csv
import sys
from collections import Counter
from compare_lfo_logs import capture, inventory, observed

def compare(own_folder, first_folder, second_folder, output):
    own, first, second = map(inventory, [own_folder, first_folder, second_folder])
    rows = []
    for name in sorted(own.keys() | first.keys() | second.keys()):
        if name not in own or name not in first or name not in second:
            rows.append([name, 'missing capture', '', '', '', '', ''])
            continue
        a, ca = capture(own[name], 544)
        b, cb = capture(first[name], 544)
        c, cc = capture(second[name], 544)
        if ca != cb or cb != cc:
            raise ValueError('Source CRC mismatch: ' + name)
        av, bv, cv = [[observed(name, s, 0) for s in states] for states in (a,b,c)]
        mismatch = [tick for tick in range(544) if av[tick] != bv[tick]]
        rows.append([name, 'different' if mismatch else 'match',
                     len(mismatch), 544, mismatch[0] if mismatch else '',
                     bv == cv, f'{ca:08X}'])
    with open(output, 'w', newline='', encoding='utf-8') as stream:
        writer = csv.writer(stream)
        writer.writerow(['case','status','different_ticks','checks','first_tick',
                         'reference_repeats_equal','source_crc32'])
        writer.writerows(rows)
    print(dict(Counter(row[1] for row in rows)))

if __name__ == '__main__':
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    compare(*sys.argv[1:])
