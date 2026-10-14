#include "gzip_decode.h"
#include "vgm.h"
#include "vgz_export.h"
#include <random>
#include <sstream>
using namespace mdx;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
Bytes fixture(bool lfo = false, bool missingVoice = false) {
  Bytes b{'T', 13, 10, 26, 0};
  size_t base = b.size();
  b.resize(base + 20);
  auto word = [&](size_t p, size_t n) {
    b[p] = uint8_t(n >> 8);
    b[p + 1] = uint8_t(n);
  };
  for (int ch = 0; ch < 9; ++ch) {
    word(base + 2 + ch * 2, b.size() - base);
    Bytes t =
        ch ? Bytes{241, 0} : Bytes{253, 0, 251, 128, 180, 7, 241, 255, 250};
    if (!ch) {
      if (missingVoice) t[1] = 1;
      if (lfo) t.insert(t.begin(), {0xec, 2, 0, 6, 0, 48, 0xeb, 1, 0, 12, 9, 0});
    }
    b.insert(b.end(), t.begin(), t.end());
  }
  word(base, b.size() - base);
  Bytes voice{0,  7, 1, 1, 1, 1, 1, 0, 127, 127, 127, 31, 31, 31,
              31, 0, 0, 0, 0, 0, 0, 0, 0,   15,  15,  15, 15};
  b.insert(b.end(), voice.begin(), voice.end());
  return b;
}
int main() {
  try {
    auto root =
        std::filesystem::temp_directory_path() / "independent-mdx-export-tests";
    check(std::filesystem::create_directory(root),
          "test directory already exists");
    struct Clean {
      std::filesystem::path p;
      ~Clean() {
        std::error_code e;
        std::filesystem::remove_all(p, e);
      }
    } clean{root};
    auto write = [](const auto &p, const Bytes &b) {
      std::ofstream f(p, std::ios::binary);
      f.write((const char *)b.data(), b.size());
    };
    auto report = [](const std::string &) {};
    std::mt19937 rng(19);
    for (size_t n : {size_t(0), size_t(1), size_t(65536), size_t(131079)}) {
      for (int pattern = 0; pattern < 2; ++pattern) {
        std::string raw(n, 'x');
        if (pattern)
          for (auto &c : raw)
            c = char(rng());
        std::istringstream input(raw);
        std::ostringstream output;
        gzipVgm(input, output);
        auto s = output.str();
        Bytes bytes(s.begin(), s.end());
        auto decoded = unpackVgm(bytes);
        check(std::string(decoded.begin(), decoded.end()) == raw,
              "gzip roundtrip");
      }
    }
    auto source = root / "song.mdx";
    write(source, fixture());
    exportVgz(source, root / "one.vgz", {1, 30}, report);
    exportVgz(source, root / "two.vgz", {2, 30}, report);
    VgmSong one(readFile(root / "one.vgz")), two(readFile(root / "two.vgz"));
    check(!one.looping && !two.looping, "finite VGM output");
    check(two.samples > one.samples, "requested loop count changes duration");
    check(!one.pwm, "FM-only output has no PWM chip");
    bool failed = false;
    try {
      exportVgz(source, root / "one.vgz", {2, 30}, report);
    } catch (...) {
      failed = true;
    }
    check(failed, "existing output protected");
    failed = false;
    try {
      exportVgz(source, root / "short.vgz", {100, 1}, report);
    } catch (...) {
      failed = true;
    }
    check(failed && !std::filesystem::exists(root / "short.vgz"),
          "time limit publishes no partial output");
    // Bundled original sample exercises PDX mixing and direct-PWM replay.
    auto demo = std::filesystem::path(__FILE__).parent_path().parent_path() /
                "samples/DEMO.MDX";
    exportVgz(demo, root / "pcm.vgz", {1, 60}, report);
    VgmSong pcm(readFile(root / "pcm.vgz"));
    check(pcm.pwm, "PCM preserved");
    exportVgz(demo, root / "no-pcm.vgz", {1, 60, false}, report);
    VgmSong noPcm(readFile(root / "no-pcm.vgz"));
    check(!noPcm.pwm && noPcm.samples == pcm.samples,
          "PCM omission preserves duration");
    size_t j = 0;
    for (const auto &e : pcm.events)
      if (e.address < 256) {
        check(j < noPcm.events.size(), "FM events missing");
        auto f = noPcm.events[j++];
        check(e.time == f.time && e.address == f.address && e.value == f.value,
              "FM events changed by PCM omission");
      }
    check(j == noPcm.events.size(), "extra FM events");
    auto japanese = fixture();
    japanese.erase(japanese.begin());
    japanese.insert(japanese.begin(),
                    {0x93, 0xfa, 0x96, 0x7b}); // CP932 Japanese title.
    write(root / "japanese.mdx", japanese);
    exportVgz(root / "japanese.mdx", root / "japanese.vgz", {1, 30, false},
              report);
    auto tagged = unpackVgm(readFile(root / "japanese.vgz"));
    auto word = [&](size_t at) {
      return uint32_t(tagged[at]) | (uint32_t(tagged[at + 1]) << 8) |
             (uint32_t(tagged[at + 2]) << 16) |
             (uint32_t(tagged[at + 3]) << 24);
    };
    size_t gd3 = word(0x14) + 0x14;
    check(word(gd3 + 4) == 0x100 && gd3 + 12 + word(gd3 + 8) == tagged.size(),
          "GD3 header/length");
    std::vector<std::u16string> fields(1);
    for (size_t at = gd3 + 12; at < tagged.size(); at += 2) {
      char16_t c = char16_t(tagged[at] | (tagged[at + 1] << 8));
      if (c)
        fields.back() += c;
      else
        fields.emplace_back();
    }
    check(fields.size() == 12 && fields[0] == u"日本" && fields[1] == u"日本",
          "Japanese GD3 titles");
    check(fields[9] == u"Independent MDX" &&
              fields[10] == u"Exported from Independent MDX",
          "GD3 ripper and comment");
    check(VgmSong(readFile(root / "japanese.vgz")).title == u8"日本",
          "GD3 player title");
    Options opt;
    opt.gain = 1;
    opt.independentGain = 0;
    opt.maxSeconds = 5;
    VgmPlayer player(std::move(pcm), opt);
    std::array<int16_t, 2048> audio{};
    bool heard = false;
    while (auto n = player.render(audio.data(), 1024))
      for (size_t i = 0; i < n * 2; ++i)
        heard |= audio[i] != 0;
    check(heard, "PCM replay audible with FM disabled");
    // Real MP/MA commands must complete export; missing voices still fail.
    write(root / "lfo.mdx", fixture(true));
    exportRegisterLog(root / "lfo.mdx", root / "lfo.mrl", {1, 30});
    check(VgmSong(readFile(root / "lfo.mrl")).events.size() > 20,
          "LFO register log completed");
    exportVgz(root / "lfo.mdx", root / "lfo.vgz", {1, 30, false}, report);
    check(VgmSong(readFile(root / "lfo.vgz")).events.size() > 20,
          "LFO VGZ completed");
    write(root / "missing-voice.mdx", fixture(true, true));
    for (bool mrl : {false, true}) {
      auto target = root / (mrl ? "error.mrl" : "error.vgz");
      bool rejected = false;
      try {
        if (mrl) exportRegisterLog(root / "missing-voice.mdx", target, {1, 30});
        else exportVgz(root / "missing-voice.mdx", target, {1, 30, false}, report);
      } catch (const std::exception &) { rejected = true; }
      check(rejected && !std::filesystem::exists(target),
            "data error after LFO warning must not publish output");
    }
    VgzBatchOptions legacy{1,30};legacy.legacyMrl=true;
    exportRegisterLog(source, root / "song.mrl", legacy);
    auto binary = readFile(root / "song.mrl");
    auto integer = [&](size_t offset, unsigned size) {
      uint64_t v = 0;
      for (unsigned i = 0; i < size; ++i)
        v |= uint64_t(binary.at(offset + i)) << (8 * i);
      return v;
    };
    check(std::string(binary.begin(), binary.begin() + 7) == "MDXRLOG" &&
              binary[7] == 0,
          "register log magic");
    check(integer(8, 2) == 1 && integer(10, 2) == 0 && integer(12, 4) == 64 &&
              integer(16, 4) == 16,
          "register log version and sizes");
    check(integer(20, 4) == 1 && integer(24, 8) == 4000000 &&
              integer(48, 4) == 1 && integer(52, 4) == 1 &&
              integer(60, 4) == 64,
          "register log profile");
    check(binary.size() == 64 + 16 * integer(32, 8), "register log length");
    Options traceOptions;
    traceOptions.loops = 1;
    traceOptions.fadeSeconds = 0;
    traceOptions.maxSeconds = 0;
    traceOptions.sampleRate = 62500;
    traceOptions.resampleOutput = false;
    traceOptions.skipPcmSynthesis = true;
    uint64_t eventIndex = 0, lastEvent = 0;
    traceOptions.registerLogOutput = [&](uint64_t tick, int address, int value) {
      size_t at = 64 + 16 * eventIndex++;
      check(integer(at, 8) == tick * 64 && integer(at + 8, 1) == 0 &&
                integer(at + 9, 1) == unsigned(address) &&
                integer(at + 10, 1) == unsigned(value),
            "register log differs from playback writes");
      check(integer(at + 11, 1) == 0 && integer(at + 12, 4) == 0,
            "register log phase/reserved");
      lastEvent = tick * 64;
    };
    Player normal(Song(readFile(source)), {}, traceOptions);
    while (normal.render(audio.data(), 1024)) {
    }
    check(eventIndex == integer(32, 8) && integer(40, 8) >= lastEvent,
          "register log count/end");
    bool existsRejected = false;
    try {
      exportRegisterLog(source, root / "song.mrl", {1, 30});
    } catch (...) {
      existsRejected = true;
    }
    check(existsRejected, "register log no overwrite");
    bool timeout = false;
    try {
      exportRegisterLog(source, root / "timeout.mrl", {100, 1});
    } catch (...) {
      timeout = true;
    }
    check(timeout && !std::filesystem::exists(root / "timeout.mrl"),
          "register log no partial file");
    exportRegisterLog(demo, root / "pdx-free.mrl", {1, 60});
    auto previous = std::filesystem::current_path();
    struct Restore {
      std::filesystem::path p;
      ~Restore() { std::filesystem::current_path(p); }
    } restore{previous};
    auto input = root / "input";
    auto output = root / "output";
    std::filesystem::create_directories(input / "nested");
    std::filesystem::create_directory(output);
    write(input / "nested/a.MDX", fixture());
    write(input / "bad.mdx", Bytes{0});
    std::filesystem::current_path(output);
    auto result = runVgzBatch(
        {"app", "--vgz-folder", input.u8string(), "--loops", "1", "--no-pcm"},
        report);
    check(result && *result == 1, "batch reports invalid input and continues");
    check(std::filesystem::exists(output / "input/nested/a.vgz"),
          "relative hierarchy preserved");
    check(std::filesystem::file_size(output / "input/vgz-errors.csv") > 20,
          "error log written");
    check(
        *runVgzBatch({"app", "--vgz-folder", input.u8string(), "--loops", "0"},
                     report) == 2,
        "infinite export rejected");
    auto logResult = runVgzBatch(
        {"app", "--reglog-folder", input.u8string(), "--loops", "1"}, report);
    check(logResult && *logResult == 1 &&
              std::filesystem::exists(output / "input/nested/a.mrl"),
          "register batch mirrors folders and continues errors");
    check(std::filesystem::exists(output / "input/reglog-errors.csv"),
          "register batch error log");
    check(*runVgzBatch({"app", "--reglog-folder", input.u8string(), "--no-pcm"},
                       report) == 2,
          "incompatible register options rejected");
    std::cout << "VGZ/register export regression checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
