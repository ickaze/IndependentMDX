// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

// RichEdit formatting and persistent voice-window geometry.
POINT voiceMinimumSize() {
  HDC dc = GetDC(window);
  HFONT temporary = nullptr;
  HFONT font = voiceFont;
  if (!font) {
    temporary = CreateFontW(
        -MulDiv(10, GetDeviceCaps(dc, LOGPIXELSY), 72), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
    font = temporary;
  }
  auto previous = SelectObject(dc, font);
  TEXTMETRICW metrics{};
  GetTextMetricsW(dc, &metrics);
  SelectObject(dc, previous);
  if (temporary)
    DeleteObject(temporary);
  ReleaseDC(window, dc);
  // Layout reserves 495 scaled pixels below/around the MML field.
  int textHeight =
      metrics.tmHeight * 9 + scale(8) + GetSystemMetrics(SM_CYHSCROLL);
  RECT bounds{0, 0, scale(800), scale(495) + textHeight};
  AdjustWindowRectEx(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0);
  return {scale(800), bounds.bottom - bounds.top};
}
void restoreVoicePlacement() {
  auto minimum = voiceMinimumSize();
  WINDOWPLACEMENT placement{sizeof(placement)};
  GetWindowPlacement(voiceWindow, &placement);
  RECT original = placement.rcNormalPosition;
  int width =
      std::clamp(int(setting(L"VoiceWidth", minimum.x)), int(minimum.x), 16000);
  int height = std::clamp(int(setting(L"VoiceHeight", minimum.y)),
                          int(minimum.y), 16000);
  int x = int(setting(L"VoiceX", original.left));
  int y = int(setting(L"VoiceY", original.top));
  RECT saved{x, y, x + width, y + height};
  MONITORINFO monitor{sizeof(monitor)};
  GetMonitorInfoW(MonitorFromRect(&saved, MONITOR_DEFAULTTONEAREST), &monitor);
  auto work = monitor.rcWork;
  width =
      std::min(width, std::max(int(minimum.x), int(work.right - work.left)));
  height =
      std::min(height, std::max(int(minimum.y), int(work.bottom - work.top)));
  x = std::clamp(x, int(work.left),
                 std::max(int(work.left), int(work.right) - width));
  y = std::clamp(y, int(work.top),
                 std::max(int(work.top), int(work.bottom) - height));
  placement.rcNormalPosition = {x, y, x + width, y + height};
  placement.showCmd = SW_HIDE;
  placement.flags = setting(L"VoiceFlags", 0) & WPF_RESTORETOMAXIMIZED;
  SetWindowPlacement(voiceWindow, &placement);
}
void colorVoiceParameters() {
  if (!voiceEdit)
    return;
  // GT_DEFAULT uses RichEdit's own character indexing (no CR/LF expansion).
  GETTEXTLENGTHEX lengthInfo{GTL_NUMCHARS | GTL_PRECISE, 1200};
  int length =
      int(SendMessageW(voiceEdit, EM_GETTEXTLENGTHEX, WPARAM(&lengthInfo), 0));
  std::wstring text(length + 1, L'\0');
  GETTEXTEX get{DWORD(text.size() * sizeof(wchar_t)), GT_DEFAULT, 1200, nullptr,
                nullptr};
  length = int(
      SendMessageW(voiceEdit, EM_GETTEXTEX, WPARAM(&get), LPARAM(text.data())));
  text.resize(length);
  CHARRANGE selection{};
  POINT scroll{};
  SendMessageW(voiceEdit, EM_EXGETSEL, 0, LPARAM(&selection));
  SendMessageW(voiceEdit, EM_GETSCROLLPOS, 0, LPARAM(&scroll));
  bool modified = SendMessageW(voiceEdit, EM_GETMODIFY, 0, 0) != 0;
  bool updating = updatingVoiceEdit;
  updatingVoiceEdit = true;
  SendMessageW(voiceEdit, WM_SETREDRAW, FALSE, 0);
  auto color = [&](LONG begin, LONG end, COLORREF value) {
    CHARRANGE range{begin, end};
    SendMessageW(voiceEdit, EM_EXSETSEL, 0, LPARAM(&range));
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = value;
    SendMessageW(voiceEdit, EM_SETCHARFORMAT, SCF_SELECTION, LPARAM(&format));
  };
  color(0, -1, textColor);
  auto spans = mdx::voiceParameterSpans(text);
  if (baselineVoiceValid && spans.size() == 47)
    for (size_t i = 0; i < spans.size(); ++i)
      if (parameterValues[i] != baselineParameters[i])
        color(LONG(spans[i].first), LONG(spans[i].second), RGB(255, 95, 95));
  SendMessageW(voiceEdit, EM_EXSETSEL, 0, LPARAM(&selection));
  SendMessageW(voiceEdit, EM_SETSCROLLPOS, 0, LPARAM(&scroll));
  SendMessageW(voiceEdit, EM_SETMODIFY, modified, 0);
  SendMessageW(voiceEdit, WM_SETREDRAW, TRUE, 0);
  updatingVoiceEdit = updating;
  InvalidateRect(voiceEdit, nullptr, FALSE);
}
