# IndependentMDX 0.9 — distribution audit

Updated independent-opm-0.11b.zip (Library version 6), downloaded 2026-09-22.
Archive SHA-256: `8cef9b0b8c04bd21f38f6e470f5ba2285d04e2228456283e75a2f41c02444015`

Supplied x64 independent_opm.dll SHA-256: `26ca691c1cdb0b090fc7f76e7c6224882c1129b5f20dfdff5b187750e693e20f`

The packaged DLL is byte-identical to the attachment. Its PE machine is AMD64 and its exported functions match C ABI 1. No binary/source equivalence claim is made from export inspection alone. Actual DLL execution remains to be verified on Windows.

All upstream files are copied without modification. The main upstream tree remains unmodified. A separately named monitor_core copy adds a read-only channel tap for visualization only; its full changes are in MONITOR_CORE_PATCH.diff. Windows playback dynamically resolves the C API in the supplied DLL; audible output does not use a static fallback. The visualization-only mirror is statically linked in its own namespace. Linux regression builds link the same upstream C API source. All eight monitor signals come from one visualization mirror; the authoritative audible mix uses the supplied DLL.

Current upstream LICENSE.txt and THIRD_PARTY_NOTICES.txt are preserved. No statement that these match a different 0.10b archive is added. The upstream package retains its own historical correction documents.

C86CTL interface declarations are derived from the official public header and are covered by the retained BSD license in third_party/. No C86CTL DLL or implementation is bundled. Root/bin notices include this scope, the application license, the current OPM notices and application compiler/runtime notices. Actual packaged hashes are recorded in SHA256SUMS.txt.
