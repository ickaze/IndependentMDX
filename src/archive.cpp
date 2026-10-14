// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

#include "archive.h"
#include "stream_zip.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <cstring>
#include <sstream>
extern "C" {
#include "../third_party/lhasa/lib/lha_file_header.h"
#include "../third_party/lhasa/lib/public/lha_decoder.h"
}
namespace mdx {
bool isArchive(const std::filesystem::path &p) {
  auto e = p.extension().u8string();
  for (auto &ch : e)
    if (ch >= 'A' && ch <= 'Z')
      ch += 32;
  return e == ".zip" || e == ".lzh" || e == ".lha";
}
namespace {
[[noreturn]] void invalid() {
  throw std::runtime_error("Invalid or unsupported ZIP (stored/deflate, "
                           "unencrypted, single volume required)");
}
struct Member {
  uint64_t offset = 0, packed = 0, size = 0;
  uint32_t crc = 0;
  unsigned method = 0, flags = 0;
  std::string raw;
  bool ready = false;
  bool lzh = false;
};
struct Archive {
  std::filesystem::path source, root;
  uintmax_t size;
  std::filesystem::file_time_type time;
  std::map<std::filesystem::path, Member> members;
};
struct Cache {
  std::recursive_mutex mutex;
  std::filesystem::path base;
  std::map<std::filesystem::path, Archive> archives;
  ~Cache() {
    std::error_code ec;
    if (!base.empty())
      std::filesystem::remove_all(base, ec);
  }
};
Cache &cache() {
  static Cache c;
  return c;
}
Bytes readAt(std::ifstream &in, uint64_t at, size_t size) {
  Bytes b(size);
  in.clear();
  in.seekg(std::streamoff(at));
  if (!in.read(reinterpret_cast<char *>(b.data()), std::streamsize(size)))
    invalid();
  return b;
}
uint64_t number(const Bytes &b, size_t at, unsigned len) {
  if (at > b.size() || len > b.size() - at)
    invalid();
  uint64_t v = 0;
  for (unsigned i = 0; i < len; ++i)
    v |= uint64_t(b[at + i]) << (i * 8);
  return v;
}
std::filesystem::path safeName(std::string name) {
  std::replace(name.begin(), name.end(), '\\', '/');
  if (name.empty() || name[0] == '/' || name.find(':') != std::string::npos ||
      name.find('\0') != std::string::npos)
    invalid();
  auto path = std::filesystem::u8path(name);
  for (const auto &part : path) {
    auto s = part.u8string();
    if (s == "." || s == ".." ||
        (!s.empty() && (s.back() == '.' || s.back() == ' ')))
      invalid();
    auto stem = part.stem().u8string();
    for (auto &ch : stem)
      if (ch >= 'a' && ch <= 'z')
        ch -= 32;
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
        (stem.size() == 4 &&
         (stem.substr(0, 3) == "COM" || stem.substr(0, 3) == "LPT") &&
         stem[3] >= '0' && stem[3] <= '9'))
      invalid();
  }
  return path;
}
std::filesystem::path newRoot() {
  auto &c = cache();
  if (c.base.empty()) {
    for (unsigned i = 0; i < 10000; ++i) {
      auto p =
          std::filesystem::temp_directory_path() /
          std::filesystem::u8path(
              "IndependentMDX-zip-" +
              std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count()) +
              "-" + std::to_string(i));
      if (std::filesystem::create_directory(p)) {
        c.base = p;
        break;
      }
    }
    if (c.base.empty())
      throw std::runtime_error("Cannot create ZIP cache");
  }
  auto root =
      c.base / std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count());
  std::filesystem::create_directory(root);
  return root;
}

#include "lzh_archive.h"

