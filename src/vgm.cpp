// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

#include "vgm.h"
#include "archive.h"
#include "gzip_decode.h"
#include "opm_backend.h"
#include "stereo_output.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
namespace mdx {
bool isVgm(const std::filesystem::path &p) {
  auto e = p.extension().u8string();
  for (auto &c : e)
    if (c >= 'A' && c <= 'Z')
      c += 32;
  return e == ".vgm" || e == ".vgz" || e == ".mrl" || e == ".mrz";
}
void VgmSong::setLogLoop(const MrlHeader &h) {
  if (!h.loopEnd) return;
  looping = true;
  loopSample = double(h.loopStart) * 44100 / h.hz;
  logLoopEnd = double(h.loopEnd) * 44100 / h.hz;
  samples = uint64_t(std::ceil(logLoopEnd));
  loopEvent = size_t(h.loopEvent);
  logLoopEndEvent = h.loopEndEvent;
}
VgmSong::VgmSong(Bytes bytes) {
  if (bytes.size() >= 8 &&
      std::equal(bytes.begin(), bytes.begin() + 8, "MDXRLOG\0")) {
    std::string raw(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    std::istringstream in(raw, std::ios::binary);
    auto h = readMrlHeader(in, bytes.size());
    title = h.title.empty() ? "MDX register log" : h.title;
    if (h.embedded) {
      Bytes bank(bytes.begin() + h.bankOffset, bytes.begin() + h.offset);
      if (dataCrc(bank.data(), bank.size()) != h.pdxCrc)
        throw std::runtime_error("MRL PDX CRC mismatch");
      pdx = std::make_shared<Pdx>(std::move(bank));
    }
    uint64_t previous = 0;
    unsigned stage = 0;
    for (uint64_t i = 0; i < h.count; ++i) {
      auto r = readMrlRecord(bytes.data() + h.offset + i * 16, h);
      unsigned rank = r.phase == 1 ? 0 : r.phase == 0 ? 1 : 2;
      if (r.time < previous || rank < stage)
        throw std::runtime_error("Invalid MRL event order");
      previous = r.time;
      stage = rank;
      events.push_back(
          {r.argument,
           uint16_t(r.chip ? 0x200 + (r.chip - 1) * 8 + r.op : r.op), r.value});
      preciseTimes.push_back(double(r.time) * 44100 / h.hz);
    }
    samples = uint64_t(std::ceil(double(h.end) * 44100 / h.hz));
    clock = 4000000;
    registerLog = true;
    setLogLoop(h);
    return;
  }
  auto b = unpackVgm(std::move(bytes));
  auto fail = [](size_t p, const std::string &why) {
    std::ostringstream s;
    s << "VGM error at file offset 0x" << std::hex << p << ": " << why;
    throw std::runtime_error(s.str());
  };
  auto need = [&](size_t p, size_t n) {
    if (p > b.size() || n > b.size() - p)
      fail(p, "truncated data");
  };
  auto u32 = [&](size_t p) {
    need(p, 4);
    return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) |
           (uint32_t(b[p + 2]) << 16) | (uint32_t(b[p + 3]) << 24);
  };
  need(0, 64);
  if (b[0] != 'V' || b[1] != 'g' || b[2] != 'm' || b[3] != ' ')
    fail(0, "expected Vgm signature");
  uint32_t ver = u32(8);
  if (ver < 0x100 || ver > 0x171)
    fail(8, "supported versions: 1.00..1.71");
  size_t end = u32(4) ? size_t(u32(4)) + 4 : b.size();
  if (end > b.size() || end < 64)
    fail(4, "invalid EOF offset");
  b.resize(end);
  size_t start = ver >= 0x150 && u32(0x34) ? size_t(u32(0x34)) + 0x34 : 0x40;
  if (start < 64 || start >= end)
    fail(0x34, "invalid data offset");
  clock = ver <= 0x101 ? u32(0x10) : u32(0x30);
  if (clock & 0xc0000000u)
    fail(0x30, "dual YM2151 and YM2164 are not supported");
  if (clock < 100000 || clock > 10000000)
    fail(0x30, "YM2151 clock must be 100000..10000000 Hz");
  pwm = ver >= 0x151 && start >= 0x74 && u32(0x70) != 0;
  if (pwm && u32(0x70) != 23011361)
    fail(0x70, "unsupported PWM clock (direct export profile required)");
  for (size_t p :
       {0x0cu, 0x10u, 0x2cu, 0x38u, 0x40u, 0x44u, 0x48u, 0x4cu, 0x50u, 0x54u,
        0x58u, 0x5cu, 0x60u, 0x64u, 0x68u, 0x6cu, 0x74u, 0x80u, 0x84u, 0x88u,
        0x8cu, 0x90u, 0x98u, 0x9cu, 0xa0u, 0xa4u, 0xa8u, 0xacu, 0xb0u, 0xb4u,
        0xb8u, 0xc0u, 0xc4u, 0xc8u, 0xccu, 0xd0u, 0xd8u, 0xdcu, 0xe0u}) {
    if (p + 4 > start || (ver <= 0x101 && p == 0x10) ||
        (ver < 0x110 && p >= 0x2c) || (ver < 0x151 && p >= 0x38))
      continue;
    if (u32(p)) {
      const char *name = "other chip";
      switch (p) {
      case 0x0c:
        name = "SN76489";
        break;
      case 0x10:
        name = "YM2413";
        break;
      case 0x2c:
        name = "YM2612";
        break;
      case 0x38:
        name = "Sega PCM";
        break;
      case 0x44:
        name = "YM2203";
        break;
      case 0x48:
        name = "YM2608";
        break;
      case 0x4c:
        name = "YM2610";
        break;
      case 0x74:
        name = "AY8910";
        break;
      case 0x90:
        name = "OKIM6258";
        break;
      case 0x98:
        name = "OKIM6295";
        break;
      }
      fail(p, std::string("unsupported sound chip: ") + name +
                  "; only one YM2151 is supported");
    }
  }
  if (start >= 0xc0 && u32(0xbc))
    fail(0xbc, "extra chip headers are not supported");
  if (ver >= 0x150 && start > 0x7c) {
    int v = b[0x7c];
    if (v > 192)
      v -= 256;
    if (v == -63)
      v = -64;
    volume = std::exp2(v / 32.0);
  }
  size_t loop = u32(0x1c) ? size_t(u32(0x1c)) + 0x1c : 0;
  if (loop && (loop < start || loop >= end))
    fail(0x1c, "loop offset outside command data");
  unsigned wait60 = 735, wait50 = 882;
  bool sawLoop = false, ended = false;
  // Every register command occupies three bytes. Reserve an input-bounded
  // capacity once, avoiding repeated growth/copies for dense register streams.
  events.reserve((end - start) / 3);
  size_t p = start;
  auto wait = [&](unsigned n) {
    samples += n;
    if (samples > 44100ull * 86400)
      fail(p, "duration exceeds 24 hours");
  };
  while (p < end) {
    if (p == loop) {
      sawLoop = true;
      loopSample = samples;
      loopEvent = events.size();
    }
    size_t at = p;
    unsigned cmd = b[p++];
    auto take = [&](size_t n) {
      need(p, n);
      size_t old = p;
      p += n;
      return old;
    };
    if (cmd == 0x54) {
      auto q = take(2);
      events.push_back({uint32_t(samples), b[q], b[q + 1]});
    } else if (cmd == 0xb2 && pwm) {
      auto q = take(2);
      unsigned reg = b[q] >> 4, value = ((b[q] & 15) << 8) | b[q + 1];
      // Direct signed DAC profile written by the MDX exporter. Other PWM
      // modes and streamed DAC commands need a full PWM chip implementation.
      if (reg < 1 || reg > 4 || ((reg == 1 || reg == 4) && value != 0))
        fail(q, "unsupported PWM mode (direct export profile required)");
      events.push_back(
          {uint32_t(samples), uint16_t(0x100 + reg), uint16_t(value)});
    } else if (cmd == 0x61) {
      auto q = take(2);
      wait(b[q] | unsigned(b[q + 1]) << 8);
    } else if (cmd == 0x62)
      wait(wait60);
    else if (cmd == 0x63)
      wait(wait50);
    else if (cmd >= 0x70 && cmd <= 0x7f)
      wait((cmd & 15) + 1);
    else if (cmd == 0x64) {
      auto q = take(3);
      unsigned n = b[q + 1] | unsigned(b[q + 2]) << 8;
      if (b[q] == 0x62)
        wait60 = n;
      else if (b[q] == 0x63)
        wait50 = n;
      else
        fail(q, "wait override target must be 0x62 or 0x63");
    } else if (cmd == 0x67) {
      auto q = take(6);
      if (b[q] != 0x66)
        fail(q, "invalid data-block marker");
      uint32_t n = u32(q + 2);
      take(n);
    } else if (cmd == 0x66) {
      ended = true;
      break;
    } else {
      std::ostringstream s;
      s << "unsupported command/chip 0x" << std::hex << cmd
        << " (YM2151 command: 0x54)";
      fail(at, s.str());
    }
  }
  if (!ended)
    fail(p, "missing end command 0x66");
  if (loop && (!sawLoop || loopSample >= samples))
    fail(0x1c, "loop must target a command boundary with positive duration");
  looping = loop != 0;
  if (events.empty())
    fail(start, "no YM2151 register writes");
  if (u32(0x14)) {
    size_t gd = size_t(u32(0x14)) + 0x14;
    need(gd, 12);
    if (u32(gd) != 0x20336447 || u32(gd + 4) != 0x100)
      fail(gd, "invalid GD3 tag");
    size_t n = u32(gd + 8);
    need(gd + 12, n);
    if (n & 1)
      fail(gd + 8, "odd GD3 length");
    size_t q = gd + 12, stop = q + n;
    auto text = [&]() {
      std::string t;
      bool term = false;
      while (q < stop) {
        uint32_t c = b[q] | uint32_t(b[q + 1]) << 8;
        q += 2;
        if (!c) {
          term = true;
          break;
        }
        if (c >= 0xd800 && c <= 0xdbff) {
          if (q + 2 > stop)
            fail(q, "truncated GD3 surrogate");
          uint32_t d = b[q] | uint32_t(b[q + 1]) << 8;
          q += 2;
          if (d < 0xdc00 || d > 0xdfff)
            fail(q - 2, "invalid GD3 surrogate");
          c = 0x10000 + ((c - 0xd800) << 10) + d - 0xdc00;
        } else if (c >= 0xdc00 && c <= 0xdfff)
          fail(q - 2, "invalid GD3 surrogate");
        if (c < 128)
          t += char(c);
        else if (c < 2048) {
          t += char(0xc0 | (c >> 6));
          t += char(0x80 | (c & 63));
        } else if (c < 65536) {
          t += char(0xe0 | (c >> 12));
          t += char(0x80 | ((c >> 6) & 63));
          t += char(0x80 | (c & 63));
        } else {
          t += char(0xf0 | (c >> 18));
          t += char(0x80 | ((c >> 12) & 63));
          t += char(0x80 | ((c >> 6) & 63));
          t += char(0x80 | (c & 63));
        }
      }
      if (!term)
        fail(q, "unterminated GD3 string");
      return t;
    };
    title = text();
    auto ja = text();
    if (title.empty())
      title = ja;
  }
}
namespace {
bool mrzPath(const std::filesystem::path &path) {
  auto e = path.extension().u8string();
  for (auto &c : e)
    if (c >= 'A' && c <= 'Z')
      c += 32;
  return e == ".mrz";
}
} // namespace
VgmSong loadVgmSong(const std::filesystem::path &path,
                    const std::filesystem::path &pdxOverride) {
  auto file = mrzPath(path) ? openMrz(path) : resolveArchivePath(path);
  std::ifstream in(file, std::ios::binary);
  char magic[8]{};
  in.read(magic, 8);
  in.clear();
  in.seekg(0);
  if (!std::equal(magic, magic + 8, "MDXRLOG\0"))
    return VgmSong(readFile(file));
  VgmSong s;
  s.mrl = std::make_shared<MrlHeader>(
      readMrlHeader(in, std::filesystem::file_size(file)));
  auto &h = *s.mrl;
  h.file = file;
  s.registerLog = true;
  s.clock = 4000000;
  s.samples = uint64_t(std::ceil(double(h.end) * 44100 / h.hz));
  s.setLogLoop(h);
  s.title = h.title.empty() ? "MDX register log" : h.title;
  try {
    if (h.embedded) {
      in.seekg(std::streamoff(h.bankOffset));
      Bytes bank(size_t(h.pdxSize));
      if (!in.read(reinterpret_cast<char *>(bank.data()), bank.size()))
        throw std::runtime_error("Truncated embedded PDX");
      if (dataCrc(bank.data(), bank.size()) != h.pdxCrc)
        throw std::runtime_error("MRL PDX CRC mismatch");
      s.pdx = std::make_shared<Pdx>(std::move(bank));
    } else if (!h.pdxName.empty()) {
      s.pdx = locatePdxByName(h.pdxName, path, pdxOverride);
      if (!s.pdx)
        throw std::runtime_error("MRL PDX not found; FM playback continues");
      if (h.pdxSize &&
          (s.pdx->bytes.size() != h.pdxSize ||
           dataCrc(s.pdx->bytes.data(), s.pdx->bytes.size()) != h.pdxCrc)) {
        s.pdx.reset();
        throw std::runtime_error(
            "MRL external PDX size/CRC mismatch; FM playback continues");
      }
    }
  } catch (const std::exception &e) {
    s.pdx.reset();
    s.pcmWarning = e.what();
  }
  return s;
}
std::string readVgmTitle(const std::filesystem::path &path) {
  auto file = mrzPath(path) ? openMrz(path, false) : path;
  auto prefix = archivePrefix(file, 128);
  if (prefix.size() >= 64 &&
      std::equal(prefix.begin(), prefix.begin() + 8, "MDXRLOG\0")) {
    if (mrlNumber(prefix.data() + 10, 2) == 0)
      return {};
    if (prefix.size() < 128)
      throw std::runtime_error("Truncated MRL header");
    auto titleSize = mrlNumber(prefix.data() + 64, 4),
         nameSize = mrlNumber(prefix.data() + 68, 4);
    if (titleSize > 1048576 || nameSize > 65536)
      throw std::runtime_error("MRL metadata too large");
    prefix = archivePrefix(file, size_t(128 + titleSize + nameSize));
    std::string bytes(prefix.begin(), prefix.end());
    std::istringstream in(bytes, std::ios::binary);
    auto physical = resolveArchivePath(file.parent_path()) / file.filename();
    return readMrlHeader(in, archiveFileSize(physical)).title;
  }
  return VgmSong(readFile(file)).title;
}
struct VgmPlayer::Impl {
  VgmSong song;
  Options o;
  std::unique_ptr<OpmBackend> chip;
  Clock64Output resampler;
  std::array<uint8_t, 256> regs{};
  std::array<uint8_t, 8> keys{};
  uint8_t amd = 0, pmd = 0;
  int pwmLeft = 0, pwmRight = 0;
  MonitorFrame monitor;
  size_t event = 0;
  double time = 0;
  uint64_t native = 0, output = 0, loops = 0, quiet = 0;
  int limit;
  uint32_t mask = 0;
  int keyShift = 0;
  double fadeAt = -1, tailAt = -1;
  bool done = false, seeking = false;
  std::ifstream logStream;
  uint64_t cachedEvent = UINT64_MAX, previousClock = 0, logBlock = UINT64_MAX;
  std::array<uint8_t, 65536> logBuffer{};
  size_t logBytes = 0;
  unsigned logStage = 0;
  VgmSong::Event logEvent{};
  double logTime = 0;
  struct PcmChannel {
    std::vector<int16_t> data;
    double position = 0, gain = 1;
    int pan = 3, format = 4, note = -1;
    PcmPitchShift shift;
  };
  std::array<PcmChannel, 8> pcmChannels;
  size_t eventCount() const {
    return song.mrl ? size_t(song.mrl->count) : song.events.size();
  }
  void readEvent() {
    if (!song.mrl || cachedEvent == event)
      return;
    auto &h = *song.mrl;
    uint64_t block = event / 4096;
    if (block != logBlock) {
      logBlock = block;
      logStream.clear();
      logStream.seekg(std::streamoff(h.offset + block * 65536));
      logBytes = size_t(std::min<uint64_t>(4096, h.count - block * 4096)) * 16;
      if (!logStream.read(reinterpret_cast<char *>(logBuffer.data()), logBytes))
        throw std::runtime_error("Truncated MRL event stream");
    }
    auto r = readMrlRecord(logBuffer.data() + (event % 4096) * 16, h);
    unsigned rank = r.phase == 1 ? 0 : r.phase == 0 ? 1 : 2;
    if (event && cachedEvent != UINT64_MAX && event == cachedEvent + 1 &&
        (r.time < previousClock || rank < logStage))
      throw std::runtime_error("Invalid MRL event order");
    cachedEvent = event;
    previousClock = r.time;
    logStage = rank;
    logEvent = {r.argument,
                uint16_t(r.chip ? 0x200 + (r.chip - 1) * 8 + r.op : r.op),
                r.value};
    logTime = double(r.time) * 44100 / h.hz;
  }
  double eventTime() {
    readEvent();
    return song.mrl                    ? logTime
           : song.preciseTimes.empty() ? double(song.events[event].time)
                                       : song.preciseTimes[event];
  }
  VgmSong::Event getEvent() {
    readEvent();
    return song.mrl ? logEvent : song.events[event];
  }
  void pcmEvent(const VgmSong::Event &e) {
    auto &p = pcmChannels[(e.address - 0x200) / 8];
    unsigned op = e.address & 7;
    if (op == 0) {
      p.position = 0;
      p.shift.reset();
      p.data.clear();
      p.note = int(e.time % 96);
      if (song.pdx) {
        try {
          p.data = song.pdx->decode(e.time, e.value);
        } catch (const std::exception &error) {
          if (o.diagnostic)
            o.diagnostic(error.what());
        }
      }
    } else if (op == 1) {
      p.data.clear();
      p.note = -1;
    } else if (op == 2)
      p.pan = int(e.time);
    else if (op == 3)
      p.gain = e.time / 65536.0;
    else if (op == 4)
      p.format = int(e.time);
  }
  explicit Impl(VgmSong s, Options opt)
      : song(std::move(s)), o(opt), limit(o.loops) {
    if (o.sampleRate < 8000 || o.sampleRate > 192000 || o.loops < 0 ||
        o.loops > 1000 || !std::isfinite(o.gain) || o.gain < 0 || o.gain > 4 ||
        !std::isfinite(o.independentGain) || o.independentGain < 0 ||
        o.independentGain > 8 || !std::isfinite(o.fadeSeconds) ||
        o.fadeSeconds < 0 || o.fadeSeconds > 60 ||
        !std::isfinite(o.maxSeconds) || o.maxSeconds < 0)
      throw std::runtime_error("Invalid VGM playback options");
    if ((o.externalFm || o.fmBackend == 1) && song.clock != 4000000)
      throw std::runtime_error("VGM: C86CTL/X68Sound output requires a 4000000 "
                               "Hz YM2151 clock; select Independent OPM DLL");
    if (song.mrl) {
      logStream.open(song.mrl->file, std::ios::binary);
      if (!logStream)
        throw std::runtime_error("Cannot open MRL event stream");
    }
    if (!song.pcmWarning.empty() && o.diagnostic)
      o.diagnostic(song.pcmWarning);
    chip = std::make_unique<OpmBackend>(true, !o.externalFm, o.fmBackend,
                                        song.clock);
    chip->set_measured_output_timing(o.measuredOutputTiming);
  }
  void send(int a, int v) {
    chip->write_register(uint8_t(a), uint8_t(v));
    if (!seeking && o.registerOutput)
      o.registerOutput(native, a, v);
  }
  void pitch(unsigned ch) {
    constexpr int notes[] = {0, 1, 2, 2, 3, 4, 5, 5, 6, 7, 8, 8, 9, 10, 11, 11};
    constexpr int kc[] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14};
    int old = regs[0x28 + ch];
    int n = std::clamp((old >> 4) * 12 + notes[old & 15] + keyShift, 0, 95);
    send(0x28 + ch, (n / 12) * 16 + kc[n % 12]);
    send(0x30 + ch, regs[0x30 + ch]);
  }
  double fade() const {
    return fadeAt < 0 ? 1
           : o.fadeSeconds <= 0
               ? 0
               : std::max(0.0, 1 - (double(native) / 62500 - fadeAt) /
                                       o.fadeSeconds);
  }
  void write(unsigned a, unsigned v) {
    if (a >= 0x100) {
      int value = (v & 2048) ? int(v) - 4096 : int(v);
      if (a == 0x102)
        pwmLeft = value;
      if (a == 0x103)
        pwmRight = value;
      if (a == 0x104)
        pwmLeft = pwmRight = value;
      return;
    }
    regs[a] = uint8_t(v);
    if (a == 8)
      keys[v & 7] = uint8_t(v & 0x78);
    if (a == 0x19) {
      if (v & 128)
        pmd = uint8_t(v);
      else
        amd = uint8_t(v);
    }
    unsigned ch = a & 7;
    if (a >= 0x28 && a <= 0x37) {
      if (!(mask & (1u << ch)))
        pitch(ch);
      return;
    }
    if (a >= 0x20 && a <= 0x27 && (mask & (1u << ch)))
      v &= 63;
    if (a == 8 && (mask & (1u << (v & 7))))
      v &= 7;
    if (a >= 0x60 && a <= 0x7f && o.externalFm) {
      constexpr unsigned carriers[] = {8, 8, 8, 8, 12, 14, 14, 15};
      if (carriers[regs[0x20 + ch] & 7] & (1u << ((a - 0x60) / 8)))
        v = unsigned(std::clamp(
            int(v) +
                int(std::clamp(
                    -20 * std::log10(std::max(fade() * song.volume, 1e-8)) /
                        .75,
                    -127.0, 127.0)),
            0, 127));
    }
    send(a, v);
  }
  void sync() {
    send(0x19, amd);
    send(0x19, pmd | 128);
    for (unsigned a = 0; a < 256; ++a)
      if (a != 8 && a != 0x19)
        write(a, regs[a]);
    for (unsigned ch = 0; ch < 8; ++ch)
      write(8, ch | keys[ch]);
  }
  bool next(AudioStem &stem) {
    if (done)
      return false;
    if (o.liveLoops && fadeAt < 0)
      limit = std::clamp(o.liveLoops->load(), 0, 1000);
    if (fadeAt < 0 && limit > 0 && loops >= uint64_t(limit) && song.looping)
      fadeAt = double(native) / 62500;
    int shift = o.liveKeyShift ? o.liveKeyShift->load() : o.keyShift;
    shift = std::clamp(shift, -24, 24);
    uint32_t nextMask = o.liveMuteMask ? o.liveMuteMask->load() : o.fmMuteMask;
    if (shift != keyShift || nextMask != mask) {
      keyShift = shift;
      mask = nextMask;
      for (unsigned ch = 0; ch < 8; ++ch) {
        write(0x20 + ch, regs[0x20 + ch]);
        pitch(ch);
        send(8, ch | ((mask & (1u << ch)) ? 0 : keys[ch]));
      }
    }
    const double speed =
        seeking ? 1
                : (o.liveSpeed ? std::clamp(o.liveSpeed->load(), 0, 200) / 100.0
                               : 1);
    const double step = 44100.0 / 62500 * speed;
    const uint32_t span = chip->begin_timed_frame();
    if (tailAt < 0) {
      while (event < eventCount() && (!song.logLoopEndEvent || event < song.logLoopEndEvent) && eventTime() <= time + step + 1e-8) {
        const double eventTime = this->eventTime();
        auto e = getEvent();
        ++event;
        // VGM timestamps are in 44.1 kHz units; retain sub-frame master clocks
        // instead of delaying every write to the next 64-clock boundary.
        auto offset = step > 0 ? std::llround((eventTime - time) * song.clock /
                                              (44100.0 * speed))
                               : 0;
        chip->set_event_offset(
            uint32_t(std::clamp<long long>(offset, 0, span)));
        if (e.address >= 0x200)
          pcmEvent(e);
        else
          write(e.address, e.value);
      }
      chip->set_event_offset(span);
      const double boundary = song.logLoopEnd ? song.logLoopEnd : double(song.samples);
      if (time >= boundary) {
        ++loops;
        if (song.looping) {
          if (fadeAt < 0 && limit > 0 && loops >= uint64_t(limit))
            fadeAt = double(native) / 62500;
          time = song.loopSample + (time - boundary);
          event = song.loopEvent;
        } else {
          tailAt = double(native) / 62500;
          for (unsigned ch = 0; ch < 8; ++ch)
            write(8, ch);
        }
      }
    }
    chip->advance(64);
    double gain = o.liveGain ? o.liveGain->load() : o.gain,
           fm = o.liveIndependentGain ? o.liveIndependentGain->load()
                                      : o.independentGain;
    auto mix = chip->last_sample();
    double f = fade();
    gain *= song.volume;
    stem.fm = {mix.left * 32767 * gain * fm * f,
               mix.right * 32767 * gain * fm * f};
    double pcm = o.livePcmGain ? o.livePcmGain->load() : o.pcmGain;
    uint32_t pcmMask =
        o.livePcmMuteMask ? o.livePcmMuteMask->load() : o.pcmMuteMask;
    if (pcmMask & 1)
      pcm = 0;
    stem.pcm = {pwmLeft * 16 * gain * pcm * f, pwmRight * 16 * gain * pcm * f};
    bool silent = pwmLeft == 0 && pwmRight == 0;
    if (song.registerLog) {
      constexpr double rates[] = {
          3906.25, 4000000.0 / 768, 7812.5, 8000000.0 / 768,
          15625,   15625,           15625};
      int mode = o.liveKeyMode ? o.liveKeyMode->load() : o.keyMode;
      double pcmGain = o.livePcmGain ? o.livePcmGain->load() : o.pcmGain;
      for (unsigned ch = 0; ch < 8; ++ch) {
        auto &p = pcmChannels[ch];
        double value = 0;
        bool active = p.position < p.data.size();
        if (active) {
          value = p.shift.read(p.data, p.position, keyShift, mode) * p.gain;
          p.position += rates[p.format] *
                        (mode == 1 ? std::exp2(keyShift / 12.0) : 1) / 62500;
        }
        if (pcmMask & (1u << ch))
          value = 0;
        silent &= !active || std::abs(value) < 1;
        if (p.pan & 1)
          stem.pcm.left += value * gain * pcmGain * f;
        if (p.pan & 2)
          stem.pcm.right += value * gain * pcmGain * f;
        if (native % 8 == 0) {
          monitor.note[8 + ch] =
              active && !(pcmMask & (1u << ch)) ? p.note : -1;
          monitor.waveform[8 + ch][monitor.cursor] =
              o.monitor && o.monitor->waveforms
                  ? float(value * gain * pcmGain * f / 32768)
                  : 0;
        }
      }
    }
    if (native % 8 == 0 && !song.registerLog)
      monitor.waveform[8][monitor.cursor] =
          o.monitor && o.monitor->waveforms
              ? float((stem.pcm.left + stem.pcm.right) / 65536)
              : 0;
    constexpr int notes[] = {0, 1, 2, 2, 3, 4, 5, 5, 6, 7, 8, 8, 9, 10, 11, 11};
    for (unsigned ch = 0; ch < 8; ++ch) {
      auto w = chip->channel_sample(ch);
      silent &=
          std::abs(w.left) < 1.0 / 32768 && std::abs(w.right) < 1.0 / 32768;
      if (native % 8 == 0) {
        monitor.waveform[ch][monitor.cursor] =
            o.monitor && o.monitor->waveforms
                ? float((w.left + w.right) * .5 * gain * fm * f)
                : 0;
        int kc = regs[0x28 + ch];
        monitor.note[ch] =
            keys[ch] && !(mask & (1u << ch))
                ? std::clamp((kc >> 4) * 12 + notes[kc & 15] + keyShift, 0, 95)
                : -1;
      }
    }
    if (native % 8 == 0)
      monitor.cursor = (monitor.cursor + 1) % MonitorFrame::points;
    if (o.externalFm && fadeAt >= 0 && native % 64 == 0)
      for (unsigned a = 0x60; a <= 0x7f; ++a)
        write(a, regs[a]);
    if (tailAt >= 0) {
      quiet = silent ? quiet + 1 : 0;
      if (limit == 0 && fadeAt < 0 &&
          quiet >= 6250 + uint64_t(std::max(o.fmDelayMs, o.pcmDelayMs)) *
                              62500 / 1000) {
        time = 0;
        event = 0;
        tailAt = -1;
        quiet = 0;
        chip = std::make_unique<OpmBackend>(true, !o.externalFm, o.fmBackend,
                                            song.clock);
        regs.fill(0);
        keys.fill(0);
        amd = pmd = 0;
        if (o.externalFm)
          sync();
      } else if (limit != 0 && double(native) / 62500 - tailAt >= .5)
        done = true;
    }
    if (f <= 0)
      done = true;
    if (o.maxSeconds > 0) {
      double now = double(native) / 62500;
      if (fadeAt < 0 && now >= std::max(0.0, o.maxSeconds - o.fadeSeconds))
        fadeAt = now;
      if (now >= o.maxSeconds)
        done = true;
    }
    if (tailAt < 0)
      time += 44100.0 / 62500 * speed;
    ++native;
    return true;
  }
  void publish() {
    if (o.monitor) {
      std::lock_guard<std::mutex> lock(o.monitor->mutex);
      monitor.seconds = double(output) / o.sampleRate;
      o.monitor->frame = monitor;
    }
    if (o.voiceState) {
      std::lock_guard<std::mutex> lock(o.voiceState->mutex);
      auto &v = *o.voiceState;
      v.registers = regs;
      for (unsigned ch = 0; ch < 8; ++ch) {
        v.valid[ch] = true;
        v.ids[ch] = -1;
        auto &voice = v.voices[ch];
        voice[0] = uint8_t(ch);
        voice[1] = regs[0x20 + ch] & 63;
        voice[2] = keys[ch] >> 3;
        for (unsigned group = 0; group < 6; ++group)
          for (unsigned op = 0; op < 4; ++op)
            voice[3 + group * 4 + op] = regs[0x40 + group * 32 + op * 8 + ch];
      }
    }
  }
};
VgmPlayer::VgmPlayer(VgmSong s, Options o)
    : p(std::make_unique<Impl>(std::move(s), o)) {}
