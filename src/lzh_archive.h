// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

// Lhasa is used only for archive headers and decompression, never MDX interpretation.
struct LzhInput {
  std::ifstream file;
  uint64_t remaining;
  explicit LzhInput(const std::filesystem::path &path, uint64_t start, uint64_t length)
      : file(path, std::ios::binary), remaining(length) {
    file.seekg(std::streamoff(start));
  }
  static int read(void *context, void *buffer, size_t length) {
    auto &input = *static_cast<LzhInput *>(context);
    size_t count = size_t(std::min<uint64_t>(input.remaining, length));
    input.file.read(static_cast<char *>(buffer), std::streamsize(count));
    auto got = input.file.gcount();
    input.remaining -= uint64_t(got);
    return int(got);
  }
  static size_t decodeRead(void *buffer, size_t length, void *context) {
    return size_t(read(context, buffer, length));
  }
};
bool supportedLzh(const char *method) {
  for (auto allowed : {"-lh0-", "-lh1-", "-lh4-", "-lh5-", "-lh6-", "-lh7-", "-lz4-", "-lz5-", "-lzs-", "-lhd-"})
    if (std::strcmp(method, allowed) == 0) return true;
  return false;
}
Archive indexLzh(const std::filesystem::path &path, uint64_t size,
                 std::filesystem::file_time_type time) {
  Archive a{path, newRoot(), size, time, {}};
  uint64_t at = 0;
  unsigned count = 0;
  std::set<std::string> names;
  std::ifstream check(path, std::ios::binary);
  try {
    while (at < size) {
      auto lead = readAt(check, at, size_t(std::min<uint64_t>(size - at, 24)));
      if (lead.size() == 1 && lead[0] == 0) break;
      // LZH level 2 can have a zero low byte of its 16-bit header size.
      if (lead.size() < 24 || lead[2] != '-' || lead[6] != '-' || ++count > 100000)
        throw std::runtime_error("Invalid LZH header or entry count");
      LzhInput input(path, at, size - at);
      static const LHAInputStreamType callbacks{LzhInput::read, nullptr, nullptr};
      auto stream = std::unique_ptr<LHAInputStream, decltype(&lha_input_stream_free)>(
          lha_input_stream_new(&callbacks, &input), lha_input_stream_free);
      if (!stream) throw std::bad_alloc();
      auto header = std::unique_ptr<LHAFileHeader, decltype(&lha_file_header_free)>(
          lha_file_header_read(stream.get()), lha_file_header_free);
      if (!header || header->symlink_target || !supportedLzh(header->compress_method))
        throw std::runtime_error("Invalid/unsupported LZH header, method or link");
      // Reject legacy path traversal before Lhasa normalizes legacy filenames.
      if (header->raw_data_len > 22 && header->raw_data[20] <= 1) {
        size_t length = header->raw_data[21];
        if (length > header->raw_data_len - 22)
          throw std::runtime_error("Invalid LZH filename length");
        if (length) safeName(utf8(std::string(
            reinterpret_cast<const char *>(header->raw_data + 22), length)));
      }
      auto start = size - input.remaining;
      if (start != at + header->raw_data_len || header->compressed_length > size - start ||
          header->length > (uint64_t(1) << 40))
        throw std::runtime_error("LZH member exceeds archive bounds");
      std::string raw = std::string(header->path ? header->path : "") +
                        (header->filename ? header->filename : "");
      auto relative = safeName(utf8(raw));
      auto key = relative.generic_u8string();
      for (auto &ch : key) if (ch >= 'A' && ch <= 'Z') ch += 32;
      if (!names.insert(key).second) throw std::runtime_error("Duplicate LZH entry");
      auto target = a.root / relative;
      if (std::strcmp(header->compress_method, "-lhd-") == 0) {
        std::filesystem::create_directories(target);
      } else {
        std::filesystem::create_directories(target.parent_path());
        if (std::filesystem::exists(target)) throw std::runtime_error("Conflicting LZH path");
        std::ofstream placeholder(target, std::ios::binary);
        if (!placeholder) throw std::runtime_error("Cannot create LZH cache file");
        Member member;
        member.offset = start;
        member.size = header->length;
        member.packed = header->compressed_length;
        member.crc = header->crc;
        member.raw = header->compress_method;
        member.lzh = true;
        a.members.emplace(target, std::move(member));
      }
      at = start + header->compressed_length;
    }
    if (!count) throw std::runtime_error("LZH contains no entries");
  } catch (...) {
    std::error_code ec;
    std::filesystem::remove_all(a.root, ec);
    throw;
  }
  return a;
}
void decodeLzh(const Archive &archive, const Member &member, std::ostream &out,
               uint64_t limit) {
  LzhInput input(archive.source, member.offset, member.packed);
  auto type = lha_decoder_for_name(member.raw.c_str());
  if (!type) throw std::runtime_error("Unsupported LZH compression method");
  auto decoder = std::unique_ptr<LHADecoder, decltype(&lha_decoder_free)>(
      lha_decoder_new(type, LzhInput::decodeRead, &input, member.size), lha_decoder_free);
  if (!decoder) throw std::runtime_error("Cannot initialize LZH decoder");
  std::array<uint8_t, 65536> buffer{};
  uint64_t written = 0;
  while (written < limit) {
    auto count = lha_decoder_read(decoder.get(), buffer.data(),
                                 size_t(std::min<uint64_t>(buffer.size(), limit - written)));
    if (!count) throw std::runtime_error("Truncated LZH compressed data");
    out.write(reinterpret_cast<const char *>(buffer.data()), count);
    if (!out) throw std::runtime_error("Cannot write extracted LZH data");
    written += count;
  }
  if (limit == member.size &&
      (lha_decoder_get_length(decoder.get()) != member.size ||
       lha_decoder_get_crc(decoder.get()) != member.crc))
    throw std::runtime_error("LZH size or CRC mismatch");
}
