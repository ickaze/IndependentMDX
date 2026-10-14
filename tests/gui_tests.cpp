// Windows-only tests. Use an isolated temporary INI file; never user preferences.
#include "../src/gui.cpp"
#include <iostream>
void requireUi(bool condition, const char *reason) {
    if (!condition)
        throw std::runtime_error(reason);
}
void pump(DWORD milliseconds) {
    ULONGLONG end = GetTickCount64() + milliseconds;
    do {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message != WM_QUIT) {
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }
        }
        Sleep(5);
    } while (GetTickCount64() < end);
}
int wmain() {
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls{sizeof(controls),
                                  ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    auto temp = std::filesystem::temp_directory_path() /
                (L"IndependentMDX-Test-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(temp / L"child");
    settingsPath = temp / L"IndependentMDX.ini";
    std::filesystem::remove(settingsPath);
    for (const char *name : {"2.mdx", "10.mdx", "sample.pdx", "ignore.txt"})
        std::ofstream(temp / name) << "test";
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW cls{};
    cls.hInstance = instance;
    cls.lpfnWndProc = procedure;
    cls.lpszClassName = L"IndependentMDXGuiTest";
    RegisterClassW(&cls);
    HWND test =
        CreateWindowW(cls.lpszClassName, L"UI regression", WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL, 0,
                      0, 1320, 840, nullptr, nullptr, instance, nullptr);
    if (!test) {

        std::filesystem::remove_all(temp);
        return 1;
    }
    int result = 0;
    try {
        requireUi(columns.front() == 9 && std::wstring(columnName(9)) == tr(L"曲名", L"Song title"),
                  "title is not first default column");
        requireUi(SendMessageW(outputBox, CB_GETCOUNT, 0, 0) == 3, "output choices missing");
        requireUi(SendMessageW(midiBox, CB_GETCOUNT, 0, 0) >= 1, "MIDI choices missing");

        {
            EnableWindow(seekSlider, TRUE);
            songDuration = 100;
            RECT channel{};
            SendMessageW(seekSlider, TBM_GETCHANNELRECT, 0, LPARAM(&channel));
            int x = channel.left + (channel.right - 1 - channel.left) * 3 / 4;
            int y = (channel.top + channel.bottom) / 2;
            SendMessageW(seekSlider, TBM_SETPOS, FALSE, 0);
            SendMessageW(seekSlider, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
            SendMessageW(seekSlider, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
            requireUi(std::abs(seekRequest.load() - 75) < 1, "blank track click did not seek to pointer");
            RECT thumb{};
            SendMessageW(seekSlider, TBM_GETTHUMBRECT, 0, LPARAM(&thumb));
            requireUi(std::abs((thumb.left + thumb.right) / 2 - x) <= 2, "thumb not visually at click");
            requireUi(!draggingSeek, "seek drag state stuck");
            songDuration = 0;
            seekRequest = -1;
        }
        requireUi(!liveAftertouch->load(), "aftertouch must default off");
        requireUi(liveGain->load() == 1 && liveIndependentGain->load() == 2 && livePcmGain->load() == 1,
                  "software gain defaults");
        SetWindowTextW(gainEdit, L"125");
        captureOutputGains();
        selectedOutput = 2;
        applyOutputGains();
        requireUi(liveGain->load() == 1 && liveIndependentGain->load() == 1, "hardware gain defaults");
        SetWindowTextW(independentGainEdit, L"80");
        captureOutputGains();
        selectedOutput = 0;
        applyOutputGains();
        requireUi(liveGain->load() == 1.25 && liveIndependentGain->load() == 2, "output gains mixed");
        SendMessageW(aftertouchButton, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(test, WM_COMMAND, MAKEWPARAM(49, BN_CLICKED), LPARAM(aftertouchButton));
        requireUi(liveAftertouch->load(), "aftertouch switch");
        {
            mdx::MonitorFrame f;
            f.note.fill(-1);
            rollWallTime = 0;
            rollTime = 0;
            rollMoving = false;
            rollHistory.clear();
            running = false;
            captureRoll(f);
            auto frozen = rollTime;
            Sleep(50);
            captureRoll(f);
            requireUi(rollTime == frozen, "stopped roll advanced");
            running = true;
            pauseFlag = true;
            retainedVoices->midiNotes[0] = 60;
            captureRoll(f);
            Sleep(50);
            captureRoll(f);
            requireUi(rollTime > frozen, "paused MIDI roll did not advance");
            retainedVoices->midiNotes[0] = -1;
            captureRoll(f);
            frozen = rollTime;
            Sleep(50);
            captureRoll(f);
            requireUi(rollTime == frozen, "paused silent roll advanced");
            running = false;
            pauseFlag = false;
        }
        {
            auto rowOf = [&](HWND h) {
                for (auto &it : items)
                    if (it.handle == h)
                        return it.row;
                return -1;
            };
            requireUi(rowOf(aftertouchButton) == 18 && rowOf(viewBox) == 21 && rowOf(carrierButton)==21, "functional grouping missing");
            layout();
            RECT client, list, keyboard;
            GetClientRect(test, &client);
            GetWindowRect(browserList, &list);
            MapWindowPoints(nullptr, test, reinterpret_cast<POINT *>(&list), 2);
            requireUi(list.bottom <= client.bottom && list.bottom - list.top >= scale(120),
                      "selector clipped at default size");
            GetWindowRect(keyboards, &keyboard);
            int originalWidth = keyboard.right - keyboard.left;
            splitPermille = 500;
            layout();
            GetWindowRect(keyboards, &keyboard);
            requireUi(keyboard.right - keyboard.left > originalWidth, "splitter did not widen keyboard");
            // No message pump between layouts: reproduce continuous captured moves.
            ShowWindow(test, SW_SHOWNOACTIVATE);
            bufferSplitter(true);
            requireUi(splitterOverlay && splitterDC &&
                          !(GetWindowLongPtrW(test, GWL_EXSTYLE) & WS_EX_COMPOSITED),
                      "drag buffering missing");
            int paints = 0;
            auto probe = +[](HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) -> LRESULT {
                if (m == WM_PRINT)
                    ++*reinterpret_cast<int *>(data);
                return DefSubclassProc(h, m, w, l);
            };
            SetWindowSubclass(keyboards, probe, 77, DWORD_PTR(&paints));
            SetWindowSubclass(seekSlider, probe, 77, DWORD_PTR(&paints));
            for (int ratio : {550, 600, 500}) {
                paints = 0;
                splitPermille = ratio;
                layout();
                requireUi(paints >= 2, "keyboard/seek paint deferred during splitter movement");
                requireUi(!GetUpdateRect(splitterOverlay, nullptr, FALSE),
                          "splitter left dirty child regions");
            }
            bufferSplitter(false);
            requireUi(!splitterOverlay && !splitterDC && !splitterBuffered, "drag buffering stuck");
            RemoveWindowSubclass(keyboards, probe, 77);
            RemoveWindowSubclass(seekSlider, probe, 77);
            RECT before, after;
            GetWindowRect(test, &before);
            auto style = GetWindowLongPtrW(test, GWL_STYLE);
            SendMessageW(test, WM_SYSKEYDOWN, VK_RETURN, LPARAM(1) << 29);
            requireUi(fullscreen && !(GetWindowLongPtrW(test, GWL_STYLE) & WS_CAPTION), "fullscreen failed");
            SendMessageW(test, WM_SYSKEYDOWN, VK_RETURN, LPARAM(1) << 29);
            GetWindowRect(test, &after);
            requireUi(!fullscreen && GetWindowLongPtrW(test, GWL_STYLE) == style &&
                          EqualRect(&before, &after),
                      "window restoration failed");
        }
        requireUi(liveSpeed->load() == 100 && !carrierPitch, "new control defaults");
        SendMessageW(speedSlider, TBM_SETPOS, TRUE, 200);
        SendMessageW(test, WM_HSCROLL, TB_THUMBPOSITION, LPARAM(speedSlider));
        requireUi(liveSpeed->load() == 200, "speed slider wiring");
        liveSpeed->store(100);
        requireUi(liveMidiPolyphonic->load(), "Polyphonic must be default");
        SendMessageW(polyBox, CB_SETCURSEL, 1, 0);
        SendMessageW(test, WM_COMMAND, MAKEWPARAM(44, CBN_SELCHANGE), LPARAM(polyBox));
        requireUi(!liveMidiPolyphonic->load(), "Monophonic switch not connected");
        SendMessageW(viewBox, CB_SETCURSEL, 1, 0);
        SendMessageW(test, WM_COMMAND, MAKEWPARAM(45, CBN_SELCHANGE), LPARAM(viewBox));
        requireUi(pianoRoll, "piano roll selector not connected");
        SendMessageW(masterSlider, TBM_SETPOS, TRUE, 400);
        SendMessageW(test, WM_HSCROLL, TB_THUMBPOSITION, LPARAM(masterSlider));
        requireUi(liveGain->load() == 4 && integer(gainEdit, 0, 400) == 400, "master slider range/wiring");
        SetWindowTextW(pcmGainEdit, L"175");
        requireUi(livePcmGain->load() == 1.75, "PCM gain not live");
        SetWindowTextW(fmDelayEdit, L"35");
        SetWindowTextW(pcmDelayEdit, L"50");
        // Return to channel layout for the older click-to-mask regression below.
        SendMessageW(viewBox, CB_SETCURSEL, 0, 0);
        SendMessageW(test, WM_COMMAND, MAKEWPARAM(45, CBN_SELCHANGE), LPARAM(viewBox));
        requireUi(liveMeasuredOutputTiming->load(), "measured timing default");
        requireUi(SendMessageW(mdxFilter, BM_GETCHECK, 0, 0) == BST_CHECKED &&
                      SendMessageW(pdxFilter, BM_GETCHECK, 0, 0) == BST_CHECKED,
                  "filter defaults");
        requireUi(SendMessageW(keyModeBox, CB_GETCOUNT, 0, 0) == 3, "key mode choices missing");
        SendMessageW(keyModeBox, CB_SETCURSEL, 2, 0);
        SendMessageW(test, WM_COMMAND, MAKEWPARAM(37, CBN_SELCHANGE), LPARAM(keyModeBox));
        requireUi(liveKeyMode->load() == 2, "live key mode wiring");
        SetWindowTextW(loopsEdit, L"7");
        RECT keys;
        GetClientRect(keyboards, &keys);
        int row = std::max(1, int(keys.bottom) / 16);
        SendMessageW(keyboards, WM_LBUTTONDOWN, MK_LBUTTON,
                     MAKELPARAM(scale(50), 15 * row + row / 2));
        requireUi(livePcmMuteMask->load() == 128, "W keyboard mute wiring");
        requireUi(monitorView->waveforms.load(), "waveforms not enabled by default");
        SendMessageW(waveformButton,BM_SETCHECK,BST_UNCHECKED,0);
        SendMessageW(test,WM_COMMAND,MAKEWPARAM(53,BN_CLICKED),LPARAM(waveformButton));
        requireUi(!monitorView->waveforms.load(), "waveform switch did not reach renderer");
        SendMessageW(waveformButton,BM_SETCHECK,BST_CHECKED,0);
        SendMessageW(test,WM_COMMAND,MAKEWPARAM(53,BN_CLICKED),LPARAM(waveformButton));
        for(HWND slider:{speedSlider,keySlider,masterSlider}) {
            int saved=int(SendMessageW(slider,TBM_GETPOS,0,0));
            RECT channel{}; SendMessageW(slider,TBM_GETCHANNELRECT,0,LPARAM(&channel));
            for(double fraction:{0.0,0.75,1.0}) {
                int x=channel.left+int((channel.right-channel.left-1)*fraction);
                auto point=MAKELPARAM(x,(channel.top+channel.bottom)/2);
                SendMessageW(slider,WM_LBUTTONDOWN,MK_LBUTTON,point);
                SendMessageW(slider,WM_LBUTTONUP,0,point);
                int low=int(SendMessageW(slider,TBM_GETRANGEMIN,0,0));
                int high=int(SendMessageW(slider,TBM_GETRANGEMAX,0,0));
                requireUi(std::abs(int(SendMessageW(slider,TBM_GETPOS,0,0))-(low+int(std::lround(fraction*(high-low)))))<=2,
                          "absolute value-slider click missed target");
            }
            SendMessageW(slider,TBM_SETPOS,TRUE,saved);
            SendMessageW(test,WM_HSCROLL,TB_THUMBTRACK,LPARAM(slider));
        }
        SendMessageW(midiBox,CB_SETCURSEL,0,0);
        SendMessageW(test,WM_COMMAND,MAKEWPARAM(39,CBN_DROPDOWN),LPARAM(midiBox));
        requireUi(SendMessageW(midiBox,CB_GETCURSEL,0,0)==0,"device refresh changed No MIDI input");
        std::vector<RECT> previousLayout;
        for (int language : {1, 0, 1}) {
            SendMessageW(languageBox, CB_SETCURSEL, language, 0);
            changeLanguage(false);
            requireUi(SendMessageW(keyModeBox, CB_GETCURSEL, 0, 0) == 2, "language lost key mode");
            auto boundsOf=[](HWND h){RECT r{};GetWindowRect(h,&r);return r;};
            std::vector<RECT> currentLayout;
            for(const auto &it:items) currentLayout.push_back(boundsOf(it.handle));
            if(!previousLayout.empty()) for(size_t i=0;i<currentLayout.size();++i)
                requireUi(EqualRect(&previousLayout[i],&currentLayout[i]), "language changed control geometry");
            previousLayout=currentLayout;
            auto key=boundsOf(keySlider), speed=boundsOf(speedSlider), master=boundsOf(masterSlider);
            requireUi(key.left==speed.left && key.right==speed.right && key.left==master.left && key.right==master.right,
                      "sliders do not share columns");
            auto pcm=boundsOf(pcmGainEdit), pcmDelay=boundsOf(pcmDelayEdit), gain=boundsOf(gainEdit);
            requireUi(pcmDelay.left>boundsOf(fmDelayEdit).right && pcm.right==gain.right, "numeric columns misaligned");
            auto keyboard=boundsOf(keyboards), view=boundsOf(viewBox), carrier=boundsOf(carrierButton);
            requireUi(view.bottom<=keyboard.top && boundsOf(keyboardHint).right==keyboard.right && view.left>=keyboard.left && carrier.left<boundsOf(waveformButton).left,
                      "view controls not aligned above keyboard");
            for(size_t i=0;i<items.size();++i) for(size_t j=i+1;j<items.size();++j) {
                RECT intersection;
                requireUi(!IntersectRect(&intersection,&currentLayout[i],&currentLayout[j]), "controls overlap");
            }

            for (const auto &it : items) {
                if ((it.kind == ControlKind::button || it.kind == ControlKind::label) && it.handle!=keyboardHint) {
                    wchar_t label[512];
                    GetWindowTextW(it.handle, label, 512);
                    requireUi(std::wstring(label) == tr(it.ja, it.en), "fixed label not translated");
                    HDC dc = GetDC(it.handle);
                    auto old = SelectObject(dc, uiFont);
                    SIZE size{};
                    GetTextExtentPoint32W(dc, label, int(wcslen(label)), &size);
                    SelectObject(dc, old);
                    ReleaseDC(it.handle, dc);
                    RECT bounds;
                    GetClientRect(it.handle, &bounds);
                    requireUi(bounds.right >= size.cx + scale(20) && bounds.bottom >= size.cy,
                              "label clipped");
                }
                if (it.kind == ControlKind::combo) {
                    RECT drop, field;
                    SendMessageW(it.handle, CB_GETDROPPEDCONTROLRECT, 0, (LPARAM)&drop);
                    GetWindowRect(it.handle, &field);
                    requireUi(drop.bottom - drop.top > field.bottom - field.top + scale(40),
                              "combo list height collapsed");
                }
            }
            requireUi(integer(loopsEdit, 1, 1000) == 7 && livePcmMuteMask->load() == 128,
                      "language change loses state");
        }
        auto archiveFixture=std::filesystem::path(__FILE__).parent_path()/"fixtures/browser.zip";
        navigate(archiveFixture);
        requireUi(folder==std::filesystem::absolute(archiveFixture).lexically_normal(),"ZIP virtual folder path");
        auto archiveSub=folder/std::filesystem::u8path(u8"日本語");
        navigate(archiveSub);
        requireUi(files.size()==3,"ZIP child listing");
        auto savedBookmarks=bookmarks;
        extrasCommand(MAKEWPARAM(108,BN_CLICKED));
        requireUi(bookmarks.back()==archiveSub,"ZIP bookmark stores virtual path");
        navigate(temp);
        navigate(bookmarks.back());
        requireUi(folder==archiveSub,"ZIP bookmark restore");
        bookmarks=savedBookmarks;updateBookmarks();
        navigate(archiveSub.parent_path());
        int zipSelection=ListView_GetNextItem(browserList,-1,LVNI_SELECTED);
        requireUi(zipSelection>=0 && files[zipSelection].path==archiveSub,"ZIP parent selection");
        navigate(temp);
        requireUi(files.size() == 4 && files[0].directory() && files[1].name == L"2.mdx" &&
                      files[2].name == L"10.mdx",
                  "browser natural sort or filters");
        SendMessageW(pdxFilter, BM_SETCHECK, BST_UNCHECKED, 0);
        navigate(temp, false);
        requireUi(files.size() == 3, "PDX filter");
        SendMessageW(mdxFilter, BM_SETCHECK, BST_UNCHECKED, 0);
        navigate(temp, false);
        requireUi(files.size() == 1 && files[0].directory(), "folders must remain navigable");
        SendMessageW(mdxFilter, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(pdxFilter, BM_SETCHECK, BST_CHECKED, 0);
        navigate(temp, false);
        input = temp / L"2.mdx";
        requireUi(nextFile() == temp / L"10.mdx", "next file follows natural order");
        input = temp / L"sample.pdx";
        requireUi(nextFile().empty(), "auto next wraps unexpectedly");
        input.clear();
        std::ofstream(temp / L"2.mdx", std::ios::binary) << "Test song title\r\n\x1a";
        navigate(temp, false);
        requireUi(files[1].title == L"Test song title", "MDX title preview");
        for (int i = 0; i < 60; ++i)
            std::filesystem::create_directory(temp / (L"folder" + std::to_wstring(i)));
        navigate(temp, false);
        selectPath(temp / L"folder50");
        int oldTop = ListView_GetTopIndex(browserList);
        navigate(temp / L"folder50");
        navigate(temp);
        int selection = ListView_GetNextItem(browserList, -1, LVNI_SELECTED);
        requireUi(selection >= 0 && files[selection].path == temp / L"folder50" &&
                      ListView_GetTopIndex(browserList) == oldTop,
                  "parent navigation loses folder selection/scroll");
        extrasCommand(MAKEWPARAM(108, BN_CLICKED));
        requireUi(bookmarks.size() == 1 && bookmarks[0] == temp, "bookmark add");
        SendMessageW(infiniteButton, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(autoNextButton, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(keySlider, TBM_SETPOS, TRUE, -24);
        SendMessageW(test, WM_HSCROLL, TB_THUMBPOSITION, (LPARAM)keySlider);
        requireUi(liveKeyShift->load() == -24, "live key slider");
        requireUi(SendMessageW(keyModeBox, CB_GETCOUNT, 0, 0) == 3, "key mode choices missing");
        SendMessageW(keyModeBox, CB_SETCURSEL, 2, 0);
        SendMessageW(test, WM_COMMAND, MAKEWPARAM(37, CBN_SELCHANGE), LPARAM(keyModeBox));
        requireUi(liveKeyMode->load() == 2, "live key mode wiring");
        SetWindowTextW(loopsEdit, L"7");
        columns = {0, 2, 4, 6};
        rebuildColumns();
        int order[] = {3, 0, 1, 2};
        ListView_SetColumnOrderArray(browserList, 4, order);
        ListView_SetColumnWidth(browserList, 0, scale(270));
        settingsChanged();
        pump(1000);
        requireUi(setting(L"Loops", 999) == 999, "preferences saved before debounce");
        lastPlayedFile = temp / L"10.mdx";
        lastPlayedBank = temp / L"sample.pdx";
        SetWindowTextW(loopsEdit, L"8");
        pump(1200);
        requireUi(setting(L"Loops", 999) == 999, "debounce did not restart");
        pump(1100);
        requireUi(settingText(L"LastPlayedFile") == lastPlayedFile.wstring() &&
                      settingText(L"LastPlayedPdx") == lastPlayedBank.wstring(),
                  "last played file not persisted");
        requireUi(setting(L"Loops", 999) == 8 && setting(L"PcmMask", 0) == 128,
                  "timer did not persist settings");
        requireUi(settingText(L"Columns") == L"6,0,2,4" && setting(L"ColumnWidth0", 0) == 270,
                  "column order/width not saved");
        settingsReady = false;
        SetWindowTextW(loopsEdit, L"2");
        livePcmMuteMask->store(0);
        liveKeyMode->store(0);
        readSettings();
        requireUi(liveKeyMode->load() == 2, "key mode not restored");
        requireUi(splitPermille == 500 && liveAftertouch->load() && outputGains[2].fm == 80,
                  "new preferences not restored");
        requireUi(!liveMidiPolyphonic->load() && liveGain->load() == 4 && livePcmGain->load() == 1.75 &&
                      integer(fmDelayEdit, 0, 2000) == 35 && integer(pcmDelayEdit, 0, 2000) == 50,
                  "new audio preferences not restored");
        settingsReady = true;
        requireUi(integer(loopsEdit, 1, 1000) == 8 && livePcmMuteMask->load() == 128 &&
                      columns == std::vector<int>({6, 0, 2, 4}),
                  "settings restore");
        requireUi(bookmarks.size() == 1 && bookmarks[0] == temp && liveKeyShift->load() == -24 &&
                      SendMessageW(infiniteButton, BM_GETCHECK, 0, 0) == BST_CHECKED &&
                      SendMessageW(autoNextButton, BM_GETCHECK, 0, 0) == BST_CHECKED,
                  "new preferences restore");
        SetWindowTextW(loopsEdit, L"9999");
        saveSettings();
        requireUi(setting(L"Loops", 0) == 8, "invalid edit overwrote saved value");
        {
            std::lock_guard<std::mutex> lock(retainedVoices->mutex);
            retainedVoices->defined[23] = true;
            retainedVoices->definitions[23][1] = 7;
            retainedVoices->definitions[23][2] = 15;
            retainedVoices->definitions[23][0]=23;
            retainedVoices->beginVoiceSession(retainedVoices->definitions,retainedVoices->defined);
        }
        showVoices();
        requireUi(ListView_GetItemCount(voiceList) == 1, "instrument catalog missing");
        ListView_SetItemState(voiceList, 0, LVIS_SELECTED, LVIS_SELECTED);
        requireUi(retainedVoices->requestedVoice[0] == 23, "catalog did not select current channel voice");
        requireUi(selectedVoice == 23 && shownMml.find(L"@23 = {") == 0 && shownAlgorithm == 7,
                  "instrument click did not show definition");
        SendMessageW(voiceWindow, WM_COMMAND, MAKEWPARAM(1, CBN_SELCHANGE), LPARAM(voiceChannel));
        requireUi(selectedVoice == -1, "channel selection did not restore live mode");
        requireUi(voiceWindow && voiceEdit && registerEdit, "voice viewer creation");
        requireUi(voiceKeyboard && auditionOctave == 4, "audition keyboard/default octave");
        requireUi(baselineVoiceValid && IsWindowEnabled(parameterKnobs[0]), "MDX controls disabled");
        setParameterValue(0,17); changedVoiceParameter();
        NMLVCUSTOMDRAW draw{};
        draw.nmcd.hdr.hwndFrom=voiceList; draw.nmcd.hdr.code=NM_CUSTOMDRAW;
        draw.nmcd.dwDrawStage=CDDS_ITEMPREPAINT; draw.nmcd.lItemlParam=23;
        draw.nmcd.uItemState=CDIS_SELECTED;
        SendMessageW(voiceWindow,WM_NOTIFY,4,LPARAM(&draw));
        requireUi(draw.clrText==RGB(255,95,95),"changed instrument list text not red");
        CHARRANGE changedSpan{0, 0};
        GETTEXTLENGTHEX textLength{GTL_NUMCHARS | GTL_PRECISE,1200};
        int count=int(SendMessageW(voiceEdit,EM_GETTEXTLENGTHEX,WPARAM(&textLength),0));
        std::wstring richText(count+1,L'\0');
        GETTEXTEX getText{DWORD(richText.size()*sizeof(wchar_t)),GT_DEFAULT,1200,nullptr,nullptr};
        richText.resize(SendMessageW(voiceEdit,EM_GETTEXTEX,WPARAM(&getText),LPARAM(richText.data())));
        auto spans=mdx::voiceParameterSpans(richText);
        requireUi(spans.size()==47,"MML parameter span count");
        changedSpan={LONG(spans[0].first),LONG(spans[0].second)};
        SendMessageW(voiceEdit,EM_EXSETSEL,0,LPARAM(&changedSpan));
        CHARFORMAT2W format{}; format.cbSize=sizeof(format);
        SendMessageW(voiceEdit,EM_GETCHARFORMAT,SCF_SELECTION,LPARAM(&format));
        requireUi(format.crTextColor==RGB(255,95,95),"changed MML parameter not red");
        RECT textBounds{}; SendMessageW(voiceEdit,EM_GETRECT,0,LPARAM(&textBounds));
        HDC textDc=GetDC(voiceEdit); auto oldFont=SelectObject(textDc,voiceFont);
        TEXTMETRICW metric{}; GetTextMetricsW(textDc,&metric);
        SelectObject(textDc,oldFont); ReleaseDC(voiceEdit,textDc);
        requireUi(textBounds.bottom-textBounds.top>=9*metric.tmHeight,"nine MML rows do not fit");
        requireUi((retainedVoices->definitions[23][11]&31)==17 && IsWindowEnabled(voiceReset),
                  "knob edit not published");
        SendMessageW(voiceWindow,WM_COMMAND,8,0);
        requireUi(retainedVoices->definitions[23]==retainedVoices->initialDefinitions[23], "reset failed");
        showVoices();
        requireUi(!IsWindowVisible(voiceWindow) && !voiceWantedVisible,"voice toggle did not hide");
        showVoices();
        requireUi(IsWindowVisible(voiceWindow) && voiceWantedVisible,"voice toggle did not show");
        saveSettings();
        requireUi(setting(L"VoiceVisible",0)==1 && setting(L"VoiceWidth",0)>=DWORD(scale(800)),
                  "voice window preferences missing");
        RECT savedVoiceRect{}; GetWindowRect(voiceWindow,&savedVoiceRect);
        DestroyWindow(voiceWindow);
        showVoices(false);
        RECT restoredVoiceRect{}; GetWindowRect(voiceWindow,&restoredVoiceRect);
        requireUi(EqualRect(&savedVoiceRect,&restoredVoiceRect),"voice placement not restored");
        MSG typing{}; typing.hwnd=voiceEdit; typing.message=WM_KEYDOWN; typing.wParam='A';
        requireUi(!voiceKeyMessage(typing), "MML typing triggered audition");
        running = true; // Exercise queue wiring without opening an audio device.
        midiQueue->clear();
        MSG key{};
        key.hwnd = voiceKeyboard;
        key.message = WM_KEYDOWN;
        key.wParam = 'A';
        requireUi(voiceKeyMessage(key), "audition key not intercepted from keyboard");
        requireUi(auditionKeys[0].note == 60, "A did not play o4 C");
        key.wParam = 'K';
        voiceKeyMessage(key);
        requireUi(auditionKeys[12].note == 72, "K did not play o5 C");
        key.wParam = 'U';
        voiceKeyMessage(key);
        requireUi(auditionKeys[10].note == 70, "U did not play o4 A sharp");
        key.wParam = VK_PRIOR;
        voiceKeyMessage(key);
        requireUi(auditionOctave == 5 && auditionKeys[0].note == -1 && auditionKeys[12].note == -1,
                  "octave change left held notes");
        key.wParam = 'A';
        voiceKeyMessage(key);
        requireUi(auditionKeys[0].note == 72, "new octave not used");
        key.lParam = LPARAM(1) << 30;
        auto pending = midiQueue->messages.size();
        voiceKeyMessage(key);
        requireUi(midiQueue->messages.size() == pending, "key repeat retriggered note");
        SendMessageW(voiceWindow, WM_ACTIVATE, WA_INACTIVE, 0);
        requireUi(auditionKeys[0].note == -1, "deactivation left audition note");
        key.lParam = 0;
        key.wParam = VK_PRIOR;
        for (int i = 0; i < 10; ++i)
            voiceKeyMessage(key);
        requireUi(auditionOctave == 8, "octave 8 unavailable");
        key.wParam = VK_NEXT;
        for (int i = 0; i < 10; ++i)
            voiceKeyMessage(key);
        requireUi(auditionOctave == 0, "octave 0 unavailable");
        running = false;
        midiQueue->clear();
        showLicenses();
        requireUi(IsWindow(licenseWindow) && IsWindow(licenseEdit), "internal license window absent");
        wchar_t exe[32768];
        GetModuleFileNameW(nullptr, exe, 32768);
        if (std::filesystem::exists(std::filesystem::path(exe).parent_path() / L"THIRD_PARTY_NOTICES.txt"))
            requireUi(GetWindowTextLengthW(licenseEdit) > 1000, "license text not loaded");
        {
            for(auto ext:{"TXT","HED","MUS","PDL"})std::ofstream(temp / (std::string("2.")+ext)) << ext;
            requireUi(textFile(L"readme.HED") && textFile(L"readme.pdl"),"HED/PDL text detection");
            std::ofstream(temp / "2.DOC") << "Line one\nLine two\n";
            navigate(temp);
            auto row=std::find_if(files.begin(),files.end(),[](const FileRow &f){return f.name==L"2.mdx";});
            requireUi(row!=files.end() && !row->document.empty() && row->icon==documentIcons.at(L"DOC"),"sidecar DOC icon missing");
            requireUi(row->documents.size()==5,"sidecar menu choices missing");
            for(size_t i=0;i<5;++i)requireUi(documentPriority(row->documents[i].extension().wstring())==int(i),"sidecar priority incorrect");
            auto path=row->document;
            running=true; showDocument(path);
            requireUi(running && IsWindowVisible(documentWindow),"viewer stopped playback");
            requireUi(GetWindowTextLengthW(documentEdit)>10,"document text missing");
            showDocument(path);requireUi(!IsWindowVisible(documentWindow),"viewer toggle failed");
            running=false;
            SendMessageW(allFilter,BM_SETCHECK,BST_CHECKED,0);navigate(temp);
            requireUi(std::any_of(files.begin(),files.end(),[](const FileRow &f){return f.name==L"ignore.txt";}),"all-files filter missing text");
            SendMessageW(allFilter,BM_SETCHECK,BST_UNCHECKED,0);
        }
        std::cout << "PASS: language, bounds, dropdown, defaults, keyboard mute, browser, columns, debounce "
                     "and restore, license window\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    settingsReady = false;
    KillTimer(test, 2);
    DestroyWindow(test);

    std::filesystem::remove_all(temp);
    return result;
}
