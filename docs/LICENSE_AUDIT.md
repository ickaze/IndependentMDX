# IndependentMDX 0.33.5 distribution audit

The unmodified Independent OPM 0.14 Windows x64 DLL, C API header and license are copied from the 0.14 Windows package. No synthesis implementation or private monitor core is bundled or statically linked. Linux tests use a separately built, unmodified upstream 0.14 shared library outside this distribution.

DLL SHA-256: `701166ec2784e4b59db30a65ab07006f78b02b9883e88d8c663784b4ebf7309b`

YM2151 key-on register bits are passed to the DLL unchanged. The obsolete host-side bit 4/5 swap was removed in 0.30.1; the DLL handles operator-slot mapping. The DLL itself is not modified.

Instrument text and knob controls adapted from NOTE-MDX-Workbench 2.8.21 retain Copyright (C) 2026 by I.C.KaZe and the 0BSD license. C86CTL interface declarations retain their BSD notices. C86CTL and X68Sound DLLs are not distributed. Compiler/runtime notices remain in THIRD_PARTY_NOTICES.txt.

Windows PE dependency inspection does not establish runtime correctness. See VALIDATION.md for executed versus build-only checks. SHA256SUMS.txt lists packaged file hashes.

Lhasa 0.6.0 archive parsing/decoding sources are bundled unmodified in third_party/lhasa/lib and statically linked. The ISC copyright/license is reproduced in third_party/lhasa/COPYING.md and both copies of THIRD_PARTY_NOTICES.txt. Integration and source provenance are documented in third_party/lhasa/README.md. No Lhasa CLI or external extraction DLL is required.

Packaging correction within 0.33.5: build_vs2022.bat uses CRLF line endings; distribution notices identify the current version and Independent OPM author. Project source headers follow the existing LICENSE.txt scope; third-party notices are retained. The IndependentMDX contributors attribution is retained rather than reassigning copyright ownership. The historical LFO note explicitly records its unverified excerpt provenance. Executable and DLL bytes are unchanged by this packaging correction. SHA256SUMS.txt has been regenerated.
