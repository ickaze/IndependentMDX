// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

#pragma once
#include "mrl.h"
#include "player.h"
#include <algorithm>
#include <array>
#include <codecvt>
#include <csignal>
#include <deque>
#include <fstream>
#include <iostream>
#include <locale>
#include <optional>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace mdx {
// This diagnostic describes fidelity, not invalid input. Keep all other
// diagnostics fatal during export so partial/error logs are not published.
inline bool exportLfoWarning(const std::string &message) {
  return message == "Software LFO uses an independent approximation; not "
                    "verified against MXDRV integer behavior.";
}
inline volatile std::sig_atomic_t vgzCancelled = 0;
// Streaming gzip with fixed Huffman DEFLATE blocks and bounded LZ77 history.
// No external compression DLL or subprocess is used.
inline void gzipVgm(std::istream &input, std::ostream &output) {
  const unsigned char header[] = {31, 139, 8, 0, 0, 0, 0, 0, 0, 255};
  output.write(reinterpret_cast<const char *>(header), 10);
  uint64_t bits = 0;
  unsigned used = 0;
  uint32_t crc = ~0u, total = 0;
  static const auto crcTable = [] {
    std::array<uint32_t, 256> table{};
    for (unsigned i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int bit = 0; bit < 8; ++bit)
        c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
      table[i] = c;
    }
    return table;
  }();
  auto put = [&](unsigned value, unsigned count) {
    bits |= uint64_t(value) << used;
    used += count;
    while (used >= 8) {
      output.put(char(bits));
      bits >>= 8;
      used -= 8;
    }
  };
  auto reverse = [](unsigned value, unsigned count) {
    unsigned r = 0;
    while (count--) {
      r = (r << 1) | (value & 1);
      value >>= 1;
    }
    return r;
  };
  auto symbol = [&](unsigned s) {
    unsigned code, n;
    if (s < 144) {
      code = s + 48;
      n = 8;
    } else if (s < 256) {
      code = s - 144 + 400;
      n = 9;
    } else if (s < 280) {
      code = s - 256;
      n = 7;
    } else {
      code = s - 280 + 192;
      n = 8;
    }
    put(reverse(code, n), n);
  };
  constexpr unsigned lb[] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                             15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                             67, 83, 99, 115, 131, 163, 195, 227, 258};
  constexpr unsigned le[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                             2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
  constexpr unsigned db[] = {1,    2,    3,    4,     5,     7,    9,    13,
                             17,   25,   33,   49,    65,    97,   129,  193,
                             257,  385,  513,  769,   1025,  1537, 2049, 3073,
                             4097, 6145, 8193, 12289, 16385, 24577};
  constexpr unsigned de[] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                             4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                             9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
  std::array<unsigned char, 65536> data{};
  do {
    if (vgzCancelled)
      throw std::runtime_error("Cancelled");
    input.read(reinterpret_cast<char *>(data.data()), data.size());
    size_t count = size_t(input.gcount());
    if (input.bad())
      throw std::runtime_error("Cannot read temporary VGM");
    bool last = input.peek() == std::char_traits<char>::eof();
    put(last ? 3 : 2, 3); // BFINAL, BTYPE=fixed Huffman.
    for (size_t i = 0; i < count; ++i) {
      crc = (crc >> 8) ^ crcTable[(crc ^ data[i]) & 255];
    }
    total += uint32_t(count);
    std::array<int, 65536> table;
    table.fill(-1);
    for (size_t i = 0; i < count;) {
      size_t length = 0, distance = 0;
      if (i + 2 < count) {
        unsigned hash =
            ((unsigned(data[i]) * 251 + data[i + 1]) * 251 + data[i + 2]) &
            65535;
        int previous = table[hash];
        table[hash] = int(i);
        if (previous >= 0 && i - size_t(previous) <= 32768) {
          size_t maximum = std::min<size_t>(258, count - i);
          while (length < maximum &&
                 data[size_t(previous) + length] == data[i + length])
            ++length;
          distance = i - size_t(previous);
        }
      }
      if (length >= 3) {
        unsigned l = 0, d = 0;
        while (l < 28 && lb[l + 1] <= length)
          ++l;
        while (d < 29 && db[d + 1] <= distance)
          ++d;
        symbol(257 + l);
        put(unsigned(length - lb[l]), le[l]);
        put(reverse(d, 5), 5);
        put(unsigned(distance - db[d]), de[d]);
        i += length;
      } else
        symbol(data[i++]);
    }
    symbol(256);
    if (last)
      break;
  } while (true);
  if (used)
    output.put(char(bits));
  for (uint32_t value : {~crc, total})
    for (int i = 0; i < 4; ++i)
      output.put(char(value >> (8 * i)));
  if (!output)
    throw std::runtime_error("Cannot write VGZ (disk full or I/O error)");
}
struct VgzBatchOptions {
  int loops = 2;
  unsigned seconds = 3600;
  bool pcm = true;
  bool embedPdx = true, mrz = false, legacyMrl = false;
};
inline void cancelVgz(int) { vgzCancelled = 1; }
#ifdef _WIN32
inline BOOL WINAPI cancelVgzConsole(DWORD event) {
  if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
    vgzCancelled = 1;
    return TRUE;
  }
  return FALSE;
}
#endif
inline std::string csvField(const std::string &s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '\"')
      out += '\"';
    out += c;
  }
  return out + '\"';
}
inline void exportVgz(const std::filesystem::path &input,
                      const std::filesystem::path &target,
                      const VgzBatchOptions &settings,
                      const std::function<void(const std::string &)> &report) {
  if (std::filesystem::exists(target))
    throw std::runtime_error("Output already exists (not overwritten)");
  Song song(readFile(input));
  const std::string title = song.title;
  std::shared_ptr<Pdx> pdx;
  try {
    if (settings.pcm)
      pdx = locatePdx(song, input, {});
  } catch (const std::exception &e) {
    if (std::string(e.what()).find("Required PDX not found:") != 0)
      throw;
    report(std::string("WARNING: ") + e.what());
  }
  auto parent = target.parent_path();
  std::filesystem::create_directories(parent);
  // An exclusively created directory owns both temporary files.
  std::filesystem::path temporary;
  for (unsigned i = 0; i < 10000; ++i) {
    temporary = target;
    temporary += std::filesystem::u8path(".work-" + std::to_string(i));
    if (std::filesystem::create_directory(temporary))
      break;
    temporary.clear();
  }
  if (temporary.empty())
    throw std::runtime_error("Cannot allocate temporary export directory");
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(path, e);
    }
  } cleanup{temporary};
  auto rawPath = temporary / "audio.vgm", gzipPath = temporary / "audio.vgz";
  std::fstream raw(rawPath, std::ios::binary | std::ios::in | std::ios::out |
                                std::ios::trunc);
  if (!raw)
    throw std::runtime_error("Cannot create temporary VGM");
  std::array<char, 128> header{};
  raw.write(header.data(), header.size());
  uint64_t timestamp = 0;
  auto at = [&](uint64_t sample) {
    if (sample < timestamp)
      throw std::runtime_error("Non-monotonic export timestamp");
    while (sample > timestamp) {
      unsigned n = unsigned(std::min<uint64_t>(sample - timestamp, 65535));
      raw.put(char(0x61));
      raw.put(char(n));
      raw.put(char(n >> 8));
      timestamp += n;
    }
  };
  auto fm = [&](int a, int v) {
    raw.put(char(0x54));
    raw.put(char(a));
    raw.put(char(v));
  };
  auto pwm = [&](int a, int v) {
    raw.put(char(0xb2));
    raw.put(char((a << 4) | ((v >> 8) & 15)));
    raw.put(char(v));
  };
  struct Event {
    uint64_t time;
    int address, value;
  };
  std::deque<Event> pending;
  Options o;
  o.loops = settings.loops;
  o.fadeSeconds = 0;
  o.maxSeconds = 0;
  o.sampleRate = 44100;
  o.gain = 1;
  o.independentGain = 0; // Render only PCM; FM stays as register events.
  o.skipFmSynthesis = true;
  o.skipPcmSynthesis = !settings.pcm;
  o.registerOutput = [&](uint64_t tick, int a, int v) {
    pending.push_back({(tick * 44100 + 31250) / 62500, a, v});
  };
  o.diagnostic = [&](const std::string &message) {
    if (!pdx && message.find("without a PDX") != std::string::npos)
      return;
    if (!exportLfoWarning(message))
      throw std::runtime_error(message);
  };
  Player player(std::move(song), pdx, o);
  std::array<int16_t, 2048> audio{};
  uint64_t frames = 0;
  bool hasPcm = false;
  int oldLeft = 0, oldRight = 0;
  for (;;) {
    if (vgzCancelled)
      throw std::runtime_error("Cancelled");
    size_t n = player.render(audio.data(), audio.size() / 2);
    if (!n)
      break;
    if (frames + n > uint64_t(settings.seconds) * 44100)
      throw std::runtime_error(
          "Export time limit exceeded; no partial VGZ published");
    if (!settings.pcm) {
      while (!pending.empty()) {
        auto e = pending.front();
        pending.pop_front();
        at(e.time);
        fm(e.address, e.value);
      }
    }
    for (size_t i = 0; settings.pcm && i < n; ++i) {
      uint64_t t = frames + i;
      while (!pending.empty() && pending.front().time <= t) {
        auto e = pending.front();
        pending.pop_front();
        at(e.time);
        fm(e.address, e.value);
      }
      // Signed 12-bit stereo DAC values carried by standard PWM writes.
      int left = std::clamp(int(std::lround(audio[2 * i] / 16.0)), -2048, 2047);
      int right =
          std::clamp(int(std::lround(audio[2 * i + 1] / 16.0)), -2048, 2047);
      if (left != oldLeft || right != oldRight) {
        at(t);
        if (!hasPcm) {
          pwm(1, 0);
          pwm(4, 0);
          hasPcm = true;
        }
        if (left != oldLeft)
          pwm(2, left & 4095);
        if (right != oldRight)
          pwm(3, right & 4095);
        oldLeft = left;
        oldRight = right;
      }
    }
    frames += n;
    if (!raw || uint64_t(raw.tellp()) > 0xffffff00ull)
      throw std::runtime_error("VGM exceeds 32-bit file size or write failed");
  }
  while (!pending.empty()) {
    auto e = pending.front();
    pending.pop_front();
    at(e.time);
    fm(e.address, e.value);
  }
  at(std::max(frames, timestamp));
  for (int ch = 0; ch < 8; ++ch) {
    fm(8, ch);
    fm(0x20 + ch, 0);
  }
  if (hasPcm)
    pwm(4, 0);
  raw.put(char(0x66));
  const uint64_t gd3Offset = uint64_t(raw.tellp());
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  std::u16string tags;
  const std::array<std::string, 11> fields = {title,
                                              title,
                                              "",
                                              "",
                                              "",
                                              "",
                                              "",
                                              "",
                                              "",
                                              "Independent MDX",
                                              "Exported from Independent MDX"};
  for (const auto &field : fields) {
    tags += converter.from_bytes(field);
    tags.push_back(0);
  }
  raw.write("Gd3 ", 4);
  auto le32 = [&](uint32_t value) {
    for (int i = 0; i < 4; ++i)
      raw.put(char(value >> (8 * i)));
  };
  le32(0x100);
  le32(uint32_t(tags.size() * 2));
  for (char16_t value : tags) {
    raw.put(char(value));
    raw.put(char(value >> 8));
  }
  uint64_t size = uint64_t(raw.tellp());
  if (size > 0xffffffffull || timestamp > 0xffffffffull)
    throw std::runtime_error("VGM size/duration overflow");
  auto word = [&](int p, uint32_t v) {
    for (int i = 0; i < 4; ++i)
      header[p + i] = char(v >> (i * 8));
  };
  header[0] = 'V';
  header[1] = 'g';
  header[2] = 'm';
  header[3] = ' ';
  word(4, uint32_t(size - 4));
  word(8, 0x171);
  word(0x14, uint32_t(gd3Offset - 0x14));
  word(0x18, uint32_t(timestamp));
  word(0x30, 4000000);
  word(0x34, 128 - 0x34);
  if (hasPcm)
    word(0x70, 23011361);
  raw.seekp(0);
  raw.write(header.data(), header.size());
  raw.flush();
  if (!raw)
    throw std::runtime_error("Cannot finalize VGM");
  raw.close();
  std::ifstream source(rawPath, std::ios::binary);
  std::ofstream compressed(gzipPath, std::ios::binary);
  gzipVgm(source, compressed);
  compressed.close();
  if (!compressed)
    throw std::runtime_error("Cannot finalize VGZ");
  if (vgzCancelled)
    throw std::runtime_error("Cancelled");
