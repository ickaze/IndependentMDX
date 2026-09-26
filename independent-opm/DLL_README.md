# Independent OPM 0.11b — Windows x64 DLL

Copyright (C) 2026 by I.C.KaZe

## 日本語

YM2151のレジスタ入力からステレオPCMを生成するライブラリです。
Windowsの64bit（x64）アプリケーション用です。32bitアプリケーションでは使用できません。

### 利用方法

対応するホストアプリケーションが指定するフォルダーへ`independent_opm.dll`を配置してください。DLL単体では曲を再生できません。MDX/PDXの解析・演奏制御はホスト側で行います。
既存DLLを更新する場合は、ホストアプリケーションを終了してから差し替えてください。

開発者向けにC ABI 1の宣言`independent_opm.h`と[API概要](API.md)を同梱しています。
インポートライブラリ（.lib）は含みません。動的ロードにはWindowsのLoadLibrary/GetProcAddressを利用できます。
コアを直接組み込む場合や再ビルドする場合は、別配布のソースパッケージを使用してください。

### ライセンス

本プロジェクトは0BSDです。許諾本文と著作権表示は[LICENSE.txt](LICENSE.txt)を参照してください。

### 内容と確認範囲

提供されたビルド済みDLLを、ファイル名だけ`independent_opm.dll`に統一して収録しています。DLL本体のバイト列は変更していません。
PEヘッダーのx64指定、内部バージョン文字列`0.11b / C ABI 1`、17個の公開API（状態保存・復元を含む）を静的に確認しました。直接のインポート先はKERNEL32.dllです。
この梱包作業ではWindows上でのロード・再生試験や、ソースとの再現ビルド比較は行っていません。
実機とのビット一致・サイクル一致を保証するものではありません。
SHA256SUMS.txtは各同梱ファイルの照合用です。

## English

Independent OPM generates stereo PCM from YM2151 register writes. This package is for Windows x64 applications, not 32-bit hosts. Place `independent_opm.dll` in the location required by your host application. Close the host before replacing an existing DLL. The DLL is not a standalone player and does not parse MDX/PDX files.

C ABI 1 declarations and an API overview are included. No import library is supplied; dynamic loading through LoadLibrary/GetProcAddress is available. Obtain the separate source package for rebuilding or direct core integration.

License: 0BSD; see LICENSE.txt. The supplied DLL bytes are unchanged. PE architecture, version string and 17 exports were inspected statically. This packaging task did not execute the DLL on Windows or verify a reproducible build against source. Hardware bit/cycle accuracy is not guaranteed.
