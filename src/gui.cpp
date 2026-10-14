// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

#define UNICODE
#define _UNICODE
#define NOMINMAX
#include "algorithm_layout.h"
#include "c86_output.h"
#include "carrier_notes.h"
#include "live_session.h"
#include "player.h"
#include "vgm.h"
#include "archive.h"
#include "vgz_export.h"
#include "voice_text.h"
#include "win_audio.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <commctrl.h>
#include <commdlg.h>
#include <fstream>
#include <gdiplus.h>
#include <map>
#include <mutex>
#include <shellapi.h>
#include <shlobj.h>
#include <ole2.h>
#include <shlwapi.h>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <uxtheme.h>
#include <windows.h>
#include <richedit.h>
#include <windowsx.h>

namespace {
HWND window, pathText, titleText, statusText, sampleEdit, formatBox, loopsEdit, gainEdit, pcmGainEdit,
    independentGainEdit, dacButton, measuredButton;
HWND positionLabel;
std::atomic<int> seekProgressPercent{-1};
HWND outputBox, midiBox, seekSlider, timeLabel, voiceWindow, voiceEdit, registerEdit, voiceChannel;
HWND voiceList, voiceListLabel, polyBox, viewBox, masterSlider, fmDelayEdit, pcmDelayEdit, aftertouchButton;
auto liveMidiPolyphonic = std::make_shared<std::atomic<bool>>(true);
auto liveGain = std::make_shared<std::atomic<double>>(1);
auto liveAftertouch = std::make_shared<std::atomic<bool>>(false);
auto livePcmGain = std::make_shared<std::atomic<double>>(1);
HWND splitter, carrierButton, speedSlider, speedValue, waveformButton, keyboardHint, keyboardHintTip;
bool languageUpdating = false;
bool carrierPitch = false, splitterBuffered = false;
auto liveLoops = std::make_shared<std::atomic<int>>(2);
auto liveSpeed = std::make_shared<std::atomic<int>>(100);
int splitPermille = 620, splitMinimum = 640, splitMaximum = 900;
bool fullscreen = false;
WINDOWPLACEMENT windowedPlacement{sizeof(WINDOWPLACEMENT)};
LONG_PTR windowedStyle = 0;
void layout();
void toggleFullscreen() {
    if (!fullscreen) {
        GetWindowPlacement(window, &windowedPlacement);
        windowedStyle = GetWindowLongPtrW(window, GWL_STYLE);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
        fullscreen = true;
        SetWindowLongPtrW(window, GWL_STYLE, windowedStyle & ~WS_OVERLAPPEDWINDOW);
        auto r = monitor.rcMonitor;
        SetWindowPos(window, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_NOZORDER | SWP_FRAMECHANGED);
    } else {
        fullscreen = false;
        SetWindowLongPtrW(window, GWL_STYLE, windowedStyle);
        SetWindowPlacement(window, &windowedPlacement);
        SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    layout();
}
bool pianoRoll = false;
HMIDIIN midiInput = nullptr;
int activeMidiDevice = -1;
std::vector<std::filesystem::path> pendingCommandFiles;
constexpr ULONG_PTR commandFileTag = 0x494d4458;
mdx::C86Output hardware;
std::atomic<int> selectedOutput{0};
auto retainedVoices = std::make_shared<mdx::VoiceState>();
auto midiQueue = std::make_shared<mdx::MidiQueue>();
std::atomic<double> seekRequest{-1}, songPosition{0}, songDuration{0};
std::atomic<bool> idleMidi{false};
bool draggingSeek = false;
std::filesystem::path lastPlayedMdx, lastPlayedFile, lastPlayedBank;
void settingsChanged();
// Trackbar page clicks are absolute; thumb dragging and keyboard control stay native.
LRESULT CALLBACK valueSliderSubclass(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    auto move = [&](int x) {
        RECT channel{};
        SendMessageW(h, TBM_GETCHANNELRECT, 0, LPARAM(&channel));
        int low=int(SendMessageW(h,TBM_GETRANGEMIN,0,0));
        int high=int(SendMessageW(h,TBM_GETRANGEMAX,0,0));
        double fraction=double(x-channel.left)/std::max(1,int(channel.right-channel.left-1));
        int value=low+int(std::lround(std::clamp(fraction,0.0,1.0)*(high-low)));
        SendMessageW(h,TBM_SETPOS,TRUE,value);
        SendMessageW(GetParent(h),WM_HSCROLL,MAKEWPARAM(TB_THUMBTRACK,value),LPARAM(h));
    };
    if(m==WM_LBUTTONDOWN) {
        RECT thumb{}; SendMessageW(h,TBM_GETTHUMBRECT,0,LPARAM(&thumb));
        POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        if(!PtInRect(&thumb,point)) {
            SetFocus(h); SetCapture(h); SetPropW(h,L"AbsoluteValue",HANDLE(1));
            move(point.x); return 0;
        }
    }
    if(GetPropW(h,L"AbsoluteValue")) {
        if(m==WM_MOUSEMOVE) { move(GET_X_LPARAM(l)); return 0; }
        if(m==WM_LBUTTONUP) {
            move(GET_X_LPARAM(l)); RemovePropW(h,L"AbsoluteValue"); ReleaseCapture(); return 0;
        }
        if(m==WM_CAPTURECHANGED || m==WM_CANCELMODE) RemovePropW(h,L"AbsoluteValue");
    }
    return DefSubclassProc(h,m,w,l);
}
LRESULT CALLBACK seekSubclass(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    auto seekAt = [&](int x) {
        RECT channel{};
        SendMessageW(h, TBM_GETCHANNELRECT, 0, LPARAM(&channel));
        int lo = channel.left, hi = channel.right - 1;
        int pos = hi > lo ? std::clamp((x - lo) * 10000 / (hi - lo), 0, 10000) : 0;
        SendMessageW(h, TBM_SETPOS, TRUE, pos);
    };
    if (m == WM_LBUTTONDOWN) {
        RECT thumb{};
        SendMessageW(h, TBM_GETTHUMBRECT, 0, LPARAM(&thumb));
        POINT point{short(LOWORD(l)), short(HIWORD(l))};
        if (!PtInRect(&thumb, point)) {
            SetFocus(h);
            SetCapture(h);
            SetPropW(h, L"AbsoluteSeek", HANDLE(1));
            draggingSeek = true;
            seekAt(point.x);
            return 0;
        }
    }
    if (GetPropW(h, L"AbsoluteSeek")) {
        if (m == WM_MOUSEMOVE) {
            seekAt(short(LOWORD(l)));
            return 0;
        }
        if (m == WM_LBUTTONUP) {
            seekAt(short(LOWORD(l)));
            RemovePropW(h, L"AbsoluteSeek");
            ReleaseCapture();
            draggingSeek = false;
            if (songDuration > 0)
                seekRequest = double(SendMessageW(h, TBM_GETPOS, 0, 0)) / 10000 * songDuration.load();
            return 0;
        }
        if (m == WM_CAPTURECHANGED || m == WM_CANCELMODE) {
            RemovePropW(h, L"AbsoluteSeek");
            draggingSeek = false;
        }
    }
    return DefSubclassProc(h, m, w, l);
}

void logPlaybackError(const std::filesystem::path &, const std::string &, const std::string &) noexcept;
bool collectErrorMdx=false;
HWND previousFocus=nullptr;
void startMidiIdle();
void closeMidi();
void updateAdvancedLanguage();
void showVoices(bool toggle = true);
void exportVoices();
void refreshVoiceWindow();
HWND infiniteButton, autoNextButton, keySlider, keyValue, keyModeBox;
auto liveKeyMode = std::make_shared<std::atomic<int>>(1);
auto liveKeyShift = std::make_shared<std::atomic<int>>(0);
std::atomic<uint64_t> playbackGeneration{0};
std::wstring playbackErrors;
void advanceFile();
auto monitorView = std::make_shared<mdx::Monitor>();
std::filesystem::path input, explicitPdx;
std::thread worker;
std::atomic<bool> stopFlag{false}, pauseFlag{false}, running{false};
std::atomic<double> playbackSeconds{0};
std::mutex messageMutex;
std::wstring message;
auto livePcmMuteMask = std::make_shared<std::atomic<uint32_t>>(0);
auto liveMuteMask = std::make_shared<std::atomic<uint32_t>>(0);
auto liveIndependentGain = std::make_shared<std::atomic<double>>(2.0);
auto liveMeasuredOutputTiming = std::make_shared<std::atomic<bool>>(true);
auto liveDacRightDelay = std::make_shared<std::atomic<bool>>(false);
std::atomic<bool> japanese{false};
const wchar_t *tr(const wchar_t *ja, const wchar_t *en) { return japanese ? ja : en; }
std::wstring wide(const std::string &text) {
    int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count);
    return result;
}
const std::pair<const wchar_t *, const wchar_t *> translations[] = {
    {L"value=", L"参照値="},
    {L"expected=", L"対応範囲="},
    {L"defined voice IDs", L"定義済み音色番号"},
    {L"PCM is silent while FM continues", L"PCMを消音してFMの再生を続行します"},
    {L"Numeric field is outside its allowed range", L"数値が入力可能な範囲外です"},
    {L"WAV output must not overwrite the input file", L"入力ファイルをWAVで上書きすることはできません"},
    {L"Cannot create audio event", L"音声イベントを作成できません"},
    {L"Windows audio device could not be opened", L"Windowsの音声デバイスを開けません"},
    {L"Audio buffer preparation failed", L"音声バッファーの準備に失敗しました"},
    {L"Audio buffer submission failed", L"音声バッファーの送信に失敗しました"},
    {L"Truncated file", L"ファイルが途中で切れています"},
    {L"Cannot open: ", L"ファイルを開けません: "},
    {L"File size must be 1..128 MiB", L"ファイルサイズは1バイト以上128 MiB以下にしてください"},
    {L"File read failed", L"ファイルの読み込みに失敗しました"},
    {L"Not an MDX: title terminator missing", L"MDXのタイトル終端がありません"},
    {L"PDX filename terminator missing", L"PDXファイル名の終端がありません"},
    {L"Unsupported MDX track table (expected 9 or 16 compact tracks)",
     L"未対応のMDXトラック表です（9または16トラックが必要です）"},
    {L"Invalid MDX track offset", L"MDXトラックの位置が不正です"},
    {L"Invalid voice offset", L"音色データの位置が不正です"},
    {L"PDX header is shorter than 96 entries", L"PDXヘッダーが96項目に満たない長さです"},
    {L"Invalid PDX sample offset/length (compressed PDX is unsupported)",
     L"PDXのサンプル位置／長さが不正です（圧縮PDXは未対応）"},
    {L"PDX sample/bank does not exist", L"PDXのサンプル／バンクが存在しません"},
    {L"PCM format must be F0..F6", L"PCM形式はF0〜F6を指定してください"},
    {L"P16 sample has odd byte length", L"P16サンプルのバイト数が奇数です"},
    {L"Ambiguous PDX filename; specify --pdx / Choose PDX",
     L"PDX候補が複数あります。「PDXを指定」で選択してください"},
    {L"Multiple numbered PDX copies found; specify --pdx / Choose PDX",
     L"番号付きPDXが複数あります。「PDXを指定」で選択してください"},
    {L"Required PDX not found: ", L"必要なPDXが見つかりません: "},
    {L". Place it beside the MDX or specify --pdx.", L"。MDXの隣に置くか「PDXを指定」で選択してください。"},
    {L"Invalid sample playback options", L"サンプル再生設定が不正です"},
    {L"Cannot create WAV output", L"WAV出力ファイルを作成できません"},
    {L"WAV exceeds RIFF size limit", L"WAVがRIFF形式のサイズ上限を超えています"},
    {L"WAV write failed", L"WAVの書き込みに失敗しました"},
    {L"WAV finalization failed", L"WAVの保存終了処理に失敗しました"},
    {L"Invalid playback options", L"再生設定が不正です"},
    {L"Software LFO uses an independent approximation; not verified against MXDRV integer behavior.",
     L"ソフトLFOは独自近似です。MXDRVの整数演算との一致は未検証です。"},
    {L"F5/F6 linear PCM extension: interpreted as big-endian P16 / signed P8 at 15625 Hz.",
     L"F5/F6拡張PCMは15625 HzのビッグエンディアンP16／符号付きP8として再生します。"},
    {L"Embedded fade speed is approximated; use the player's fade option for a defined duration.",
     L"埋め込みフェード速度は近似です。正確な時間はプレーヤーのフェード設定を使用してください。"},
    {L"All active MDX tracks are waiting for sync; playback cannot continue",
     L"全MDXトラックが同期待ちになったため、再生を続行できません"},
    {L"Unexpected end of MDX command stream", L"MDX命令が途中で終了しています"},
    {L"MDX jump outside sequence data", L"MDXのジャンプ先がシーケンス範囲外です"},
    {L"Referenced FM voice is missing", L"指定されたFM音色がありません"},
    {L"PCM note without a PDX sample bank", L"PCM音符に必要なPDXがありません"},
    {L"Decoded PCM cache exceeds 128 MiB", L"復号PCMキャッシュが128 MiBを超えています"},
    {L"Unsupported software LFO waveform", L"未対応のソフトLFO波形です"},
    {L"Unsupported extension inside repeat escape", L"リピート脱出内に未対応の拡張命令があります"},
    {L"Unterminated or malformed repeat escape", L"リピート脱出の終端がないか不正です"},
    {L"Repeat escape scan limit exceeded", L"リピート脱出の探索上限を超えました"},
    {L"Unsupported coarse volume outside v0..v15", L"粗音量はv0〜v15のみ対応します"},
    {L"Invalid q value", L"q値が不正です"},
    {L"Invalid repeat count or excessive repeat nesting", L"リピート回数が不正か、入れ子が深すぎます"},
    {L"Repeat end without repeat start", L"リピート開始なしに終端があります"},
    {L"Repeat end requires a backward offset", L"リピート終端には後方オフセットが必要です"},
    {L"Repeat escape without repeat start", L"リピート開始なしに脱出があります"},
    {L"Repeat escape requires a forward offset", L"リピート脱出には前方オフセットが必要です"},
    {L"End-loop pointer must be negative", L"終端ループのポインターは負数である必要があります"},
    {L"Sync channel is outside the track table", L"同期先チャンネルがトラック表の範囲外です"},
    {L"Unsupported PCM frequency/format (F0..F6 supported)", L"未対応のPCM周波数／形式です（F0〜F6に対応）"},
    {L"Invalid hardware LFO switch", L"ハードLFO切り替え値が不正です"},
    {L"PCM8 declaration requires a 16-track header", L"PCM8宣言には16トラックのヘッダーが必要です"},
    {L"Unsupported E7 extension ", L"未対応のE7拡張命令: "},
    {L"Unsupported E6 extension ", L"未対応のE6拡張命令: "},
    {L"Unsupported MDX opcode ", L"未対応のMDX命令: "},
    {L"Too many zero-time commands (possible cyclic/corrupt sequence)",
     L"時間を進めない命令が多すぎます（循環または破損の可能性）"},
    {L" (channel ", L"（チャンネル "},
    {L", file offset 0x", L"、ファイル位置 0x"},
    {L"ERROR: ", L"エラー: "},
    {L"Exporting WAV...", L"WAVを書き出しています…"},
    {L"Exporting WAV", L"WAV保存中"},
    {L"WAV saved", L"WAV保存完了"},
    {L"Playing", L"再生中"},
    {L"Stopped", L"停止しました"},
    {L"Finished", L"再生終了"},
    {L"Ready", L"準備完了"},
};
std::wstring translated(std::wstring text, bool toJapanese) {
    for (const auto &pair : translations) {
        std::wstring from = toJapanese ? pair.first : pair.second;
        const wchar_t *to = toJapanese ? pair.second : pair.first;
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::wstring::npos) {
            text.replace(pos, from.size(), to);
            pos += wcslen(to);
        }
    }
    return text;
}
std::wstring diagnostic(const std::string &text) {
    return japanese ? translated(wide(text), true) : wide(text);
}
void status(std::wstring text) {
    std::lock_guard<std::mutex> lock(messageMutex);
    message = translated(std::move(text), false);
}
void stop() {
    ++playbackGeneration;
    stopFlag = true;
    if (worker.joinable())
        worker.join();
    running = false;
    idleMidi = false;
    midiQueue->clear();
    try {
        hardware.reset();
    } catch (const std::exception &e) {
        status(wide(e.what()));
    }
    pauseFlag = false;
    std::lock_guard<std::mutex> lock(monitorView->mutex);
    monitorView->frame = mdx::MonitorFrame{};
}
std::filesystem::path choose(bool save, bool pdxOnly = false) {
    wchar_t name[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFile = name;
    dialog.nMaxFile = 32768;
    dialog.lpstrFilter = save      ? tr(L"WAV音声\0*.wav\0\0", L"WAV audio\0*.wav\0\0")
                         : pdxOnly ? tr(L"PDXサンプル\0*.pdx\0\0", L"PDX samples\0*.pdx\0\0")
                                   : tr(L"MDX / PDX / VGM / VGZ / MRL / MRZ / ZIP / LZH\0*.mdx;*.pdx;*.vgm;*.vgz;*.mrl;*.mrz;*.zip;*.lzh;*.lha\0すべてのファイル\0*.*\0\0",
                                        L"MDX / PDX / VGM / VGZ / MRL / MRZ / ZIP / LZH\0*.mdx;*.pdx;*.vgm;*.vgz;*.mrl;*.mrz;*.zip;*.lzh;*.lha\0All files\0*.*\0\0");
    dialog.Flags =
        OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    dialog.lpstrDefExt = save ? L"wav" : nullptr;
    if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))
        return name;
    return {};
}
bool isPdx(const std::filesystem::path &file) { return _wcsicmp(file.extension().c_str(), L".pdx") == 0; }
void navigate(std::filesystem::path target, bool remember);
bool load(const std::filesystem::path &file) {
    if(mdx::isArchive(file)) { navigate(file,true); return false; }
    if (file.empty())
        return false;
    stop();
    try {
        std::wstring description;
        if (isPdx(file)) {
            mdx::Pdx bank(mdx::readFile(file));
            description =
                L"PDX: " + std::to_wstring(bank.entries.size()) +
                tr(L" スロット。下の番号を選んで試聴できます。", L" slots. Select a sample number below.");
        } else if(mdx::isVgm(file)) {
            auto song = mdx::loadVgmSong(file);
            description=wide(song.title)+(song.registerLog?L"\r\nMRL / YM2151 / ":L"\r\nVGM / YM2151 / ")+std::to_wstring(song.clock)+L" Hz";
            std::lock_guard<std::mutex> lock(retainedVoices->mutex);
            retainedVoices->defined.fill(false);retainedVoices->initialDefined.fill(false);retainedVoices->valid.fill(false);retainedVoices->used.fill(false);
            ++retainedVoices->catalogRevision;
        } else {
            mdx::Song song(mdx::readFile(file));
            {
                std::lock_guard<std::mutex> lock(retainedVoices->mutex);
                retainedVoices->beginVoiceSession(song.voices, song.hasVoice);
                for (auto &request : retainedVoices->requestedVoice)
                    request = -1;
            }
            description = wide(song.title) + L"\r\n" + std::to_wstring(song.tracks.size()) +
                          tr(L" トラック / PDX: ", L" tracks / PDX: ") + wide(mdx::utf8(song.pdxName));
        }
        input = file;
        explicitPdx.clear();
        SetWindowTextW(pathText, file.c_str());
        SetWindowTextW(titleText, description.c_str());
        EnableWindow(GetDlgItem(window,41), !mdx::isVgm(file));
        EnableWindow(sampleEdit, isPdx(file));
        EnableWindow(formatBox, isPdx(file));
        status(tr(L"準備完了", L"Ready"));
        playbackSeconds = 0;
        return true;
    } catch (const std::exception &e) {
        logPlaybackError(file,"",e.what());
        MessageBoxW(window, diagnostic(e.what()).c_str(), L"IndependentMDX", MB_OK | MB_ICONERROR);
        return false;
    }
}
int integer(HWND control, int low, int high) {
    wchar_t buffer[64];
    GetWindowTextW(control, buffer, 64);
    wchar_t *end = nullptr;
    long value = wcstol(buffer, &end, 10);
    if (end == buffer || *end || value < low || value > high)
        throw std::runtime_error("Numeric field is outside its allowed range");
    return static_cast<int>(value);
}
// UTF-8 CSV, one physical line per error; append across playback sessions.
void logPlaybackError(const std::filesystem::path &file, const std::string &title,
                      const std::string &error) noexcept {
    if(error.find("Required PDX not found:")!=std::string::npos ||
       error.find("PCM note without a PDX sample bank")!=std::string::npos) return;
    try {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);
        wchar_t path[32768];
        GetModuleFileNameW(nullptr,path,32768);
        auto folder=std::filesystem::path(path).parent_path();
        const bool dataError=error.find("file offset")!=std::string::npos ||
            error.find("MDX")!=std::string::npos || error.find("voice")!=std::string::npos;
        if(collectErrorMdx && dataError && !isPdx(file) && !mdx::isVgm(file)) {
          try {
            std::error_code ec;
            auto destination=folder/L"ErrorMDX";
            std::filesystem::create_directories(destination,ec);
            auto bytes=mdx::readFile(file);
            uint64_t hash=14695981039346656037ULL;
            for(auto b:bytes) { hash^=b; hash*=1099511628211ULL; }
            std::wostringstream name;
            name<<file.stem().wstring()<<L"_"<<std::hex<<hash<<L".MDX";
            std::filesystem::copy_file(file,destination/name.str(),std::filesystem::copy_options::skip_existing,ec);
          } catch (...) { }
        }
        std::ofstream log(folder/L"IndependentMDX_errors.csv",
                          std::ios::binary|std::ios::app);
        auto field=[](std::string text) {
            std::string escaped="\"";
            for(char c:text) {
                if(c=='\r' || c=='\n') escaped+=' ';
                else { if(c=='"') escaped+='"'; escaped+=c; }
            }
            return escaped+'"';
        };
        log<<field(std::filesystem::absolute(file).u8string())<<','<<field(title)<<','<<field(error)<<"\r\n";
    } catch (...) { }
}
void start(bool exportWav, double resumePosition = 0, bool resumePaused = false, bool continuing = false) {
    if (input.empty()) {
        load(choose(false));
        if (input.empty())
            return;
    }
    stop();
    {
        std::lock_guard<std::mutex> lock(messageMutex);
        playbackErrors.clear();
    }
    auto logTitle=std::make_shared<std::string>();
    const auto logFile=input;
    try {
        mdx::Options options;
        options.voiceState = retainedVoices;
        options.fastSeek = !exportWav;
        seekProgressPercent = -1;
        options.seekProgress = [](double current, double target) {
            seekProgressPercent = target>0 && current<target ? int(current/target*100) : -1;
        };
        options.fmBackend = selectedOutput == 1 ? 1 : 0;
        options.keyShift = liveKeyShift->load();
        options.keyMode = liveKeyMode->load();
        options.diagnostic = [logFile, logTitle](const std::string &note) {
            if (note.rfind("ERROR:", 0) == 0) {
                logPlaybackError(logFile,*logTitle,note);
                std::lock_guard<std::mutex> lock(messageMutex);
                playbackErrors += wide(note) + L"\r\n";
            }
        };
        options.fmMuteMask = liveMuteMask->load();
        options.pcmMuteMask = livePcmMuteMask->load();
        options.measuredOutputTiming = liveMeasuredOutputTiming->load();
        options.dacRightDelay = liveDacRightDelay->load();
        options.independentGain = integer(independentGainEdit, 0, 800) / 100.0;
        if (!exportWav) {
            options.externalFm = hardware.active();
            if (options.externalFm)
                options.registerOutput = [](uint64_t t, int a, int v) {
                    int ch = a == 8 ? (v & 7) : (a >= 0x20 ? (a & 7) : -1);
                    if (ch >= 0 && (retainedVoices->midiFmOverride.load() & (1u << ch)))
                        return;
                    hardware.scheduled(t, a, v, stopFlag);
                };
            options.monitor = monitorView;
            options.liveKeyShift = liveKeyShift;
            options.liveSpeed = liveSpeed;
            options.liveKeyMode = liveKeyMode;
            options.liveMeasuredOutputTiming = liveMeasuredOutputTiming;
            options.liveDacRightDelay = liveDacRightDelay;
            options.liveMuteMask = liveMuteMask;
            options.livePcmMuteMask = livePcmMuteMask;
            options.liveIndependentGain = liveIndependentGain;
            options.liveGain = liveGain;
            options.livePcmGain = livePcmGain;
            options.liveMidiPolyphonic = liveMidiPolyphonic;
            options.liveAftertouch = liveAftertouch;
            options.fmDelayMs = integer(fmDelayEdit, 0, 2000);
            options.pcmDelayMs = integer(pcmDelayEdit, 0, 2000);
            hardware.setDelay(options.fmDelayMs);
        }
        options.loops =
            !exportWav && SendMessageW(infiniteButton, BM_GETCHECK, 0, 0) == BST_CHECKED ? 0 : integer(loopsEdit, 1, 1000);
        if(!exportWav) { liveLoops->store(options.loops); options.liveLoops=liveLoops; }
        options.maxSeconds = 0; // GUI transport covers the complete song; loops still bound finite playback.
        options.gain = integer(gainEdit, 0, 400) / 100.0;
        options.pcmGain = integer(pcmGainEdit, 0, 400) / 100.0;
        int sample = integer(sampleEdit, 0, 24575);
        int format = static_cast<int>(SendMessageW(formatBox, CB_GETCURSEL, 0, 0));
        auto output = exportWav ? choose(true) : std::filesystem::path();
        if (exportWav && output.empty())
            return;
        if (!output.empty() &&
            (std::filesystem::weakly_canonical(input) == std::filesystem::weakly_canonical(output) ||
             (!explicitPdx.empty() &&
              std::filesystem::weakly_canonical(explicitPdx) == std::filesystem::weakly_canonical(output))))
            throw std::runtime_error("WAV output must not overwrite the input file");
        auto file = input, bankPath = explicitPdx;
        if (!exportWav) {
            lastPlayedFile = file;
            lastPlayedBank = bankPath;
            settingsChanged();
        }
        if (!isPdx(file) && !mdx::isVgm(file))
            lastPlayedMdx = file;
        songPosition = 0;
        songDuration = 0;
        seekRequest = -1;
        stopFlag = false;
        pauseFlag = false;
        running = true;
        playbackSeconds = 0;
        status(exportWav ? tr(L"WAVを書き出しています…", L"Exporting WAV...") : tr(L"再生中", L"Playing"));
        const uint64_t generation = playbackGeneration.load();
        worker = std::thread([file, bankPath, options, sample, format, output, generation, logTitle, resumePosition, resumePaused, continuing] {
            try {
                std::unique_ptr<mdx::Source> source;
                mdx::Player *music = nullptr;
                if (isPdx(file)) {
                    mdx::Pdx bank(mdx::readFile(file));
                    source = std::make_unique<mdx::SamplePlayer>(bank, sample, format, options);
                } else if(mdx::isVgm(file)) {
                    auto song = mdx::loadVgmSong(file,bankPath);
                    *logTitle=song.title;
                    source=std::make_unique<mdx::VgmPlayer>(std::move(song),options);
                } else {
                    mdx::Song song(mdx::readFile(file));
                    *logTitle=song.title;
                    if (output.empty() && !continuing) {
                        std::lock_guard<std::mutex> lock(retainedVoices->mutex);
                        retainedVoices->beginVoiceSession(song.voices, song.hasVoice);
                    }
                    std::shared_ptr<mdx::Pdx> bank;
                    try {
                        bank = mdx::locatePdx(song, file, bankPath);
                    } catch (const std::exception &e) {
                        options.diagnostic(std::string("ERROR: ") + e.what());
                    }
                    if (bank)
                        status((output.empty() ? tr(L"再生中", L"Playing")
                                               : tr(L"WAV保存中", L"Exporting WAV")) +
                               std::wstring(L"\r\nPDX: ") + bank->sourcePath.wstring());
                    auto player = std::make_unique<mdx::Player>(std::move(song), bank, options);
                    music = player.get();
                    source = std::move(player);
                }
                if (output.empty()) {
                    mdx::Timeline timeline;
                    if (music)
                        timeline = music->analyze([] { return stopFlag.load(); });
                    if (timeline.truncated)
                        options.diagnostic("ERROR: Timeline analysis reached 3600 seconds; displayed "
                                           "duration is a lower bound");
                    if (stopFlag.load()) {
                        running = false;
                        return;
                    }
                    mdx::LiveSession session(
                        source.get(), music, timeline, retainedVoices, midiQueue, options, pauseFlag,
                        seekRequest, songPosition, songDuration, [] { return stopFlag.load(); },
                        [] { hardware.reset(); }, [](int a, int v) { hardware.write(a, v); });
                    if(resumePosition>0) {
                        seekRequest=resumePosition;
                        session.service();
                    }
                    if(resumePaused) { pauseFlag=true; session.service(); }
                    mdx::playWindows(
                        session, options.sampleRate, [] { return stopFlag.load(); }, [] { return false; },
                        [](double) { playbackSeconds = songPosition.load(); });
                } else {
                    mdx::writeWav(*source, output, options.sampleRate, [&] {
                        playbackSeconds = source->seconds();
                        return stopFlag.load();
                    });
                }
                std::wstring result = stopFlag         ? tr(L"停止しました", L"Stopped")
                                      : output.empty() ? tr(L"再生終了", L"Finished")
                                                       : tr(L"WAV保存完了", L"WAV saved");
                if (music)
                    for (const auto &note : music->warnings())
                        if (note.rfind("ERROR:", 0) != 0)
                            result += L"\r\n" + wide(note);
                status(result);
                if (!stopFlag && output.empty())
                    PostMessageW(window, WM_APP + 1, static_cast<WPARAM>(generation), 0);
            } catch (const std::exception &e) {
                logPlaybackError(file,*logTitle,e.what());
                status(L"ERROR: " + wide(e.what()));
            }
            try {
                hardware.reset();
            } catch (const std::exception &e) {
                logPlaybackError(file,*logTitle,e.what());
                status(L"ERROR: " + wide(e.what()));
            }
            running = false;
        });
    } catch (const std::exception &e) {
        logPlaybackError(logFile,*logTitle,e.what());
        MessageBoxW(window, diagnostic(e.what()).c_str(), tr(L"プレーヤー", L"Player"), MB_OK | MB_ICONERROR);
    }
}
HWND control(const wchar_t *cls, const wchar_t *text, int x, int y, int w, int h, int id, DWORD style = 0) {
    HWND handle = CreateWindowExW(wcscmp(cls, L"EDIT") == 0 ? WS_EX_CLIENTEDGE : 0, cls, text,
                                  WS_CHILD | WS_VISIBLE | style, x, y, w, h, window,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SendMessageW(handle, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    return handle;
}
HFONT uiFont;
int scale(int value) {
    HDC dc = GetDC(window);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(window, dc);
    return MulDiv(value, dpi, 96);
}
#include "ui_extras.h"
enum class ControlKind { button, label, combo, edit };
struct Item {
    ControlKind kind;
    HWND handle;
    const wchar_t *ja;
    const wchar_t *en;
    int row, minimum, height;
    bool stretch;
};
std::vector<Item> items;
HWND languageBox;
HWND item(const wchar_t *cls, const wchar_t *ja, const wchar_t *en, int row, int width, int height, int id,
          DWORD style = 0, bool stretch = false) {
    HWND h = control(cls, tr(ja, en), 0, 0, 1, 1, id, style);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
    const auto kind = _wcsicmp(cls, L"BUTTON") == 0     ? ControlKind::button
                      : _wcsicmp(cls, L"STATIC") == 0   ? ControlKind::label
                      : _wcsicmp(cls, L"COMBOBOX") == 0 ? ControlKind::combo
                                                        : ControlKind::edit;
    if (kind == ControlKind::button && !(style & BS_AUTOCHECKBOX))
        SetWindowLongPtrW(h, GWL_STYLE, GetWindowLongPtrW(h, GWL_STYLE) | BS_OWNERDRAW);
    SetWindowTheme(h, L"", L"");
    items.push_back({kind, h, ja, en, row, width, height, stretch});
    return h;
}

// One bilingual grid: geometry is based on both languages, never the selected one.
void organizeControls() {
    auto named = [&](const wchar_t *ja) {
        for (auto &it : items) if (wcscmp(it.ja, ja) == 0) return it.handle;
        return HWND(nullptr);
    };
    auto caption = [&](HWND h, const wchar_t *ja, const wchar_t *en) {
        for (auto &it : items) if (it.handle == h) { it.ja=ja; it.en=en; }
        return h;
    };
    auto addLabel = [&](const wchar_t *ja,const wchar_t *en) {
        return item(L"STATIC",ja,en,0,0,24,0);
    };
    HWND playbackLabel=addLabel(L"再生",L"Playback");
    positionLabel=addLabel(L"再生位置",L"Position");
    HWND timingLabel=addLabel(L"出力タイミング",L"Output timing");
    HWND instrumentsLabel=addLabel(L"音色",L"Instruments");
    HWND volumeLabel=caption(named(L"全体音量 % (0–400)"),L"音量 / 全体 %",L"Volume / Master %");
    HWND fmLabel=caption(named(L"FM音量 % (0–800)"),L"FM % (0–800)",L"FM % (0–800)");
    HWND pcmLabel=caption(named(L"PCM音量 % (0–400)"),L"PCM % (0–400)",L"PCM % (0–400)");
    HWND modeLabel=caption(named(L"停止／一時停止中のMIDI"),L"発音モード",L"Voice mode");
    HWND fmDelayLabel=caption(named(L"C86CTL FM遅延 ms"),L"FM ms (0–2000)",L"FM ms (0–2000)");
    HWND pcmDelayLabel=caption(named(L"PCM遅延 ms"),L"PCM ms (0–2000)",L"PCM ms (0–2000)");
    caption(dacButton,L"YM3012: 右 +7.00 μs",L"YM3012: R +7.00 us");
    caption(measuredButton,L"実測タイミング: OPM DLL / ALG5・7",L"Measured timing: OPM DLL / ALG5,7");
    HWND delayInfo=caption(named(L"遅延: 0–2000 ms、次回再生時に反映"),
        L"遅延は次回再生から",L"Delay applies on next play");
    std::vector<std::vector<HWND>> rows{
        {},{pathText},{titleText},
        {playbackLabel,GetDlgItem(window,3),GetDlgItem(window,4),GetDlgItem(window,5),GetDlgItem(window,6)},
        {positionLabel,seekSlider,timeLabel},
        {named(L"周回数 (1–1000)"),loopsEdit,infiniteButton,autoNextButton},
        {named(L"PDX試聴番号"),sampleEdit,formatBox},
        {named(L"再生速度 %"),speedSlider,speedValue},
        {named(L"キー（半音）"),keySlider,keyValue},
        {named(L"キー変更対象"),keyModeBox},
        {volumeLabel,masterSlider,gainEdit},
        {fmLabel,independentGainEdit,pcmLabel,pcmGainEdit},
        {named(L"出力"),outputBox},
        {fmDelayLabel,fmDelayEdit,pcmDelayLabel,pcmDelayEdit,delayInfo},
        {},
        {timingLabel,measuredButton},
        {dacButton},
        {named(L"MIDI入力"),midiBox},
        {modeLabel,polyBox,aftertouchButton},
        {instrumentsLabel,GetDlgItem(window,40),GetDlgItem(window,41)},
        {statusText},
        {named(L"表示"),viewBox,carrierButton,waveformButton,keyboardHint}
    };
    for (auto &it:items) if (it.row==0 && it.handle!=playbackLabel && it.handle!=positionLabel &&
        it.handle!=timingLabel && it.handle!=instrumentsLabel) rows[0].push_back(it.handle);
    std::vector<Item> sorted;
    for (int row=0;row<int(rows.size());++row) for (HWND h:rows[row])
        for (auto it:items) if (it.handle==h) {
            it.row=row;
            if(row>=3) {
                it.minimum=it.kind==ControlKind::edit?62:0;
                if(h==seekSlider || h==speedSlider || h==keySlider || h==masterSlider) it.minimum=240;
                if(h==timeLabel) it.minimum=128;
                if(it.kind==ControlKind::combo) it.minimum=120;
            }
            sorted.push_back(it); break;
        }
    if(sorted.size()!=items.size()) throw std::runtime_error("Incomplete control layout");
    items=std::move(sorted);
    for(const auto &it:items)
        SetWindowPos(it.handle,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
}
void layout() {
    if (layoutBusy || !browserList)
        return;
    layoutBusy = true;
    HDC dc = GetDC(window);
    auto old = SelectObject(dc, uiFont);
    TEXTMETRICW fontMetrics{};
    GetTextMetricsW(dc, &fontMetrics);
    int widths[22]{}, heights[22]{};
    int labelColumn=scale(150);
    int controlHeight = fontMetrics.tmHeight + scale(8);
    if (languageBox) {
        SendMessageW(languageBox, CB_SETITEMHEIGHT, WPARAM(-1), fontMetrics.tmHeight + scale(2));
        RECT box{};
        GetWindowRect(languageBox, &box);
        controlHeight = std::max(controlHeight, int(box.bottom - box.top));
    }
    std::vector<int> measured;
    for (const auto &it : items) {
        int width = scale(it.minimum);
        if (it.kind == ControlKind::button || it.kind == ControlKind::label) {
            for (const auto *text : {it.ja, it.en}) {
                SIZE extent{};
                GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &extent);
                width=std::max(width,int(extent.cx)+scale(it.kind==ControlKind::label?20:24));
            }
        }
        if(it.handle==keyboardHint) width=scale(80); // Remaining space, ellipsis if narrow.
        if (it.kind == ControlKind::combo) {
            std::vector<const wchar_t*> alternatives;
            if(it.handle==keyModeBox) alternatives={L"FM only",L"FMのみ",L"FM + PCM (playback rate)",L"FM＋PCM（再生速度変更）",L"FM + PCM (preserve duration)",L"FM＋PCM（長さを保つピッチシフト）"};
            if(it.handle==formatBox) alternatives={L"F5 P16 ビッグエンディアン 15.625 kHz",L"F5 P16 big endian 15.625 kHz",L"F6 P8 符号付き 15.625 kHz",L"F6 P8 signed 15.625 kHz"};
            if(it.handle==viewBox) alternatives={L"チャンネル別鍵盤",L"Channel keyboards",L"縦ピアノロール",L"Vertical piano roll"};
            if(it.handle==midiBox) alternatives={L"MIDI入力なし",L"No MIDI input"};
            for(auto text:alternatives) { SIZE extent{};GetTextExtentPoint32W(dc,text,int(wcslen(text)),&extent);width=std::max(width,int(extent.cx)+scale(40)); }
            int count = static_cast<int>(SendMessageW(it.handle, CB_GETCOUNT, 0, 0));
            for (int i = 0; i < count; ++i) {
                int length = static_cast<int>(SendMessageW(it.handle, CB_GETLBTEXTLEN, i, 0));
                if (length < 0)
                    continue;
                std::vector<wchar_t> text(static_cast<size_t>(length) + 1);
                SendMessageW(it.handle, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(text.data()));
                SIZE extent{};
                GetTextExtentPoint32W(dc, text.data(), length, &extent);
                width = std::max(width, static_cast<int>(extent.cx) + scale(40));
            }
            SendMessageW(it.handle, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1),
                         fontMetrics.tmHeight + scale(2));
            SendMessageW(it.handle, CB_SETITEMHEIGHT, 0, fontMetrics.tmHeight + scale(2));
        }
        if(it.handle==positionLabel) for(auto text:{L"再生位置（検索100%）",L"Position (seeking 100%)"}) {
            SIZE extent{};GetTextExtentPoint32W(dc,text,int(wcslen(text)),&extent);
            width=std::max(width,int(extent.cx)+scale(8));
        }
        bool first=measured.empty() || items[measured.size()-1].row!=it.row;
        if(first && it.row>=3 && it.row<20 && it.row!=14 && it.kind==ControlKind::label) labelColumn=std::max(labelColumn,width);
        measured.push_back(width);
        widths[it.row] += width + scale(6);
        int h = (it.handle == titleText ? 2*fontMetrics.tmHeight+scale(10) : it.handle == statusText ? std::max(controlHeight, scale(36)) : controlHeight);
        heights[it.row] = std::max(heights[it.row], h);
    }
    SelectObject(dc, old);
    ReleaseDC(window, dc);
    int content = scale(640);
    for (int row=0;row<21;++row) {
        int width=widths[row];
        if(row>=3 && row<20 && row!=14 && row!=16) {
            auto first=std::find_if(items.begin(),items.end(),[&](const Item &it){return it.row==row;});
            if(first!=items.end()) width+=labelColumn-measured[size_t(first-items.begin())];
        } else if(row==14 || row==16) width+=labelColumn+scale(6);
        content=std::max(content,width-scale(6));
    }
    RECT client;
    GetClientRect(window, &client);
    int margin = scale(8), bar = scale(6), minKeyboard = std::max(scale(240),widths[21]-scale(6));
    splitMinimum = content;
    int canvasWidth = std::max(int(client.right), content + minKeyboard + 3 * margin + bar);
    splitMaximum = canvasWidth - minKeyboard - 3 * margin - bar;
    content = std::clamp(canvasWidth * splitPermille / 1000, splitMinimum, splitMaximum);
    int keyboardWidth = canvasWidth - content - 3 * margin - bar;
    int totalHeight = scale(120) + 3 * (controlHeight + scale(4)) + 2 * margin;
    for (int row=0;row<21;++row)
        if (heights[row]) totalHeight+=heights[row]+scale(3)+((row==3 || row==10 || row==12 || row==17)?scale(5):0);
    virtualWidth = canvasWidth;
    virtualHeight = std::max(totalHeight, int(client.bottom));
    SCROLLINFO si{
        sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS, 0, virtualWidth - 1, UINT(client.right), scrollX, 0};
    SetScrollInfo(window, SB_HORZ, &si, TRUE);
    si.nMax = virtualHeight - 1;
    si.nPage = client.bottom;
    si.nPos = scrollY;
    SetScrollInfo(window, SB_VERT, &si, TRUE);
    scrollX = GetScrollPos(window, SB_HORZ);
    scrollY = GetScrollPos(window, SB_VERT);
    struct Placement {
        HWND h;
        int x, y, width, height;
    };
    std::vector<Placement> placements;
    auto place = [&](HWND h, int x, int y, int width, int height, BOOL) {
        placements.push_back({h, x, y, width, height});
    };
    int y = margin - scrollY;
    for (int row = 0; row < 21; ++row) {
        if(row==3 || row==10 || row==12 || row==17) y+=scale(5);
        int x = margin - scrollX;
        const int startX=x;
        if(row==14 || row==16) x+=labelColumn+scale(6);
        bool first=true;
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].row == row) {
                auto &it = items[i];
                int width = it.stretch ? content : measured[i];
                if(row>=3 && row<20 && row!=14 && row!=16 && first) width=labelColumn;
                if(it.handle==keySlider || it.handle==speedSlider || it.handle==masterSlider)
                    width=content-labelColumn-scale(6)-scale(62)-scale(6);
                if(it.handle==seekSlider) width=content-labelColumn-scale(6)-measured[i+1]-scale(6);
                if(row==11 && !first && it.kind==ControlKind::label) {
                    width=scale(142);
                    x=startX+content-scale(62)-scale(6)-width;
                }
                first=false;
                int height = it.kind == ControlKind::combo ? scale(230) : heights[row];
                place(it.handle, x, y, width, height, TRUE);
                x += width + scale(6);
            }
        if (heights[row])
            y += heights[row] + scale(3);
    }
    int x = margin - scrollX, h = controlHeight, gap = scale(4);
    place(backButton, x, y, scale(70), h, TRUE);
    place(upButton, x + scale(78), y, scale(70), h, TRUE);
    place(folderEdit, x + scale(156), y, content - scale(156), h, TRUE);
    y += h + gap;
    place(mdxFilter, x, y, scale(60), h, TRUE);
    place(pdxFilter, x + scale(65), y, scale(60), h, TRUE);
    place(vgmFilter, x + scale(130), y, scale(100), h, TRUE);
    place(allFilter,x+scale(235),y,scale(85),h,TRUE);
    y += h + gap;
    place(bookmarkAdd, x + scale(0), y, scale(125), h, TRUE);
    place(bookmarkRemove, x + scale(133), y, scale(90), h, TRUE);
    place(bookmarkBox, x + scale(231), y, std::max(scale(40),content - scale(231)), scale(240), TRUE);
    SendMessageW(bookmarkBox, CB_SETDROPPEDWIDTH, content, 0);
    y += h + gap;
    place(browserList, x, y, content, virtualHeight - scrollY - y - margin, TRUE);
    place(splitter, x + content + margin, margin - scrollY, bar, virtualHeight - 2 * margin, TRUE);
    int rightX=x+content+margin+bar;
    int viewX=rightX;
    for(size_t i=0;i<items.size();++i) if(items[i].row==21) {
        int itemWidth=items[i].handle==keyboardHint ? rightX+keyboardWidth-viewX : measured[i];
        place(items[i].handle,viewX,margin-scrollY,itemWidth,items[i].kind==ControlKind::combo?scale(230):controlHeight,TRUE);
        viewX+=measured[i]+scale(6);
    }
    place(keyboards,rightX,margin-scrollY+controlHeight+gap,keyboardWidth,
          virtualHeight-2*margin-controlHeight-gap,TRUE);
    // Commit all sibling bounds before painting. Copying old window pixels while
    // siblings overlap during a resize leaves stale keyboard/trackbar images.
    constexpr UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOCOPYBITS;
    HDWP batch = BeginDeferWindowPos(int(placements.size()));
    for (const auto &p : placements) {
        if (!batch)
            break;
        batch = DeferWindowPos(batch, p.h, nullptr, p.x, p.y, p.width, p.height, flags);
    }
    bool committed = batch && EndDeferWindowPos(batch);
    if (!committed)
        for (const auto &p : placements)
            SetWindowPos(p.h, nullptr, p.x, p.y, p.width, p.height, flags);
    if (languageUpdating) {} // One redraw after every caption and bound has changed.
    else if (splitterBuffered)
        paintSplitterFrame();
    else
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    layoutBusy = false;
}
#include "advanced_ui.h"
void updateKeyModeNames() {
    if (!keyModeBox)
        return;
    int selected = liveKeyMode->load();
    SendMessageW(keyModeBox, CB_RESETCONTENT, 0, 0);
    const wchar_t *names[] = {tr(L"FMのみ", L"FM only"),
                              tr(L"FM＋PCM（再生速度変更）", L"FM + PCM (playback rate)"),
                              tr(L"FM＋PCM（長さを保つピッチシフト）", L"FM + PCM (preserve duration)")};
    for (auto name : names)
        SendMessageW(keyModeBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    SendMessageW(keyModeBox, CB_SETCURSEL, selected, 0);
}
void updateFormatNames() {
    int selected = static_cast<int>(SendMessageW(formatBox, CB_GETCURSEL, 0, 0));
    if (selected < 0)
        selected = 4;
    SendMessageW(formatBox, CB_RESETCONTENT, 0, 0);
    const wchar_t *names[] = {L"F0 ADPCM 3.906 kHz",
                              L"F1 ADPCM 5.208 kHz",
                              L"F2 ADPCM 7.812 kHz",
                              L"F3 ADPCM 10.416 kHz",
                              L"F4 ADPCM 15.625 kHz",
                              tr(L"F5 P16 ビッグエンディアン 15.625 kHz", L"F5 P16 big endian 15.625 kHz"),
                              tr(L"F6 P8 符号付き 15.625 kHz", L"F6 P8 signed 15.625 kHz")};
    for (auto name : names)
        SendMessageW(formatBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    SendMessageW(formatBox, CB_SETCURSEL, selected, 0);
}
void changeLanguage(bool savePreference = true) {
    languageUpdating=true;
    std::vector<HWND> redrawWindows;
    EnumChildWindows(window,[](HWND h,LPARAM data)->BOOL {
        if(GetWindowLongPtrW(h,GWL_STYLE)&WS_VISIBLE) reinterpret_cast<std::vector<HWND>*>(data)->push_back(h);
        return TRUE;
    },LPARAM(&redrawWindows));
    SendMessageW(window,WM_SETREDRAW,FALSE,0);
    for(auto h:redrawWindows) SendMessageW(h,WM_SETREDRAW,FALSE,0);
    japanese = SendMessageW(languageBox, CB_GETCURSEL, 0, 0) == 0;
    SetThreadUILanguage(japanese ? MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT)
                                 : MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    for (const auto &it : items) {
        if (it.kind == ControlKind::button || it.kind == ControlKind::label)
            SetWindowTextW(it.handle, tr(it.ja, it.en));
    }
    updateFormatNames();
    updateKeyModeNames();
    if (!input.empty()) {
        try {
            if (isPdx(input)) {
                mdx::Pdx bank(mdx::readFile(input));
                SetWindowTextW(
                    titleText,
                    (L"PDX: " + std::to_wstring(bank.entries.size()) + tr(L" スロット", L" slots")).c_str());
            } else {
                mdx::Song song(mdx::readFile(input));
                SetWindowTextW(titleText,
                               (wide(song.title) + L"\r\n" + std::to_wstring(song.tracks.size()) +
                                tr(L" トラック / PDX: ", L" tracks / PDX: ") + wide(mdx::utf8(song.pdxName)))
                                   .c_str());
            }
        } catch (const std::exception &) {
        }
    }
    if(keyboardHintTip) {
        TOOLINFOW tip{sizeof(tip)}; tip.hwnd=window; tip.uId=UINT_PTR(keyboardHint);
        for(const auto &it:items) if(it.handle==keyboardHint) tip.lpszText=const_cast<wchar_t*>(tr(it.ja,it.en));
        SendMessageW(keyboardHintTip,TTM_UPDATETIPTEXTW,0,LPARAM(&tip));
    }
    extrasLanguage();
    updateAdvancedLanguage();
        if(voiceReset) SetWindowTextW(voiceReset,tr(L"リセット",L"Reset"));
    if (savePreference)
        settingsChanged();
    layout();
    for(auto h:redrawWindows) SendMessageW(h,WM_SETREDRAW,TRUE,0);
    SendMessageW(window,WM_SETREDRAW,TRUE,0);
    languageUpdating=false;
    RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
LRESULT CALLBACK procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_ACTIVATE:
        if(LOWORD(wparam)==WA_INACTIVE) {
            HWND focused=GetFocus();
            if(focused && IsChild(hwnd,focused)) previousFocus=focused;
        } else if(IsWindow(previousFocus) && IsWindowVisible(previousFocus) && IsWindowEnabled(previousFocus))
            SetFocus(previousFocus);
        return 0;
    case WM_CREATE: {
        window = hwnd;
        SendMessageW(
            hwnd, WM_SETICON, ICON_SMALL,
            LPARAM(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED)));
        SendMessageW(hwnd, WM_SETICON, ICON_BIG,
                     LPARAM(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                                       GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED)));
        uiFont = CreateFontW(-MulDiv(9, scale(96), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"MS Gothic");
        item(L"BUTTON", L"開く…", L"Open...", 0, 100, 32, 1, WS_TABSTOP);
        item(L"BUTTON", L"PDXを指定…", L"Choose PDX...", 0, 130, 32, 2, WS_TABSTOP);
        item(L"BUTTON", L"ライセンス", L"Licenses", 0, 100, 32, 7, WS_TABSTOP);
        item(L"STATIC", L"表示言語", L"Language", 0, 75, 32, 0);
        languageBox = item(L"COMBOBOX", L"", L"", 0, 130, 32, 32, WS_TABSTOP | CBS_DROPDOWNLIST);
        for (auto name : {L"日本語", L"English"})
            SendMessageW(languageBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
        SendMessageW(languageBox, CB_SETCURSEL, japanese ? 0 : 1, 0);
        pathText = item(L"EDIT", L"", L"", 1, 640, 28, 20, ES_READONLY | ES_AUTOHSCROLL, true);
        titleText = item(L"STATIC", L"MDX / PDX / VGM / VGZを開くか、ここへドロップしてください。",
                         L"Open an MDX / PDX / VGM / VGZ file, or drop it here.", 2, 640, 36, 21, 0, true);
        item(L"STATIC", L"周回数 (1–1000)", L"Loops (1–1000)", 3, 100, 28, 0);
        loopsEdit = item(L"EDIT", L"2", L"2", 3, 70, 28, 22, WS_TABSTOP | ES_NUMBER);
        item(L"STATIC", L"全体音量 % (0–400)", L"Master volume % (0–400)", 18, 190, 28, 0);
        masterSlider = item(TRACKBAR_CLASSW, L"", L"", 18, 270, 30, 48, WS_TABSTOP | TBS_NOTICKS);
        SendMessageW(masterSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 400));
        SendMessageW(masterSlider, TBM_SETPOS, TRUE, 100);
        gainEdit = item(L"EDIT", L"100", L"100", 18, 70, 28, 23, WS_TABSTOP | ES_NUMBER);
        item(L"STATIC", L"PDX試聴番号", L"PDX sample index", 4, 120, 28, 0);
        sampleEdit = item(L"EDIT", L"0", L"0", 4, 90, 28, 24, WS_TABSTOP | ES_NUMBER);
        formatBox = item(L"COMBOBOX", L"", L"", 4, 310, 28, 25, WS_TABSTOP | CBS_DROPDOWNLIST);
        updateFormatNames();
        updateKeyModeNames();
        item(L"BUTTON", L"再生", L"Play", 5, 100, 34, 3, WS_TABSTOP);
        item(L"BUTTON", L"一時停止 / 再開", L"Pause / Resume", 5, 130, 34, 4, WS_TABSTOP);
        item(L"BUTTON", L"停止", L"Stop", 5, 100, 34, 5, WS_TABSTOP);
        item(L"BUTTON", L"WAV保存…", L"Export WAV...", 5, 120, 34, 6, WS_TABSTOP);
        statusText = item(L"EDIT", L"", L"", 6, 640, 48, 26,
                          ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, true);
        item(L"STATIC", L"FM音量 % (0–800)", L"FM gain % (0–800)", 7, 150, 28, 0);
        independentGainEdit = item(L"EDIT", L"200", L"200", 7, 80, 28, 29, WS_TABSTOP | ES_NUMBER);
        item(L"STATIC", L"PCM音量 % (0–400)", L"PCM gain % (0–400)", 7, 170, 28, 0);
        pcmGainEdit = item(L"EDIT", L"100", L"100", 7, 70, 28, 27, WS_TABSTOP | ES_NUMBER);
        dacButton = item(L"BUTTON", L"YM3012時差モデル: R +7.00 μs (Clock/64 → 48 kHz)",
                         L"YM3012 timing model: R +7.00 us (Clock/64 → 48 kHz)", 8, 640, 28, 30,
                         WS_TABSTOP | BS_AUTOCHECKBOX);
        measuredButton = item(L"BUTTON", L"実測出力タイミング（Independent OPM DLL / ALG5・7）",
                              L"Measured timing: Independent OPM DLL / ALG5,7", 9, 640, 28, 31,
                              WS_TABSTOP | BS_AUTOCHECKBOX);
        infiniteButton =
            item(L"BUTTON", L"無限ループ", L"Infinite loop", 10, 135, 28, 33, WS_TABSTOP | BS_AUTOCHECKBOX);
        autoNextButton = item(L"BUTTON", L"終了後に次のファイルを再生", L"Play next file on completion", 10,
                              270, 28, 34, WS_TABSTOP | BS_AUTOCHECKBOX);
        item(L"STATIC", L"キー（半音）", L"Key (semitones)", 11, 140, 32, 0);
        keySlider = item(TRACKBAR_CLASSW, L"", L"", 11, 350, 32, 35, WS_TABSTOP | TBS_AUTOTICKS);
        SendMessageW(keySlider, TBM_SETRANGE, TRUE, MAKELPARAM(-24, 24));
        SendMessageW(keySlider, TBM_SETTICFREQ, 12, 0);
        item(L"STATIC", L"キー変更対象", L"Key mode", 12, 100, 24, 0);
        keyModeBox = item(L"COMBOBOX", L"", L"", 12, 350, 28, 37, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL);
        updateKeyModeNames();
        keyValue = item(L"EDIT", L"0", L"0", 11, 55, 32, 36, ES_READONLY);
        item(L"STATIC", L"出力", L"Output", 13, 80, 28, 0);
        outputBox = item(L"COMBOBOX", L"", L"", 13, 280, 28, 38, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL);
        item(L"STATIC", L"MIDI入力", L"MIDI input", 14, 100, 28, 0);
        midiBox = item(L"COMBOBOX", L"", L"", 14, 310, 28, 39, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL);
        item(L"BUTTON", L"音色表示", L"Instrument viewer", 13, 160, 28, 40, WS_TABSTOP);
        item(L"BUTTON", L"全音色をMML保存", L"Export all voices as MML", 14, 220, 28, 41, WS_TABSTOP);
        seekSlider = item(TRACKBAR_CLASSW, L"", L"", 15, 350, 30, 42, WS_TABSTOP | TBS_NOTICKS);
        SendMessageW(seekSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 10000));
        SetWindowSubclass(seekSlider, seekSubclass, 1, 0);
        timeLabel = item(L"STATIC", L"00:00 / --:--", L"00:00 / --:--", 15, 200, 30, 43);
        item(L"STATIC", L"停止／一時停止中のMIDI", L"MIDI while stopped / paused", 16, 210, 28, 0);
        polyBox = item(L"COMBOBOX", L"", L"", 16, 175, 28, 44, CBS_DROPDOWNLIST | WS_TABSTOP);
        item(L"STATIC", L"表示", L"View", 3, 60, 28, 0);
        viewBox = item(L"COMBOBOX", L"", L"", 3, 160, 28, 45, CBS_DROPDOWNLIST | WS_TABSTOP);
        item(L"STATIC", L"C86CTL FM遅延 ms", L"C86CTL FM delay ms", 17, 170, 28, 0);
        fmDelayEdit = item(L"EDIT", L"0", L"0", 17, 70, 28, 46, WS_TABSTOP | ES_NUMBER);
        item(L"STATIC", L"PCM遅延 ms", L"PCM delay ms", 17, 130, 28, 0);
        pcmDelayEdit = item(L"EDIT", L"0", L"0", 17, 70, 28, 47, WS_TABSTOP | ES_NUMBER);
        item(L"STATIC", L"遅延: 0–2000 ms、次回再生時に反映", L"Delay: 0–2000 ms; applies on next playback",
             19, 500, 24, 0);
        aftertouchButton =
            item(L"BUTTON", L"アフタータッチ", L"Aftertouch", 16, 180, 28, 49, WS_TABSTOP | BS_AUTOCHECKBOX);
        carrierButton =
            item(L"BUTTON", L"キャリア音程", L"Carrier pitch", 3, 140, 28, 50, WS_TABSTOP | BS_AUTOCHECKBOX);
        for (auto &it : items)
            if (it.row >= 11)
                ++it.row;
        item(L"STATIC", L"再生速度 %", L"Playback speed %", 11, 140, 28, 0);
        speedSlider = item(TRACKBAR_CLASSW, L"", L"", 11, 350, 28, 51, WS_TABSTOP | TBS_AUTOTICKS);
        SendMessageW(speedSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
        SendMessageW(speedSlider, TBM_SETTICFREQ, 50, 0);
        SendMessageW(speedSlider, TBM_SETPOS, TRUE, 100);
        speedValue = item(L"EDIT", L"100", L"100", 11, 55, 28, 52, ES_READONLY);
        waveformButton=item(L"BUTTON",L"波形表示",L"Waveforms",21,0,28,53,WS_TABSTOP|BS_AUTOCHECKBOX);
        SendMessageW(waveformButton,BM_SETCHECK,BST_CHECKED,0);
        keyboardHint=item(L"STATIC",L"FM / PCM 鍵盤クリックで消音",
            L"FM / PCM Click to mute",21,0,28,0,SS_RIGHT|SS_CENTERIMAGE|SS_ENDELLIPSIS);
        keyboardHintTip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
            CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        TOOLINFOW hintTool{sizeof(hintTool)};
        hintTool.hwnd=window; hintTool.uId=UINT_PTR(keyboardHint); hintTool.uFlags=TTF_IDISHWND|TTF_SUBCLASS;
        hintTool.lpszText=const_cast<wchar_t*>(L"FM / PCM Click to mute");
        SendMessageW(keyboardHintTip,TTM_ADDTOOLW,0,LPARAM(&hintTool));
        SendMessageW(keyboardHintTip,TTM_SETMAXTIPWIDTH,0,scale(480));
        for(auto slider:{speedSlider,keySlider,masterSlider}) SetWindowSubclass(slider,valueSliderSubclass,1,0);
        organizeControls();
        updateAdvancedLanguage();
        if(voiceReset) SetWindowTextW(voiceReset,tr(L"リセット",L"Reset"));
        createExtras();
        readSettings();
        changeLanguage(false);
        settingsReady = true;
        auto midiName = settingText(L"MidiInputName");
        for(int entry=1;!midiName.empty() && entry<SendMessageW(midiBox,CB_GETCOUNT,0,0);++entry) {
            wchar_t name[256]{}; SendMessageW(midiBox,CB_GETLBTEXT,entry,LPARAM(name));
            if(midiName==name) {
                SendMessageW(midiBox,CB_SETCURSEL,entry,0);
                SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(39,CBN_SELCHANGE),LPARAM(midiBox)); break;
            }
        }
        if (int savedOutput = setting(L"OutputBackend", setting(L"HardwareOutput", 0) ? 2 : 0)) {
            SendMessageW(outputBox, CB_SETCURSEL, std::clamp(savedOutput, 0, 2), 0);
            SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(38, CBN_SELCHANGE), LPARAM(outputBox));
        }
        auto previous = settingText(L"LastPlayedFile");
        if (!previous.empty()) {
            lastPlayedFile = previous;
            lastPlayedBank = settingText(L"LastPlayedPdx");
            std::error_code error;
            bool available=false;
            try { available=std::filesystem::is_regular_file(mdx::resolveArchivePath(lastPlayedFile), error); } catch(...) {}
            if (available) {
                load(lastPlayedFile);
                explicitPdx = lastPlayedBank;
            } else
                status(tr(L"前回のファイルが見つかりません", L"Previous file was not found"));
        }
        layout();
        DragAcceptFiles(hwnd, TRUE);
        SetTimer(hwnd, 1, 33, nullptr);
        return 0;
    }
    case WM_COPYDATA: {
        const auto *data=reinterpret_cast<const COPYDATASTRUCT *>(lparam);
        if(!data || data->dwData!=commandFileTag || !data->lpData ||
           data->cbData<sizeof(wchar_t) || data->cbData>65536 || data->cbData%sizeof(wchar_t)) return FALSE;
        const auto *path=static_cast<const wchar_t *>(data->lpData);
        const size_t length=data->cbData/sizeof(wchar_t);
        if(path[length-1]!=0 || wcsnlen(path,length)!=length-1) return FALSE;
        pendingCommandFiles.emplace_back(path);
        PostMessageW(hwnd,WM_APP+2,0,0);
        return TRUE;
    }
    case WM_APP + 2:
        if(!pendingCommandFiles.empty()) {
            auto file=pendingCommandFiles.front();
            pendingCommandFiles.erase(pendingCommandFiles.begin());

            if(load(file)) start(false);
        }
        return 0;
    case WM_APP + 1:
        if (static_cast<uint64_t>(wparam) == playbackGeneration.load() &&
            SendMessageW(autoNextButton, BM_GETCHECK, 0, 0) == BST_CHECKED)
            advanceFile();
        return 0;
    case MM_MIM_DATA:
        if (reinterpret_cast<HMIDIIN>(wparam) == midiInput) {
            if (!running)
                startMidiIdle();
            midiQueue->push(uint32_t(lparam));
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wparam) == 50 && HIWORD(wparam) == BN_CLICKED) {
            carrierPitch = SendMessageW(carrierButton, BM_GETCHECK, 0, 0) == BST_CHECKED;
            rollHistory.clear();
            settingsChanged();
            InvalidateRect(keyboards, nullptr, FALSE);
            return 0;
        }
        if (LOWORD(wparam) == 49 && HIWORD(wparam) == BN_CLICKED) {
            liveAftertouch->store(SendMessageW(aftertouchButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
            settingsChanged();
            return 0;
        }
        if ((LOWORD(wparam) == 44 || LOWORD(wparam) == 45) && HIWORD(wparam) == CBN_SELCHANGE) {
            if (LOWORD(wparam) == 44)
                releaseVoiceKeys();
            liveMidiPolyphonic->store(SendMessageW(polyBox, CB_GETCURSEL, 0, 0) == 0);
            pianoRoll = SendMessageW(viewBox, CB_GETCURSEL, 0, 0) == 1;
            settingsChanged();
            InvalidateRect(keyboards, nullptr, FALSE);
            return 0;
        }
        if ((LOWORD(wparam) == 46 || LOWORD(wparam) == 47) && HIWORD(wparam) == EN_CHANGE) {
            settingsChanged();
            return 0;
        }
        if ((LOWORD(wparam) == 23 || LOWORD(wparam) == 27) && HIWORD(wparam) == EN_CHANGE) {
            try {
                liveGain->store(integer(gainEdit, 0, 400) / 100.0);
                livePcmGain->store(integer(pcmGainEdit, 0, 400) / 100.0);
                SendMessageW(masterSlider, TBM_SETPOS, TRUE, LPARAM(liveGain->load() * 100));
                hardware.setGain(liveGain->load(), liveIndependentGain->load());
            } catch (const std::exception &) {
            }
            settingsChanged();
            return 0;
        }
        if (LOWORD(wparam) == 38 && HIWORD(wparam) == CBN_SELCHANGE) {
            const bool resume=running.load() && !idleMidi;
            const bool paused=pauseFlag.load();
            const double position=songPosition.load();
            const auto playingFile=lastPlayedFile, playingBank=lastPlayedBank;
            captureOutputGains();
            stop();
            hardware.close();
            selectedOutput = int(SendMessageW(outputBox, CB_GETCURSEL, 0, 0));
            if (selectedOutput == 2) {
                try {
                    hardware.open(executableFolder() / L"c86ctl.dll");
                    hardware.setChannelOverride(&retainedVoices->midiFmOverride, liveMuteMask.get());
                    status(tr(L"C86CTL OPM 出力", L"C86CTL OPM output"));
                } catch (const std::exception &e) {
                    SendMessageW(outputBox, CB_SETCURSEL, 0, 0);
                    selectedOutput = 0;
                    status(L"ERROR: " + wide(e.what()));
                }
            }
            applyOutputGains();
            settingsChanged();
            if(resume) { input=playingFile; explicitPdx=playingBank; start(false,position,paused,true); }
            return 0;
        }
        if (LOWORD(wparam)==53 && HIWORD(wparam)==BN_CLICKED) {
            monitorView->waveforms.store(SendMessageW(waveformButton,BM_GETCHECK,0,0)==BST_CHECKED);
            InvalidateRect(keyboards,nullptr,FALSE); settingsChanged(); return 0;
        }
        if (LOWORD(wparam)==39 && HIWORD(wparam)==CBN_DROPDOWN) {
            refreshMidiDevices(); return 0;
        }
        if (LOWORD(wparam) == 39 && HIWORD(wparam) == CBN_SELCHANGE) {
            int selection=int(SendMessageW(midiBox, CB_GETCURSEL, 0, 0));
            int index=selection>0 ? int(SendMessageW(midiBox,CB_GETITEMDATA,selection,0)) : -1;
            if (index == activeMidiDevice && midiInput) return 0;
            if (index < 0) closeMidi();
            else {
                HMIDIIN next = nullptr;
                MMRESULT error = midiInOpen(&next, UINT(index), DWORD_PTR(window), 0, CALLBACK_WINDOW);
                if (error == MMSYSERR_NOERROR) error = midiInStart(next);
                if (error != MMSYSERR_NOERROR) {
                    if (next) { midiInReset(next); midiInClose(next); }
                    int restore=0;
                    for(int n=1;n<SendMessageW(midiBox,CB_GETCOUNT,0,0);++n)
                        if(midiInput && SendMessageW(midiBox,CB_GETITEMDATA,n,0)==activeMidiDevice) restore=n;
                    SendMessageW(midiBox, CB_SETCURSEL, restore, 0);
                    wchar_t detail[MAXERRORLENGTH]{};
                    midiInGetErrorTextW(error, detail, MAXERRORLENGTH);
                    status(std::wstring(tr(L"MIDI入力を開けません: ", L"Cannot open MIDI input: ")) +
                           detail + L" (" + std::to_wstring(error) + L")");
                } else {
                    closeMidi();
                    midiInput = next;
                    activeMidiDevice = index;
                }
            }
            settingsChanged();
            return 0;
        }
        if (LOWORD(wparam) == 40 && HIWORD(wparam) == BN_CLICKED) {
            showVoices();
            return 0;
        }
        if (LOWORD(wparam) == 41 && HIWORD(wparam) == BN_CLICKED) {
            exportVoices();
            return 0;
        }
        if (LOWORD(wparam) == 33 && HIWORD(wparam) == BN_CLICKED) {
            const bool infinite=SendMessageW(infiniteButton,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(loopsEdit, !infinite);
            try { liveLoops->store(infinite ? 0 : integer(loopsEdit,1,1000)); }
            catch(const std::exception &) {}
            settingsChanged();
            return 0;
        }
        if (LOWORD(wparam) == 34 && HIWORD(wparam) == BN_CLICKED) {
            settingsChanged();
            return 0;
        }
        if (extrasCommand(wparam))
            return 0;
        if ((HIWORD(wparam) == EN_CHANGE &&
             (LOWORD(wparam) == 22 || LOWORD(wparam) == 23 || LOWORD(wparam) == 24 || LOWORD(wparam) == 27 ||
              LOWORD(wparam) == 29)) ||
            (HIWORD(wparam) == CBN_SELCHANGE && (LOWORD(wparam) == 25 || LOWORD(wparam) == 32)) ||
            (HIWORD(wparam) == BN_CLICKED && (LOWORD(wparam) == 30 || LOWORD(wparam) == 31)))
            settingsChanged();
        if (LOWORD(wparam) == 32 && HIWORD(wparam) == CBN_SELCHANGE) {
            changeLanguage();
            return 0;
        }
        if (LOWORD(wparam) == 31 && HIWORD(wparam) == BN_CLICKED) {
            liveMeasuredOutputTiming->store(SendMessageW(measuredButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        }
        if (LOWORD(wparam) == 30 && HIWORD(wparam) == BN_CLICKED) {
            liveDacRightDelay->store(SendMessageW(dacButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        }
        if (LOWORD(wparam) == 29 && HIWORD(wparam) == EN_CHANGE && independentGainEdit) {
            try {
                liveIndependentGain->store(integer(independentGainEdit, 0, 800) / 100.0);
                hardware.setGain(liveGain->load(), liveIndependentGain->load());
            } catch (const std::exception &) {
            }
            return 0;
        }
        if (LOWORD(wparam) == 37 && HIWORD(wparam) == CBN_SELCHANGE) {
            liveKeyMode->store(int(SendMessageW(keyModeBox, CB_GETCURSEL, 0, 0)));
            settingsChanged();
            return 0;
        }
        if (HIWORD(wparam) != BN_CLICKED)
            break;
        switch (LOWORD(wparam)) {
        case 1:
            load(choose(false));
            break;
        case 2: {
            auto path = choose(false, true);
            if (!path.empty()) {
                stop();
                explicitPdx = path;
                status(L"PDX: " + path.wstring());
            }
            break;
        }
        case 3:
            start(false);
            break;
        case 4:
            if (running)
                pauseFlag = !pauseFlag.load();
            break;
        case 5:
            stop();
            status(tr(L"停止しました", L"Stopped"));
            break;
        case 6:
            start(true);
            break;
        case 7:
            showLicenses();
            break;
        }
        return 0;
    case WM_DROPFILES: {
        HDROP drop = reinterpret_cast<HDROP>(wparam);
        wchar_t file[32768];
        DragQueryFileW(drop, 0, file, 32768);
        DragFinish(drop);
        load(file);
        return 0;
    }
    case WM_TIMER: {
        if(wparam==1) updateTitles();
        if (wparam == 4) {
            if (splitterLayoutPending) {
                splitterLayoutPending = false;
                layout();
                settingsChanged();
            } else
                paintSplitterFrame();
            return 0;
        }
        if (wparam == 2) {
            KillTimer(hwnd, 2);
            saveSettings();
            return 0;
        }
        if (wparam == 3) {
            KillTimer(hwnd, 3);
            settingsChanged();
            return 0;
        }
        refreshVoiceWindow();
        int seekPercent = seekProgressPercent.load();
        auto progressText = seekPercent>=0 ? std::wstring(tr(L"再生位置（検索",L"Position (seeking ")) + std::to_wstring(seekPercent) + tr(L"%）",L"%)") : tr(L"再生位置",L"Position");
        wchar_t oldPositionLabel[96]; GetWindowTextW(positionLabel,oldPositionLabel,96);
        if(progressText!=oldPositionLabel) SetWindowTextW(positionLabel, progressText.c_str());
        double current = seekRequest >= 0 ? seekRequest.load() : songPosition.load(),
               total = songDuration.load();
        wchar_t positionText[128];
        swprintf(positionText, 128, L"%02d:%02d / %02d:%02d", int(current) / 60, int(current) % 60,
                 int(total) / 60, int(total) % 60);
        wchar_t shownPosition[128]{};
        GetWindowTextW(timeLabel,shownPosition,128);
        if(wcscmp(shownPosition,positionText)!=0) SetWindowTextW(timeLabel,positionText);
        if (!draggingSeek && total > 0) {
            int position=int(std::clamp(current/total,0.0,1.0)*10000);
            if(SendMessageW(seekSlider,TBM_GETPOS,0,0)!=position)
                SendMessageW(seekSlider,TBM_SETPOS,TRUE,position);
        }
        EnableWindow(seekSlider, running && !idleMidi && total > 0);
        InvalidateRect(keyboards, nullptr, FALSE);
        std::wstring text;
        {
            std::lock_guard<std::mutex> lock(messageMutex);
            text = japanese ? translated(message + L"\r\n" + playbackErrors, true)
                            : message + L"\r\n" + playbackErrors;
        }
        wchar_t time[64];
        swprintf(time, 64, L"%02d:%02d%s\r\n", int(playbackSeconds.load()) / 60,
                 int(playbackSeconds.load()) % 60, pauseFlag ? tr(L" [一時停止]", L" [PAUSED]") : L"");
        text = time + text;
        static std::wstring displayedStatus;
        if (text != displayedStatus) {
            LRESULT firstLine = SendMessageW(statusText, EM_GETFIRSTVISIBLELINE, 0, 0);
            SetWindowTextW(statusText, text.c_str());
            if (firstLine > 0)
                SendMessageW(statusText, EM_LINESCROLL, 0, firstLine);
            displayedStatus = text;
        }
        return 0;
    }
    case WM_NOTIFY:
        return browserNotify(reinterpret_cast<NMHDR *>(lparam));
    case WM_SYSKEYDOWN:
        if (wparam == VK_RETURN) {
            if (!(lparam & (LPARAM(1) << 30)))
                toggleFullscreen();
            return 0;
        }
        break;
    case WM_SIZE:
        layout();
        settingsChanged();
        return 0;
    case WM_MOVE:
        settingsChanged();
        return 0;
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lparam) == speedSlider) {
            int value = int(SendMessageW(speedSlider, TBM_GETPOS, 0, 0));
            liveSpeed->store(value);
            SetWindowTextW(speedValue, std::to_wstring(value).c_str());
            settingsChanged();
            return 0;
        }
        if (reinterpret_cast<HWND>(lparam) == masterSlider) {
            SetWindowTextW(gainEdit, std::to_wstring(SendMessageW(masterSlider, TBM_GETPOS, 0, 0)).c_str());
            return 0;
        }
        if (reinterpret_cast<HWND>(lparam) == seekSlider) {
            int code = LOWORD(wparam);
            draggingSeek = code == TB_THUMBTRACK;
            if (code != TB_THUMBTRACK && code != TB_ENDTRACK && songDuration > 0)
                seekRequest =
                    double(SendMessageW(seekSlider, TBM_GETPOS, 0, 0)) / 10000 * songDuration.load();
            return 0;
        }
        if (reinterpret_cast<HWND>(lparam) == keySlider) {
            int key = int(SendMessageW(keySlider, TBM_GETPOS, 0, 0));
            liveKeyShift->store(key);
            SetWindowTextW(keyValue, std::to_wstring(key).c_str());
            settingsChanged();
            return 0;
        }
        [[fallthrough]];
    case WM_VSCROLL:
        if (!lparam) {
            int bar = msg == WM_HSCROLL ? SB_HORZ : SB_VERT;
            SCROLLINFO si{sizeof(si), SIF_ALL};
            GetScrollInfo(hwnd, bar, &si);
            int pos = si.nPos;
            switch (LOWORD(wparam)) {
            case SB_LINEUP:
                pos -= scale(24);
                break;
            case SB_LINEDOWN:
                pos += scale(24);
                break;
            case SB_PAGEUP:
                pos -= si.nPage;
                break;
            case SB_PAGEDOWN:
                pos += si.nPage;
                break;
            case SB_THUMBTRACK:
                pos = si.nTrackPos;
                break;
            }
            SetScrollPos(hwnd, bar, pos, TRUE);
            if (bar == SB_HORZ)
                scrollX = GetScrollPos(hwnd, bar);
            else
                scrollY = GetScrollPos(hwnd, bar);
            layout();
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        bool edit = msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX || (HWND)lparam == pathText ||
                    (HWND)lparam == statusText;
        SetTextColor((HDC)wparam, textColor);
        SetBkColor((HDC)wparam, edit ? editColor : panelColor);
        return (LRESULT)(edit ? editBrush : panelBrush);
    }
    case WM_DRAWITEM: {
        auto d = (DRAWITEMSTRUCT *)lparam;
        if (d->CtlType == ODT_BUTTON) {
            FillRect(d->hDC, &d->rcItem, panelBrush);
            SetTextColor(d->hDC, d->itemState & ODS_DISABLED ? RGB(90, 90, 110) : textColor);
            SetBkMode(d->hDC, TRANSPARENT);
            FrameRect(d->hDC, &d->rcItem, accentBrush);
            wchar_t t[256];
            GetWindowTextW(d->hwndItem, t, 256);
            DrawTextW(d->hDC, t, -1, &d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (d->itemState & ODS_FOCUS)
                DrawFocusRect(d->hDC, &d->rcItem);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        if (settingsDirty)
            saveSettings();
        settingsReady = false;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if(titleJob)titleJob->cancelled=true;
        {std::unique_lock<std::mutex> lock(titleWorkerMutex);titleWorkersFinished.wait(lock,[]{return titleWorkers==0;});}
        closeMidi();
        stop();
        hardware.close();
        if (voiceWindow)
            DestroyWindow(voiceWindow);
        if (licenseWindow)
            DestroyWindow(licenseWindow);
        if (documentWindow) DestroyWindow(documentWindow);
        if (browserImages) {ListView_SetImageList(browserList,nullptr,LVSIL_SMALL);ImageList_Destroy(browserImages);browserImages=nullptr;}
        DeleteObject(uiFont);
        KillTimer(hwnd, 1);
        Gdiplus::GdiplusShutdown(gdiToken);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}
} // namespace
int validateFolder(const std::filesystem::path &root) {
    collectErrorMdx=true;
    AttachConsole(ATTACH_PARENT_PROCESS);
    HANDLE console=GetStdHandle(STD_OUTPUT_HANDLE);
    std::filesystem::path lastFolder;
    std::error_code ec;
    if(!std::filesystem::is_directory(root,ec)) return 2;
    int failures=0;
    auto excluded=(executableFolder()/L"ErrorMDX").lexically_normal();
    std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;
    while(it!=end) {
        auto file=it->path();
        if(it->is_directory(ec) && std::filesystem::absolute(file).lexically_normal()==excluded) it.disable_recursion_pending();
        if(it->is_regular_file(ec) && _wcsicmp(file.extension().c_str(),L".mdx")==0) {
            if(file.parent_path()!=lastFolder) {
                lastFolder=file.parent_path();
                auto line=lastFolder.wstring()+L"\r\n";
                DWORD written=0,mode=0;
                if(GetConsoleMode(console,&mode)) WriteConsoleW(console,line.data(),DWORD(line.size()),&written,nullptr);
                else {auto text=lastFolder.u8string()+"\r\n";WriteFile(console,text.data(),DWORD(text.size()),&written,nullptr);}
            }
            std::string title;
            bool failed=false;
            auto report=[&](const std::string &error) {
                if(error.find("Required PDX not found:")!=std::string::npos || error.find("PCM note without a PDX sample bank")!=std::string::npos) return;
                failed=true; logPlaybackError(file,title,error);
            };
            try {
                mdx::Song song(mdx::readFile(file)); title=song.title;
                mdx::Options options; options.analysisOnly=true;
                options.diagnostic=[&](const std::string &e){if(e.rfind("ERROR:",0)==0) report(e);};
                std::shared_ptr<mdx::Pdx> bank;
                try { bank=mdx::locatePdx(song,file,{}); } catch(const std::exception &e) { report(e.what()); }
                mdx::Player player(std::move(song),bank,options);
                if(player.analyze().truncated) report("MDX validation incomplete: timeline exceeds 3600 seconds");
            } catch(const std::exception &e) { report(e.what()); }
            if(failed) ++failures;
        }
        it.increment(ec);
        if(ec) return 2;
    }
    return failures ? 1 : 0;
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    int count = 0;
    wchar_t **args = CommandLineToArgvW(GetCommandLineW(), &count);
    std::vector<std::string> batchArgs;
    for(int i=0;args && i<count;++i) batchArgs.push_back(std::filesystem::path(args[i]).u8string());
    if(std::find(batchArgs.begin(),batchArgs.end(),"--vgz-folder")!=batchArgs.end() ||
       std::find(batchArgs.begin(),batchArgs.end(),"--reglog-folder")!=batchArgs.end()) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        auto result=mdx::runVgzBatch(batchArgs,[](const std::string &s) {
            HANDLE output=GetStdHandle(STD_OUTPUT_HANDLE); DWORD written=0,mode=0;
            if(GetConsoleMode(output,&mode)) {
                auto text=wide(s+"\r\n"); WriteConsoleW(output,text.data(),DWORD(text.size()),&written,nullptr);
            } else {auto text=s+"\r\n";WriteFile(output,text.data(),DWORD(text.size()),&written,nullptr);}
        });
        if(args)LocalFree(args);
        return result.value_or(2);
    }
    std::filesystem::path commandFile, validationRoot;
    bool validate=false;
    for(int i=1;args && i<count;++i) {
        if(wcscmp(args[i],L"--validate-folder")==0) { validate=true; if(i+1<count) validationRoot=args[++i]; }
        else if(wcscmp(args[i],L"--collect-error-mdx")==0) collectErrorMdx=true;
        else if(commandFile.empty()) commandFile=args[i];
    }
    if(args) LocalFree(args);
    if(validate) return validationRoot.empty()?2:validateFolder(validationRoot);
    if(!commandFile.empty()) commandFile=std::filesystem::absolute(commandFile);
    // Serialize startup so simultaneous file launches reach one GUI instance.
    HANDLE instanceMutex=CreateMutexW(nullptr,TRUE,L"Local\\IndependentMDX.GuiStartup");
    if(instanceMutex && GetLastError()==ERROR_ALREADY_EXISTS) {
        DWORD wait=WaitForSingleObject(instanceMutex,5000);
        if(wait!=WAIT_OBJECT_0 && wait!=WAIT_ABANDONED) { CloseHandle(instanceMutex); return 1; }
    }
    HWND existing=FindWindowW(L"IndependentMDX",nullptr);
    if(existing) {
        if(!commandFile.empty()) {
            auto path=commandFile.wstring();
            COPYDATASTRUCT data{commandFileTag,DWORD((path.size()+1)*sizeof(wchar_t)),path.data()};
            DWORD_PTR accepted=0;
            if(!SendMessageTimeoutW(existing,WM_COPYDATA,0,LPARAM(&data),SMTO_ABORTIFHUNG,5000,&accepted) || !accepted) {
                if(instanceMutex) { ReleaseMutex(instanceMutex); CloseHandle(instanceMutex); }
                return 1;
            }
        }

        if(instanceMutex) { ReleaseMutex(instanceMutex); CloseHandle(instanceMutex); }
        return 0;
    }
    japanese = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_JAPANESE;
    DWORD saved = setting(L"Language", 2);
    if (saved <= 1)
        japanese = saved == 0;
    SetThreadUILanguage(japanese ? MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT)
                                 : MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls{sizeof(controls),
                                  ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSW cls{};
    cls.lpfnWndProc = procedure;
    cls.hInstance = instance;
    cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    cls.lpszClassName = L"IndependentMDX";
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = backgroundBrush;
    RegisterClassW(&cls);
    HWND mainWindow =
        CreateWindowW(cls.lpszClassName, L"IndependentMDX 0.33.5",
                      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL, CW_USEDEFAULT,
                      CW_USEDEFAULT, 1320, 840, nullptr, nullptr, instance, nullptr);
    if(instanceMutex) { ReleaseMutex(instanceMutex); CloseHandle(instanceMutex); }
    if (!mainWindow)
        return 1;
    restoreWindow();
    ShowWindow(mainWindow, restoreMaximized ? SW_SHOWMAXIMIZED : show);
    if (voiceWantedVisible) showVoices(false);
    if(!commandFile.empty()) {
        pendingCommandFiles.push_back(commandFile);
        PostMessageW(mainWindow,WM_APP+2,0,0);
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (voiceKeyMessage(msg))
            continue;
        if (msg.message == WM_SYSKEYDOWN && msg.wParam == VK_RETURN &&
            (msg.hwnd == mainWindow || IsChild(mainWindow, msg.hwnd))) {
            if (!(msg.lParam & (LPARAM(1) << 30)))
                toggleFullscreen();
            continue;
        }
        if (!IsDialogMessageW(mainWindow, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
