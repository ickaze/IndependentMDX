#!/usr/bin/env python3
"""Compare original samples/LFO_Comparison captures, without a driver implementation.
Usage: python compare_lfo_logs.py OWN_LOG_FOLDER REFERENCE_LOG_FOLDER OUTPUT.csv
Uses only Python's standard library. Accepts .mrl and single-member .mrz.
This measures tick-end modulation state, not whole-file/event-order equivalence.
"""
import argparse
import csv
import struct
import zipfile
from pathlib import Path

KEYS = [0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14]
TICK_CLOCKS = 57344  # Fixture tempo 200 at 4 MHz.


def capture(path, ticks):
    if path.suffix.lower() == '.mrz':
        with zipfile.ZipFile(path) as archive:
            names = [n for n in archive.namelist() if n.lower().endswith('.mrl')]
            if len(names) != 1:
                raise ValueError('Expected one MRL in ' + str(path))
            data = archive.read(names[0])
    else:
        data = path.read_bytes()
    if data[:8] != b'MDXRLOG\0' or len(data) < 64:
        raise ValueError('Invalid MRL header')
    major, minor, offset, record_size = struct.unpack_from('<HHII', data, 8)
    hz, count, end = struct.unpack_from('<QQQ', data, 24)
    if (major != 1 or minor > 2 or record_size != 16 or hz != 4000000
            or offset < 64 or offset > len(data) or len(data)-offset != count*16):
        raise ValueError('Unexpected MRL version, clock, or record size')
    events = {}
    for time, chip, reg, value, phase, arg in struct.iter_unpack('<QBBBBI', data[offset:]):
        if chip == 0 and phase == 0:
            events.setdefault(time, []).append((reg, value))
    registers = [0]*257
    result = []
    ordered = iter(sorted(events.items()))
    pending = next(ordered, None)
    for tick in range(ticks):
        while pending is not None and pending[0] <= tick*TICK_CLOCKS:
            for reg, value in pending[1]:
                if reg == 0x19 and value & 0x80:
                    registers[256] = value & 0x7f  # PMD is a separate bank.
                else:
                    registers[reg] = value
            pending = next(ordered, None)
        result.append(tuple(registers))
    return result, struct.unpack_from("<I", data, 56)[0]


def observed(name, state, channel):
    if name.startswith('MP'):
        kc = state[0x28+channel]
        return ((kc >> 4)*12+KEYS.index(kc & 15))*64+(state[0x30+channel] >> 2)
    if name.startswith('MA'):
        return state[0x60+channel]  # Fixture's audible carrier OP1.
    return tuple(state[r] for r in [1, 15, 0x18, 0x19, 256, 0x1b, 0x38])


def inventory(folder):
    result = {}
    for p in sorted(Path(folder).rglob('*')):
        if p.is_file() and p.suffix.lower() in ('.mrl', '.mrz'):
            if p.stem in result:
                raise ValueError('Duplicate diagnostic basename: '+p.stem)
            result[p.stem] = p
    return result


def compare(own, reference):
    left, right = inventory(own), inventory(reference)
    rows = []
    for name in sorted(left.keys() | right.keys()):
        if name not in left or name not in right:
            rows.append((name, 'missing own' if name not in left else 'missing reference', '', '', ''))
            continue
        ticks = 288 if name.endswith('_switch') else 384
        channels = 8 if '8CH' in name else 2 if '2CH' in name or 'STAGGER' in name else 1
        (a, crc_a), (b, crc_b) = capture(left[name], ticks), capture(right[name], ticks)
        if crc_a != crc_b:
            raise ValueError("Source MDX CRC differs: " + name)
        differences = [(t, c) for t in range(ticks) for c in range(channels)
                       if observed(name, a[t], c) != observed(name, b[t], c)]
        rows.append((name, 'different' if differences else 'match', len(differences),
                     ticks*channels, differences[0][0] if differences else ''))
    return rows


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('own')
    parser.add_argument('reference')
    parser.add_argument('output')
    args = parser.parse_args()
    rows = compare(args.own, args.reference)
    with open(args.output, 'w', newline='', encoding='utf-8') as stream:
        writer = csv.writer(stream)
        writer.writerow(['case', 'status', 'mismatched_tick_channels', 'tested_tick_channels', 'first_mismatch_tick'])
        writer.writerows(rows)
    for status in ['match', 'different', 'missing own', 'missing reference']:
        print(status+':', sum(row[1] == status for row in rows))
