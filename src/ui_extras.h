// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

// Native browser, channel monitor and delayed preferences for IndependentMDX.
const COLORREF textColor = RGB(178, 178, 255), panelColor = RGB(7, 8, 24), editColor = RGB(20, 22, 48);
HBRUSH backgroundBrush = CreateSolidBrush(RGB(4, 5, 15)), panelBrush = CreateSolidBrush(panelColor),
       editBrush = CreateSolidBrush(editColor), accentBrush = CreateSolidBrush(RGB(92, 210, 255));
HWND folderEdit, backButton, upButton, mdxFilter, pdxFilter, vgmFilter, allFilter, browserList, keyboards, licenseWindow,
    licenseEdit, bookmarkBox, bookmarkAdd, bookmarkRemove;
ULONG_PTR gdiToken;
bool settingsReady = false, settingsDirty = false, layoutBusy = false, restoreMaximized = false;
bool voiceWantedVisible = false;
int voiceShowState = SW_SHOWNORMAL;
int scrollX = 0, scrollY = 0, virtualWidth = 0, virtualHeight = 0;
std::filesystem::path executableFolder() {
    wchar_t path[32768];
    GetModuleFileNameW(nullptr, path, 32768);
    return std::filesystem::path(path).parent_path();
}
std::filesystem::path settingsPath = executableFolder() / L"IndependentMDX.ini";
std::vector<std::filesystem::path> bookmarks;
struct FolderView {
    std::filesystem::path selected, top;
};
std::map<std::filesystem::path, FolderView> folderViews;
std::filesystem::path folder;
std::vector<std::filesystem::path> history;
std::vector<int> columns{9, 0, 1, 2, 3, 4};
int columnWidths[] = {240, 160, 160, 135, 100, 160, 90, 100, 220, 280};
int sortColumn = 0;
bool ascending = true;
struct FileRow {
    std::filesystem::path path;
    std::wstring name, type, ext;
    FILETIME created, modified, accessed;
    uint64_t size;
    DWORD attributes;
    int icon;
    std::wstring title;
    std::filesystem::path document;
    std::vector<std::filesystem::path> documents;
    bool directory() const { return attributes & FILE_ATTRIBUTE_DIRECTORY; }
};
std::vector<FileRow> files;
#include "browser_support.h"
void layout();
void changeLanguage(bool);
void saveSettings();
void extrasLanguage();
void settingsChanged() {
    if (settingsReady) {
        settingsDirty = true;
        SetTimer(window, 2, 2000, nullptr);
    }
}
std::wstring settingText(const wchar_t *name) {
    std::vector<wchar_t> text(32768);
    GetPrivateProfileStringW(L"Settings", name, L"", text.data(), DWORD(text.size()), settingsPath.c_str());
    return text.data();
}
DWORD setting(const wchar_t *name, DWORD fallback) {
    auto text = settingText(name);
    if (text.empty())
        return fallback;
    wchar_t *end = nullptr;
    auto n = wcstoull(text.c_str(), &end, 10);
    return !end || *end || n > UINT32_MAX ? fallback : DWORD(n);
}
const wchar_t *columnName(int id) {
    static const wchar_t *ja[] = {L"名前",         L"作成日時", L"更新日時", L"種類",       L"サイズ",
                                  L"アクセス日時", L"拡張子",   L"属性",     L"フォルダー", L"曲名"};
    static const wchar_t *en[] = {L"Name",   L"Date created",  L"Date modified", L"Type",
                                  L"Size",   L"Date accessed", L"Extension",     L"Attributes",
                                  L"Folder", L"Song title"};
    return tr(ja[id], en[id]);
}
std::wstring dateText(FILETIME f) {
    FILETIME local;
    SYSTEMTIME t;
    if (!FileTimeToLocalFileTime(&f, &local) || !FileTimeToSystemTime(&local, &t))
        return {};
    wchar_t b[64];
    swprintf(b, 64, L"%04d/%02d/%02d %02d:%02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
             t.wSecond);
    return b;
}
std::wstring cell(const FileRow &f, int col) {
    switch (col) {
    case 0:
        return f.name;
    case 1:
        return dateText(f.created);
    case 2:
        return dateText(f.modified);
    case 3:
        return f.directory() ? tr(L"フォルダー", L"Folder") : f.type;
    case 4:
        return f.directory() ? L"" : std::to_wstring(f.size);
    case 5:
        return dateText(f.accessed);
    case 6:
        return f.ext;
    case 7: {
        std::wstring s;
        for (auto p : {std::pair<DWORD, wchar_t>{FILE_ATTRIBUTE_READONLY, L'R'},
                       {FILE_ATTRIBUTE_HIDDEN, L'H'},
                       {FILE_ATTRIBUTE_SYSTEM, L'S'},
                       {FILE_ATTRIBUTE_ARCHIVE, L'A'},
                       {FILE_ATTRIBUTE_COMPRESSED, L'C'}})
            if (f.attributes & p.first)
                s += p.second;
        return s;
    }
    case 8:
        return f.path.parent_path().wstring();
    case 9:
        return f.title;
    }
    return {};
}
std::vector<int> visualColumns() {
    std::vector<int> order(columns.size()), result;
    if (ListView_GetColumnOrderArray(browserList, int(order.size()), order.data()))
        for (int i : order)
            result.push_back(columns[i]);
    else
        result = columns;
    return result;
}
void captureWidths() {
    for (size_t i = 0; i < columns.size(); ++i)
        columnWidths[columns[i]] = MulDiv(ListView_GetColumnWidth(browserList, int(i)), 96, scale(96));
}
void populate() {
    std::filesystem::path selected;
    int index = ListView_GetNextItem(browserList, -1, LVNI_SELECTED);
    if (index >= 0 && size_t(index) < files.size())
        selected = files[index].path;
    std::stable_sort(files.begin(), files.end(), [](const FileRow &a, const FileRow &b) {
        if (a.directory() != b.directory())
            return a.directory();
        int c = 0;
        if (sortColumn == 4)
            c = a.size < b.size ? -1 : a.size > b.size ? 1 : 0;
        else if (sortColumn == 1 || sortColumn == 2 || sortColumn == 5) {
            FILETIME x = sortColumn == 1   ? a.created
                         : sortColumn == 2 ? a.modified
                                           : a.accessed,
                     y = sortColumn == 1   ? b.created
                         : sortColumn == 2 ? b.modified
                                           : b.accessed;
            c = CompareFileTime(&x, &y);
        } else
            c = StrCmpLogicalW(cell(a, sortColumn).c_str(), cell(b, sortColumn).c_str());
        if (!c)
            c = StrCmpLogicalW(a.name.c_str(), b.name.c_str());
        return ascending ? c < 0 : c > 0;
    });
    SendMessageW(browserList, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(browserList);
    for (size_t i = 0; i < files.size(); ++i) {
        for (size_t j = 0; j < columns.size(); ++j) {
            auto t = cell(files[i], columns[j]);
            if (!j) {
                LVITEMW item{};
                item.mask = LVIF_TEXT | LVIF_IMAGE;
                item.iItem = int(i);
                item.pszText = t.data();
                item.iImage = files[i].icon;
                ListView_InsertItem(browserList, &item);
            } else
                ListView_SetItemText(browserList, int(i), int(j), t.data());
        }
        if (files[i].path == selected)
            ListView_SetItemState(browserList, int(i), LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
    }
    HWND header = ListView_GetHeader(browserList);
    for (size_t i = 0; i < columns.size(); ++i) {
        HDITEMW h{};
        h.mask = HDI_FORMAT;
        Header_GetItem(header, int(i), &h);
        h.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (columns[i] == sortColumn)
            h.fmt |= ascending ? HDF_SORTUP : HDF_SORTDOWN;
        Header_SetItem(header, int(i), &h);
    }
    if(!languageUpdating) SendMessageW(browserList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(browserList, nullptr, TRUE);
}
void rebuildColumns() {
    while (ListView_DeleteColumn(browserList, 0)) {
    }
    for (size_t i = 0; i < columns.size(); ++i) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        c.pszText = const_cast<wchar_t *>(columnName(columns[i]));
        c.cx = scale(columnWidths[columns[i]]);
        c.fmt = columns[i] == 4 ? LVCFMT_RIGHT : LVCFMT_LEFT;
        ListView_InsertColumn(browserList, int(i), &c);
    }
    populate();
}
std::wstring readSongTitle(const std::filesystem::path &path) {
    try {
    if(mdx::isVgm(path)) { try { return wide(mdx::readVgmTitle(path)); } catch(...) { return {}; } }
    std::ifstream file(mdx::resolveArchivePath(path), std::ios::binary);
    std::string bytes;
    char c;
    while (bytes.size() < 8192 && file.get(c)) {
        bytes += c;
        if (bytes.size() >= 3 && bytes.compare(bytes.size() - 3, 3, "\r\n\x1a") == 0) {
            bytes.resize(bytes.size() - 3);
            return wide(mdx::utf8(bytes));
        }
    }
    return {};
    } catch(...) {return {};}
}
void selectPath(const std::filesystem::path &path) {
    for (size_t i = 0; i < files.size(); ++i)
        if (files[i].path == path) {
            ListView_SetItemState(browserList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_SetItemState(browserList, int(i), LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(browserList, int(i), FALSE);
            break;
        }
}
void rememberFolderView() {
    if (folder.empty())
        return;
    FolderView view;
    int i = ListView_GetNextItem(browserList, -1, LVNI_SELECTED), top = ListView_GetTopIndex(browserList);
    if (i >= 0 && size_t(i) < files.size())
        view.selected = files[i].path;
    if (top >= 0 && size_t(top) < files.size())
        view.top = files[top].path;
    folderViews[folder] = view;
}
struct TitleJob {
    std::atomic<bool> cancelled{false};
    std::mutex mutex;
    std::vector<std::pair<std::filesystem::path,std::wstring>> ready;
};
std::shared_ptr<TitleJob> titleJob;
std::mutex titleWorkerMutex;
std::condition_variable titleWorkersFinished;
unsigned titleWorkers=0;
void updateTitles() {
    if(!titleJob) return;
    std::vector<std::pair<std::filesystem::path,std::wstring>> ready;
    { std::lock_guard<std::mutex> lock(titleJob->mutex); ready.swap(titleJob->ready); }
    for(auto &entry:ready) for(size_t i=0;i<files.size();++i) if(files[i].path==entry.first) {
        files[i].title=std::move(entry.second);
        for(size_t j=0;j<columns.size();++j) if(columns[j]==9)
            ListView_SetItemText(browserList,int(i),int(j),files[i].title.data());
        break;
    }
}
void navigate(std::filesystem::path target, bool remember = true) {
    std::error_code ec;
    target = std::filesystem::absolute(target, ec).lexically_normal();
    std::filesystem::path physical;
    try { physical=mdx::resolveArchivePath(target); }
    catch(const std::exception &e) { MessageBoxW(window,wide(e.what()).c_str(),L"ZIP",MB_OK|MB_ICONERROR);return; }
    if (ec || !std::filesystem::is_directory(physical, ec)) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    if(titleJob) titleJob->cancelled=true;
    titleJob=std::make_shared<TitleJob>();
    auto previous = folder;
    rememberFolderView();
    if (remember && !folder.empty() && target != folder)
        history.push_back(folder);
    folder = target;
    SetWindowTextW(folderEdit, folder.c_str());
    ListView_DeleteAllItems(browserList);
    files.clear();
    resetBrowserImages();
    std::map<std::wstring,std::vector<std::filesystem::path>> documents;
    for(const auto &entry:std::filesystem::directory_iterator(physical,ec)) {
        if(entry.is_directory(ec))continue;
        auto ext=entry.path().extension().wstring();
        if(documentPriority(ext)>=int(documentExtensions.size()))continue;
        auto stem=entry.path().stem().wstring();for(auto &c:stem)c=wchar_t(towlower(c));
        documents[stem].push_back(folder/entry.path().filename());
    }
    for(auto &entry:documents)std::sort(entry.second.begin(),entry.second.end(),[](const auto &a,const auto &b){
        int ap=documentPriority(a.extension().wstring()),bp=documentPriority(b.extension().wstring());
        return ap!=bp ? ap<bp : a.native()<b.native();
    });
    WIN32_FIND_DATAW data;
    HANDLE search = FindFirstFileW((physical / L"*").c_str(), &data);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L".."))
                continue;
            auto path = folder / data.cFileName;
            auto ext = path.extension().wstring();
            bool zip=mdx::isArchive(path);
            bool dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || zip;
            if(zip)data.dwFileAttributes|=FILE_ATTRIBUTE_DIRECTORY;
            if (!dir && SendMessageW(allFilter,BM_GETCHECK,0,0)!=BST_CHECKED && !((_wcsicmp(ext.c_str(), L".mdx") == 0 &&
                           SendMessageW(mdxFilter, BM_GETCHECK, 0, 0) == BST_CHECKED) ||
                          (_wcsicmp(ext.c_str(), L".pdx") == 0 &&
                           SendMessageW(pdxFilter, BM_GETCHECK, 0, 0) == BST_CHECKED) ||
                          (mdx::isVgm(path) && SendMessageW(vgmFilter,BM_GETCHECK,0,0)==BST_CHECKED)))
                continue;
            SHFILEINFOW info{};
            auto images = SHGetFileInfoW(path.c_str(), data.dwFileAttributes, &info, sizeof(info),
                                         SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON |
                                             SHGFI_TYPENAME);
            int icon=info.hIcon ? ImageList_AddIcon(browserImages,info.hIcon) : -1;
            if(info.hIcon)DestroyIcon(info.hIcon);
            std::filesystem::path document;
            auto stem=path.stem().wstring();for(auto &c:stem)c=wchar_t(towlower(c));
            std::vector<std::filesystem::path> sidecars;
            if(!dir && !_wcsicmp(ext.c_str(),L".mdx") && documents.count(stem)) {
                sidecars=documents[stem];document=sidecars.front();icon=documentIcons.at(documentType(document));
            }
            files.push_back({path,
                             data.cFileName,
                             info.szTypeName,
                             ext,
                             data.ftCreationTime,
                             data.ftLastWriteTime,
                             data.ftLastAccessTime,
                             dir ? 0 : mdx::archiveFileSize(physical / data.cFileName),
                             data.dwFileAttributes,
                             icon,
                             zip ? path.filename().wstring() : std::wstring{},
                             document,sidecars});

        } while (FindNextFileW(search, &data));
        FindClose(search);
    }
    populate();
    std::vector<std::filesystem::path> pending;
    for(const auto &f:files) if(!f.directory() && (_wcsicmp(f.ext.c_str(),L".mdx")==0 || mdx::isVgm(f.path))) pending.push_back(f.path);
    {std::lock_guard<std::mutex> lock(titleWorkerMutex);++titleWorkers;}
    std::thread([job=titleJob,pending=std::move(pending)] {
        struct Done {~Done(){std::lock_guard<std::mutex> lock(titleWorkerMutex);--titleWorkers;titleWorkersFinished.notify_all();}} done;
        for(const auto &path:pending) {
            if(job->cancelled) break;
            auto title=readSongTitle(path);
            std::lock_guard<std::mutex> lock(job->mutex);
            if(!job->cancelled) job->ready.emplace_back(path,std::move(title));
        }
    }).detach();
    auto savedView = folderViews.find(folder);
    if (savedView != folderViews.end()) {
        for (size_t i = 0; i < files.size(); ++i)
            if (files[i].path == savedView->second.top) {
                RECT r;
                if (ListView_GetItemRect(browserList, 0, &r, LVIR_BOUNDS))
                    ListView_Scroll(browserList, 0, int(i) * (r.bottom - r.top));
                break;
            }
        selectPath(savedView->second.selected);
    }
    if (previous.parent_path() == folder)
        selectPath(previous);
    settingsChanged();
}
void activateFile() {
    int i = ListView_GetNextItem(browserList, -1, LVNI_SELECTED);
    if (i < 0 || size_t(i) >= files.size())
        return;
    auto f = files[i];
    if (f.directory())
        navigate(f.path);
    else if (textFile(f.path)) showDocument(f.path);
    else if (_wcsicmp(f.ext.c_str(),L".mdx")==0 || _wcsicmp(f.ext.c_str(),L".pdx")==0 || mdx::isVgm(f.path)) {
        load(f.path);
        if (input == f.path)
            start(false);
    }
}
std::filesystem::path nextFile() {
    auto current = std::find_if(files.begin(), files.end(), [](const FileRow &f) { return f.path == input; });
    if (current == files.end())
        return {};
    for (++current; current != files.end(); ++current)
        if (!current->directory() && (_wcsicmp(current->ext.c_str(),L".mdx")==0 || mdx::isVgm(current->path)))
            return current->path;
    return {};
}
void advanceFile() {
    auto path = nextFile();
    if (path.empty())
        return;
    selectPath(path);
    load(path);
    if (input == path)
        start(false);
}
void updateBookmarks() {
    SendMessageW(bookmarkBox, CB_RESETCONTENT, 0, 0);
    for (auto &path : bookmarks)
        SendMessageW(bookmarkBox, CB_ADDSTRING, 0, (LPARAM)path.c_str());
    SetWindowTextW(bookmarkBox, L"");
}
void columnMenu() {
    captureWidths();
    auto order = visualColumns();
    HMENU menu = CreatePopupMenu();
    for (int i = 0; i < 10; ++i)
        AppendMenuW(menu,
                    MF_STRING |
                        (std::find(columns.begin(), columns.end(), i) != columns.end() ? MF_CHECKED : 0),
                    200 + i, columnName(i));
    POINT p;
    GetCursorPos(&p);
    int id = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, window, nullptr) - 200;
    DestroyMenu(menu);
    if (id >= 0 && id < 10) {
        auto it = std::find(order.begin(), order.end(), id);
        if (it == order.end())
            order.push_back(id);
        else if (order.size() > 1)
            order.erase(it);
        columns = order;
        rebuildColumns();
        settingsChanged();
    }
}
LRESULT CALLBACK listSubclass(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_GETDLGCODE && l && ((MSG *)l)->wParam == VK_RETURN)
        return DLGC_WANTMESSAGE;
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        activateFile();
        return 0;
    }
    if (m == WM_KEYDOWN && w == VK_SPACE) {
        int i=ListView_GetNextItem(browserList,-1,LVNI_SELECTED);
        if(i>=0 && size_t(i)<files.size())showDocument(files[i].document);
        return 0;
    }
    if (m == WM_KEYDOWN && w == VK_BACK) {
        navigate(folder.parent_path());
        return 0;
    }
    if (m == WM_NOTIFY) {
        auto n = (NMHDR *)l;
        if (n->code == HDN_ENDDRAG || n->code == HDN_ENDTRACKW)
            SetTimer(window, 3, 1, nullptr);
        if (n->code == NM_RCLICK) {
            columnMenu();
            return 0;
        }
        if (n->code == NM_CUSTOMDRAW) {
            auto c = (NMCUSTOMDRAW *)l;
            if (c->dwDrawStage == CDDS_PREPAINT)
                return CDRF_NOTIFYITEMDRAW;
            if (c->dwDrawStage == CDDS_ITEMPREPAINT) {
                FillRect(c->hdc, &c->rc, panelBrush);
                SetTextColor(c->hdc, textColor);
                SetBkMode(c->hdc, TRANSPARENT);
                wchar_t t[128];
                HDITEMW item{};
                item.mask = HDI_TEXT;
                item.pszText = t;
                item.cchTextMax = 128;
                Header_GetItem(n->hwndFrom, int(c->dwItemSpec), &item);
                RECT r = c->rc;
                r.left += 6;
                std::wstring title = t;
                if (c->dwItemSpec < columns.size() && columns[c->dwItemSpec] == sortColumn)
                    title += ascending ? L" ↑" : L" ↓";
                DrawTextW(c->hdc, title.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER);
                return CDRF_SKIPDEFAULT;
            }
        }
    }
    return DefSubclassProc(h, m, w, l);
}
LRESULT CALLBACK addressSubclass(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_GETDLGCODE && l && ((MSG *)l)->wParam == VK_RETURN)
        return DLGC_WANTMESSAGE;
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        wchar_t b[32768];
        GetWindowTextW(h, b, 32768);
        navigate(b);
        return 0;
    }
    return DefSubclassProc(h, m, w, l);
}
LRESULT browserNotify(NMHDR *n) {
    if (n->hwndFrom == browserList) {
        if(n->code==NM_RCLICK) {
            auto click=reinterpret_cast<NMITEMACTIVATE*>(n);
            int i=click->iItem;
            if(i>=0 && size_t(i)<files.size()) {
                POINT point=click->ptAction;ClientToScreen(browserList,&point);
                documentMenu(files[i],point);
            }
            return 0;
        }
        if (n->code == LVN_BEGINDRAG) {
            int i=reinterpret_cast<NMLISTVIEW*>(n)->iItem;
            if(i>=0 && size_t(i)<files.size())copyDrag(files[i]);
            return 0;
        }
        if (n->code == LVN_COLUMNCLICK) {
            int c = ((NMLISTVIEW *)n)->iSubItem;
            if (c >= 0 && size_t(c) < columns.size()) {
                if (sortColumn == columns[c])
                    ascending = !ascending;
                else {
                    sortColumn = columns[c];
                    ascending = true;
                }
                populate();
                settingsChanged();
            }
        }
        if (n->code == NM_DBLCLK)
            activateFile();
    }
    return 0;
}
bool extrasCommand(WPARAM w) {
    switch (LOWORD(w)) {
    case 107:
        if (HIWORD(w) == CBN_SELCHANGE) {
            int i = int(SendMessageW(bookmarkBox, CB_GETCURSEL, 0, 0));
            if (i >= 0 && size_t(i) < bookmarks.size())
                navigate(bookmarks[i]);
        }
        return true;
    case 108:
        if (HIWORD(w) == BN_CLICKED && !folder.empty() && bookmarks.size() < 100 &&
            std::find(bookmarks.begin(), bookmarks.end(), folder) == bookmarks.end()) {
            bookmarks.push_back(folder);
            updateBookmarks();
            SendMessageW(bookmarkBox, CB_SETCURSEL, bookmarks.size() - 1, 0);
            settingsChanged();
        }
        return true;
    case 109:
        if (HIWORD(w) == BN_CLICKED) {
            int i = int(SendMessageW(bookmarkBox, CB_GETCURSEL, 0, 0));
            if (i >= 0 && size_t(i) < bookmarks.size()) {
                bookmarks.erase(bookmarks.begin() + i);
                updateBookmarks();
                settingsChanged();
            }
        }
        return true;
    case 100:
        if (!history.empty()) {
            auto p = history.back();
            history.pop_back();
            navigate(p, false);
        }
        return true;
    case 101:
        navigate(folder.parent_path());
        return true;
    case 103:
    case 104:
    case 116:
    case 155:
        if (HIWORD(w) == BN_CLICKED)
            navigate(folder, false);
        return true;
    }
    return false;
}
bool blackKey(int note) {
    int p = note % 12;
    return p == 1 || p == 3 || p == 6 || p == 8 || p == 10;
}
mdx::DisplayChannels displayedNotes(const mdx::MonitorFrame &frame) {
    mdx::DisplayChannels result;
    std::lock_guard<std::mutex> lock(retainedVoices->mutex);
    for (int ch = 0; ch < 16; ++ch)
        result[ch] = mdx::carrierNotes(frame.note[ch], retainedVoices->registers, ch, carrierPitch);
    return result;
}
#include "piano_roll.h"
LRESULT CALLBACK keyboardProcedure(HWND h, UINT m, WPARAM w, LPARAM l) {
    RECT r;
    GetClientRect(h, &r);
    int top = 0, row = std::max(1, (int(r.bottom) - top) / 16);
    if (m == WM_LBUTTONDOWN) {
        int ch = (GET_Y_LPARAM(l) - top) / row;
        bool hit = GET_Y_LPARAM(l) >= top && ch < 16;
        if (pianoRoll) {
            int y = GET_Y_LPARAM(l) - top;
            int width = std::max(1, (int(r.right) - scale(12)) / 96) * 96;
            int x = GET_X_LPARAM(l) - (int(r.right) - width) / 2;
            hit = y >= 0 && y < scale(48) && x >= 0 && x < width;
            ch = std::clamp(x * 8 / width, 0, 7) + (y / scale(24)) * 8;
        }
        if (hit) {
            auto mask = ch < 8 ? liveMuteMask : livePcmMuteMask;
            mask->fetch_xor(1u << (ch % 8));
            settingsChanged();
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    }
    if (m == WM_ERASEBKGND)
        return 1;
    if (m == WM_PAINT || m == WM_PRINTCLIENT || m == WM_PRINT) {
        PAINTSTRUCT ps;
        HDC target = m != WM_PAINT ? HDC(w) : BeginPaint(h, &ps);
        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap =
            CreateCompatibleBitmap(target, std::max(1, int(r.right)), std::max(1, int(r.bottom)));
        auto old = SelectObject(dc, bitmap);
        FillRect(dc, &r, panelBrush);
        SelectObject(dc, uiFont);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, textColor);
        mdx::MonitorFrame frame;
        {
            std::lock_guard<std::mutex> lock(monitorView->mutex);
            frame = monitorView->frame;
            if (pauseFlag || liveSpeed->load() == 0 || idleMidi || !running)
                frame.note.fill(-1);
            for (int ch = 0; ch < 16; ++ch) {
                int n = retainedVoices->midiNotes[ch].load();
                if (n >= 0)
                    frame.note[ch] = std::clamp(n - 15, 0, 95);
            }
        }
        auto display = displayedNotes(frame);
        captureRoll(frame, display);
        if (pianoRoll)
            paintPianoRoll(dc, r, frame);
        int left = scale(35), width = std::max(56, int(r.right) - left - 4);
        auto paint = [&](RECT box, COLORREF color) {
            SetDCBrushColor(dc,color);
            FillRect(dc,&box,HBRUSH(GetStockObject(DC_BRUSH)));
        };
        for (int ch = 0; !pianoRoll && ch < 16; ++ch) {
            bool muted = ((ch < 8 ? liveMuteMask : livePcmMuteMask)->load() >> (ch % 8)) & 1;
            int y = top + ch * row, bottom = y + row - 2;
            wchar_t name[2] = {wchar_t(ch < 8 ? L'A' + ch : L'P' + ch - 8), 0};
            SetTextColor(dc, muted ? RGB(55, 65, 105) : RGB(140, 165, 255));
            TextOutW(dc, 4, y + 4, name, 1);
            paint(RECT{left, y, left + width, bottom}, muted ? RGB(38, 41, 55) : RGB(205, 208, 215));
            int white = 0;
            for (int note = 0; note < 96; ++note)
                if (!blackKey(note + 3)) {
                    RECT key{left + white * width / 56, y, left + (white + 1) * width / 56 - 1, bottom};
                    paint(key, (retainedVoices->midiNotes[ch] >= 0 && mdx::hasDisplayNote(display[ch], note))
                                   ? RGB(170, 245, 55)
                               : muted                                  ? RGB(38, 41, 55)
                               : mdx::hasDisplayNote(display[ch], note) ? RGB(170, 245, 55)
                                                                        : RGB(205, 208, 215));
                    ++white;
                }
            white = 0;
            for (int note = 0; note < 96; ++note) {
                if (!blackKey(note + 3)) {
                    ++white;
                    continue;
                }
                int center = left + white * width / 56;
                RECT key{std::max(left, center - std::max(1, width / 180)), y,
                         center + std::max(1, width / 180), y + (row - 2) * 3 / 5};
                paint(key, (retainedVoices->midiNotes[ch] >= 0 && mdx::hasDisplayNote(display[ch], note))
                               ? RGB(170, 245, 55)
                           : muted                                  ? RGB(8, 10, 17)
                           : mdx::hasDisplayNote(display[ch], note) ? RGB(125, 205, 30)
                                                                    : RGB(12, 14, 22));
            }
            // Extend E/F and B/C dividers to the top; keep light gaps beside black keys.
            white = 0;
            for (int note = 0; note < 96; ++note)
                if (!blackKey(note + 3)) {
                    ++white;
                    int x = left + white * width / 56 - 1;
                    int top = note + 1 < 96 && !blackKey(note + 4) ? y : y + (row - 2) * 3 / 5;
                    paint(RECT{x, top, x + 1, bottom}, muted ? RGB(15, 17, 25) : RGB(65, 70, 90));
                }

            if (!monitorView->waveforms.load(std::memory_order_relaxed)) continue;
            float peak = .001f;
            for (float v : frame.waveform[ch])
                peak = std::max(peak, std::abs(v));
            Gdiplus::Graphics graphics(dc);
            Gdiplus::Pen pen(Gdiplus::Color(muted ? 55 : 150, 75, 205, 255), 1.5f);
            std::array<Gdiplus::PointF, mdx::MonitorFrame::points> points;
            for (size_t i = 0; i < points.size(); ++i)
                points[i] = Gdiplus::PointF(float(left) + float(i) * width / (points.size() - 1),
                                            float(y + row / 2) -
                                                frame.waveform[ch][(frame.cursor + i) % points.size()] /
                                                    peak * row * .42f);
            graphics.DrawLines(&pen, points.data(), int(points.size()));
        }
        BitBlt(target, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, old);
        DeleteObject(bitmap);
        DeleteDC(dc);
        if (m == WM_PAINT)
            EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
LRESULT CALLBACK licenseProcedure(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SIZE && licenseEdit) {
        MoveWindow(licenseEdit, 8, 8, std::max(1, int(LOWORD(l)) - 16), std::max(1, int(HIWORD(l)) - 16),
                   TRUE);
        return 0;
    }
    if (m == WM_CTLCOLORSTATIC || m == WM_CTLCOLOREDIT) {
        SetTextColor((HDC)w, textColor);
        SetBkColor((HDC)w, panelColor);
        return (LRESULT)panelBrush;
    }
    if (m == WM_DESTROY) {
        licenseWindow = nullptr;
        licenseEdit = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
void showLicenses() {
    if (licenseWindow) {
        ShowWindow(licenseWindow, SW_RESTORE);
        SetForegroundWindow(licenseWindow);
        return;
    }
    WNDCLASSW c{};
    c.lpfnWndProc = licenseProcedure;
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = L"IndependentMDXLicenses";
    c.hbrBackground = backgroundBrush;
    c.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&c);
    licenseWindow = CreateWindowW(
        c.lpszClassName, tr(L"ライセンス — IndependentMDX", L"Licenses — IndependentMDX"),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 850, 650, window, nullptr, c.hInstance, nullptr);
    licenseEdit = CreateWindowW(L"EDIT", L"",
                                WS_VISIBLE | WS_CHILD | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL |
                                    WS_VSCROLL | WS_HSCROLL,
                                8, 8, 800, 580, licenseWindow, nullptr, c.hInstance, nullptr);
    SendMessageW(licenseEdit, WM_SETFONT, (WPARAM)uiFont, TRUE);
    SendMessageW(licenseEdit, EM_SETLIMITTEXT, 2 * 1024 * 1024, 0);
    wchar_t exe[32768];
    GetModuleFileNameW(nullptr, exe, 32768);
    try {
        auto bytes = mdx::readFile(std::filesystem::path(exe).parent_path() / L"THIRD_PARTY_NOTICES.txt");
        std::string text(bytes.begin(), bytes.end());
        auto value = wide(text);
        std::wstring normalized;
        for (size_t i = 0; i < value.size(); ++i) {
            if (value[i] == L'\n' && (i == 0 || value[i - 1] != L'\r'))
                normalized += L'\r';
            normalized += value[i];
        }
        SetWindowTextW(licenseEdit, normalized.c_str());
    } catch (const std::exception &e) {
        SetWindowTextW(licenseEdit, diagnostic(e.what()).c_str());
    }
    RECT r;
    GetClientRect(licenseWindow, &r);
    SendMessageW(licenseWindow, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
    ShowWindow(licenseWindow, SW_SHOW);
}
void extrasLanguage() {
    SetWindowTextW(bookmarkAdd, tr(L"ブックマーク追加", L"Add bookmark"));
    SetWindowTextW(bookmarkRemove, tr(L"削除", L"Remove"));
    SetWindowTextW(backButton, tr(L"戻る", L"Back"));
    SetWindowTextW(upButton, tr(L"上へ", L"Up"));
    SetWindowTextW(allFilter,tr(L"すべて",L"All files"));
    if (licenseWindow)
        SetWindowTextW(licenseWindow, tr(L"ライセンス — IndependentMDX", L"Licenses — IndependentMDX"));
    captureWidths();
    columns = visualColumns();
    rebuildColumns();
    InvalidateRect(keyboards, nullptr, FALSE);
}
#include "splitter_buffer.h"
LRESULT CALLBACK splitterProcedure(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_PAINT || m == WM_PRINTCLIENT || m == WM_PRINT) {
        PAINTSTRUCT ps;
        HDC dc = m != WM_PAINT ? HDC(w) : BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);
        FillRect(dc, &r, panelBrush);
        int center = (r.left + r.right) / 2;
        RECT grip{center - 1, r.top, center + 1, r.bottom};
        FillRect(dc, &grip, accentBrush);
        if (m == WM_PAINT)
            EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_SETCURSOR) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        return TRUE;
    }
    if (m == WM_LBUTTONDOWN) {
        bufferSplitter(true);
        SetCapture(h);
        return 0;
    }
    if (m == WM_MOUSEMOVE && GetCapture() == h) {
        POINT point;
        GetCursorPos(&point);
        ScreenToClient(window, &point);
        int content = std::clamp(int(point.x) + scrollX - scale(16), splitMinimum, splitMaximum);
        int ratio = std::clamp(MulDiv(content, 1000, std::max(1, virtualWidth)), 1, 999);
        if (ratio != splitPermille) {
            splitPermille = ratio;
            splitterLayoutPending = true;
            if (!splitterBuffered) {
                layout();
                settingsChanged();
            }
        }
        return 0;
    }
    if (m == WM_LBUTTONUP && GetCapture() == h) {
        if (splitterLayoutPending) {
            splitterLayoutPending = false;
            layout();
            settingsChanged();
        }
        ReleaseCapture();
        return 0;
    }
    if (m == WM_CANCELMODE && GetCapture() == h) {
        if (splitterLayoutPending) {
            splitterLayoutPending = false;
            layout();
            settingsChanged();
        }
        ReleaseCapture();
        return 0;
    }
    if (m == WM_CAPTURECHANGED || m == WM_NCDESTROY) {
        if (splitterLayoutPending && m != WM_NCDESTROY) {
            splitterLayoutPending = false;
            layout();
            settingsChanged();
        }
        bufferSplitter(false);
    }
    if (m == WM_LBUTTONDBLCLK) {
        splitPermille = 620;
        layout();
        settingsChanged();
        return 0;
    }
    return DefSubclassProc(h, m, w, l);
}
void createExtras() {
    Gdiplus::GdiplusStartupInput startup;
    Gdiplus::GdiplusStartup(&gdiToken, &startup, nullptr);
    bookmarkBox = control(L"COMBOBOX", L"", 0, 0, 1, 1, 107, WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST);
    bookmarkAdd = control(L"BUTTON", L"", 0, 0, 1, 1, 108, WS_TABSTOP | BS_OWNERDRAW);
    bookmarkRemove = control(L"BUTTON", L"", 0, 0, 1, 1, 109, WS_TABSTOP | BS_OWNERDRAW);
    backButton = control(L"BUTTON", L"Back", 0, 0, 1, 1, 100, WS_TABSTOP | BS_OWNERDRAW);
    upButton = control(L"BUTTON", L"Up", 0, 0, 1, 1, 101, WS_TABSTOP | BS_OWNERDRAW);
    folderEdit = control(L"EDIT", L"", 0, 0, 1, 1, 102, WS_TABSTOP | ES_AUTOHSCROLL);
    mdxFilter = control(L"BUTTON", L"MDX", 0, 0, 1, 1, 103, WS_TABSTOP | BS_AUTOCHECKBOX);
    vgmFilter = control(L"BUTTON", L"VGM / MRL", 0, 0, 1, 1, 116, WS_TABSTOP | BS_AUTOCHECKBOX);
    allFilter = control(L"BUTTON", L"All files", 0,0,1,1,155,WS_TABSTOP|BS_AUTOCHECKBOX);
    pdxFilter = control(L"BUTTON", L"PDX", 0, 0, 1, 1, 104, WS_TABSTOP | BS_AUTOCHECKBOX);
    browserList = control(WC_LISTVIEWW, L"", 0, 0, 1, 1, 105,
                          WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS);
    for (HWND h : {backButton, upButton, folderEdit, mdxFilter, pdxFilter, vgmFilter, allFilter, browserList, bookmarkBox,
                   bookmarkAdd, bookmarkRemove}) {
        SendMessageW(h, WM_SETFONT, (WPARAM)uiFont, TRUE);
        SetWindowTheme(h, L"", L"");
    }
    ListView_SetExtendedListViewStyle(browserList,
                                      LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);
    ListView_SetBkColor(browserList, panelColor);
    ListView_SetTextBkColor(browserList, panelColor);
    ListView_SetTextColor(browserList, textColor);
    SetWindowSubclass(browserList, listSubclass, 1, 0);
    SetWindowSubclass(folderEdit, addressSubclass, 1, 0);
    WNDCLASSW c{};
    c.lpfnWndProc = keyboardProcedure;
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = L"IndependentMDXKeyboard";
    c.hCursor = LoadCursorW(nullptr, IDC_HAND);
    RegisterClassW(&c);
    keyboards = control(c.lpszClassName, L"", 0, 0, 1, 1, 106);
    splitter = control(L"STATIC", L"", 0, 0, 1, 1, 110, SS_NOTIFY);
    SetWindowSubclass(splitter, splitterProcedure, 1, 0);
    rebuildColumns();
}
struct OutputGains {
    int fm = 200, pcm = 100, master = 100;
};
std::array<OutputGains, 3> outputGains{{{200, 100, 100}, {200, 100, 100}, {100, 100, 100}}};
void captureOutputGains() {
    auto &v = outputGains[std::clamp(selectedOutput.load(), 0, 2)];
    try {
        v.fm = integer(independentGainEdit, 0, 800);
    } catch (...) {
    }
    try {
        v.pcm = integer(pcmGainEdit, 0, 400);
    } catch (...) {
    }
    try {
        v.master = integer(gainEdit, 0, 400);
    } catch (...) {
    }
}
void applyOutputGains() {
    EnableWindow(measuredButton, selectedOutput == 0);
    auto v = outputGains[std::clamp(selectedOutput.load(), 0, 2)];
    SetWindowTextW(independentGainEdit, std::to_wstring(v.fm).c_str());
    SetWindowTextW(pcmGainEdit, std::to_wstring(v.pcm).c_str());
    SetWindowTextW(gainEdit, std::to_wstring(v.master).c_str());
    liveIndependentGain->store(v.fm / 100.0);
    livePcmGain->store(v.pcm / 100.0);
    liveGain->store(v.master / 100.0);
    SendMessageW(masterSlider, TBM_SETPOS, TRUE, v.master);
    hardware.setGain(v.master / 100.0, v.fm / 100.0);
}
void readSettings() {
    voiceWantedVisible = setting(L"VoiceVisible", 0) != 0;
    voiceShowState = int(setting(L"VoiceShow", SW_SHOWNORMAL));
    splitPermille = std::clamp(int(setting(L"SplitPermille", 620)), 1, 999);
    for (int i = 0; i < 3; ++i) {
        auto key = L"Output" + std::to_wstring(i);
        outputGains[i].fm = std::clamp(int(setting((key + L"FmGain").c_str(), i == 2 ? 100 : 200)), 0, 800);
        outputGains[i].pcm = std::clamp(int(setting((key + L"PcmGain").c_str(), 100)), 0, 400);
        outputGains[i].master = std::clamp(int(setting((key + L"MasterGain").c_str(), 100)), 0, 400);
    }
    applyOutputGains();
    liveAftertouch->store(setting(L"Aftertouch", 0) != 0);
    SendMessageW(aftertouchButton, BM_SETCHECK, liveAftertouch->load() ? BST_CHECKED : BST_UNCHECKED, 0);
    struct Numeric {
        HWND h;
        const wchar_t *n;
        DWORD d, lo, hi;
    };
    for (auto v : {Numeric{loopsEdit, L"Loops", 2, 1, 1000},
                   {fmDelayEdit, L"FmDelayMs", 0, 0, 2000},
                   {pcmDelayEdit, L"PcmDelayMs", 0, 0, 2000},
                   {sampleEdit, L"Sample", 0, 0, 24575}}) {
        DWORD n = setting(v.n, v.d);
        if (n < v.lo || n > v.hi)
            n = v.d;
        SetWindowTextW(v.h, std::to_wstring(n).c_str());
    }
    SendMessageW(infiniteButton, BM_SETCHECK, setting(L"Infinite", 0) ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(loopsEdit, SendMessageW(infiniteButton, BM_GETCHECK, 0, 0) != BST_CHECKED);
    SendMessageW(autoNextButton, BM_SETCHECK, setting(L"AutoNext", 0) ? BST_CHECKED : BST_UNCHECKED, 0);
    int mode = std::clamp(int(setting(L"KeyMode", 1)), 0, 2);
    liveKeyMode->store(mode);
    SendMessageW(keyModeBox, CB_SETCURSEL, mode, 0);
    int key = std::clamp(int(setting(L"KeyShift", 0)), -24, 24);
    liveKeyShift->store(key);
    SendMessageW(keySlider, TBM_SETPOS, TRUE, key);
    SetWindowTextW(keyValue, std::to_wstring(key).c_str());
    liveMidiPolyphonic->store(setting(L"MidiPolyphonic", 1) != 0);
    pianoRoll = setting(L"PianoRoll", 0) != 0;
    monitorView->waveforms.store(setting(L"Waveforms",1)!=0);
    SendMessageW(waveformButton,BM_SETCHECK,monitorView->waveforms.load()?BST_CHECKED:BST_UNCHECKED,0);
    carrierPitch = setting(L"CarrierPitch", 0) != 0;
    SendMessageW(carrierButton, BM_SETCHECK, carrierPitch ? BST_CHECKED : BST_UNCHECKED, 0);
    int speed = std::clamp(int(setting(L"PlaybackSpeed", 100)), 0, 200);
    liveSpeed->store(speed);
    SendMessageW(speedSlider, TBM_SETPOS, TRUE, speed);
    SetWindowTextW(speedValue, std::to_wstring(speed).c_str());
    liveGain->store(integer(gainEdit, 0, 400) / 100.0);
    livePcmGain->store(integer(pcmGainEdit, 0, 400) / 100.0);
    SendMessageW(masterSlider, TBM_SETPOS, TRUE, LPARAM(liveGain->load() * 100));
    bookmarks.clear();
    DWORD count = std::min<DWORD>(100, setting(L"BookmarkCount", 0));
    for (DWORD i = 0; i < count; ++i) {
        auto path = settingText((L"Bookmark" + std::to_wstring(i)).c_str());
        if (!path.empty())
            bookmarks.emplace_back(path);
    }
    updateBookmarks();
    liveIndependentGain->store(integer(independentGainEdit, 0, 800) / 100.0);
    hardware.setGain(liveGain->load(), liveIndependentGain->load());
    DWORD fmt = setting(L"Format", 4);
    SendMessageW(formatBox, CB_SETCURSEL, fmt < 7 ? fmt : 4, 0);
    liveMuteMask->store(setting(L"FmMask", 0) & 255);
    livePcmMuteMask->store(setting(L"PcmMask", 0) & 255);
    liveMeasuredOutputTiming->store(setting(L"Measured", 1) != 0);
    liveDacRightDelay->store(setting(L"Dac", 0) != 0);
    SendMessageW(measuredButton, BM_SETCHECK, liveMeasuredOutputTiming->load() ? BST_CHECKED : BST_UNCHECKED,
                 0);
    SendMessageW(dacButton, BM_SETCHECK, liveDacRightDelay->load() ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(allFilter,BM_SETCHECK,setting(L"ShowAll",0)?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(mdxFilter, BM_SETCHECK, setting(L"ShowMdx", 1) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(vgmFilter, BM_SETCHECK, setting(L"ShowVgm",1)?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(pdxFilter, BM_SETCHECK, setting(L"ShowPdx", 1) ? BST_CHECKED : BST_UNCHECKED, 0);
    sortColumn = int(setting(L"SortColumn", 0));
    if (sortColumn > 9 || sortColumn < 0)
        sortColumn = 0;
    ascending = setting(L"Ascending", 1) != 0;
    auto saved = settingText(L"Columns");
    if (!saved.empty()) {
        std::wistringstream in(saved);
        std::vector<int> parsed;
        int id;
        while (in >> id) {
            if (id >= 0 && id < 10 && std::find(parsed.begin(), parsed.end(), id) == parsed.end())
                parsed.push_back(id);
            if (in.peek() == L',')
                in.get();
        }
        if (!parsed.empty())
            columns = parsed;
    }
    if (!setting(L"TitleColumnMigrated", 0) && std::find(columns.begin(), columns.end(), 9) == columns.end())
        columns.insert(columns.begin(), 9);
    for (int i = 0; i < 10; ++i)
        columnWidths[i] = std::clamp(
            int(setting((L"ColumnWidth" + std::to_wstring(i)).c_str(), columnWidths[i])), 40, 2000);
    rebuildColumns();
    auto dir = settingText(L"Folder");
    std::error_code ec;
    bool validFolder=false;
    try { validFolder=!dir.empty() && std::filesystem::is_directory(mdx::resolveArchivePath(dir),ec); } catch(...) {}
    if (!validFolder) {
        wchar_t current[32768];
        GetCurrentDirectoryW(32768, current);
        dir = current;
    }
    navigate(dir, false);
}
void saveSettings() {
    if (!settingsReady)
        return;
    std::map<std::wstring, std::wstring> values;
    auto number = [&](const wchar_t *n, DWORD v) { values[n] = std::to_wstring(v); };
    auto text = [&](const wchar_t *n, const std::wstring &v) { values[n] = v; };
    number(L"VoiceVisible", voiceWantedVisible);
    for (auto name : {L"VoiceX", L"VoiceY", L"VoiceWidth", L"VoiceHeight", L"VoiceShow", L"VoiceFlags"}) {
        auto previous = settingText(name);
        if (!previous.empty()) text(name, previous);
    }
    if (voiceWindow) {
        WINDOWPLACEMENT voicePlacement{sizeof(voicePlacement)};
        if (GetWindowPlacement(voiceWindow, &voicePlacement)) {
            auto bounds = voicePlacement.rcNormalPosition;
            number(L"VoiceX", bounds.left);
            number(L"VoiceY", bounds.top);
            number(L"VoiceWidth", bounds.right - bounds.left);
            number(L"VoiceHeight", bounds.bottom - bounds.top);
            number(L"VoiceShow", voiceShowState);
            number(L"VoiceFlags", voicePlacement.flags & WPF_RESTORETOMAXIMIZED);
        }
    }
    number(L"SplitPermille", splitPermille);
    captureOutputGains();
    for (int i = 0; i < 3; ++i) {
        auto key = L"Output" + std::to_wstring(i);
        number((key + L"FmGain").c_str(), outputGains[i].fm);
        number((key + L"PcmGain").c_str(), outputGains[i].pcm);
        number((key + L"MasterGain").c_str(), outputGains[i].master);
    }
    number(L"Aftertouch", liveAftertouch->load());
    number(L"MidiPolyphonic", liveMidiPolyphonic->load());
    number(L"PianoRoll", pianoRoll);
    number(L"CarrierPitch", carrierPitch);
    number(L"Waveforms",monitorView->waveforms.load());
    number(L"PlaybackSpeed", liveSpeed->load());
    number(L"Infinite", SendMessageW(infiniteButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
    number(L"AutoNext", SendMessageW(autoNextButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
    number(L"KeyShift", DWORD(liveKeyShift->load()));
    number(L"KeyMode", DWORD(liveKeyMode->load()));
    text(L"LastPlayedFile", lastPlayedFile.wstring());
    text(L"LastPlayedPdx", lastPlayedBank.wstring());
    number(L"OutputBackend", SendMessageW(outputBox, CB_GETCURSEL, 0, 0));
    int midiSelection = int(SendMessageW(midiBox, CB_GETCURSEL, 0, 0));
    int midiIndex = midiSelection > 0 ? int(SendMessageW(midiBox, CB_GETITEMDATA, midiSelection, 0)) : -1;
    MIDIINCAPSW midiCaps{};
    if (midiIndex >= 0)
        midiInGetDevCapsW(UINT(midiIndex), &midiCaps, sizeof(midiCaps));
    text(L"MidiInputName", midiIndex >= 0 ? midiCaps.szPname : L"");
    number(L"BookmarkCount", DWORD(bookmarks.size()));
    for (size_t i = 0; i < bookmarks.size(); ++i)
        text((L"Bookmark" + std::to_wstring(i)).c_str(), bookmarks[i].wstring());
    number(L"Language", japanese ? 0 : 1);
    struct Numeric {
        HWND h;
        const wchar_t *n;
        int lo, hi;
    };
    for (auto v : {Numeric{loopsEdit, L"Loops", 1, 1000},
                   {fmDelayEdit, L"FmDelayMs", 0, 2000},
                   {pcmDelayEdit, L"PcmDelayMs", 0, 2000},
                   {sampleEdit, L"Sample", 0, 24575}})
        try {
            number(v.n, integer(v.h, v.lo, v.hi));
        } catch (const std::exception &) {
            auto previous = settingText(v.n);
            if (!previous.empty())
                text(v.n, previous);
        }
    number(L"Format", DWORD(SendMessageW(formatBox, CB_GETCURSEL, 0, 0)));
    number(L"FmMask", liveMuteMask->load());
    number(L"PcmMask", livePcmMuteMask->load());
    number(L"Measured", liveMeasuredOutputTiming->load());
    number(L"Dac", liveDacRightDelay->load());
    number(L"ShowAll",SendMessageW(allFilter,BM_GETCHECK,0,0)==BST_CHECKED);
    number(L"ShowMdx", SendMessageW(mdxFilter, BM_GETCHECK, 0, 0) == BST_CHECKED);
    number(L"ShowVgm",SendMessageW(vgmFilter,BM_GETCHECK,0,0)==BST_CHECKED);
    number(L"ShowPdx", SendMessageW(pdxFilter, BM_GETCHECK, 0, 0) == BST_CHECKED);
    number(L"SortColumn", sortColumn);
    number(L"Ascending", ascending);
    captureWidths();
    std::wstring ids;
    for (int id : visualColumns()) {
        if (!ids.empty())
            ids += L',';
        ids += std::to_wstring(id);
    }
    text(L"Columns", ids);
    number(L"TitleColumnMigrated", 1);
    for (int i = 0; i < 10; ++i)
        number((L"ColumnWidth" + std::to_wstring(i)).c_str(), columnWidths[i]);
    text(L"Folder", folder.wstring());
    WINDOWPLACEMENT placement{sizeof(placement)};
    if (fullscreen)
        placement = windowedPlacement;
    else
        GetWindowPlacement(window, &placement);
    RECT r = placement.rcNormalPosition;
    number(L"WindowX", r.left);
    number(L"WindowY", r.top);
    number(L"WindowWidth", r.right - r.left);
    number(L"WindowHeight", r.bottom - r.top);
    number(L"Maximized", placement.showCmd == SW_SHOWMAXIMIZED);
    // Write UTF-16LE atomically in the executable directory. Never fall back to registry.
    std::wstring ini = L"[Settings]\r\n";
    for (auto &pair : values)
        ini += pair.first + L"=" + pair.second + L"\r\n";
    auto temporary = settingsPath;
    temporary += L".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    const unsigned char bom[] = {255, 254};
    file.write((const char *)bom, 2);
    file.write((const char *)ini.data(), std::streamsize(ini.size() * sizeof(wchar_t)));
    file.close();
    if (!file || !MoveFileExW(temporary.c_str(), settingsPath.c_str(),
                              MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        status(
            tr(L"設定ファイルを保存できません。実行ファイルのフォルダーへの書き込み権限を確認してください。",
               L"Cannot save settings. Check write access to the executable folder."));
        return;
    }
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, settingsPath.c_str());
    settingsDirty = false;
}
void restoreWindow() {
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int width = std::clamp(int(setting(L"WindowWidth", scale(1320))), 320,
                           std::max(320, int(work.right - work.left))),
        height = std::clamp(int(setting(L"WindowHeight", scale(840))), 300,
                            std::max(300, int(work.bottom - work.top)));
    int x = std::clamp(int(setting(L"WindowX", work.left)), int(work.left), int(work.right) - width),
        y = std::clamp(int(setting(L"WindowY", work.top)), int(work.top), int(work.bottom) - height);
    restoreMaximized = setting(L"Maximized", 0) != 0;
    SetWindowPos(window, nullptr, x, y, width, height, SWP_NOZORDER);
}
