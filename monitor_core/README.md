# Visualization-only Independent OPM mirror

Private copy of the updated Independent OPM 0.11b (0BSD), including the key-on mapping fix and full-state serialization update, with namespace isolation and a per-channel output tap. MDX and MIDI/viewer audition use its output only for keyboard waveforms. Audible Windows software FM and WAV export use the supplied independent_opm.dll; C86CTL skips the audible software renderer. There is no hardware audio capture or source separation.

Changes relative to the bundled updated upstream: namespace isolation, channels_ storage/reset and channel_sample() taps. LFO/envelope/oscillator behavior is otherwise identical to that source. The old visualization copy had the same C1/M2 key-on bug as the previous upstream; both sources are now updated. See docs/MONITOR_CORE_PATCH.diff for the complete diff. The taps are before final audio filtering and are not hardware measurements. LICENSE.txt is upstream 0BSD.
