# C ABI 1 — API overview

Copyright (C) 2026 by I.C.KaZe

License: 0BSD, see LICENSE.txt. Exact declarations and argument rules: independent_opm.h.

## 呼び出しの流れ / Lifecycle

1. `iopm_create(clock_hz, output_rate, &handle)` — create. Clock: 100000–10000000 Hz; output: 8000–192000 Hz. Example: 4000000 / 48000.
2. `iopm_write_register(handle, address, value)` — immediate register write, bypassing BUSY rejection.
3. `iopm_render_f32` or `iopm_render_s16` — fill a caller-owned interleaved L,R buffer of at least `2 * frames` samples.
4. `iopm_destroy(handle)` — release.

`iopm_render_events_f32` applies sorted register events at offsets measured in master clocks relative to render entry. See the header for endpoint behavior and range checks.

`iopm_read_status`, `iopm_clock_count`, `iopm_reset`, `iopm_clone`, `iopm_set_measured_output_timing`, `iopm_set_output_delays`, `iopm_version`, `iopm_abi_version` provide status, timing, configuration and lifecycle support.

同じハンドルへの並列アクセスは行わないでください。float出力はクリップなし、int16出力はクリップありです。実測出力タイミングは`iopm_set_measured_output_timing(handle, 1)`で有効化します。
Do not access the same handle concurrently. Float output is unclipped; int16 output is clipped. Enable the measured ALG5/7 output timing model explicitly if desired.

## 状態保存・復元 / Save and restore

- `iopm_state_size(handle, &size)` obtains the required byte count.
- Allocate a caller-owned buffer and call `iopm_save_state(handle, buffer, capacity, &written)`.
- `iopm_load_state(handle, buffer, size)` restores the snapshot; a failed load leaves the handle unchanged.

`iopm_save_state(handle, NULL, 0, &size)` is also a size query. State includes core state, resampler history and coefficients, output rate and fractional clock remainder. Loading restores saved configuration as well as playback state. Host-owned sequencer position and queued audio must be saved/synchronized separately.

保存・復元はメモリを確保します。再生処理と直列に実行し、リアルタイム音声コールバック内での使用は避けてください。形式はバージョン付きで、32/64bitの構造体配置に依存しませんが、異なるCPU・ビルドでの復元後の浮動小数点出力完全一致は保証しません。
Save/load allocate memory. Serialize access and avoid real-time audio callbacks. The versioned format does not depend on native struct layout; bit-identical floating-point continuation across different builds/CPUs is not guaranteed.

## Return codes

| Value | Name |
|---:|---|
| 0 | IOPM_OK |
| -1 | IOPM_INVALID_ARGUMENT |
| -2 | IOPM_OUT_OF_MEMORY |
| -3 | IOPM_INTERNAL_ERROR |
| -4 | IOPM_BUFFER_TOO_SMALL |
| -5 | IOPM_INVALID_STATE |

The returned string from iopm_version is owned by the DLL; do not free it. The functions use the C ABI and IOPM_CALL as declared in the header. This DLL is not a drop-in replacement for X68Sound or other emulators' APIs.
