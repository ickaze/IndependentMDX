#include "gzip_decode.h"
#include "vgm.h"
#include "audio_buffer_order.h"
#include "opm_backend.h"
#include "stereo_output.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
using namespace mdx;
void check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
void word(Bytes &b, size_t p, uint32_t v) {
  for (int n = 0; n < 4; ++n)
    b[p + n] = uint8_t(v >> (8 * n));
}
Bytes file(Bytes commands, bool loop = false, uint32_t clock = 4000000) {
  Bytes b(64);
  b[0] = 'V';
  b[1] = 'g';
  b[2] = 'm';
  b[3] = ' ';
  word(b, 8, 0x171);
  word(b, 0x30, clock);
  word(b, 0x34, 12);
  if (loop)
    word(b, 0x1c, 64 - 0x1c);
  b.insert(b.end(), commands.begin(), commands.end());
  word(b, 4, uint32_t(b.size() - 4));
  return b;
}
Bytes tone(unsigned length = 4410, bool loop = false) {
  Bytes c;
  auto wr = [&](int a, int v) {
    c.insert(c.end(), {0x54, uint8_t(a), uint8_t(v)});
  };
  wr(0x20, 0xc7);
  wr(0x28, 0x48);
  for (int i = 0; i < 4; ++i) {
    wr(0x40 + i * 8, 1);
    wr(0x60 + i * 8, i ? 127 : 20);
    wr(0x80 + i * 8, 31);
    wr(0xa0 + i * 8, 0);
    wr(0xc0 + i * 8, 0);
    wr(0xe0 + i * 8, 15);
  }
  wr(8, 0x78);
  c.insert(c.end(), {0x61, uint8_t(length), uint8_t(length >> 8), 0x66});
  return file(c, loop);
}
std::vector<int16_t> render(Source &s, size_t block = 701,
                            size_t maximum = 48000 * 5) {
  std::vector<int16_t> out;
  std::vector<int16_t> b(block * 2);
  while (out.size() / 2 < maximum) {
    auto n = s.render(b.data(), block);
    if (!n)
      break;
    out.insert(out.end(), b.begin(), b.begin() + n * 2);
  }
  return out;
}
int main() {
  try {
    int count = 0;
    auto f = file({0x54, 0x20, 0xc7, 0x61, 0x34, 0x12, 0x62, 0x63, 0x7f, 0x64,
                   0x62, 10, 0, 0x62, 0x66});
    VgmSong s(f);
    check(s.samples == 0x1234 + 735 + 882 + 16 + 10, "wait timing");
    ++count;
    auto bad = f;
    word(bad, 0x1c, 65 - 0x1c);
    try {
      VgmSong x(bad);
      check(false, "interior loop accepted");
    } catch (const std::runtime_error &e) {
      check(std::string(e.what()).find("loop") != std::string::npos,
            "loop diagnostic");
    }
    ++count;
    bad = f;
    word(bad, 0xc, 3579545);
    try {
      VgmSong x(bad);
      check(false, "other chip accepted");
    } catch (const std::runtime_error &e) {
      check(std::string(e.what()).find("unsupported") != std::string::npos,
            "chip diagnostic");
    }
    ++count;
    auto tagged = f;
    size_t gd = tagged.size();
    word(tagged, 0x14, uint32_t(gd - 0x14));
    tagged.insert(tagged.end(), {'G', 'd', '3', ' ', 0, 1, 0,    0,    0, 0,
                                 0,   0,   'T', 0,   0, 0, 0x42, 0x30, 0, 0});
    word(tagged, gd + 8, 8);
    word(tagged, 4, uint32_t(tagged.size() - 4));
    check(VgmSong(tagged).title == "T", "GD3 title");
    ++count;
    auto root = std::filesystem::path(__FILE__).parent_path() / "fixtures";
    auto plain = readFile(root / "synthetic.vgm");
    for (auto name : {"stored.vgz", "fixed.vgz", "dynamic.vgz"}) {
      auto compressed = readFile(root / name);
      check(unpackVgm(compressed) == plain, "VGZ inflate");
      check(VgmSong(compressed).samples == VgmSong(plain).samples,
            "VGZ parsing");
      compressed.back() ^= 1;
      bool rejected = false;
      try {
        unpackVgm(compressed);
      } catch (...) {
        rejected = true;
      }
      check(rejected, "VGZ checksum");
    }
    ++count;
    Options o;
    o.maxSeconds = 0;
    o.fadeSeconds = .02;
    o.loops = 1;
    o.measuredOutputTiming = false;
    VgmPlayer a(VgmSong(tone(44100)), o), b(VgmSong(tone(44100)), o);
    auto audio = render(a, 1), other = render(b, 1024);
    check(audio == other, "chunk invariant");
    check(std::abs(audio.size() / 2.0 / 48000 - 1.5) < .001, "end duration");
    check(*std::max_element(audio.begin(), audio.end()) > 100,
          "audible signal");
    ++count;
    VgmPlayer seek(VgmSong(tone(44100)), o);
    check(seek.seek(.25), "seek failed");
    std::vector<int16_t> after(2048);
    check(seek.render(after.data(), 1024) == 1024, "seek render");
    for (size_t i = 128; i < after.size(); ++i)
      check(std::abs(int(after[i]) - audio[24000 + i]) <= 1,
            "seek audio mismatch");
    ++count;
    o.monitor = std::make_shared<Monitor>();
    VgmPlayer monitored(VgmSong(tone(44100)), o);
    std::vector<int16_t> block(2048);
    monitored.render(block.data(), 1024);
    check(std::any_of(o.monitor->frame.waveform[0].begin(),
                      o.monitor->frame.waveform[0].end(),
                      [](float x) { return x != 0; }),
          "channel waveform");
    for (int ch = 1; ch < 8; ++ch)
      check(std::all_of(o.monitor->frame.waveform[ch].begin(),
                        o.monitor->frame.waveform[ch].end(),
                        [](float x) { return x == 0; }),
            "channel isolation");
    ++count;
    o.liveLoops = std::make_shared<std::atomic<int>>(0);
    VgmPlayer live(VgmSong(tone(2205, true)), o);
    for (int i = 0; i < 20; ++i)
      check(live.render(block.data(), 1024) == 1024, "infinite stopped");
    o.liveLoops->store(2);
    live.render(block.data(), 1024);
    o.liveLoops->store(0);
    for (int i = 0; i < 10; ++i)
      live.render(block.data(), 1024);
    check(live.render(block.data(), 1024) == 0, "fade switch lock");
    ++count;
    o.liveLoops.reset();
    o.loops = 0;
    VgmPlayer finite(VgmSong(tone()), o);
    double last = 0;
    int wraps = 0;
    for (int i = 0; i < 100; ++i) {
      check(finite.render(block.data(), 1024) > 0, "finite repeat stopped");
      double now = finite.positionSeconds();
      if (now < last)
        ++wraps;
      last = now;
    }
    check(wraps >= 2, "finite repeat");
    ++count;
    o.loops = 1;
    o.externalFm = true;
    VgmPlayer hardware(VgmSong(tone()), o);
    auto silent = render(hardware);
    check(
        std::all_of(silent.begin(), silent.end(), [](int16_t x) { return !x; }),
        "hardware monitor leaked");
    bool rejected = false;
    try {
      VgmPlayer wrong(VgmSong(tone()), o);
      VgmPlayer invalid(VgmSong(file({0x54, 0x20, 0xc7, 0x66}, false, 3579545)),
                        o);
    } catch (...) {
      rejected = true;
    }
    check(rejected, "hardware clock mismatch");
    ++count;
    std::mt19937 random(0x56474d);
    for (int i = 0; i < 1000; ++i) {
      auto mutation = f;
      mutation[random() % mutation.size()] = uint8_t(random());
      try {
        VgmSong x(mutation);
      } catch (const std::exception &) {
      }
    }
    ++count;
    auto compressed = readFile(root / "dynamic.vgz");
    for (int i = 0; i < 200; ++i) {
      auto mutation = compressed;
      mutation[random() % mutation.size()] = uint8_t(random());
      try {
        unpackVgm(std::move(mutation));
      } catch (const std::exception &) {
      }
    }
    ++count;
    {
      // A dense stream beyond the former host limit must retain every write.
      constexpr size_t writes = 4000001;
      Bytes commands;
      commands.reserve(writes * 3 + 4);
      for (size_t i = 0; i < writes; ++i)
        commands.insert(commands.end(), {0x54, 0x60, uint8_t(i & 127)});
      commands.insert(commands.end(), {0x61, 1, 0, 0x66});
      VgmSong large(file(std::move(commands)));
      check(sizeof(VgmSong::Event) == 8, "compact register event");
      check(large.events.size() == writes, "large stream was truncated");
      check(large.events.back().value == ((writes - 1) & 127), "last write lost");
      check(large.samples == 1, "large stream timing");
    }
    ++count;
    {
      size_t next=5;
      std::array<bool,6> busy{false,false,true,true,true,false};
      std::vector<size_t> submitted;
      refillAudioBuffers(next,6,[&](size_t i){
        if(busy[i]) return false;
        busy[i]=true; submitted.push_back(i); return true;
      });
      check(submitted==std::vector<size_t>({5,0,1}), "audio ring wrap reordered");
      check(next==2,"audio ring cursor");
      busy.fill(false); submitted.clear();
      refillAudioBuffers(next,6,[&](size_t i){submitted.push_back(i);return true;});
      check(submitted==std::vector<size_t>({2,3,4,5,0,1}),"audio ring stall recovery");
      next=0; submitted.clear();
      refillAudioBuffers(next,6,[&](size_t i){submitted.push_back(i);return true;});
      check(submitted==std::vector<size_t>({0,1,2,3,4,5}),"audio ring reset");
    }
    ++count;
    {
      auto bytes=tone(1); bytes.pop_back();
      for(unsigned i=0;i<2000;++i)
        bytes.insert(bytes.end(),{0x54,0x60,uint8_t(10+i%60),0x70});
      bytes.push_back(0x66); word(bytes,4,uint32_t(bytes.size()-4));
      VgmSong score(bytes); Options opt; opt.gain=opt.independentGain=1;
      opt.measuredOutputTiming=false; opt.dacRightDelay=false;
      VgmPlayer host(score,opt);
      auto &api=OpmApi::get(); iopm_handle raw=nullptr;
      OpmApi::check(api.create(4000000,62500,&raw));
      OpmApi::check(api.set_measured_output_timing(raw,0));
      Clock64Output output; size_t event=0; uint64_t clock=0;
      std::vector<iopm_event> writes;
      std::array<int16_t,2000> actual{},expected{};
      host.render(actual.data(),1000);
      output.render(expected.data(),1000,48000,false,[&](AudioStem &stem){
        writes.clear();
        while(event<score.events.size()) {
          auto ev=score.events[event];
          uint64_t at=(uint64_t(ev.time)*4000000+22050)/44100;
          if(at>clock+64)break;
          unsigned value=ev.value;
          writes.push_back({uint32_t(at-clock),ev.address,value});++event;
        }
        float mix[2],channels[16];
        OpmApi::check(api.render_events_channels_f32(raw,mix,channels,1,writes.data(),uint32_t(writes.size())));
        clock+=64;stem.fm={double(mix[0])*32767,double(mix[1])*32767};stem.pcm={};return true;
      });
      api.destroy(raw);
      check(actual==expected,"VGM sub-frame event clock mismatch");
    }
    ++count;
    {
      // KON bit order differs from parameter-register slot order.
      // Test each isolated carrier through immediate and timestamped writes.
      constexpr unsigned keyBits[] = {3, 5, 4, 6};
      for (bool timed : {false, true}) {
        for (unsigned slot = 0; slot < 4; ++slot) {
          OpmBackend core(true, true);
          if (timed) core.begin_timed_frame();
          core.write_register(0x20, 0xc7);
          core.write_register(0x28, 0x48);
          for (unsigned op = 0; op < 4; ++op) {
            core.write_register(uint8_t(0x40 + op * 8), 1);
            core.write_register(uint8_t(0x60 + op * 8), op == slot ? 20 : 127);
            core.write_register(uint8_t(0x80 + op * 8), op == slot ? 31 : 0);
            core.write_register(uint8_t(0xe0 + op * 8), 15);
          }
          core.write_register(8, uint8_t(1u << keyBits[slot]));
          double peak = 0;
          for (int frame = 0; frame < 1024; ++frame) {
            core.advance(64);
            peak = std::max(peak, std::abs(core.last_sample().left));
          }
          check(peak > .001, "isolated KON carrier was routed to the wrong operator");
        }
      }
    }
    ++count;
    std::cout << "PASS " << count << " VGM test groups\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
  }
}
