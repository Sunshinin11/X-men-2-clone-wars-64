#pragma once
#include <stdint.h>
#include <string.h>
#include "audio_commands.hpp"
#include "sound_core.hpp"
#ifdef N64
#include <libdragon.h>
#else
#include <stdio.h>
#endif
#ifndef X2_AUDIO_OPT
#define X2_AUDIO_OPT 1
#endif
#ifndef X2_PACE_SLICE_OPT
#define X2_PACE_SLICE_OPT 1
#endif
#define X2_AUDIO_SAMPLE_RATE 12000

class X2AudioSystem {
    bool pumping = false;
    unsigned mode = 0;
public:
    X2SoundCommands commands;
    void init(unsigned m = 3) {
        mode = m;
        unsigned rate = X2_AUDIO_SAMPLE_RATE;
#ifdef N64
        if (mode) {
            audio_init(rate, AUDIO_DEFAULT_LATENCY);
            rate = audio_get_frequency();
        }
#endif
        if (mode == 2 || mode == 3) x2_sound_init(rate);
#if defined(N64) || defined(X2_NATIVE_SAMPLE_AUDIO)
        if (mode == 3) {
            extern void x2_sound_command(uint8_t, const uint8_t*, unsigned);
            commands.sink = [](void*, uint8_t c, const uint8_t* a, unsigned n) {
                x2_sound_command(c, a, n);
            };
        }
#endif
    }
    void process_ring(const uint8_t* z, uint8_t index) {
        commands.process(z, index);
    }
    bool service_if_below(int max_queued, int max_samples = 0) {
        if ((mode != 2 && mode != 3) || pumping) return false;
        pumping = true;
#ifdef N64
        if (mode == 3) {
            extern void x2_sound_pump();
            extern bool x2_sound_pump_if_below(int);
            extern bool x2_sound_pump_sliced(int, int);
            bool did = true;
            if (max_samples > 0) did = x2_sound_pump_sliced(max_queued, max_samples);
            else if (max_queued < 0) x2_sound_pump();
            else did = x2_sound_pump_if_below(max_queued);
            pumping = false;
            return did;
        }
#endif
        pumping = false;
        return true;
    }
    void service() { (void)service_if_below(-1); }
    void render_and_submit() { service(); }
    void render_and_submit_emergency() {
#if X2_AUDIO_OPT && X2_PACE_SLICE_OPT
        (void)service_if_below(0);
#elif X2_AUDIO_OPT
        (void)service_if_below(1);
#else
        service();
#endif
    }
};
extern X2AudioSystem g_x2_audio;
