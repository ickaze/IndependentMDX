#include "archive.h"
#include "vgm.h"
#include <iostream>
#include <stdexcept>
void check(bool ok, const char *s) {
  if (!ok)
    throw std::runtime_error(s);
}
int main() {
  try {
    auto root = std::filesystem::path(__FILE__).parent_path();
    auto zip = root / "fixtures/browser.zip";
    auto virtualPath = zip / std::filesystem::u8path(u8"日本語/DEMO.MDX");
    auto directory = mdx::resolveArchivePath(virtualPath.parent_path());
    check(std::filesystem::is_directory(directory), "ZIP directory");
    auto copied=mdx::archiveCopyPath(virtualPath);
    check(std::filesystem::file_size(copied)>0,"drag source not materialized");
    auto source = mdx::readFile(virtualPath);
    check(source == mdx::readFile(root.parent_path() / "samples/DEMO.MDX"),
          "Deflate bytes differ");
    mdx::Song song(source);
    auto pdx = mdx::locatePdx(song, virtualPath, {});
    check(bool(pdx), "PDX inside ZIP missing");
    for (auto name : {"pdx-same.zip", "pdx-other.zip", "browser-lh0.lzh", "browser-lh5.lzh", "browser-lh6.lzh", "browser-lh7.lzh", "browser-level2.lzh"}) {
      auto file = root / "fixtures" / name / "Music/DEMO.MDX";
      check(mdx::readFile(file) == source, "Archive MDX bytes differ");
      auto bank = mdx::locatePdx(song, file, {});
      check(bank && mdx::readFile(bank->sourcePath) == mdx::readFile(root.parent_path()/"samples/DEMO.PDX"), "Archive PDX bytes differ");
      check(mdx::archivePrefix(file,32) == mdx::Bytes(source.begin(),source.begin()+32), "Archive title prefix differs");
    }
#ifdef _WIN32
    auto casePath = root / "fixtures/pdx-same.zip/music/demo.mdx";
    check(mdx::readFile(casePath) == source, "Windows case-insensitive cache lookup failed");
#endif
    for (auto name : {"bad-header.lzh", "traversal.lzh"}) {
      bool failed=false;
      try {mdx::resolveArchivePath(root / "fixtures" / name);} catch(...) {failed=true;}
      check(failed,"Unsafe/corrupt LZH accepted");
    }
    bool badCrc=false;
    try {mdx::readFile(root/"fixtures/bad-data.lzh/Music/DEMO.MDX");} catch(...) {badCrc=true;}
    check(badCrc,"Corrupt LZH payload accepted");
    bool ambiguous=false;
    try {mdx::locatePdx(song,root/"fixtures/pdx-ambiguous.zip/Music/DEMO.MDX",{});} catch(...) {ambiguous=true;}
    check(ambiguous,"Ambiguous archive PDX chosen silently");
    auto log = mdx::readFile(zip / "nested.zip/record.mrl");
    check(log == mdx::readFile(root.parent_path() / "samples/DEMO.mrl"),
          "Nested/stored ZIP");
    mdx::VgmSong mrl(log);
    check(mrl.registerLog && mrl.events.size() == 588 &&
              mrl.preciseTimes.size() == 588,
          "MRL parse");
    bool fractional=false;
    for(double t:mrl.preciseTimes)fractional|=std::abs(t-std::floor(t))>1e-6;
    check(fractional,"MRL timing rounded to integer VGM samples");
    mdx::Options options;
    options.maxSeconds = 2;
    options.gain = 1;
    mdx::VgmPlayer player(std::move(mrl), options);
    std::array<int16_t, 2048> audio{};
    bool audible = false;
    while (auto n = player.render(audio.data(), 1024))
      for (size_t i = 0; i < n * 2; ++i)
        audible |= audio[i] != 0;
    check(audible, "MRL output silent");
    check(player.seek(.5), "MRL seek");
    check(player.render(audio.data(), 1024) > 0, "MRL seek resume");
    bool rejected = false;
    try {
      mdx::resolveArchivePath(root / "fixtures/traversal.zip");
    } catch (...) {
      rejected = true;
    }
    check(rejected, "ZIP traversal accepted");
    auto bad = log;
    bad[12] = 63;
    rejected = false;
    try {
      mdx::VgmSong invalid(bad);
    } catch (...) {
      rejected = true;
    }
    check(rejected, "MRL invalid header accepted");
    bad = log;
    bad[64 + 8] = 1;
    rejected = false;
    try {
      mdx::VgmSong invalid(bad);
    } catch (...) {
      rejected = true;
    }
    check(rejected, "MRL invalid chip accepted");
    bad = log;
    bad.pop_back();
    rejected = false;
    try {
      mdx::VgmSong invalid(bad);
    } catch (...) {
      rejected = true;
    }
    check(rejected, "MRL truncation accepted");
    std::cout << "Archive/MRL regression checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
