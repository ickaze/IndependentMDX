// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

#pragma once
#include <array>
#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <vector>
namespace mdx {
inline const std::array<uint32_t, 256> &crcTable() {
  static const auto table = [] {
    std::array<uint32_t, 256> t{};
    for (unsigned i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int j = 0; j < 8; ++j)
        c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
      t[i] = c;
    }
    return t;
  }();
  return table;
}
inline uint32_t dataCrc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  while (n--)
    c = (c >> 8) ^ crcTable()[(c ^ *p++) & 255];
  return ~c;
}
inline void inflateZip(std::istream &input, uint64_t packed,
                       std::ostream &output, uint64_t expectedSize,
                       uint32_t expectedCrc) {
  auto fail = [] {
    throw std::runtime_error("Invalid ZIP deflate stream or CRC/size mismatch");
  };
  uint64_t pos = 0;
  unsigned bit = 0, current = 0;
  auto get = [&](unsigned n) {
    unsigned v = 0;
    for (unsigned i = 0; i < n; ++i) {
      if (!bit) {
        if (pos >= packed)
          fail();
        int c = input.get();
        if (c < 0)
          fail();
        current = unsigned(c);
        ++pos;
      }
      v |= ((current >> bit) & 1u) << i;
      bit = (bit + 1) & 7;
    }
    return v;
  };
  struct Huff {
    std::array<unsigned, 16> count{};
    std::vector<unsigned> symbols;
    explicit Huff(const std::vector<unsigned> &lengths) {
      for (auto n : lengths) {
        if (n > 15)
          throw std::runtime_error("Invalid VGZ Huffman length");
        ++count[n];
      }
      int available = 1;
      for (int n = 1; n <= 15; ++n) {
        available = available * 2 - int(count[n]);
        if (available < 0)
          throw std::runtime_error("Invalid VGZ Huffman tree");
      }
      std::array<unsigned, 16> offset{};
      for (int n = 1; n < 15; ++n)
        offset[n + 1] = offset[n] + count[n];
      symbols.resize(lengths.size() - count[0]);
      for (unsigned i = 0; i < lengths.size(); ++i)
        if (lengths[i])
          symbols[offset[lengths[i]]++] = i;
    }
  };
  auto decode = [&](const Huff &h) {
    unsigned code = 0, first = 0, index = 0;
    for (int n = 1; n <= 15; ++n) {
      code = (code << 1) | get(1);
      unsigned count = h.count[n];
      if (code >= first && code - first < count)
        return h.symbols[index + code - first];
      index += count;
      first = (first + count) << 1;
    }
    fail();
    return 0u;
  };
  std::array<uint8_t, 32768> history{};
  std::array<char, 65536> outputBuffer{};
  size_t buffered = 0;
  uint64_t produced = 0;
  uint32_t crc = ~0u;
  auto put = [&](uint8_t v) {
    if (produced >= expectedSize)
      fail();
    history[produced++ % history.size()] = v;
    crc = (crc >> 8) ^ crcTable()[(crc ^ v) & 255];
    outputBuffer[buffered++] = char(v);
    if (buffered == outputBuffer.size()) {
      output.write(outputBuffer.data(), buffered);
      buffered = 0;
      if (!output)
        fail();
    }
  };
  bool last = false;
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
  while (!last) {
    last = get(1);
    unsigned type = get(2);
    if (type == 0) {
      if (bit) {
        bit = 0;
      }
      unsigned n = get(16), inverse = get(16);
      if ((n ^ inverse) != 65535)
        fail();
      while (n--)
        put(uint8_t(get(8)));
      continue;
    }
    if (type == 3)
      fail();
    std::vector<unsigned> lit(288), dist(32, 5);
    if (type == 1) {
      for (unsigned i = 0; i < 288; ++i)
        lit[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
    } else {
      unsigned nl = get(5) + 257, nd = get(5) + 1, nc = get(4) + 4;
      if (nl > 286)
        fail();
      constexpr unsigned co[] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                 11, 4,  12, 3, 13, 2, 14, 1, 15};
      std::vector<unsigned> cl(19);
      for (unsigned i = 0; i < nc; ++i)
        cl[co[i]] = get(3);
      Huff ch(cl);
      std::vector<unsigned> lens;
      while (lens.size() < nl + nd) {
        unsigned v = decode(ch), n = 1;
        if (v == 16) {
          if (lens.empty())
            fail();
          v = lens.back();
          n = get(2) + 3;
        } else if (v == 17) {
          v = 0;
          n = get(3) + 3;
        } else if (v == 18) {
          v = 0;
          n = get(7) + 11;
        }
        if (lens.size() + n > nl + nd)
          fail();
        lens.insert(lens.end(), n, v);
      }
      lit.assign(lens.begin(), lens.begin() + nl);
      dist.assign(lens.begin() + nl, lens.end());
    }
    if (!lit[256])
      fail();
    Huff lh(lit), dh(dist);
    for (;;) {
      unsigned v = decode(lh);
      if (v < 256) {
        put(uint8_t(v));
        continue;
      }
      if (v == 256)
        break;
      if (v > 285)
        fail();
      v -= 257;
      unsigned n = lb[v] + get(le[v]), d = decode(dh);
      if (d >= 30)
        fail();
      d = db[d] + get(de[d]);
      if (d > produced)
        fail();
      while (n--)
        put(history[(produced - d) % history.size()]);
    }
  }
  if (buffered)
    output.write(outputBuffer.data(), buffered);
  if (!output || pos != packed || produced != expectedSize ||
      ~crc != expectedCrc)
    fail();
}
} // namespace mdx
