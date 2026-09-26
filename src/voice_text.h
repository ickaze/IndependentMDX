#pragma once
#include "player.h"
#include <iomanip>
#include <sstream>
namespace mdx {
using Voice = std::array<uint8_t, 27>;
inline std::string voiceMml(const Voice &v, int id) {
    std::ostringstream s;
    s << "@" << id << " = {\r\n/* AR, DR, SR, RR, SL, OL, KS, ML, DT1, DT2, AME */\r\n";
    // MML/diagram order is OP1, OP2, OP3, OP4; register slots are 0,2,1,3.
    for (int op : {0, 2, 1, 3}) {
        int dt = v[3 + op], tl = v[7 + op], ar = v[11 + op], dr = v[15 + op], sr = v[19 + op],
            rr = v[23 + op];
        const int values[] = {ar & 31, dr & 31, sr & 31,       rr & 15, rr >> 4, tl & 127,
                              ar >> 6, dt & 15, (dt >> 4) & 7, sr >> 6, dr >> 7};
        s << "  ";
        for (int x : values)
            s << std::setw(3) << x << ',';
        s << "\r\n";
    }
    s << "/* CON, FL, OP */\r\n  " << (v[1] & 7) << ", " << ((v[1] >> 3) & 7) << ", " << (v[2] & 15)
      << "\r\n}\r\n";
    return s.str();
}
inline Voice registerVoice(const std::array<uint8_t, 256> &r, int ch, int mask) {
    Voice v{};
    v[1] = r[0x20 + ch] & 63;
    v[2] = uint8_t(mask);
    for (int g = 0; g < 6; ++g)
        for (int op = 0; op < 4; ++op)
            v[3 + g * 4 + op] = r[0x40 + g * 32 + op * 8 + ch];
    return v;
}
inline std::string voiceRegisters(const std::array<uint8_t, 256> &r, int ch) {
    std::ostringstream s;
    s << "/* Channel " << char('A' + ch) << " : y register,value */\r\n";
    s << std::uppercase << std::hex << std::setfill('0');
    auto row = [&](int a) { s << "y$" << std::setw(2) << a << ",$" << std::setw(2) << int(r[a]) << "\r\n"; };
    for (int a = 0x20 + ch; a < 0x40; a += 8)
        row(a);
    for (int g = 0; g < 6; ++g)
        for (int op = 0; op < 4; ++op)
            row(0x40 + g * 32 + op * 8 + ch);
    return s.str();
}
inline std::string songVoicesMml(const Song &s) {
    std::string out =
        "/* IndependentMDX: MXDRV instrument definitions (OP1,OP2,OP3,OP4 / M1,C1,M2,C2) */\r\n";
    for (int i = 0; i < 256; ++i)
        if (s.hasVoice[i])
            out += voiceMml(s.voices[i], i) + "\r\n";
    return out;
}
} // namespace mdx
