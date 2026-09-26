# MDX layout handling

The loader finds the normal title terminator and NUL-terminated PDX name, then validates a compact table with nine or sixteen channel entries. All offsets are relative to the table base.

The minimum voice/track offset must equal the table size: 20 bytes for nine channels or 34 for sixteen. This permits voices before sequences and sequences stored in a different order from the channel table. Shared track entries are allowed. Each track must start after the table and inside the file; the voice offset may point to EOF to represent an empty bank.

Voice records are read in 27-byte units up to the nearest following track or EOF. Duplicate IDs use the first complete record; later records of the same ID are skipped without stopping distinct-ID loading. The parser does not introduce a format-specific tempo, startup wait or entry-point rewrite.

This is bounded layout validation, not unrestricted recovery. Missing terminators, incomplete headers, out-of-range offsets and unsupported table boundaries are errors with the file offset, observed value and expected bounds. Sequence validation and instruction budgets still apply during playback. File identification strings do not select alternate parsing behavior.