#ifdef _WIN32
  if (!MoveFileW(gzipPath.c_str(), target.c_str()))
    throw std::runtime_error("Cannot publish VGZ (target exists or I/O error)");
#else
  std::filesystem::create_hard_link(gzipPath,
                                    target); // Atomic no-replace publication.
#endif
  if (hasPcm)
    report("PCM: stereo PWM, 44.1 kHz / 12-bit quantization");
}
// MRZ is a single-member ZIP64 container with raw DEFLATE from the gzip writer.
inline void zipMrl(const std::filesystem::path &source,
                   const std::filesystem::path &target) {
  auto gzip = target;
  gzip += ".gz";
  {
    std::ifstream in(source, std::ios::binary);
    std::ofstream out(gzip, std::ios::binary);
    gzipVgm(in, out);
  }
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove(p, e);
    }
  } cleanup{gzip};
  uint64_t size = std::filesystem::file_size(source),
           packed = std::filesystem::file_size(gzip) - 18;
  std::ifstream in(gzip, std::ios::binary);
  in.seekg(-8, std::ios::end);
  std::array<uint8_t, 8> tail{};
  in.read(reinterpret_cast<char *>(tail.data()), 8);
  uint32_t crc = uint32_t(mrlNumber(tail.data(), 4));
  std::ofstream out(target, std::ios::binary);
  auto put = [&](uint64_t v, unsigned n) {
    while (n--) {
      out.put(char(v));
      v >>= 8;
    }
  };
  std::string name = "record.mrl";
  put(0x04034b50, 4);
  put(45, 2);
  put(0x800, 2);
  put(8, 2);
  put(0, 4);
  put(crc, 4);
  put(0xffffffffu, 4);
  put(0xffffffffu, 4);
  put(name.size(), 2);
  put(20, 2);
  out << name;
  put(1, 2);
  put(16, 2);
  put(size, 8);
  put(packed, 8);
  in.clear();
  in.seekg(10);
  std::array<char, 65536> b{};
  uint64_t left = packed;
  while (left) {
    if (vgzCancelled)
      throw std::runtime_error("Cancelled");
    size_t n = size_t(std::min<uint64_t>(left, b.size()));
    if (!in.read(b.data(), n))
      throw std::runtime_error("Cannot read compressed MRL");
    out.write(b.data(), n);
    left -= n;
  }
  uint64_t central = uint64_t(out.tellp());
  put(0x02014b50, 4);
  put(45, 2);
  put(45, 2);
  put(0x800, 2);
  put(8, 2);
  put(0, 4);
  put(crc, 4);
  put(0xffffffffu, 4);
  put(0xffffffffu, 4);
  put(name.size(), 2);
  put(28, 2);
  put(0, 2);
  put(0, 2);
  put(0, 2);
  put(0, 4);
  put(0xffffffffu, 4);
  out << name;
  put(1, 2);
  put(24, 2);
  put(size, 8);
  put(packed, 8);
  put(0, 8);
  uint64_t end = uint64_t(out.tellp());
  put(0x06064b50, 4);
  put(44, 8);
  put(45, 2);
  put(45, 2);
  put(0, 8);
  put(1, 8);
  put(1, 8);
  put(end - central, 8);
  put(central, 8);
  put(0x07064b50, 4);
  put(0, 4);
  put(end, 8);
  put(1, 4);
  put(0x06054b50, 4);
  put(0, 4);
  put(65535, 2);
  put(65535, 2);
  put(0xffffffffu, 4);
  put(0xffffffffu, 4);
  put(0, 2);
  out.close();
  if (!out)
    throw std::runtime_error("Cannot finalize MRZ");
}
// MDXRLOG 1.0: logical YM2151 writes, before any output-backend adaptation.
inline void exportRegisterLog(const std::filesystem::path &input,
                              const std::filesystem::path &target,
                              const VgzBatchOptions &settings) {
  if (std::filesystem::exists(target))
    throw std::runtime_error("Output already exists");
  auto data = readFile(input);
  uint32_t crc = ~0u;
  for (uint8_t value : data) {
    crc ^= value;
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
  }
  Song song(std::move(data));
  std::filesystem::create_directories(target.parent_path());
  std::filesystem::path temporary;
  for (unsigned i = 0; i < 10000; ++i) {
    temporary = target;
    temporary += ".work-" + std::to_string(i);
    if (std::filesystem::create_directory(temporary))
      break;
    temporary.clear();
  }
  if (temporary.empty())
    throw std::runtime_error("Cannot create log temporary directory");
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(p, e);
    }
  } cleanup{temporary};
  auto file = temporary / "output.mrl";
  std::ofstream out(file, std::ios::binary);
  if (!out)
    throw std::runtime_error("Cannot create register log");
  std::vector<unsigned char> header(settings.legacyMrl ? 64 : 128);
  auto put = [](auto &b, size_t at, uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
      b[at + i] = static_cast<unsigned char>(value >> (8 * i));
  };
  const char magic[8] = {'M', 'D', 'X', 'R', 'L', 'O', 'G', 0};
  std::copy(magic, magic + 8, header.begin());
  put(header, 8, 1, 2);
  put(header, 10, settings.legacyMrl ? 0 : 2, 2);
  put(header, 12, header.size(), 4);
  put(header, 16, 16, 4);
  put(header, 20, settings.legacyMrl ? 1 : 3, 4);
  put(header, 24, 4000000, 8);
  put(header, 48, settings.loops, 4);
  put(header, 52, 1, 4);
  put(header, 56, ~crc, 4);
  put(header, 60, 64, 4);
  std::shared_ptr<Pdx> bank;
  if (!settings.legacyMrl) {
    try {
      bank = locatePdx(song, input);
    } catch (const std::exception &e) {
      // Missing banks still permit a portable FM/PCM operation log.
      if (std::string(e.what()).rfind("Required PDX not found:", 0) != 0) throw;
    }
    put(header, 64, song.title.size(), 4);
    put(header, 68, song.pdxName.size(), 4);
    if (bank) {
      put(header, 72, bank->bytes.size(), 8);
      put(header, 80, dataCrc(bank->bytes.data(), bank->bytes.size()), 4);
    }
    bool embedded = settings.embedPdx && bool(bank);
    put(header, 84, embedded, 4);
    uint64_t offset = 128 + song.title.size() + song.pdxName.size() +
                      (embedded ? bank->bytes.size() : 0);
    put(header, 12, offset, 4);
    put(header, 88, offset, 8);
  }
  out.write(reinterpret_cast<const char *>(header.data()), header.size());
  if (!settings.legacyMrl) {
    out.write(song.title.data(), song.title.size());
    out.write(song.pdxName.data(), song.pdxName.size());
    if (bank && settings.embedPdx)
      out.write(reinterpret_cast<const char *>(bank->bytes.data()),
                bank->bytes.size());
  }
  uint64_t events = 0, lastTime = 0, frames = 0;
  Options options;
  options.loops = settings.loops;
  options.fadeSeconds = 0;
  options.maxSeconds = 0;
  options.sampleRate = 62500;
  options.resampleOutput = false;
  options.skipFmSynthesis = true;
  options.skipPcmSynthesis = true;
  auto record = [&](uint64_t tick, int chip, int address, int value,
                    uint32_t argument) {
    uint64_t time = tick * 64;
    if (time < lastTime)
      throw std::runtime_error("Non-monotonic register log timestamp");
    lastTime = time;
    std::array<unsigned char, 16> event{};
    put(event, 0, time, 8);
    event[8] = uint8_t(chip);
    event[9] = uint8_t(address);
    event[10] = uint8_t(value);
    put(event, 12, argument, 4);
    out.write(reinterpret_cast<const char *>(event.data()), event.size());
    ++events;
    if (!out)
      throw std::runtime_error("Cannot write register log");
  };
  options.registerLogOutput = [&](uint64_t tick, int a, int v) {
    record(tick, 0, a, v, 0);
  };
  if (!settings.legacyMrl)
    options.pcmLogOutput = [&](uint64_t tick, int ch, int op, uint32_t arg,
                               int format) {
      record(tick, ch + 1, op, format, arg);
    };
  options.diagnostic = [](const std::string &message) {
    if (!exportLfoWarning(message))
      throw std::runtime_error(message);
  };
  Player player(std::move(song), {}, options);
  std::array<int16_t, 2048> scratch{};
  for (;;) {
    if (vgzCancelled)
      throw std::runtime_error("Cancelled");
    auto n = player.render(scratch.data(), 1024);
    if (!n)
      break;
    frames += n;
    if (frames > uint64_t(settings.seconds) * 62500)
      throw std::runtime_error(
          "Register log time limit exceeded; no partial file published");
  }
  if (!settings.legacyMrl && events) {
    auto timeline = player.recordedTimeline();
    uint64_t begin = uint64_t(std::llround(timeline.loopStart * 4000000));
    uint64_t end = uint64_t(std::llround(timeline.loopEnd * 4000000));
    // Finite scores repeat their full captured performance.
    if (end <= begin) { begin = 0; end = std::max(frames * 64, lastTime); }
    out.flush();
    std::ifstream scan(file, std::ios::binary);
    uint64_t offset = 0;
    for (unsigned i = 0; i < 4; ++i) offset |= uint64_t(header[12+i]) << (i*8);
    scan.seekg(std::streamoff(offset));
    uint64_t first = events, last = events;
    std::array<unsigned char,16> row{};
    for (uint64_t i=0; i<events; ++i) {
      if (!scan.read(reinterpret_cast<char*>(row.data()),16)) throw std::runtime_error("Cannot index log loop");
      uint64_t t=0; for(unsigned j=0;j<8;++j)t|=uint64_t(row[j])<<(j*8);
      if (first==events && t>=begin) first=i;
      if (t>=end) {last=i;break;}
    }
    if (first<last && end>begin) {
      put(header,96,begin,8); put(header,104,end,8);
      put(header,112,first,8); put(header,120,last,8);
    }
  }
  put(header, 32, events, 8);
  put(header, 40, std::max(frames * 64, lastTime), 8);
  out.seekp(0);
  out.write(reinterpret_cast<const char *>(header.data()), header.size());
  out.close();
  if (!out)
    throw std::runtime_error("Cannot finalize register log");
  if (vgzCancelled)
    throw std::runtime_error("Cancelled");
  if (settings.mrz) {
    auto compressed = temporary / "output.mrz";
    zipMrl(file, compressed);
    file = compressed;
  }