Archive &index(const std::filesystem::path &path) {
  auto &c = cache();
  auto size = std::filesystem::file_size(path);
  auto time = std::filesystem::last_write_time(path);
  auto it = c.archives.find(path);
  if (it != c.archives.end() && it->second.size == size &&
      it->second.time == time)
    return it->second;
  auto extension = path.extension().u8string();
  for (auto &ch : extension) if (ch >= 'A' && ch <= 'Z') ch += 32;
  if (extension == ".lzh" || extension == ".lha") {
    c.archives[path] = indexLzh(path, size, time);
    return c.archives.at(path);
  }
  std::ifstream in(path, std::ios::binary);
  if (size < 22)
    invalid();
  auto tail = readAt(in, size - std::min<uint64_t>(size, 65557),
                     size_t(std::min<uint64_t>(size, 65557)));
  size_t end = tail.size() - 22;
  while (number(tail, end, 4) != 0x06054b50 ||
         end + 22 + number(tail, end + 20, 2) != tail.size()) {
    if (!end)
      invalid();
    --end;
  }
  if (number(tail, end + 4, 2) || number(tail, end + 6, 2) ||
      number(tail, end + 8, 2) != number(tail, end + 10, 2))
    invalid();
  uint64_t count = number(tail, end + 10, 2),
           centralSize = number(tail, end + 12, 4),
           at = number(tail, end + 16, 4),
           centralEnd = size - tail.size() + end;
  if (count == 65535 || centralSize == 0xffffffffu || at == 0xffffffffu) {
    if (centralEnd < 20)
      invalid();
    auto loc = readAt(in, centralEnd - 20, 20);
    if (number(loc, 0, 4) != 0x07064b50 || number(loc, 4, 4) ||
        number(loc, 16, 4) != 1)
      invalid();
    auto z = readAt(in, number(loc, 8, 8), 56);
    if (number(z, 0, 4) != 0x06064b50 || number(z, 16, 4) || number(z, 20, 4) ||
        number(z, 24, 8) != number(z, 32, 8))
      invalid();
    count = number(z, 32, 8);
    centralSize = number(z, 40, 8);
    at = number(z, 48, 8);
    centralEnd = number(loc, 8, 8);
  }
  if (count > 100000 || at > centralEnd || centralSize > centralEnd - at)
    throw std::runtime_error(
        "ZIP directory exceeds supported bounds (100000 entries)");
  auto root = newRoot();
  Archive a{path, root, size, time, {}};
  std::set<std::string> names;
  uint64_t endAt = at + centralSize;
  try {
    for (uint64_t i = 0; i < count; ++i) {
      auto h = readAt(in, at, 46);
      if (number(h, 0, 4) != 0x02014b50)
        invalid();
      Member m;
      m.flags = unsigned(number(h, 8, 2));
      m.method = unsigned(number(h, 10, 2));
      m.crc = uint32_t(number(h, 16, 4));
      m.packed = number(h, 20, 4);
      m.size = number(h, 24, 4);
      m.offset = number(h, 42, 4);
      auto nl = number(h, 28, 2), el = number(h, 30, 2), cl = number(h, 32, 2);
      if ((m.flags & ~0x080eu) || (m.method != 0 && m.method != 8) ||
          number(h, 34, 2) || ((number(h, 38, 4) >> 16) & 0170000) == 0120000 ||
          at + 46 + nl + el + cl > endAt)
        invalid();
      auto name = readAt(in, at + 46, size_t(nl));
      m.raw.assign(name.begin(), name.end());
      auto extra = readAt(in, at + 46 + nl, size_t(el));
      for (size_t e = 0; e < extra.size();) {
        auto tag = number(extra, e, 2), len = number(extra, e + 2, 2);
        e += 4;
        if (len > extra.size() - e)
          invalid();
        if (tag == 1) {
          size_t p = e;
          auto zip64 = [&](uint64_t &v) {
            if (v == 0xffffffffu) {
              if (p + 8 > e + len)
                invalid();
              v = number(extra, p, 8);
              p += 8;
            }
          };
          zip64(m.size);
          zip64(m.packed);
          zip64(m.offset);
        }
        e += size_t(len);
      }
      if (m.offset >= size || m.packed > size || m.size > uint64_t(1) << 40)
        invalid();
      std::string decoded = (m.flags & 0x800) ? m.raw : utf8(m.raw);
      auto relative = safeName(decoded);
      auto key = relative.generic_u8string();
      for (auto &ch : key)
        if (ch >= 'A' && ch <= 'Z')
          ch += 32;
      if (!names.insert(key).second)
        throw std::runtime_error("Duplicate ZIP entry");
      auto target = root / relative;
      if (decoded.back() == '/' || decoded.back() == '\\')
        std::filesystem::create_directories(target);
      else {
        std::filesystem::create_directories(target.parent_path());
        if (std::filesystem::exists(target))
          invalid();
        std::ofstream placeholder(target, std::ios::binary);
        if (!placeholder)
          invalid();
        a.members.emplace(target, std::move(m));
      }
      at += 46 + nl + el + cl;
    }
    if (at != endAt)
      invalid();
  } catch (...) {
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    throw;
  }
  // Keep obsolete roots alive: already opened playback streams may still use
  // them.
  c.archives[path] = std::move(a);
  return c.archives.at(path);
}
auto findMember(Archive &archive, const std::filesystem::path &path) {
  auto found = archive.members.find(path);
#ifdef _WIN32
  // Match Windows filename semantics, including normalized separators.
  if (found == archive.members.end()) {
    const auto wanted = path.generic_wstring();
    found = std::find_if(archive.members.begin(), archive.members.end(), [&](const auto &entry) {
      return _wcsicmp(entry.first.generic_wstring().c_str(), wanted.c_str()) == 0;
    });
  }
#endif
  return found;
}
void materialize(Archive &a, const std::filesystem::path &path, Member &m) {
  if (m.ready)
    return;
  if (m.size > std::filesystem::space(path.parent_path()).available)
    throw std::runtime_error("Not enough free disk space for ZIP member");
  if (m.lzh) {
    auto temp = path; temp += ".extracting";
    try {
      std::ofstream out(temp, std::ios::binary);
      decodeLzh(a, m, out, m.size);
      out.close();
      if (!out) throw std::runtime_error("Cannot write LZH member");
      std::filesystem::remove(path);
      std::filesystem::rename(temp, path);
      m.ready = true;
    } catch (...) {
      std::error_code ec; std::filesystem::remove(temp, ec); throw;
    }
    return;
  }
  std::ifstream in(a.source, std::ios::binary);
  auto h = readAt(in, m.offset, 30);
  if (number(h, 0, 4) != 0x04034b50 || number(h, 6, 2) != m.flags ||
      number(h, 8, 2) != m.method)
    invalid();
  auto nl = number(h, 26, 2), extra = number(h, 28, 2);
  auto name = readAt(in, m.offset + 30, size_t(nl));
  if (std::string(name.begin(), name.end()) != m.raw)
    invalid();
  auto start = m.offset + 30 + nl + extra;
  if (start > a.size || m.packed > a.size - start)
    invalid();
  auto temp = path;
  temp += ".extracting";
  try {
    std::ofstream out(temp, std::ios::binary);
    in.clear();
    in.seekg(std::streamoff(start));
    if (m.method == 8)
      inflateZip(in, m.packed, out, m.size, m.crc);
    else {
      if (m.packed != m.size)
        invalid();
      std::array<char, 65536> b{};
      uint64_t left = m.size;
      uint32_t crc = ~0u;
      while (left) {
        auto n = size_t(std::min<uint64_t>(left, b.size()));
        if (!in.read(b.data(), n))
          invalid();
        for (size_t i = 0; i < n; ++i)
          crc = (crc >> 8) ^ crcTable()[(crc ^ uint8_t(b[i])) & 255];
        out.write(b.data(), n);
        if (!out)
          invalid();
        left -= n;
      }
      if (~crc != m.crc)
        invalid();
    }
    out.close();
    if (!out)
      invalid();
    std::filesystem::remove(path);
    std::filesystem::rename(temp, path);
    m.ready = true;
  } catch (...) {
    std::error_code ec;
    std::filesystem::remove(temp, ec);
    throw;
  }
}
void cachedFile(const std::filesystem::path &p) {
  for (auto &[key, a] : cache().archives) {
    auto it = findMember(a, p);

    if (it != a.members.end()) {
      materialize(a, it->first, it->second);
      return;
    }
  }
}
} // namespace
uintmax_t archiveFileSize(const std::filesystem::path &p) {
  auto &c = cache();
  std::lock_guard<std::recursive_mutex> lock(c.mutex);
  for (auto &[key, a] : c.archives) {
    auto i = findMember(a, p);
    if (i != a.members.end())
      return i->second.size;
  }
  return std::filesystem::file_size(p);
}
std::filesystem::path resolveArchivePath(const std::filesystem::path &path) {
  auto &c = cache();
  std::lock_guard<std::recursive_mutex> lock(c.mutex);
  thread_local unsigned depth = 0;
  auto full = std::filesystem::absolute(path).lexically_normal();
  std::filesystem::path prefix;
  for (const auto &part : full) {
    prefix /= part;
    std::error_code ec;
    if (isArchive(prefix) && std::filesystem::is_regular_file(prefix, ec)) {
      cachedFile(prefix);
      if (depth >= 4)
        throw std::runtime_error("Archive nesting exceeds four levels");
      auto rest = full.lexically_relative(prefix);
      auto root = index(prefix).root;
      if (rest == ".")
        return root;
      struct Guard {
        unsigned &n;
        Guard(unsigned &v) : n(v) { ++n; }
        ~Guard() { --n; }
      } g(depth);
      return resolveArchivePath(root / rest);
    }
  }
  cachedFile(full);
  return full;
}
std::filesystem::path archiveSibling(const std::filesystem::path &source,
                                           const std::filesystem::path &name) {
  auto physical = resolveArchivePath(source);
  auto &c = cache();
  std::lock_guard<std::recursive_mutex> lock(c.mutex);
  auto lower = [](std::string value) {
    for (auto &ch : value) if (ch >= 'A' && ch <= 'Z') ch += 32;
    return value;
  };
  for (auto &[key, a] : c.archives) {
    if (findMember(a, physical) == a.members.end()) continue;
    std::filesystem::path match;
    for (auto &[p, m] : a.members) {
      if (lower(p.filename().u8string()) != lower(name.u8string())) continue;
      if (!match.empty()) throw std::runtime_error("Ambiguous PDX filename inside archive; specify --pdx / Choose PDX");
      match = p;
    }
    return match;
  }
  return {};
}
std::filesystem::path openMrz(const std::filesystem::path &path,
                              bool extractMember) {
  auto physical = resolveArchivePath(path);
  auto &c = cache();
  std::lock_guard<std::recursive_mutex> lock(c.mutex);
  auto &a = index(physical);
  std::filesystem::path result;
  for (auto &[p, m] : a.members) {
    auto e = p.extension().u8string();
    for (auto &ch : e)
      if (ch >= 'A' && ch <= 'Z')
        ch += 32;
    if (e == ".mrl") {
      if (!result.empty())
        throw std::runtime_error("MRZ must contain exactly one MRL");
      result = p;
    }
  }
  if (result.empty())
    throw std::runtime_error("MRZ contains no MRL");
  if (extractMember)
    cachedFile(result);
  return result;
}
Bytes archivePrefix(const std::filesystem::path &path, size_t limit) {
  auto &c = cache();
  std::lock_guard<std::recursive_mutex> lock(c.mutex);
  auto physical = resolveArchivePath(path.parent_path()) / path.filename();
  for (auto &[key, a] : c.archives) {
    auto i = findMember(a, physical);
    if (i == a.members.end())
      continue;
    auto &m = i->second;
    if (m.lzh) {
      std::ostringstream out;
      decodeLzh(a, m, out, std::min<uint64_t>(m.size, limit));
      auto text = out.str();
      return Bytes(text.begin(), text.end());
    }
    std::ifstream in(a.source, std::ios::binary);
    auto h = readAt(in, m.offset, 30);
    if (number(h, 0, 4) != 0x04034b50)
      invalid();
    uint64_t start = m.offset + 30 + number(h, 26, 2) + number(h, 28, 2);
    if (start > a.size || m.packed > a.size - start)
      invalid();
    in.seekg(std::streamoff(start));
    if (m.method == 0)
      return readAt(in, start, size_t(std::min<uint64_t>(limit, m.size)));
    struct Complete {};
    struct Prefix : std::streambuf {
      Bytes b;
      size_t limit;
      explicit Prefix(size_t n) : limit(n) {}
      std::streamsize xsputn(const char *p, std::streamsize n) override {
        size_t take = std::min<size_t>(size_t(n), limit - b.size());
        b.insert(b.end(), p, p + take);
        if (b.size() == limit)
          throw Complete{};
        return n;
      }
    } buffer(limit);
    std::ostream out(&buffer);
    out.exceptions(std::ios::badbit);
    try {
      inflateZip(in, m.packed, out, m.size, m.crc);
    } catch (const Complete &) {
    }
    return buffer.b;
  }
  std::ifstream in(physical, std::ios::binary);
  if (!in)
    throw std::runtime_error("Cannot open title metadata");
  Bytes b(limit);
  in.read(reinterpret_cast<char *>(b.data()), b.size());
  b.resize(size_t(in.gcount()));
  return b;
}

std::filesystem::path archiveCopyPath(const std::filesystem::path &path) {
  auto &c = cache();
  std::lock_guard<std::recursive_mutex> lock(c.mutex);
  auto physical = resolveArchivePath(path.parent_path()) / path.filename();
  cachedFile(physical);
  if (std::filesystem::is_directory(physical)) {
    for (auto &entry : std::filesystem::recursive_directory_iterator(physical))
      if (entry.is_regular_file()) cachedFile(entry.path());
  }
  return physical;
}

} // namespace mdx
