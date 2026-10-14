#include "archive.h"
#include "vgm.h"
#include "vgz_export.h"
#include <iostream>
using namespace mdx;
void check(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
}
void writeFile(const std::filesystem::path &p, const Bytes &b) {
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char *>(b.data()), b.size());
}
int main() {
  try {
    auto root =
        std::filesystem::temp_directory_path() / "independent-mdx-mrl-tests";
    check(std::filesystem::create_directory(root), "test directory exists");
    struct Clean {
      std::filesystem::path p;
      ~Clean() {
        std::error_code ec;
        std::filesystem::remove_all(p, ec);
      }
    } clean{root};
    Bytes song{'P', 'C', 'M', '8', 13, 10, 26, 'b', 'a', 'n', 'k', 0};
    size_t base = song.size();
    song.resize(base + 34);
    auto be = [&](size_t at, size_t v) {
      song[at] = uint8_t(v >> 8);
      song[at + 1] = uint8_t(v);
    };
    for (unsigned ch = 0; ch < 16; ++ch) {
      be(base + 2 + ch * 2, song.size() - base);
      Bytes t = ch == 0 ? Bytes{232, 241, 0}
                : ch < 8
                    ? Bytes{241, 0}
                    : Bytes{253, 0, 237, 4, 252, 3, 251, 8, 128, 31, 241, 0};
      song.insert(song.end(), t.begin(), t.end());
    }
    be(base, song.size() - base);
    writeFile(root / "pcm.mdx", song);
    Bytes bank(96 * 8 + 1024, 0x77);
    std::fill(bank.begin(), bank.begin() + 96 * 8, 0);
    bank[2] = 3;
    bank[6] = 4;
    writeFile(root / "bank.PDX", bank);
    VgzBatchOptions opts{1, 30};
    exportRegisterLog(root / "pcm.mdx", root / "embedded.mrl", opts);
    check(readVgmTitle(root / "embedded.mrl") == "PCM8", "MRL title");
    auto music = loadVgmSong(root / "embedded.mrl");
    check(music.mrl && music.events.empty() && music.pdx && music.mrl->embedded,
          "streaming embedded MRL");
    Options play;
    play.gain = .005;
    play.independentGain = 0;
    play.maxSeconds = 2;
    play.monitor = std::make_shared<Monitor>();
    VgmPlayer p(music, play);
    std::array<int16_t, 256> sound{};
    check(p.render(sound.data(), 128) == 128, "MRL PCM render");
    int peak = 0;
    for (auto v : sound)
      peak = std::max(peak, std::abs(int(v)));
    check(peak > 10, "MRL PCM silent");
    for (unsigned ch = 8; ch < 16; ++ch)
      check(play.monitor->frame.note[ch] == 0, "PCM8 monitor missing channel");
    play.pcmMuteMask = 127;
    VgmPlayer one(music, play);
    std::array<int16_t, 256> single{};
    one.render(single.data(), 128);
    int solo = 0;
    for (auto v : single)
      solo = std::max(solo, std::abs(int(v)));
    check(peak > solo * 7 && peak < solo * 9, "PCM8 mix/mask ratio");
    check(p.seek(.004) && p.render(sound.data(), 128) > 0, "PCM seek resume");
    opts.embedPdx = false;
    exportRegisterLog(root / "pcm.mdx", root / "external.mrl", opts);
    check(loadVgmSong(root / "external.mrl").pdx != nullptr,
          "external PDX lookup");
    std::filesystem::rename(root / "bank.PDX", root / "moved.PDX");
    auto missing = loadVgmSong(root / "external.mrl");
    check(!missing.pdx && !missing.pcmWarning.empty(), "missing PDX warning");
    check(loadVgmSong(root / "external.mrl", root / "moved.PDX").pdx != nullptr,
          "PDX override");
    bank.back() ^= 1;
    writeFile(root / "bank.PDX", bank);
    check(!loadVgmSong(root / "external.mrl").pdx,
          "PDX fingerprint mismatch accepted");
    std::filesystem::remove(root / "bank.PDX");
    zipMrl(root / "embedded.mrl", root / "packed.mrz");
    check(readVgmTitle(root / "packed.mrz") == "PCM8", "MRZ metadata");
    auto compressed = loadVgmSong(root / "packed.mrz");
    check(compressed.mrl->count == music.mrl->count &&
              compressed.pdx->bytes == music.pdx->bytes,
          "MRZ roundtrip");
    // Missing PDX with explicit embedding falls back to an external reference.
    opts.embedPdx = true;
    exportRegisterLog(root / "pcm.mdx", root / "missing.mrl", opts);
    auto fallback = loadVgmSong(root / "missing.mrl");
    check(!fallback.mrl->embedded && fallback.mrl->pdxName == "bank", "missing bank did not fall back");
    check(fallback.mrl->minor == 2 && fallback.looping, "MRL 1.2 loop metadata missing");
    auto allFrames = [&](VgmPlayer &v, size_t maximum) {
      size_t total=0;
      while(total<maximum) {auto n=v.render(sound.data(),128);if(!n)break;total+=n;}
      return total;
    };
    Options looping; looping.loops=0;looping.maxSeconds=0;
    VgmPlayer forever(music,looping);
    size_t longRun = size_t((forever.duration()*4+1)*48000);
    check(allFrames(forever,longRun)>=longRun,"infinite MRL stopped at recorded end");
    check(forever.positionSeconds() <= forever.duration(),"MRL position failed to wrap");
    looping.loops=2;looping.fadeSeconds=.05;
    VgmPlayer bounded(music,looping);
    auto boundedFrames=allFrames(bounded,longRun);
    check(boundedFrames<longRun && boundedFrames>=size_t(bounded.duration()*2*48000),"finite MRL loop/fade duration");
    unsigned progressCalls=0;bool completed=false;
    looping.seekProgress=[&](double current,double target){++progressCalls;completed=current==target;};
    VgmPlayer seeking(music,looping);
    check(seeking.seek(.1) && progressCalls>1 && completed,"MRL seek progress missing");
    // Genuine MDX loop region, while preserving the requested capture count.
    Bytes loopSong{'L',13,10,26,0};size_t lb=loopSong.size();loopSong.resize(lb+20);
    auto lw=[&](size_t at,size_t v){loopSong[at]=uint8_t(v>>8);loopSong[at+1]=uint8_t(v);};
    for(unsigned ch=0;ch<9;++ch){lw(lb+2+ch*2,loopSong.size()-lb);Bytes tr=ch?Bytes{241,0}:Bytes{253,0,251,128,180,7,241,255,250};loopSong.insert(loopSong.end(),tr.begin(),tr.end());}
    lw(lb,loopSong.size()-lb);
    Bytes voice{0,7,1,1,1,1,1,0,127,127,127,31,31,31,31,0,0,0,0,0,0,0,0,15,15,15,15};loopSong.insert(loopSong.end(),voice.begin(),voice.end());
    writeFile(root/"loop.mdx",loopSong);
    opts.loops=1;exportRegisterLog(root/"loop.mdx",root/"loop1.mrl",opts);
    opts.loops=3;exportRegisterLog(root/"loop.mdx",root/"loop3.mrl",opts);
    auto l1=loadVgmSong(root/"loop1.mrl"),l3=loadVgmSong(root/"loop3.mrl");
    check(l1.looping && l3.looping && l3.mrl->count>l1.mrl->count,"requested log loops lost");
    check(l1.mrl->loopEnd==l3.mrl->loopEnd && l1.mrl->loopEvent==l3.mrl->loopEvent,"loop region depends on capture count");
    // A sparse ZIP64 fixture places its member above 4 GiB without allocating
    // it.
    auto big = root / "large.zip";
    std::ofstream out(big, std::ios::binary);
    auto put = [&](uint64_t v, unsigned n) {
      while (n--) {
        out.put(char(v));
        v >>= 8;
      }
    };
    uint64_t offset = (uint64_t(1) << 32) + 123;
    out.seekp(std::streamoff(offset));
    std::string name = "test.mdx";
    uint32_t crc = dataCrc(song.data(), song.size());
    put(0x04034b50, 4);
    put(45, 2);
    put(0, 2);
    put(0, 2);
    put(0, 4);
    put(crc, 4);
    put(song.size(), 4);
    put(song.size(), 4);
    put(name.size(), 2);
    put(0, 2);
    out << name;
    out.write(reinterpret_cast<char *>(song.data()), song.size());
    uint64_t central = uint64_t(out.tellp());
    put(0x02014b50, 4);
    put(45, 2);
    put(45, 2);
    put(0, 2);
    put(0, 2);
    put(0, 4);
    put(crc, 4);
    put(song.size(), 4);
    put(song.size(), 4);
    put(name.size(), 2);
    put(12, 2);
    put(0, 2);
    put(0, 2);
    put(0, 2);
    put(0, 4);
    put(0xffffffffu, 4);
    out << name;
    put(1, 2);
    put(8, 2);
    put(offset, 8);
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
    check(std::filesystem::file_size(big) > uint64_t(1) << 32,
          "large fixture size");
    auto dir = resolveArchivePath(big);
    check(std::filesystem::file_size(dir / name) == 0,
          "ZIP eagerly extracted member");
    check(archiveFileSize(dir / name) == song.size(), "ZIP metadata size");
    check(readFile(big / name) == song, "ZIP64 member above 4GiB");
    std::cout << "MRL 1.1 / MRZ / PCM8 / ZIP64 tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
