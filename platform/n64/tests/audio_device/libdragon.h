#pragma once
#include <stddef.h>
#include <stdint.h>
#define AUDIO_DEFAULT_LATENCY 1.0f
#define TICKS_READ() 0u
using callback_t=void(*)(short*,size_t);
void audio_init(int,float);
int audio_get_frequency();
int audio_get_buffer_length();
void audio_set_buffer_callback(callback_t);
void audio_pause(bool);
void audio_write_silence();