VgmPlayer::~VgmPlayer() = default;
size_t VgmPlayer::render(int16_t *out, size_t count) {
  p->chip->set_measured_output_timing(
      p->o.liveMeasuredOutputTiming ? p->o.liveMeasuredOutputTiming->load()
                                    : p->o.measuredOutputTiming);
  if (p->o.monitor && !p->o.monitor->waveforms.load())
    for (auto &w : p->monitor.waveform)
      w.fill(0);
  bool dac = p->o.liveDacRightDelay ? p->o.liveDacRightDelay->load()
                                    : p->o.dacRightDelay;
  size_t n = p->resampler.render(out, count, p->o.sampleRate, dac,
                                 [&](AudioStem &s) { return p->next(s); });
  p->output += n;
  if (!p->seeking)
    p->publish();
  return n;
}
double VgmPlayer::seconds() const {
  return double(p->output) / p->o.sampleRate;
}
double VgmPlayer::positionSeconds() const {
  return std::min(double(p->song.samples), p->time) / 44100;
}
double VgmPlayer::duration() const { return double(p->song.samples) / 44100; }
bool VgmPlayer::seek(double seconds, const std::function<bool()> &cancelled) {
  if (!std::isfinite(seconds) || seconds < 0 || seconds > duration())
    throw std::runtime_error("VGM seek outside song duration");
  auto fresh = std::make_unique<Impl>(p->song, p->o);
  fresh->seeking = true;
  fresh->limit = 0;
  fresh->o.liveLoops.reset();
  fresh->o.maxSeconds = 0;
  auto opts = p->o;
  p = std::move(fresh);
  uint64_t target = uint64_t(seconds * 62500);
  while (p->native < target && !p->done) {
    if (p->native % 2048 == 0 && cancelled && cancelled()) {
      p->seeking = false;
      p->o = opts;
      if (opts.seekProgress) opts.seekProgress(seconds, seconds);
      return false;
    }
    if (p->native % 2048 == 0 && opts.seekProgress)
      opts.seekProgress(double(p->native) / 62500, seconds);
    AudioStem s;
    p->next(s);
  }
  p->seeking = false;
  p->o = opts;
  p->limit = opts.liveLoops ? opts.liveLoops->load() : opts.loops;
  p->output = uint64_t(seconds * opts.sampleRate);
  p->publish();
  if (opts.seekProgress) opts.seekProgress(seconds, seconds);
  return true;
}
void VgmPlayer::syncRegisters() { p->sync(); }
} // namespace mdx
