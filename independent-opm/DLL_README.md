# Independent OPM 0.12 — Windows x64

YM2151 emulator DLL. License: 0BSD.
Copyright (C) 2026 by I.C.KaZe

ホストアプリの説明に従って independent_opm.dll を配置してください。
64bitアプリ用です。32bit DLLは含みません。API仕様はAPI.mdを参照してください。

0.11bからの変更: 実機録音43～46を根拠とするノイズ振幅1/4補正と検証資料の追加。
API ABI=1、状態データ形式は維持。保存状態を新実装で復帰するとノイズ音量は修正後になります。

Place independent_opm.dll as required by your 64-bit host application.
This package contains no 32-bit DLL. See API.md for integration.
Release scope: measurement-based noise normalization (0.25 scale), with unchanged ABI/state layout.

# 0.12 binary package

- Version: 0.12
- Platform: Windows x64
- Version string: `0.12 / C ABI 1`
- C ABI: 1 (17 exported functions)
- DLL SHA-256: `0ce83b204dc20c917d83cbc25d01c3f3912456adc82afc2502687705a2144306`

PE形式、公開関数名、埋込みバージョン文字列を静的確認しました。
Windows上での実行検証および再現ビルドによるソースとの一致確認は行っていません。
実機録音検証・回帰テストの結果はソース版の検証資料を参照してください。

PE architecture, export names and embedded version string were checked statically.
Windows runtime behavior and reproducible-build equivalence were not tested here.
See the source package for hardware measurements and source regression results.
