// SPDX-License-Identifier: 0BSD
// Copyright (c) 2026 IndependentMDX contributors

#pragma once
#include "mrl.h"
#include "player.h"
namespace mdx {
bool isVgm(const std::filesystem::path &path);
struct VgmSong {
  struct Event {
    // The parser caps score time at 24 hours (< 2^32 samples at 44.1 kHz).
    uint32_t time;
    uint16_t address, value;
  };
  std::vector<Event> events;
  std::vector<double>
      preciseTimes; // MRL times in 44.1 kHz units, without integer rounding.
  bool registerLog = false;
  std::shared_ptr<MrlHeader> mrl;
  std::shared_ptr<Pdx> pdx;
  std::string pcmWarning;
  std::string title;
  uint32_t clock = 0;
  double volume = 1;
  uint64_t samples = 0;
  double loopSample = 0;
  size_t loopEvent = 0;
  bool looping = false, pwm = false;
  uint64_t logLoopEndEvent = 0;
  double logLoopEnd = 0;
  void setLogLoop(const MrlHeader &);
  VgmSong() = default;
  explicit VgmSong(Bytes data);
};
VgmSong loadVgmSong(const std::filesystem::path &path,
                    const std::filesystem::path &pdxOverride = {});
std::string readVgmTitle(const std::filesystem::path &path);
class VgmPlayer final : public Source {
  struct Impl;
  std::unique_ptr<Impl> p;

public:
  VgmPlayer(VgmSong song, Options options = {});
  ~VgmPlayer();
  size_t render(int16_t *, size_t) override;
  double seconds() const override;
  double positionSeconds() const override;
  double duration() const override;
  bool seek(double, const std::function<bool()> &cancelled = {}) override;
  void syncRegisters() override;
};
} // namespace mdx
