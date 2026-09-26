#pragma once
// A drag-only sibling overlay presents one completed bitmap. Native child
// windows retain their state/geometry and print into it, without style changes.
HWND splitterOverlay = nullptr;
HDC splitterDC = nullptr;
HBITMAP splitterBitmap = nullptr;
HGDIOBJ splitterOldBitmap = nullptr;
SIZE splitterBitmapSize{};
bool splitterPainting = false, splitterLayoutPending = false;
void freeSplitterBitmap() {
    if (splitterDC) {
        SelectObject(splitterDC, splitterOldBitmap);
        DeleteObject(splitterBitmap);
        DeleteDC(splitterDC);
    }
    splitterDC = nullptr;
    splitterBitmap = nullptr;
    splitterOldBitmap = nullptr;
    splitterBitmapSize = {};
}
LRESULT CALLBACK splitterOverlayProcedure(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND)
        return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (splitterDC)
            BitBlt(dc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left,
                   ps.rcPaint.bottom - ps.rcPaint.top, splitterDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
void paintSplitterFrame() {
    if (!splitterBuffered || !splitterOverlay || splitterPainting)
        return;
    splitterPainting = true;
    RECT client;
    GetClientRect(window, &client);
    int width = std::max(1L, client.right), height = std::max(1L, client.bottom);
    if (!splitterDC || splitterBitmapSize.cx != width || splitterBitmapSize.cy != height) {
        freeSplitterBitmap();
        HDC target = GetDC(window);
        splitterDC = CreateCompatibleDC(target);
        splitterBitmap = CreateCompatibleBitmap(target, width, height);
        ReleaseDC(window, target);
        if (!splitterDC || !splitterBitmap) {
            if (splitterDC)
                DeleteDC(splitterDC);
            if (splitterBitmap)
                DeleteObject(splitterBitmap);
            splitterDC = nullptr;
            splitterBitmap = nullptr;
            splitterPainting = false;
            return;
        }
        splitterOldBitmap = SelectObject(splitterDC, splitterBitmap);
        splitterBitmapSize = {width, height};
    }
    FillRect(splitterDC, &client, backgroundBrush);
    // Paint bottom-to-top; HWND order remains unchanged throughout the drag.
    std::vector<HWND> children;
    for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        if (child != splitterOverlay && IsWindowVisible(child))
            children.push_back(child);
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        RECT box;
        GetWindowRect(*it, &box);
        MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&box), 2);
        RECT clip;
        if (!IntersectRect(&clip, &box, &client))
            continue;
        int saved = SaveDC(splitterDC);
        IntersectClipRect(splitterDC, box.left, box.top, box.right, box.bottom);
        SetViewportOrgEx(splitterDC, box.left, box.top, nullptr);
        SendMessageW(*it, WM_PRINT, WPARAM(splitterDC),
                     PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND | PRF_CHILDREN);
        RestoreDC(splitterDC, saved);
    }
    SetWindowPos(splitterOverlay, HWND_TOP, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOREDRAW);
    InvalidateRect(splitterOverlay, nullptr, FALSE);
    UpdateWindow(splitterOverlay);
    splitterPainting = false;
}
void bufferSplitter(bool enabled) {
    if (enabled == splitterBuffered)
        return;
    splitterBuffered = enabled;
    if (enabled) {
        WNDCLASSW cls{};
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpfnWndProc = splitterOverlayProcedure;
        cls.lpszClassName = L"IndependentMDXSplitterFrame";
        cls.hCursor = LoadCursorW(nullptr, IDC_SIZEWE);
        RegisterClassW(&cls);
        splitterOverlay = CreateWindowExW(0, cls.lpszClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1,
                                          window, nullptr, cls.hInstance, nullptr);
        if (!splitterOverlay) {
            splitterBuffered = false;
            return;
        }
        paintSplitterFrame();
        SetTimer(window, 4, 33, nullptr);
    } else {
        KillTimer(window, 4);
        splitterLayoutPending = false;
        HWND overlay = splitterOverlay;
        splitterOverlay = nullptr;
        if (overlay)
            DestroyWindow(overlay);
        freeSplitterBitmap();
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
}
