// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

// Modeless plain-text viewing and Explorer copy integration.
HWND documentWindow = nullptr, documentEdit = nullptr;
std::filesystem::path documentPath;
HIMAGELIST browserImages = nullptr;
std::map<std::wstring,int> documentIcons;
const std::array<std::wstring,5> documentExtensions{L".doc",L".txt",L".hed",L".mus",L".pdl"};
int documentPriority(std::wstring ext) {
    for(auto &c:ext)c=wchar_t(towlower(c));
    auto it=std::find(documentExtensions.begin(),documentExtensions.end(),ext);
    return int(it-documentExtensions.begin());
}
std::wstring documentType(const std::filesystem::path &path) {
    auto ext=path.extension().wstring();if(!ext.empty())ext.erase(0,1);
    for(auto &c:ext)c=wchar_t(towupper(c));return ext;
}
bool textFile(const std::filesystem::path &p) {
    auto ext = p.extension().wstring();
    for (auto &c : ext) c = wchar_t(towlower(c));
    for (auto x : {L".txt",L".doc",L".hed",L".pdl",L".md",L".mml",L".mus",L".ini",L".log",L".csv",L".nfo",L".cfg"})
        if (ext == x) return true;
    return false;
}
LRESULT CALLBACK documentKeys(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_KEYDOWN && (w == VK_ESCAPE || w == VK_SPACE || w == VK_RETURN)) {
        ShowWindow(documentWindow, SW_HIDE);
        SetForegroundWindow(window); SetFocus(browserList); return 0;
    }
    return DefSubclassProc(h,m,w,l);
}
LRESULT CALLBACK documentProcedure(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SIZE && documentEdit) {
        MoveWindow(documentEdit,0,0,LOWORD(l),HIWORD(l),TRUE); return 0;
    }
    if (m == WM_CLOSE) { ShowWindow(h,SW_HIDE); SetFocus(browserList); return 0; }
    if (m == WM_SETFOCUS) { SetFocus(documentEdit); return 0; }
    return DefWindowProcW(h,m,w,l);
}
void showDocument(const std::filesystem::path &path, bool toggle = true) {
    if (path.empty()) return;
    if (toggle && documentWindow && documentPath == path && IsWindowVisible(documentWindow)) {
        ShowWindow(documentWindow,SW_HIDE); SetFocus(browserList); return;
    }
    try {
        auto physical = mdx::resolveArchivePath(path);
        std::ifstream in(physical,std::ios::binary|std::ios::ate);
        auto size = in.tellg();
        if (!in || size < 0 || size > 16*1024*1024) throw std::runtime_error("Text viewer limit: 16 MiB");
        std::string b(size_t(size),'\0'); in.seekg(0);
        if (!in.read(b.data(),b.size())) throw std::runtime_error("Cannot read text file");
        std::wstring text;
        if (b.size()>=2 && ((uint8_t(b[0])==255 && uint8_t(b[1])==254) || (uint8_t(b[0])==254 && uint8_t(b[1])==255))) {
            bool le=uint8_t(b[0])==255;
            if(b.size()%2) throw std::runtime_error("Invalid UTF-16 text length");
            for(size_t i=2;i+1<b.size();i+=2) text+=wchar_t(le ? uint8_t(b[i])|(uint8_t(b[i+1])<<8) : (uint8_t(b[i])<<8)|uint8_t(b[i+1]));
        } else {
            if(b.compare(0,3,"\xef\xbb\xbf")==0)b.erase(0,3);
            int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,b.data(),int(b.size()),nullptr,0);
            if(count) {text.resize(count);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,b.data(),int(b.size()),text.data(),count);}
            else text=wide(mdx::utf8(b));
        }
        if (text.find(L'\0')!=std::wstring::npos) throw std::runtime_error("Binary document is not supported by the text viewer");
        std::wstring normalized;
        for(size_t i=0;i<text.size();++i) {if(text[i]==L'\n' && (!i||text[i-1]!=L'\r'))normalized+=L'\r';normalized+=text[i];}
        if (!documentWindow) {
            WNDCLASSW c{};c.lpfnWndProc=documentProcedure;c.hInstance=GetModuleHandleW(nullptr);
            c.lpszClassName=L"IndependentMDXTextViewer";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hbrBackground=editBrush;RegisterClassW(&c);
            documentWindow=CreateWindowW(c.lpszClassName,L"",WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,CW_USEDEFAULT,800,600,window,nullptr,c.hInstance,nullptr);
            documentEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,0,0,1,1,documentWindow,nullptr,c.hInstance,nullptr);
            SendMessageW(documentEdit,WM_SETFONT,(WPARAM)GetStockObject(ANSI_FIXED_FONT),TRUE);
            SendMessageW(documentEdit,EM_SETLIMITTEXT,32*1024*1024,0);
            SetWindowSubclass(documentEdit,documentKeys,1,0);
        }
        if(documentPath!=path) {SetWindowTextW(documentEdit,normalized.c_str());SendMessageW(documentEdit,EM_SETSEL,0,0);SendMessageW(documentEdit,EM_SCROLLCARET,0,0);}
        documentPath=path;SetWindowTextW(documentWindow,path.filename().c_str());
        RECT rc;GetClientRect(documentWindow,&rc);MoveWindow(documentEdit,0,0,rc.right,rc.bottom,TRUE);
        ShowWindow(documentWindow,SW_SHOWNORMAL);SetForegroundWindow(documentWindow);SetFocus(documentEdit);
    } catch(const std::exception &e) {MessageBoxW(window,wide(e.what()).c_str(),L"Text viewer",MB_OK|MB_ICONERROR);}
}
void resetBrowserImages() {
    auto old=browserImages;
    browserImages=ImageList_Create(16,16,ILC_COLOR32|ILC_MASK,32,32);
    ListView_SetImageList(browserList,browserImages,LVSIL_SMALL);
    if(old)ImageList_Destroy(old);
    HDC dc=GetDC(window),mem=CreateCompatibleDC(dc);
    HBITMAP bitmap=CreateCompatibleBitmap(dc,16,16);auto previous=SelectObject(mem,bitmap);
    HBRUSH bg=CreateSolidBrush(RGB(255,255,255));RECT rect{0,0,16,16};FillRect(mem,&rect,bg);DeleteObject(bg);
    SetTextColor(mem,RGB(0,0,0));SetBkMode(mem,TRANSPARENT);
    HFONT font=CreateFontW(-8,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Arial");auto previousFont=SelectObject(mem,font);
    documentIcons.clear();
    for(auto &ext:documentExtensions) {
        auto label=documentType(std::filesystem::path(L"file"+ext));
        FillRect(mem,&rect,(HBRUSH)GetStockObject(WHITE_BRUSH));
        DrawTextW(mem,label.c_str(),int(label.size()),&rect,DT_SINGLELINE|DT_CENTER|DT_VCENTER);
        // Deselect the bitmap before copying it into the image list.
        SelectObject(mem,previous);
        documentIcons[label]=ImageList_AddMasked(browserImages,bitmap,RGB(255,0,255));
        SelectObject(mem,bitmap);
    }
    SelectObject(mem,previousFont);DeleteObject(font);SelectObject(mem,previous);
    DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(window,dc);
}
void documentMenu(const FileRow &row, POINT point) {
    if(row.documents.empty())return;
    HMENU menu=CreatePopupMenu();
    for(size_t i=0;i<row.documents.size();++i) {
        auto type=documentType(row.documents[i]);
        AppendMenuW(menu,MF_STRING,UINT_PTR(i+1),type.c_str());
    }
    auto paths=row.documents; // The message loop may navigate while the menu is open.
    int chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window,nullptr);
    DestroyMenu(menu);
    if(chosen>0 && size_t(chosen)<=paths.size())showDocument(paths[size_t(chosen-1)],false);
}
void copyDrag(const FileRow &row) {
    try {
        // Resolve archive members to their extracted files, retaining the basename.
        auto path=mdx::archiveCopyPath(row.path);
        HRESULT initialized=OleInitialize(nullptr);
        if(FAILED(initialized)) throw std::runtime_error("Cannot initialize Explorer drag and drop");
        struct OleScope {~OleScope(){OleUninitialize();}} scope;
        PIDLIST_ABSOLUTE pidl=nullptr;
        if(FAILED(SHParseDisplayName(path.c_str(),nullptr,&pidl,0,nullptr))) throw std::runtime_error("Cannot resolve copy source");
        IShellFolder *parent=nullptr;PCUITEMID_CHILD child=nullptr;IDataObject *data=nullptr;
        HRESULT hr=SHBindToParent(pidl,IID_IShellFolder,reinterpret_cast<void**>(&parent),&child);
        if(SUCCEEDED(hr)) {hr=parent->GetUIObjectOf(window,1,&child,IID_IDataObject,nullptr,reinterpret_cast<void**>(&data));parent->Release();}
        if(SUCCEEDED(hr)) {DWORD effect=0;SHDoDragDrop(window,data,nullptr,DROPEFFECT_COPY,&effect);data->Release();}
        CoTaskMemFree(pidl);
        if(FAILED(hr)) throw std::runtime_error("Cannot create Explorer copy operation");
    } catch(const std::exception &e) {MessageBoxW(window,wide(e.what()).c_str(),L"Copy",MB_OK|MB_ICONERROR);}
}
