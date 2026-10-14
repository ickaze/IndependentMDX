// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

// Main-window helpers. Included inside the GUI namespace after item/layout.
void closeMidi() {
    if (midiInput) {
        midiInStop(midiInput);
        midiInReset(midiInput);
        midiInClose(midiInput);
        midiInput = nullptr;
        activeMidiDevice = -1;
    }
    for (int ch = 0; ch < 16; ++ch)
        midiQueue->push(0xb0 | ch | (120 << 8));
}
void startMidiIdle() {
    if (running)
        return;
    if (worker.joinable())
        worker.join();
    stopFlag = false;
    pauseFlag = false;
    running = true;
    idleMidi = true;
    worker = std::thread([] {
        try {
            mdx::Options options;
            options.externalFm = hardware.active();
            options.monitor = monitorView;
            options.liveMeasuredOutputTiming = liveMeasuredOutputTiming;
            options.measuredOutputTiming = liveMeasuredOutputTiming->load();
            options.fmBackend = selectedOutput == 1 ? 1 : 0;
            options.liveGain = liveGain;
            options.livePcmGain = livePcmGain;
            options.liveIndependentGain = liveIndependentGain;
            options.liveMidiPolyphonic = liveMidiPolyphonic;
            options.liveAftertouch = liveAftertouch;
            mdx::LiveSession session(
                nullptr, nullptr, {}, retainedVoices, midiQueue, options, pauseFlag, seekRequest,
                songPosition, songDuration, [] { return stopFlag.load(); }, [] { hardware.reset(); },
                [](int a, int v) { hardware.write(a, v); });
            mdx::playWindows(
                session, options.sampleRate, [] { return stopFlag.load(); }, [] { return false; });
        } catch (const std::exception &e) {
            status(L"ERROR: " + wide(e.what()));
        }
        try {
            hardware.reset();
        } catch (const std::exception &e) {
            status(L"ERROR: " + wide(e.what()));
        }
        running = false;
        idleMidi = false;
    });
}
void refreshMidiDevices() {
    int selected=int(SendMessageW(midiBox,CB_GETCURSEL,0,0));
    wchar_t previous[256]{};
    if(selected>0) SendMessageW(midiBox,CB_GETLBTEXT,selected,LPARAM(previous));
    SendMessageW(midiBox,WM_SETREDRAW,FALSE,0);
    SendMessageW(midiBox,CB_RESETCONTENT,0,0);
    SendMessageW(midiBox,CB_ADDSTRING,0,LPARAM(tr(L"MIDI入力なし",L"No MIDI input")));
    int restore=0;
    for(UINT i=0;i<midiInGetNumDevs();++i) {
        MIDIINCAPSW caps{};
        if(midiInGetDevCapsW(i,&caps,sizeof(caps))!=MMSYSERR_NOERROR) continue;
        int entry=int(SendMessageW(midiBox,CB_ADDSTRING,0,LPARAM(caps.szPname)));
        SendMessageW(midiBox,CB_SETITEMDATA,entry,i);
        if(selected>0 && !restore && wcscmp(previous,caps.szPname)==0) restore=entry;
    }
    SendMessageW(midiBox,CB_SETCURSEL,restore,0);
    if(!languageUpdating) {
        SendMessageW(midiBox,WM_SETREDRAW,TRUE,0); InvalidateRect(midiBox,nullptr,TRUE);
    }
    if(selected>0 && !restore) closeMidi();
}
void updateAdvancedLanguage() {
    if (!outputBox)
        return;
    if (polyBox) {
        SendMessageW(polyBox, CB_RESETCONTENT, 0, 0);
        for (auto name : {L"Polyphonic", L"Monophonic"})
            SendMessageW(polyBox, CB_ADDSTRING, 0, LPARAM(name));
        SendMessageW(polyBox, CB_SETCURSEL, liveMidiPolyphonic->load() ? 0 : 1, 0);
        SendMessageW(viewBox, CB_RESETCONTENT, 0, 0);
        for (auto name :
             {tr(L"チャンネル別鍵盤", L"Channel keyboards"), tr(L"縦ピアノロール", L"Vertical piano roll")})
            SendMessageW(viewBox, CB_ADDSTRING, 0, LPARAM(name));
        SendMessageW(viewBox, CB_SETCURSEL, pianoRoll ? 1 : 0, 0);
    }
    int output = int(SendMessageW(outputBox, CB_GETCURSEL, 0, 0));
    SendMessageW(outputBox, CB_RESETCONTENT, 0, 0);
    for (auto name : {L"Independent OPM DLL", L"X68Sound DLL", L"C86CTL (OPM / YM2151)"})
        SendMessageW(outputBox, CB_ADDSTRING, 0, LPARAM(name));
    SendMessageW(outputBox, CB_SETCURSEL, std::max(0, output), 0);
    refreshMidiDevices();
    if (voiceListLabel)
        SetWindowTextW(voiceListLabel, tr(L"音色一覧（* 使用済み）", L"Instruments (* used)"));
    if (voiceWindow)
        SetWindowTextW(voiceWindow, tr(L"FM音色 — MML / レジスタ / 接続図",
                                       L"FM instruments — MML / registers / algorithm"));
}
void exportVoices() {
    try {
        if (lastPlayedMdx.empty())
            throw std::runtime_error("Play an MDX file before exporting its instruments");
        mdx::Song song(mdx::readFile(lastPlayedMdx));
        wchar_t filename[32768]{};
        auto defaultName=lastPlayedMdx.filename().replace_extension(L".mml").wstring();
        wcsncpy(filename,defaultName.c_str(),32767);
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = window;
        dialog.lpstrFilter = L"MXDRV MML (*.mml)\0*.mml\0Text (*.txt)\0*.txt\0\0";
        dialog.lpstrFile = filename;
        dialog.nMaxFile = 32768;
        dialog.lpstrDefExt = L"mml";
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&dialog))
            return;
        auto target = std::filesystem::path(filename);
        if (std::filesystem::weakly_canonical(target) == std::filesystem::weakly_canonical(lastPlayedMdx))
            throw std::runtime_error("Do not overwrite the MDX input");
        std::ofstream out(target, std::ios::binary);
        out << mdx::songVoicesMml(song);
        if (!out)
            throw std::runtime_error("Cannot save instrument MML");
        status(tr(L"全FM音色をMML保存しました", L"Saved all FM instruments as MML"));
    } catch (const std::exception &e) {
        status(L"ERROR: " + wide(e.what()));
    }
}
bool rebuildingVoiceList = false;
int selectedVoice = -1;
uint64_t displayedCatalog = UINT64_MAX;
std::array<bool, 256> displayedUsed{};
int shownAlgorithm = 0, shownFeedback = 0;
std::wstring shownMml, shownRegisters;
HFONT voiceFont = nullptr;
#include "voice_controls.h"
#include "voice_editor_ui.h"
void refreshVoiceWindow() {
    if (!voiceWindow || !IsWindowVisible(voiceWindow))
        return;
    bool catalogChanged = false;
    {
        uint64_t revision;
        std::array<bool,256> used,defined;
        {
            std::lock_guard<std::mutex> lock(retainedVoices->mutex);
            revision=retainedVoices->catalogRevision; used=retainedVoices->used; defined=retainedVoices->defined;
        }
        if (displayedCatalog != revision || displayedUsed != used) {
            if (displayedCatalog != revision) {
                selectedVoice = -1;
                catalogChanged = true;
            }
            displayedCatalog = revision;
            displayedUsed = used;
            rebuildingVoiceList = true;
            SendMessageW(voiceList, WM_SETREDRAW, FALSE, 0);
            ListView_DeleteAllItems(voiceList);
            int index = 0;
            for (int id = 0; id < 256; ++id)
                if (defined[id]) {
                    wchar_t title[32];
                    swprintf(title, 32, L"%s@%03d", displayedUsed[id] ? L"* " : L"", id);
                    LVITEMW item{};
                    item.mask = LVIF_TEXT | LVIF_PARAM;
                    item.iItem = index;
                    item.pszText = title;
                    item.lParam = id;
                    ListView_InsertItem(voiceList, &item);
                    if (id == selectedVoice)
                        ListView_SetItemState(voiceList, index, LVIS_SELECTED, LVIS_SELECTED);
                    ++index;
                }
            rebuildingVoiceList = false;
            SendMessageW(voiceList, WM_SETREDRAW, TRUE, 0);
            InvalidateRect(voiceList, nullptr, TRUE);
        }
    }
    // A focused edit freezes the text: selecting/copying never races a refresh.
    if (!catalogChanged && (GetFocus() == voiceEdit || GetFocus() == registerEdit))
        return;
    if (!catalogChanged && GetCapture() &&
        std::find(parameterKnobs.begin(),parameterKnobs.end(),GetCapture())!=parameterKnobs.end()) return;
    int ch = std::max(0, int(SendMessageW(voiceChannel, CB_GETCURSEL, 0, 0)));
    std::array<uint8_t, 256> regs;
    mdx::Voice voice;
    int id = ch;
    const auto previousBaseline=baselineVoice;
    const bool previousBaselineValid=baselineVoiceValid;
    baselineVoiceValid=false;
    {
        std::lock_guard<std::mutex> lock(retainedVoices->mutex);
        regs = retainedVoices->registers;
        voice = mdx::registerVoice(regs, ch, retainedVoices->voices[ch][2]);
        if (retainedVoices->ids[ch] >= 0)
            id = retainedVoices->ids[ch];
        if (selectedVoice >= 0) id = selectedVoice;
        if (id>=0 && id<256 && retainedVoices->defined[id]) {
            voice = retainedVoices->definitions[id];
            regs.fill(0);
            regs[0x20 + ch] = voice[1];
            for (int g = 0; g < 6; ++g)
                for (int op = 0; op < 4; ++op)
                    regs[0x40 + g * 32 + op * 8 + ch] = voice[3 + g * 4 + op];
        }
    }
    {
        std::lock_guard<std::mutex> lock(retainedVoices->mutex);
        baselineVoiceValid=id>=0 && id<256 && retainedVoices->initialDefined[id];
        if(baselineVoiceValid) baselineVoice=retainedVoices->initialDefinitions[id];
    }
    voice[0]=uint8_t(id);
    baselineParameters=mdx::voiceParameterValues(baselineVoice);
    syncVoiceParameters(voice);
    for(auto knob:parameterKnobs) {
        if(bool(IsWindowEnabled(knob))!=baselineVoiceValid) EnableWindow(knob,baselineVoiceValid);
        if(previousBaseline!=baselineVoice || previousBaselineValid!=baselineVoiceValid)
            InvalidateRect(knob,nullptr,FALSE);
    }
    if(bool(GetWindowLongPtrW(voiceEdit,GWL_STYLE)&ES_READONLY)==baselineVoiceValid)
        SendMessageW(voiceEdit,EM_SETREADONLY,!baselineVoiceValid,0);
    voiceDirty=baselineVoiceValid && voice!=baselineVoice;
    EnableWindow(voiceReset,voiceDirty);
    auto mml = wide(mdx::voiceMml(voice, id)), reg = wide(mdx::voiceRegisters(regs, ch));
    if (mml != shownMml) {
        shownMml = mml;
        updatingVoiceEdit=true;
        SetWindowTextW(voiceEdit, mml.c_str());
        updatingVoiceEdit=false;
        colorVoiceParameters();
    }
    if (previousBaseline != baselineVoice || previousBaselineValid != baselineVoiceValid)
        colorVoiceParameters();
    if (reg != shownRegisters) {
        shownRegisters = reg;
        SetWindowTextW(registerEdit, reg.c_str());
    }
    if (shownAlgorithm != (voice[1] & 7) || shownFeedback != ((voice[1] >> 3) & 7)) {
        shownAlgorithm = voice[1] & 7;
        shownFeedback = (voice[1] >> 3) & 7;
        InvalidateRect(voiceWindow, nullptr, FALSE);
    }
}
void applyVoiceNumber(int id) {
    selectedVoice = id;
    int channel = int(SendMessageW(voiceChannel, CB_GETCURSEL, 0, 0));
    if (channel >= 0 && channel < 8) {
        {
            std::lock_guard<std::mutex> lock(retainedVoices->mutex);
            auto &v = retainedVoices->definitions[selectedVoice];
            retainedVoices->voices[channel] = v;
            retainedVoices->ids[channel] = selectedVoice;
            retainedVoices->valid[channel] = true;
            retainedVoices->used[selectedVoice] = true;
            auto &regs = retainedVoices->registers;
            regs[0x20 + channel] = (regs[0x20 + channel] & 192) | (v[1] & 63);
            for (int g = 0; g < 6; ++g)
                for (int op = 0; op < 4; ++op)
                    regs[0x40 + g * 32 + op * 8 + channel] = v[3 + g * 4 + op];
        }
        retainedVoices->requestedVoice[channel] = selectedVoice;
        // A private queue command retains all 8 bits of the MDX voice number.
        midiQueue->push(0xf0u | uint32_t(channel) | (uint32_t(selectedVoice) << 8));
    }
    SetFocus(voiceList);
    refreshVoiceWindow();
}
#include "voice_keyboard.h"
LRESULT CALLBACK voiceProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_GETMINMAXINFO) {
        auto info = reinterpret_cast<MINMAXINFO *>(lp);
        info->ptMinTrackSize = voiceMinimumSize();
        return 0;
    }
    if (msg == WM_CREATE) {
        auto child = [&](const wchar_t *cls, const wchar_t *text, DWORD style, int id) {
            return CreateWindowW(cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1, hwnd,
                                 HMENU(INT_PTR(id)), GetModuleHandleW(nullptr), nullptr);
        };
        voiceKeyboard = child(L"STATIC", L"", SS_NOTIFY | WS_TABSTOP, 6);
        SetWindowSubclass(voiceKeyboard, auditionProcedure, 1, 0);
        voiceChannel = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 1);
        for (int ch = 0; ch < 8; ++ch) {
            std::wstring text = L"FM ";
            text += wchar_t('A' + ch);
            SendMessageW(voiceChannel, CB_ADDSTRING, 0, LPARAM(text.c_str()));
        }
        SendMessageW(voiceChannel, CB_SETCURSEL, 0, 0);
        voiceEdit =
            child(MSFTEDIT_CLASS, L"",
                  ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, 2);
        SendMessageW(voiceEdit, EM_SETBKGNDCOLOR, 0, editColor);
        SendMessageW(voiceEdit, EM_SETEVENTMASK, 0, ENM_CHANGE);
        SendMessageW(voiceEdit, EM_SETTARGETDEVICE, 0, 1); // Disable wrapping.
    const wchar_t *headers[] = {L"AR", L"DR", L"SR",  L"RR",  L"SL", L"OL",
                                L"KS", L"ML", L"DT1", L"DT2", L"AME"};
    for (int col = 0; col < 11; ++col)
      parameterHeaders[col] = child(L"STATIC", headers[col], 0, 200 + col);
    const wchar_t *globalHeaders[] = {L"CON", L"FL", L"OP"};
    for (int col = 0; col < 3; ++col)
      globalParameterHeaders[col] = child(L"STATIC", globalHeaders[col], 0, 230 + col);
    for (int row = 0; row < 4; ++row)
    {
      auto name = L"OP" + std::to_wstring(row + 1);
      parameterRows[row] = child(L"STATIC", name.c_str(), 0, 220 + row);
    }
    for (int n = 0; n < 47; ++n)
    {
      parameterKnobs[n] = child(L"STATIC", L"", SS_NOTIFY | WS_CLIPSIBLINGS | WS_TABSTOP, 300 + n);
      SetWindowSubclass(parameterKnobs[n], parameterProcedure, 3, n);
    }
        voiceReset = child(L"BUTTON",tr(L"リセット",L"Reset"),WS_TABSTOP,8);
        registerEdit =
            child(L"EDIT", L"",
                  ES_MULTILINE | ES_READONLY | ES_NOHIDESEL | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, 3);
        voiceListLabel = child(L"STATIC", tr(L"音色一覧（* 使用済み）", L"Instruments (* used)"), 0, 5);
        voiceList = child(WC_LISTVIEWW, L"", LVS_LIST | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, 4);
        ListView_SetColumnWidth(voiceList, 0, scale(95));
        ListView_SetBkColor(voiceList, editColor);
        ListView_SetTextBkColor(voiceList, editColor);
        ListView_SetTextColor(voiceList, textColor);
        ListView_SetExtendedListViewStyle(voiceList, LVS_EX_DOUBLEBUFFER);
        displayedCatalog = UINT64_MAX;
        selectedVoice = -1;
        HDC fontDc = GetDC(hwnd);
        int fontDpi = GetDeviceCaps(fontDc, LOGPIXELSY);
        ReleaseDC(hwnd, fontDc);
        voiceFont =
            CreateFontW(-MulDiv(10, fontDpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
        for (HWND h : {voiceChannel, voiceEdit, registerEdit, voiceList, voiceListLabel})
            SendMessageW(h, WM_SETFONT, WPARAM(voiceFont), TRUE);
    parameterFont = CreateFontW(-scale(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                0, 0, 0, FIXED_PITCH, L"Consolas");
    limitFont = CreateFontW(-scale(7), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0,
                            0, 0, 0, L"Segoe UI");
    for (auto h : parameterHeaders)
      SendMessageW(h, WM_SETFONT, WPARAM(voiceFont), TRUE);
    for (auto h : parameterRows)
      SendMessageW(h, WM_SETFONT, WPARAM(voiceFont), TRUE);
    for (auto h : globalParameterHeaders)
      SendMessageW(h, WM_SETFONT, WPARAM(voiceFont), TRUE);
        return 0;
    }
    if (msg == WM_SIZE && voiceWantedVisible) {
        voiceShowState = wp == SIZE_MINIMIZED ? SW_SHOWMINIMIZED :
                         wp == SIZE_MAXIMIZED ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    }
    if (msg == WM_MOVE || msg == WM_SIZE) settingsChanged();
    if (msg == WM_SIZE && wp == SIZE_MINIMIZED) return 0;
    if (msg == WM_SIZE) {
    int w = LOWORD(lp), h = HIWORD(lp), split = std::max(scale(260), w * 2 / 3);
    MoveWindow(voiceChannel, scale(12), scale(12), scale(160), scale(220), TRUE);
    MoveWindow(voiceReset, scale(190), scale(12), scale(100), scale(28), TRUE);
    MoveWindow(voiceEdit, scale(12), scale(52), std::max(scale(40), split - scale(20)),
               std::max(scale(40), h - scale(495)), TRUE);
    MoveWindow(registerEdit, split, scale(52), std::max(scale(40), w - split - scale(12)),
               std::max(scale(40), h - scale(495)), TRUE);
    int gridTop = h - scale(435), cell = (split - scale(60)) / 11;
    for (int col = 0; col < 11; ++col)
      MoveWindow(parameterHeaders[col], scale(52) + col * cell, gridTop, cell, scale(18), TRUE);
    for (int row = 0; row < 4; ++row)
      MoveWindow(parameterRows[row], scale(12), gridTop + scale(24 + row * 30),
                 scale(38), scale(16), TRUE);
    for (int col = 0; col < 3; ++col)
      MoveWindow(globalParameterHeaders[col], scale(52) + col * cell,
                 gridTop + scale(140), cell, scale(16), TRUE);
    for (int n = 0; n < 47; ++n)
    {
      int row = n < 44 ? n / 11 : 4, col = n < 44 ? n % 11 : n - 44;
      int x = scale(52) + col * cell;
      int width = std::min(cell, scale(40)),
          y = gridTop + scale(n < 44 ? 20 + row * 30 : 158);
      int left = x + (cell - width) / 2;
      MoveWindow(parameterKnobs[n], left, y, width, scale(28), TRUE);
    }
        MoveWindow(voiceKeyboard, scale(12), h - scale(248), w - scale(24), scale(96), TRUE);
        MoveWindow(voiceListLabel, scale(12), h - scale(148), w - scale(24), scale(24), TRUE);
        MoveWindow(voiceList, scale(12), h - scale(120), w - scale(24), scale(108), TRUE);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    if(msg==WM_COMMAND && LOWORD(wp)==2 && HIWORD(wp)==EN_CHANGE && !updatingVoiceEdit) {
        previewEditedVoice(false);
        return 0;
    }
    if(msg==WM_COMMAND && LOWORD(wp)==8 && baselineVoiceValid) {
        updatingVoiceEdit=true;
        SetWindowTextW(voiceEdit,wide(mdx::voiceMml(baselineVoice,baselineVoice[0])).c_str());
        updatingVoiceEdit=false;
        previewEditedVoice(false);
        return 0;
    }
    if (msg == WM_COMMAND && LOWORD(wp) == 1 && HIWORD(wp) == CBN_SELCHANGE) {
        selectedVoice = -1;
        ListView_SetItemState(voiceList, -1, 0, LVIS_SELECTED);
        SetFocus(voiceChannel);
        refreshVoiceWindow();
        return 0;
    }
    if (msg == WM_NOTIFY && reinterpret_cast<NMHDR *>(lp)->hwndFrom == voiceList) {
        auto draw = reinterpret_cast<NMLVCUSTOMDRAW *>(lp);
        if (draw->nmcd.hdr.code == NM_CUSTOMDRAW) {
            if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                int id = int(draw->nmcd.lItemlParam);
                std::lock_guard<std::mutex> lock(retainedVoices->mutex);
                bool changed = id >= 0 && id < 256 && retainedVoices->initialDefined[id] &&
                    retainedVoices->definitions[id] != retainedVoices->initialDefinitions[id];
                draw->clrText = changed ? RGB(255, 95, 95) : textColor;
                if (changed && (draw->nmcd.uItemState & CDIS_SELECTED)) {
                    // Keep changed text red even when the theme supplies selection colors.
                    draw->nmcd.uItemState &= ~CDIS_SELECTED;
                    draw->clrTextBk = RGB(50, 55, 88);
                } else draw->clrTextBk = editColor;
                return CDRF_NEWFONT;
            }
            return CDRF_DODEFAULT;
        }
        if (rebuildingVoiceList)
            return 0;
        auto n = reinterpret_cast<NMHDR *>(lp);
        if (n->code == NM_CLICK) {
            auto click = reinterpret_cast<NMITEMACTIVATE *>(lp);
            if (click->iItem >= 0) {
                LVITEMW item{};
                item.mask = LVIF_PARAM;
                item.iItem = click->iItem;
                if (ListView_GetItem(voiceList, &item))
                    applyVoiceNumber(int(item.lParam));
            }
        }
        if (n->code == LVN_ITEMCHANGED) {
            auto change = reinterpret_cast<NMLISTVIEW *>(lp);
            if ((change->uNewState & LVIS_SELECTED) && !(change->uOldState & LVIS_SELECTED)) {
                LVITEMW item{};
                item.mask = LVIF_PARAM;
                item.iItem = change->iItem;
                if (ListView_GetItem(voiceList, &item)) {
                    applyVoiceNumber(int(item.lParam));
                }
            }
        }
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        SetFocus(voiceChannel);
        refreshVoiceWindow();
        return 0;
    }
    if (msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLOREDIT) {
        SetTextColor(HDC(wp), textColor);
        SetBkColor(HDC(wp), editColor);
        return LRESULT(editBrush);
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client;
        GetClientRect(hwnd, &client);
        RECT area{client.right*2/3, std::max(0L,client.bottom-scale(435)),
                  client.right-scale(8),client.bottom-scale(255)};
        FillRect(dc, &area, panelBrush);
        auto font = SelectObject(dc, voiceFont);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, textColor);
        std::wstring title =
            L"ALG " + std::to_wstring(shownAlgorithm) + L"   FB " + std::to_wstring(shownFeedback);
        TextOutW(dc, area.left+scale(16), area.top + scale(8), title.c_str(), int(title.size()));
        auto topology = mdx::algorithmLayout(shownAlgorithm);
        int bw = scale(28), bh = scale(23), margin = int(area.left)+scale(20), right = int(area.right) - scale(24);
        int columnWidth = std::max(bw + scale(6), (right - margin) / (topology.maxLevel + 2));
        POINT point[4];
        for (int op = 0; op < 4; ++op) {
            int usable = std::max(scale(80), int(area.bottom - area.top) - scale(40));
            point[op] = {margin + topology.level[op] * columnWidth,
                         int(area.top) + scale(32) +
                             (2 * topology.row[op] + 1) * usable / (2 * topology.rows[op]) - bh / 2};
        }
        auto pen = CreatePen(PS_SOLID, 2, RGB(92, 210, 255));
        auto old = SelectObject(dc, pen);
        auto arrow = [&](int x1, int y1, int x2, int y2) {
            int middle = (x1 + x2) / 2;
            MoveToEx(dc, x1, y1, nullptr);
            LineTo(dc, middle, y1);
            LineTo(dc, middle, y2);
            LineTo(dc, x2, y2);
            LineTo(dc, x2 - 5, y2 - 4);
            MoveToEx(dc, x2, y2, nullptr);
            LineTo(dc, x2 - 5, y2 + 4);
        };
        for (int i = 0; i < topology.edgeCount; ++i) {
            int a = topology.edges[i][0], b = topology.edges[i][1];
            arrow(point[a].x + bw, point[a].y + bh / 2, point[b].x, point[b].y + bh / 2);
        }
        int busX = margin + (topology.maxLevel + 1) * columnWidth;
        int minY = INT_MAX, maxY = 0;
        for (int op = 0; op < 4; ++op)
            if (topology.carriers & (1 << op)) {
                int y = point[op].y + bh / 2;
                MoveToEx(dc, point[op].x + bw, y, nullptr);
                LineTo(dc, busX, y);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        MoveToEx(dc, busX, minY, nullptr);
        LineTo(dc, busX, maxY);
        arrow(busX, (minY + maxY) / 2, right, (minY + maxY) / 2);
        if (shownFeedback) {
            int x = point[0].x, y = point[0].y;
            MoveToEx(dc, x + bw / 2, y, nullptr);
            LineTo(dc, x + bw / 2, y - 12);
            LineTo(dc, x - 14, y - 12);
            LineTo(dc, x - 14, y + bh / 2);
            LineTo(dc, x, y + bh / 2);
        }
        for (int op = 0; op < 4; ++op) {
            RECT box{point[op].x, point[op].y, point[op].x + bw, point[op].y + bh};
            FillRect(dc, &box, editBrush);
            FrameRect(dc, &box, accentBrush);
            auto name = L"OP" + std::to_wstring(op + 1);
            DrawTextW(dc, name.c_str(), -1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(dc, old);
        DeleteObject(pen);
        SelectObject(dc, font);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE)
        releaseVoiceKeys();
    if (msg == WM_CLOSE) {
        releaseVoiceKeys();
        voiceWantedVisible = false;
        settingsChanged();
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    if (msg == WM_DESTROY) {
        releaseVoiceKeys();
        voiceKeyboard = nullptr;
        DeleteObject(parameterFont);
        DeleteObject(limitFont);
        parameterKnobs.fill(nullptr);
        DeleteObject(voiceFont);
        voiceFont = nullptr;
        voiceWindow = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
void showVoices(bool toggle) {
    if (voiceWindow && toggle && voiceWantedVisible) {
        SendMessageW(voiceWindow, WM_CLOSE, 0, 0);
        return;
    }
    bool created = !voiceWindow;
    static HMODULE richEditModule = LoadLibraryW(L"Msftedit.dll");
    if (!richEditModule) return;
    if (!voiceWindow) {
        WNDCLASSW cls{};
        cls.lpfnWndProc = voiceProcedure;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpszClassName = L"IndependentMDXVoices";
        cls.hbrBackground = panelBrush;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&cls);
        auto minimum = voiceMinimumSize();
        voiceWindow = CreateWindowW(cls.lpszClassName, L"", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                    minimum.x, minimum.y, window, nullptr, cls.hInstance, nullptr);
        if (!voiceWindow) return;
        restoreVoicePlacement();
    }
    updateAdvancedLanguage();
    voiceWantedVisible = true;
    int show = SW_SHOW;
    if (created) {
        int saved = int(setting(L"VoiceShow", SW_SHOWNORMAL));
        if (saved == SW_SHOWMAXIMIZED) show = SW_SHOWMAXIMIZED;
        else if (saved == SW_SHOWMINIMIZED || saved == SW_SHOWMINNOACTIVE) show = SW_SHOWMINNOACTIVE;
        else show = SW_SHOWNORMAL;
    }
    if (created) voiceShowState = show;
    ShowWindow(voiceWindow, show);
    settingsChanged();
    if (show != SW_SHOWMINNOACTIVE) SetForegroundWindow(voiceWindow);
    refreshVoiceWindow();
}
