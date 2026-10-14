// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

#pragma once
// Shared keyboard, pitch left-to-right; elapsed notes scroll vertically upwards.
const COLORREF trackColors[16] = {
    RGB(255, 95, 95),  RGB(255, 174, 72), RGB(242, 225, 70),  RGB(116, 226, 99),
    RGB(72, 214, 198), RGB(94, 167, 255), RGB(178, 137, 255), RGB(245, 122, 204),
    RGB(205, 72, 72),  RGB(196, 130, 40), RGB(174, 180, 45),  RGB(54, 163, 99),
    RGB(47, 155, 177), RGB(86, 104, 210), RGB(137, 82, 205),  RGB(205, 76, 143)};
struct RollFrame {
    ULONGLONG time;
    mdx::DisplayChannels notes;
};
std::deque<RollFrame> rollHistory;
ULONGLONG rollTime = 0, rollWallTime = 0;
bool rollMoving = false;
void captureRoll(const mdx::MonitorFrame &frame, const mdx::DisplayChannels &display) {
    auto wall = GetTickCount64();
    bool active = running && !idleMidi && !pauseFlag && liveSpeed->load() > 0;
    for (auto &n : retainedVoices->midiNotes)
        active |= n.load() >= 0;
    if (rollWallTime && rollMoving)
        rollTime += wall - rollWallTime;
    rollWallTime = wall;
    bool changed = active != rollMoving;
    rollMoving = active;
    auto now = rollTime;
    if (rollHistory.empty() || changed || (active && now - rollHistory.back().time >= 40)) {
        auto notes = display;
        for (int c = 0; c < 16; ++c)
            if (((c < 8 ? liveMuteMask : livePcmMuteMask)->load() & (1u << (c % 8))) &&
                retainedVoices->midiNotes[c] < 0)
                notes[c].fill(-1);
        rollHistory.push_back({now, notes});
    }
    while (!rollHistory.empty() && now - rollHistory.front().time > 8050)
        rollHistory.pop_front();
}
void captureRoll(const mdx::MonitorFrame &frame) { captureRoll(frame, displayedNotes(frame)); }
void paintPianoRoll(HDC dc, RECT r, const mdx::MonitorFrame &frame) {
    auto fill = [&](RECT box, COLORREF color) {
        SetDCBrushColor(dc,color);
        FillRect(dc,&box,HBRUSH(GetStockObject(DC_BRUSH)));
    };
    int legendHeight = scale(24), top = 2 * legendHeight;
    int keyWidth = std::max(1, (int(r.right) - scale(12)) / 96);
    int width = keyWidth * 96, left = (int(r.right) - width) / 2;
    int bottom = std::max(top + 1, int(r.bottom) - scale(70)), height = bottom - top;
    for (int c = 0; c < 16; ++c) {
        bool muted = ((c < 8 ? liveMuteMask : livePcmMuteMask)->load() & (1u << (c % 8))) != 0;
        RECT box{left + (c % 8) * width / 8, (c / 8) * legendHeight,
                 left + (c % 8 + 1) * width / 8 - 2, (c / 8 + 1) * legendHeight - 2};
        fill(box, muted ? RGB(37, 39, 47) : trackColors[c]);
        SetTextColor(dc, muted ? RGB(120, 120, 128) : RGB(8, 10, 20));
        wchar_t label[2]{wchar_t(c < 8 ? L'A' + c : L'P' + c - 8), 0};
        DrawTextW(dc, label, 1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (int n = 0; n < 96; ++n) {
        RECT lane{left + n * width / 96, top, left + (n + 1) * width / 96, bottom};
        fill(lane, blackKey(n + 3) ? RGB(13, 16, 27) : RGB(25, 29, 45));
        fill(RECT{lane.left, top, lane.left + 1, bottom}, RGB(39, 44, 61));
    }
    ULONGLONG now = rollTime;
    for (int sec = 1; sec <= 8; ++sec) {
        int y = bottom - height * sec / 8;
        fill(RECT{left, y, left + width, y + 1}, RGB(53, 58, 78));
    }
    for (size_t i = 0; i < rollHistory.size(); ++i) {
        auto &entry = rollHistory[i];
        auto end = i + 1 < rollHistory.size() ? rollHistory[i + 1].time : now;
        int y1 = std::max(top, bottom - int((now - entry.time) * height / 8000));
        int y2 = std::max(y1 + 1, bottom - int((now - end) * height / 8000));
        for (int c = 0; c < 16; ++c) {
            for (int n : entry.notes[c]) {
                if (n < 0 || n >= 96)
                    continue;
                int index = 0, count = 0;
                for (int j = 0; j < 16; ++j)
                    if (mdx::hasDisplayNote(entry.notes[j], n)) {
                        if (j < c)
                            ++index;
                        ++count;
                    }
                int x1 = left + n * width / 96, x2 = left + (n + 1) * width / 96;
                int span = std::max(1, x2 - x1);
                fill(RECT{x1 + index * span / count, y1,
                          x1 + std::max(index * span / count + 1, (index + 1) * span / count),
                          std::min(bottom, y2)},
                     trackColors[c]);
            }
        }
    }
    // A single chromatic keyboard aligned with the 96 pitch lanes (0 = o0 D#).
    for (int n = 0; n < 96; ++n) {
        RECT key{left + n * width / 96, bottom + 2, left + (n + 1) * width / 96 - 1, r.bottom - 2};
        fill(key, blackKey(n + 3) ? RGB(12, 14, 22) : RGB(205, 208, 215));
        int count = 0;
        for (int c = 0; c < 16; ++c)
            if (!rollHistory.empty() && mdx::hasDisplayNote(rollHistory.back().notes[c], n))
                ++count;
        int index = 0;
        for (int c = 0; c < 16; ++c)
            if (!rollHistory.empty() && mdx::hasDisplayNote(rollHistory.back().notes[c], n)) {
                int h = key.bottom - key.top;
                fill(RECT{key.left, key.top + index * h / count, key.right, key.top + (++index) * h / count},
                     trackColors[c]);
            }
    }
}
