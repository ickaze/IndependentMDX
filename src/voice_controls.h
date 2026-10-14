// SPDX-License-Identifier: 0BSD
// Copyright (C) 2026 by I.C.KaZe
// Knob controls shared in design with NOTE-MDX-Workbench.
HWND voiceReset = nullptr;
HFONT parameterFont = nullptr, limitFont = nullptr;
mdx::Voice baselineVoice{};
std::array<int, 47> baselineParameters{};
bool baselineVoiceValid = false, voiceDirty = false, updatingVoiceEdit = false;
void refreshVoiceWindow();
void colorVoiceParameters();
bool previewEditedVoice(bool reportError);
std::array<HWND, 47> parameterKnobs{};
std::array<int, 47> parameterValues{};
void setParameterValue(int n, int value);
std::array<HWND, 11> parameterHeaders{};
std::array<HWND, 4> parameterRows{};
std::array<HWND, 3> globalParameterHeaders{};
bool syncingParameters = false, voiceWaveWasActive = false;
mdx::Voice parameterVoice{};
constexpr int parameterMaximum[11] = {31, 31, 31, 15, 15, 127, 3, 15, 7, 3, 1};
int parameterLimit(int n) {
  return n < 44 ? parameterMaximum[n % 11] : n == 46 ? 15 : 7;
}
void setParameterValue(int n, int value) {
  int next = std::clamp(value, 0, parameterLimit(n));
  bool changed = next != parameterValues[n];
  parameterValues[n] = next;
  if (changed)
    InvalidateRect(parameterKnobs[n], nullptr, FALSE);
}
void syncVoiceParameters(const mdx::Voice &voice) {
  parameterVoice = voice;
  syncingParameters = true;
  auto values = mdx::voiceParameterValues(voice);
  for (int n = 0; n < 47; ++n)
    if (parameterKnobs[n])
      setParameterValue(n, values[n]);
  syncingParameters = false;
}
void changedVoiceParameter() {
  if (syncingParameters)
    return;
  // Use the same validated MML path for both editing surfaces.
  std::ostringstream source;
  source << "@" << int(parameterVoice[0]) << " = {\r\n";
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 11; ++col)
      source << parameterValues[row * 11 + col] << ',';
    source << "\r\n";
  }
  for (int col = 0; col < 3; ++col) {
    if (col)
      source << ',';
    source << parameterValues[44 + col];
  }
  source << "\r\n}\r\n";
  std::string error;
  auto parsed = mdx::parseVoiceMml(source.str(), error);
  if (!parsed)
    return;
  updatingVoiceEdit = true;
  SetWindowTextW(voiceEdit, wide(mdx::voiceMml(*parsed, (*parsed)[0])).c_str());
  updatingVoiceEdit = false;
  voiceDirty = true;
  previewEditedVoice(false);
}
LRESULT CALLBACK parameterProcedure(HWND hwnd, UINT message, WPARAM wp,
                                    LPARAM lp, UINT_PTR, DWORD_PTR index) {
  int n = int(index);
  static HWND dragging = nullptr;
  static int originX = 0, originValue = 0, remainder = 0;
  static HWND wheelWindow = nullptr;
  auto change = [&](int value) {
    int next = std::clamp(value, 0, parameterLimit(n));
    if (next != parameterValues[n]) {
      setParameterValue(n, next);
      changedVoiceParameter();
    }
  };
  if (message == WM_MOUSEWHEEL) {
    if (wheelWindow != hwnd) {
      remainder = 0;
      wheelWindow = hwnd;
    }
    remainder += GET_WHEEL_DELTA_WPARAM(wp);
    change(parameterValues[n] + remainder / WHEEL_DELTA);
    remainder %= WHEEL_DELTA;
    return 0;
  }
  if (hwnd == parameterKnobs[n]) {
    if (message == WM_LBUTTONDOWN) {
      SetFocus(hwnd);
      dragging = hwnd;
      originX = GET_X_LPARAM(lp);
      originValue = parameterValues[n];
      SetCapture(hwnd);
      return 0;
    }
    if (message == WM_MOUSEMOVE && dragging == hwnd) {
      RECT r;
      GetClientRect(hwnd, &r);
      int diameter = std::max(1, scale(20));
      change(originValue +
             int(std::lround(double(GET_X_LPARAM(lp) - originX) *
                             parameterLimit(n) / (2 * diameter))));
      return 0;
    }
    if (message == WM_LBUTTONUP) {
      if (GetCapture() == hwnd)
        ReleaseCapture();
      dragging = nullptr;
      return 0;
    }
    if (message == WM_CAPTURECHANGED)
      dragging = nullptr;
    if (message == WM_ERASEBKGND)
      return 1;
    if (message == WM_PAINT) {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd, &ps);
      RECT r;
      GetClientRect(hwnd, &r);
      FillRect(dc, &r, editBrush);
      bool changed =
          baselineVoiceValid && parameterValues[n] != baselineParameters[n];
      COLORREF color = changed ? RGB(255, 80, 80) : textColor;
      int radius = std::max(3, std::min(int(r.right / 2 - 2), scale(10)));
      int cx = r.right / 2, cy = radius + 2;
      HPEN pen = CreatePen(PS_SOLID, std::max(1, scale(1)), color);
      auto oldPen = SelectObject(dc, pen);
      auto oldBrush = SelectObject(dc, editBrush);
      Ellipse(dc, cx - radius, cy - radius, cx + radius, cy + radius);
      if (baselineVoiceValid) {
        double oldAngle =
            (-150.0 + 300.0 * baselineParameters[n] / parameterLimit(n)) *
            3.141592653589793 / 180.0;
        // A translucent reference pointer, composited over the flat knob
        // background.
        COLORREF reference =
            RGB((GetRValue(textColor) + GetRValue(editColor)) * 0.5,
                (GetGValue(textColor) + GetGValue(editColor)) * 0.5,
                (GetBValue(textColor) + GetBValue(editColor)) * 0.5);
        HPEN ghost = CreatePen(PS_SOLID, 1, reference);
        auto currentPen = SelectObject(dc, ghost);
        MoveToEx(dc, cx, cy, nullptr);
        LineTo(dc, cx + int(std::sin(oldAngle) * (radius - 2)),
               cy - int(std::cos(oldAngle) * (radius - 2)));
        SelectObject(dc, currentPen);
        DeleteObject(ghost);
      }
      double angle = (-150.0 + 300.0 * parameterValues[n] / parameterLimit(n)) *
                     3.141592653589793 / 180.0;
      MoveToEx(dc, cx, cy, nullptr);
      LineTo(dc, cx + int(std::sin(angle) * (radius - 3)),
             cy - int(std::cos(angle) * (radius - 3)));
      SelectObject(dc, oldPen);
      SelectObject(dc, oldBrush);
      DeleteObject(pen);
      auto oldFont = SelectObject(dc, limitFont);
      SetBkMode(dc, TRANSPARENT);
      SetTextColor(dc, textColor);
      RECT minimum{r.right / 2 - scale(16), scale(16), r.right / 2 - scale(9),
                   scale(28)};
      RECT maximum{r.right / 2 + scale(9), scale(16),
                   std::min(r.right, r.right / 2 + scale(19)), scale(28)};
      DrawTextW(dc, L"0", 1, &minimum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      auto maxText = std::to_wstring(parameterLimit(n));
      DrawTextW(dc, maxText.c_str(), -1, &maximum,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      RECT current{r.right / 2 - scale(9), scale(16), r.right / 2 + scale(9),
                   scale(28)};
      FillRect(dc, &current, editBrush);
      SelectObject(dc, parameterFont);
      SetTextColor(dc, color);
      auto value = std::to_wstring(parameterValues[n]);
      DrawTextW(dc, value.c_str(), -1, &current,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(dc, oldFont);
      EndPaint(hwnd, &ps);
      return 0;
    }
  }
  if (message == WM_CHAR || message == WM_SYSCHAR)
    return 0;
  return DefSubclassProc(hwnd, message, wp, lp);
}

bool previewEditedVoice(bool reportError) {
  int length = GetWindowTextLengthW(voiceEdit);
  std::wstring text(length + 1, L'\0');
  GetWindowTextW(voiceEdit, text.data(), length + 1);
  int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0,
                                 nullptr, nullptr);
  std::string utf8(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), length, utf8.data(), size,
                      nullptr, nullptr);
  std::string error;
  auto voice = mdx::parseVoiceMml(utf8, error);
  if (!voice || !baselineVoiceValid || (*voice)[0] != baselineVoice[0]) {
    EnableWindow(voiceReset, baselineVoiceValid);
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(retainedVoices->mutex);
    if (!retainedVoices->editVoice((*voice)[0], *voice))
      return false;
    for (int ch = 0; ch < 8; ++ch)
      if (retainedVoices->valid[ch] && retainedVoices->ids[ch] == (*voice)[0])
        retainedVoices->voices[ch] = *voice;
  }
  syncVoiceParameters(*voice);
  voiceDirty = *voice != baselineVoice;
  EnableWindow(voiceReset, voiceDirty);
  shownMml = text.substr(0, length);
  shownAlgorithm = (*voice)[1] & 7;
  shownFeedback = ((*voice)[1] >> 3) & 7;
  std::array<uint8_t, 256> regs{};
  int ch = std::max(0, int(SendMessageW(voiceChannel, CB_GETCURSEL, 0, 0)));
  regs[0x20 + ch] = (*voice)[1];
  for (int g = 0; g < 6; ++g)
    for (int op = 0; op < 4; ++op)
      regs[0x40 + g * 32 + op * 8 + ch] = (*voice)[3 + g * 4 + op];
  shownRegisters = wide(mdx::voiceRegisters(regs, ch));
  SetWindowTextW(registerEdit, shownRegisters.c_str());
  colorVoiceParameters();
  InvalidateRect(voiceList, nullptr, FALSE);
  InvalidateRect(voiceWindow, nullptr, FALSE);
  return true;
}