#ifdef _WIN32
  if (!MoveFileW(file.c_str(), target.c_str()))
    throw std::runtime_error("Cannot publish register log");
#else
  std::filesystem::create_hard_link(file, target);
#endif
}
inline std::optional<int>
runVgzBatch(const std::vector<std::string> &args,
            const std::function<void(const std::string &)> &report) {
  const bool registerLog =
      std::find(args.begin(), args.end(), "--reglog-folder") != args.end();
  if (!registerLog &&
      std::find(args.begin(), args.end(), "--vgz-folder") == args.end())
    return {};
  const std::string folderOption =
      registerLog ? "--reglog-folder" : "--vgz-folder";
  try {
    std::filesystem::path input;
    VgzBatchOptions options;
    auto number = [](const std::string &s, unsigned maximum) {
      if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("Expected positive integer");
      auto n = std::stoul(s);
      if (!n || n > maximum)
        throw std::runtime_error("Numeric option out of range");
      return unsigned(n);
    };
    bool seenLoops = false, seenSeconds = false, seenNoPcm = false,
         seenPdx = false, seenMrz = false, seenLegacy = false;
    for (size_t i = 1; i < args.size(); ++i) {
      auto name = args[i];
      if (registerLog && (name == "--embed-pdx" || name == "--no-embed-pdx") &&
          !seenPdx) {
        options.embedPdx = name == "--embed-pdx";
        seenPdx = true;
        continue;
      }
      if (registerLog && name == "--mrz" && !seenMrz) {
        options.mrz = true;
        seenMrz = true;
        continue;
      }
      if (registerLog && name == "--mrl-v1" && !seenLegacy) {
        options.legacyMrl = true;
        seenLegacy = true;
        continue;
      }
      if (name == "--no-pcm" && !seenNoPcm && !registerLog) {
        options.pcm = false;
        seenNoPcm = true;
        continue;
      }
      if (i + 1 == args.size())
        throw std::runtime_error("Missing value for " + name);
      auto value = args[++i];
      if (name == folderOption && input.empty())
        input = std::filesystem::u8path(value);
      else if (name == "--loops" && !seenLoops) {
        options.loops = int(number(value, 1000));
        seenLoops = true;
      } else if (name == "--seconds" && !seenSeconds) {
        options.seconds = number(value, 86400);
        seenSeconds = true;
      } else
        throw std::runtime_error("Unknown or repeated batch option: " + name);
    }
    if (options.legacyMrl && seenPdx)
      throw std::runtime_error("--mrl-v1 cannot include PDX metadata");
    input = std::filesystem::canonical(input);
    if (!std::filesystem::is_directory(input))
      throw std::runtime_error("Input is not a directory");
    auto name = input.filename();
    if (name.empty())
      name = registerLog ? "reglog-export" : "vgz-export";
    auto output = std::filesystem::current_path() / name;
    std::vector<std::filesystem::path> files;
    for (auto &entry : std::filesystem::recursive_directory_iterator(input)) {
      if (entry.is_symlink() || !entry.is_regular_file())
        continue;
      auto ext = entry.path().extension().u8string();
      for (auto &c : ext)
        if (c >= 'A' && c <= 'Z')
          c += 32;
      if (ext == ".mdx")
        files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    std::filesystem::create_directories(output);
    auto logPath =
        output / (registerLog ? "reglog-errors.csv" : "vgz-errors.csv");
    bool empty = !std::filesystem::exists(logPath) ||
                 std::filesystem::file_size(logPath) == 0;
    std::ofstream log(logPath, std::ios::binary | std::ios::app);
    if (!log)
      throw std::runtime_error("Cannot open conversion error log");
    if (empty)
      log << "File,Title,Error\r\n";
    vgzCancelled = 0;
    struct Signals {
      decltype(std::signal(SIGINT, cancelVgz)) previous;
      Signals() : previous(std::signal(SIGINT, cancelVgz)) {
#ifdef _WIN32
        SetConsoleCtrlHandler(cancelVgzConsole, TRUE);
#endif
      }
      ~Signals() {
        std::signal(SIGINT, previous);
#ifdef _WIN32
        SetConsoleCtrlHandler(cancelVgzConsole, FALSE);
#endif
      }
    } signals;
    unsigned ok = 0, failed = 0, skipped = 0;
    std::filesystem::path folder;
    for (auto &file : files) {
      if (vgzCancelled)
        break;
      if (folder != file.parent_path()) {
        folder = file.parent_path();
        report("Folder: " + folder.u8string());
      }
      auto target = output / file.lexically_relative(input);
      target.replace_extension(registerLog ? (options.mrz ? ".mrz" : ".mrl")
                                           : ".vgz");
      if (std::filesystem::exists(target)) {
        ++skipped;
        report("SKIP exists: " + target.u8string());
        continue;
      }
      std::string title;
      try {
        title = Song(readFile(file)).title;
        report("Convert: " + file.u8string());
        if (registerLog)
          exportRegisterLog(file, target, options);
        else
          exportVgz(file, target, options, report);
        ++ok;
      } catch (const std::exception &e) {
        if (vgzCancelled)
          break;
        ++failed;
        report("ERROR: " + file.u8string() + ": " + e.what());
        log << csvField(file.u8string()) << ',' << csvField(title) << ','
            << csvField(e.what()) << "\r\n";
        log.flush();
        if (!log)
          throw std::runtime_error("Cannot append conversion error log");
      }
    }
    report("Converted=" + std::to_string(ok) + " Failed=" +
           std::to_string(failed) + " Existing=" + std::to_string(skipped) +
           " Output=" + output.u8string());
    return vgzCancelled ? 130 : failed ? 1 : 0;
  } catch (const std::exception &e) {
    report(std::string("ERROR: ") + e.what());
    return 2;
  }
}
} // namespace mdx
