# NanoDrive 8 / NDSIF

NanoDrive 8でNDSIFモードを選び、Windowsに表示されたCOMポートを確認します。他のシリアル端末は閉じてください。IndependentMDXの「出力」行で該当COMポートとPCM出力先を選び、出力を **NDSIF (NanoDrive 8)** にします。COM一覧は開くたびに更新します。設定変更は既存の設定ファイルに保存されます。

| PCM出力先 | FM | PCM |
| --- | --- | --- |
| PCM: PC | 本体のYM2151 | PC側の既存ステレオミックス。FM/PCM遅延で調整可能 |
| PCM: NanoDrive 8 | 本体のYM2151。ADPCMバイト位置を基準にレジスタを書き込み | PCM8chをモノラル合成し、15,625 Hzで本体のMSM6258に連続転送 |

本体PCMではFM/PCM個別遅延を適用せず、再生位置には本体が消費したバイト数を使います。波形表示は実機からの音声入力ではありません。停止・シークでは本体をリセットします。接続時に必要な機能を問い合わせ、固定クロックと曲の要求が一致しない場合は再生を中止します。MDXのOPM入力は4 MHz、本体PCMは8 MHz / 512です。Windows標準のシリアル通信を使い、追加のNDSIF DLLは不要です。WAV保存はソフトウェア出力です。

## English

Select NDSIF mode on NanoDrive 8, close other serial clients, choose its Windows COM port and PCM destination in the Output row, then select **NDSIF (NanoDrive 8)**. PC PCM preserves the stereo mix and independent delay controls. Device PCM mixes eight channels to mono, encodes one continuous low-nibble-first ADPCM stream at 15,625 Hz, schedules song FM writes by byte position and follows device playback progress. Live MIDI register writes are immediate. Stop and seek reset the device. Unsupported clock or streaming capabilities produce an error. This GUI backend does not change command-line export formats.

## Interface and verification

Host implementation based on Fujix1's [NDSIF API](https://github.com/Fujix1/NanoDrive8/blob/main/NDSIF_API.md) and [NanoDrive 8 profile](https://github.com/Fujix1/NanoDrive8/blob/main/NDSIF_ND8.md); NanoDrive firmware is not included. Wire version 1, YM2151 instance 0, optional MSM6258 ADPCM profile. No device firmware update is performed. The serial mock exercises frame lengths, CRC, split responses, position continuity, seek/reset and END; it does not establish physical clock accuracy, sustained USB throughput, MIDI latency or audio quality. To verify on hardware, compare both PCM destinations with samples/DEMO.MDX, then seek, pause, resume, switch output during playback and check for a stuck note after stopping.

## ライセンス / License

NanoDrive 8の自作コードは、個別に例外が示されたものを除き、BSD 3-Clause License（Copyright (c) 2024-2026, Fujix）です。IndependentMDXのNDSIFホスト実装（src/ndsif_output.h、src/ndsif_wire.h）は公開通信仕様に基づいて独自に作成したもので、0BSDです。NanoDrive 8のソースコードやファームウェアは同梱していません。

Except where otherwise noted, NanoDrive 8 original code is licensed under BSD-3-Clause (Copyright (c) 2024-2026, Fujix). IndependentMDX's NDSIF host implementation (src/ndsif_output.h and src/ndsif_wire.h) was independently written against the public protocol specification and is licensed under 0BSD. No NanoDrive 8 source code or firmware is bundled.

[NanoDrive 8 LICENSE](https://github.com/Fujix1/NanoDrive8/blob/main/LICENSE)
