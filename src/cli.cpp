#include "player.h"
#include "win_audio.h"
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <conio.h>
#endif
static volatile std::sig_atomic_t stopped = 0;
static void interrupt(int) { stopped = 1; }
int run(const std::vector<std::string> &args) {
    try {
        mdx::Options options;
        std::filesystem::path input, output, pdxPath;
        int sample = -1, format = 4;
        bool info = false, explicitTimeLimit = false;
        for (size_t i = 1; i < args.size(); ++i) {
            auto value = [&]() -> std::string {
                if (++i >= args.size())
                    throw std::runtime_error("Missing option value");
                return args[i];
            };
            std::string arg = args[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "IndependentMDX 0.24\n"
                             "independent-mdx song.mdx [--pdx bank.pdx] [--wav output.wav]\n"
                             "independent-mdx bank.pdx --sample 0 [--format 4] [--wav output.wav]\n"
                             "Options: --loops 2 (0=infinite) --key 0 (-24..24) --key-mode 1 (0=FM,1=PCM "
                             "rate,2=PCM pitch) --seconds 600 --fade 3 --gain 0.40 "
                             "--pcm-gain 1.0 --independent-gain 2.0 --fm-mask 0 --pcm-mask 0 --dac-delay "
                             "--measured-output-timing / --no-measured-output-timing --rate 48000 --info\n"
                             "F0..F4=OKI ADPCM; F5=big-endian signed P16; F6=signed P8.\n"
                             "Windows playback: Space=pause/resume, Esc=stop. Ctrl+C=stop.\n"
                             "Uses Independent OPM 0.11b; see THIRD_PARTY_NOTICES.txt.\n";
                return 0;
            } else if (arg == "--pdx")
                pdxPath = std::filesystem::u8path(value());
            else if (arg == "--wav")
                output = std::filesystem::u8path(value());
            else if (arg == "--loops")
                options.loops = std::stoi(value());
            else if (arg == "--key-mode")
                options.keyMode = std::stoi(value());
            else if (arg == "--key")
                options.keyShift = std::stoi(value());
            else if (arg == "--seconds") {
                options.maxSeconds = std::stod(value());
                explicitTimeLimit = true;
            } else if (arg == "--fade")
                options.fadeSeconds = std::stod(value());
            else if (arg == "--gain")
                options.gain = std::stod(value());
            else if (arg == "--pcm-gain")
                options.pcmGain = std::stod(value());
            else if (arg == "--independent-gain")
                options.independentGain = std::stod(value());
            else if (arg == "--pcm-mask")
                options.pcmMuteMask = static_cast<uint32_t>(std::stoul(value()));
            else if (arg == "--fm-mask")
                options.fmMuteMask = static_cast<uint32_t>(std::stoul(value()));
            else if (arg == "--no-measured-output-timing")
                options.measuredOutputTiming = false;
            else if (arg == "--measured-output-timing")
                options.measuredOutputTiming = true;
            else if (arg == "--dac-delay")
                options.dacRightDelay = true;
            else if (arg == "--rate")
                options.sampleRate = std::stoi(value());
            else if (arg == "--sample")
                sample = std::stoi(value());
            else if (arg == "--format")
                format = std::stoi(value());
            else if (arg == "--info")
                info = true;
            else if (arg.rfind("--", 0) == 0)
                throw std::runtime_error("Unknown option: " + arg);
            else if (input.empty())
                input = std::filesystem::u8path(arg);
            else
                throw std::runtime_error("Only one input file is accepted");
        }
        if (input.empty())
            throw std::runtime_error("Specify an MDX file; use --help for usage");
        if (!output.empty()) {
            auto target = std::filesystem::weakly_canonical(output);
            if (target == std::filesystem::weakly_canonical(input) ||
                (!pdxPath.empty() && target == std::filesystem::weakly_canonical(pdxPath)))
                throw std::runtime_error("Output must not overwrite input data");
            if (std::filesystem::exists(output))
                throw std::runtime_error("Output already exists; choose another WAV filename");
        }
        if (options.loops == 0 && !explicitTimeLimit)
            options.maxSeconds = 0;
        options.diagnostic = [](const std::string &text) { std::cerr << text << "\n"; };
        std::unique_ptr<mdx::Source> source;
        mdx::Player *music = nullptr;
        std::string ext = input.extension().u8string();
        for (char &c : ext)
            if (c >= 'A' && c <= 'Z')
                c += 32;
        if (ext == ".pdx" || sample >= 0) {
            mdx::Pdx pdx(mdx::readFile(input));
            std::cout << "PDX: " << pdx.entries.size() << " slots\n";
            if (info) {
                for (size_t i = 0; i < pdx.entries.size(); ++i)
                    if (pdx.entries[i].length)
                        std::cout << "  " << i << " (bank " << i / 96 << ", note " << i % 96
                                  << "): " << pdx.entries[i].length << " bytes\n";
                return 0;
            }
            if (sample < 0)
                throw std::runtime_error("PDX is a sample bank. Specify --sample N or --info");
            source = std::make_unique<mdx::SamplePlayer>(pdx, sample, format, options);
        } else {
            mdx::Song song(mdx::readFile(input));
            std::cout << song.title << "\nTracks: " << song.tracks.size()
                      << "  PDX: " << mdx::utf8(song.pdxName) << "\n";
            if (info)
                return 0;
            std::shared_ptr<mdx::Pdx> pdx;
            try {
                pdx = mdx::locatePdx(song, input, pdxPath);
            } catch (const std::exception &e) {
                std::cerr << "ERROR: " << e.what() << "\n";
            }
            if (pdx)
                std::cout << "Loaded PDX: " << pdx->sourcePath.u8string() << "\n";
            auto player = std::make_unique<mdx::Player>(std::move(song), pdx, options);
            music = player.get();
            source = std::move(player);
        }
        std::signal(SIGINT, interrupt);
        if (!output.empty()) {
            mdx::writeWav(*source, output, options.sampleRate, [] { return stopped != 0; });
            std::cout << "WAV: " << output.u8string() << "\n";
        } else {
#ifdef _WIN32
            bool paused = false;
            mdx::playWindows(
                *source, options.sampleRate, [] { return stopped != 0; },
                [&] {
                    if (_kbhit()) {
                        int key = _getch();
                        if (key == 32)
                            paused = !paused;
                        if (key == 27)
                            stopped = 1;
                    }
                    return paused;
                });
#else
            throw std::runtime_error(
                "Direct playback is Windows-only. Use --wav output.wav on this platform.");
#endif
        }
        if (music)
            for (auto &warning : music->warnings())
                std::cerr << "Compatibility note: " << warning << "\n";
        std::cout << "Duration: " << source->seconds() << " s\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i)
        args.push_back(std::filesystem::path(argv[i]).u8string());
    return run(args);
}
#else
int main(int argc, char **argv) { return run(std::vector<std::string>(argv, argv + argc)); }
#endif
