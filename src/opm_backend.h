#pragma once
#include "../monitor_core/ym2151.hpp"
#include "independent_opm.h"
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "x68_bridge.h"
#include <filesystem>
#include <windows.h>
#endif
namespace mdx {
// Windows calls the supplied C ABI DLL. Non-Windows tests link the identical C API.
struct OpmApi {
#ifdef _WIN32
    HMODULE module = nullptr;
#endif
#define OPM_PROC(name) decltype(&iopm_##name) name = nullptr
    OPM_PROC(abi_version);
    OPM_PROC(create);
    OPM_PROC(destroy);
    OPM_PROC(state_size);
    OPM_PROC(save_state);
    OPM_PROC(load_state);
    OPM_PROC(write_register);
    OPM_PROC(render_f32);
    OPM_PROC(set_measured_output_timing);
#undef OPM_PROC
    OpmApi() {
#ifdef _WIN32
        wchar_t path[32768];
        GetModuleFileNameW(nullptr, path, 32768);
        auto file = std::filesystem::path(path).parent_path() / L"independent_opm.dll";
        module = LoadLibraryExW(file.c_str(), nullptr,
                                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module)
            throw std::runtime_error("Cannot load independent_opm.dll beside the executable (x64 required)");
#define OPM_LOAD(name)                                                                                       \
    name = reinterpret_cast<decltype(name)>(GetProcAddress(module, "iopm_" #name));                          \
    if (!name) {                                                                                             \
        FreeLibrary(module);                                                                                 \
        module = nullptr;                                                                                    \
        throw std::runtime_error("Missing DLL export: iopm_" #name);                                         \
    }
#else
#define OPM_LOAD(name) name = &iopm_##name
#endif
        OPM_LOAD(abi_version);
        OPM_LOAD(create);
        OPM_LOAD(destroy);
        OPM_LOAD(state_size);
        OPM_LOAD(save_state);
        OPM_LOAD(load_state);
        OPM_LOAD(write_register);
        OPM_LOAD(render_f32);
        OPM_LOAD(set_measured_output_timing);
#undef OPM_LOAD
        if (abi_version() != IOPM_ABI_VERSION) {
#ifdef _WIN32
            FreeLibrary(module);
            module = nullptr;
#endif
            throw std::runtime_error("Unsupported Independent OPM DLL ABI");
        }
    }
    ~OpmApi() {
#ifdef _WIN32
        if (module)
            FreeLibrary(module);
#endif
    }
    static OpmApi &get() {
        static OpmApi api;
        return api;
    }
    static void check(int result) {
        if (result != IOPM_OK)
            throw std::runtime_error("Independent OPM DLL call failed: " + std::to_string(result));
    }
};
class OpmBackend {
    OpmApi *api = nullptr;
#ifdef _WIN32
    std::unique_ptr<X68Bridge> x68;
#endif
    iopm_handle handle = nullptr;
    std::array<float, 2> samples{};
    std::array<unsigned, 8> keys{};
    bool monitorEnabled = true, monitorTiming = false;
    std::array<uint8_t,256> monitorRegisters{};
    uint8_t monitorAmd=0, monitorPmd=0;
    std::unique_ptr<independent_opm_monitor::Ym2151> monitor;

  public:
    struct Frame {
        double left, right;
    };
    struct State {
        bool key;
    };
    explicit OpmBackend(bool capture = false, bool sound = true, int backend = 0) {
        if (capture)
            monitor = std::make_unique<independent_opm_monitor::Ym2151>(4000000);
        if (sound) {
#ifdef _WIN32
            if (backend == 1) {
                x68 = std::make_unique<X68Bridge>();
                return;
            }
#endif
            api = &OpmApi::get();
            OpmApi::check(api->create(4000000, 62500, &handle));
        }
    }
    ~OpmBackend() {
        if (handle)
            api->destroy(handle);
    }
    OpmBackend(const OpmBackend &) = delete;
    OpmBackend &operator=(const OpmBackend &) = delete;
    void write_register(uint8_t a, uint8_t v) {
        if(monitor) {
            monitorRegisters[a]=v;
            if(a==0x19) { if(v&128) monitorPmd=v; else monitorAmd=v; }
        }
#ifdef _WIN32
        if (x68)
            x68->write(a, v);
#endif
        if (a == 8)
            keys[v & 7] = (v >> 3) & 15;
        if (handle)
            OpmApi::check(api->write_register(handle, a, v));
        if (monitor)
            monitor->write_register(a, v);
    }
    void set_measured_output_timing(bool on) {
        monitorTiming=on;
        if (handle)
            OpmApi::check(api->set_measured_output_timing(handle, on));
        if (monitor)
            monitor->set_measured_output_timing(on);
    }
    void set_monitor_enabled(bool enabled) {
        if(enabled && !monitorEnabled && monitor) {
            // Restart only the display model from current registers/held keys.
            // Do not show an envelope frozen at the time the view was disabled.
            monitor->reset();
            monitor->set_measured_output_timing(monitorTiming);
            for(unsigned a=0;a<256;++a) if(a!=8 && a!=0x19)
                monitor->write_register(uint8_t(a),monitorRegisters[a]);
            monitor->write_register(0x19,monitorAmd&127);
            monitor->write_register(0x19,monitorPmd|128);
            for(unsigned ch=0;ch<8;++ch) monitor->write_register(8,uint8_t(ch|(keys[ch]<<3)));
        }
        monitorEnabled=enabled;
    }
    void advance(unsigned clocks) {
#ifdef _WIN32
        if (x68)
            samples = x68->advance();
#endif
        if (handle)
            OpmApi::check(api->render_f32(handle, samples.data(), 1));
        if (monitor && monitorEnabled)
            monitor->advance(clocks);
    }
    Frame last_sample() const { return {samples[0], samples[1]}; }
    Frame channel_sample(unsigned ch) const {
        if (!monitor || !monitorEnabled)
            return {};
        auto s = monitor->channel_sample(ch);
        return {s.left, s.right};
    }
    State inspect(unsigned ch, unsigned op) const {
        constexpr unsigned bit[] = {0, 2, 1, 3};
        return {bool(keys[ch] & (1u << bit[op]))};
    }
};
} // namespace mdx
