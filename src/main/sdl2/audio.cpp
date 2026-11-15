#include <cstdio>
#include <cstring>
#include <SDL.h>

#include "sdl2/audio.hpp"
#include "../../ps3/ps3_platform.hpp"
#include "frontend/config.hpp"
#include "engine/audio/osoundint.hpp"

#ifdef COMPILE_SOUND_CODE

static inline int16_t clamp_s16(int32_t v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return static_cast<int16_t>(v);
}

Audio::Audio()
{
    sound_enabled = false;
    mix_buffer = nullptr;
    mix_cap = 0;
    wavfile.loaded = 0;
    wavfile.data = nullptr;
    wavfile.pos = 0;
    wavfile.length = 0;
    gap_est = 0;
    avg_gap = 0.0;
    dev = 0;
}

Audio::~Audio()
{
    clear_wav();
    if (mix_buffer) {
        delete[] mix_buffer;
        mix_buffer = nullptr;
    }
    mix_cap = 0;
}

void Audio::init()
{
    config.sound.rate = PS3_AUDIO_RATE;
    clear_buffers();
    ps3_audio_clear_ring();

    if (!config.sound.enabled) {
        sound_enabled = false;
        ps3_audio_pause(1);
        return;
    }

    uint32_t want = osoundint.pcm ? osoundint.pcm->buffer_size : ((PS3_AUDIO_RATE / 30u) * CHANNELS);
    if (want < 3200u) want = 3200u;
    if (!mix_buffer || want > mix_cap) {
        delete[] mix_buffer;
        mix_buffer = new int16_t[want];
        mix_cap = want;
    }

    sound_enabled = true;
    ps3_audio_pause(1);
}

void Audio::start_audio()
{
    if (!config.sound.enabled) {
        stop_audio();
        return;
    }
    sound_enabled = true;
    ps3_audio_pause(0);
}

void Audio::stop_audio()
{
    sound_enabled = false;
    ps3_audio_pause(1);
    ps3_audio_clear_ring();
}

void Audio::pause_audio()
{
    ps3_audio_pause(1);
}

void Audio::resume_audio()
{
    if (sound_enabled && config.sound.enabled)
        ps3_audio_pause(0);
}

void Audio::clear_buffers()
{
    gap_est = 0;
    avg_gap = 0.0;
    wavfile.pos = 0;
}

double Audio::adjust_speed()
{
    return 1.0;
}

void Audio::tick()
{
    if (!sound_enabled || !osoundint.pcm || !osoundint.ym) return;

    osoundint.pcm->stream_update();
    osoundint.ym->stream_update();

    uint32_t pcm_samples = osoundint.pcm->buffer_size;
    uint32_t ym_samples = osoundint.ym->buffer_size;
    uint32_t samples_total = (pcm_samples < ym_samples) ? pcm_samples : ym_samples;
    samples_total &= ~1u;
    if (samples_total == 0) return;

    if (!mix_buffer || samples_total > mix_cap) {
        delete[] mix_buffer;
        mix_buffer = new int16_t[samples_total];
        mix_cap = samples_total;
    }

    int16_t* pcm_buf = osoundint.pcm->get_buffer();
    int16_t* ym_buf = osoundint.ym->get_buffer();
    if (!pcm_buf || !ym_buf) return;

    if (wavfile.loaded && wavfile.data && wavfile.length >= 2) {
        uint32_t total_samples = wavfile.length / 2;
        for (uint32_t i = 0; i < samples_total; i++) {
            int32_t w = wavfile.data[wavfile.pos++];
            if (wavfile.pos >= total_samples) wavfile.pos = 0;
            int32_t mixed = static_cast<int32_t>(pcm_buf[i]) + static_cast<int32_t>(ym_buf[i]) + w;
            mix_buffer[i] = clamp_s16(mixed);
        }
    } else {
        for (uint32_t i = 0; i < samples_total; i++) {
            int32_t mixed = static_cast<int32_t>(pcm_buf[i]) + static_cast<int32_t>(ym_buf[i]);
            mix_buffer[i] = clamp_s16(mixed);
        }
    }

    ps3_audio_push_frames(mix_buffer, samples_total >> 1);
}

void Audio::load_wav(const char* filename)
{
    clear_wav();
    SDL_AudioSpec wave;
    uint8_t* data = nullptr;
    uint32_t length = 0;

    if (SDL_LoadWAV(filename, &wave, &data, &length) == nullptr) {
        fprintf(stderr, "Could not load WAV: %s\n", filename);
        return;
    }

    int16_t* data_vol = new int16_t[length / 2];
    int16_t* src_s16 = reinterpret_cast<int16_t*>(data);
    for (uint32_t i = 0; i < length / 2; i++) {
        data_vol[i] = src_s16[i];
    }

    if (wave.format != AUDIO_S16SYS || wave.channels != 2 || wave.freq != PS3_AUDIO_RATE) {
        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt, wave.format, wave.channels, wave.freq,
                              AUDIO_S16SYS, CHANNELS, PS3_AUDIO_RATE) >= 0) {
            cvt.buf = static_cast<Uint8*>(malloc(length * static_cast<uint32_t>(cvt.len_mult)));
            if (cvt.buf) {
                cvt.len = static_cast<int>(length);
                memcpy(cvt.buf, data, length);
                SDL_FreeWAV(data);
                if (SDL_ConvertAudio(&cvt) == 0) {
                    int16_t* conv_s16 = reinterpret_cast<int16_t*>(cvt.buf);
                    int conv_samples = cvt.len_cvt / 2;
                    for (int i = 0; i < conv_samples; i++) {
                        conv_s16[i] = static_cast<int16_t>(conv_s16[i] >> 1);
                    }
                    wavfile.data = conv_s16;
                    wavfile.pos = 0;
                    wavfile.length = static_cast<uint32_t>(cvt.len_cvt);
                    wavfile.loaded = 1;
                    delete[] data_vol;
                    return;
                }
                free(cvt.buf);
            } else {
                SDL_FreeWAV(data);
            }
        } else {
            SDL_FreeWAV(data);
        }
        delete[] data_vol;
        return;
    }

    SDL_FreeWAV(data);
    for (uint32_t i = 0; i < length / 2; i++) {
        data_vol[i] = static_cast<int16_t>(data_vol[i] >> 1);
    }
    wavfile.data = reinterpret_cast<int16_t*>(malloc(length));
    if (wavfile.data) {
        memcpy(wavfile.data, data_vol, length);
        wavfile.pos = 0;
        wavfile.length = length;
        wavfile.loaded = 1;
    }
    delete[] data_vol;
}

void Audio::clear_wav()
{
    if (wavfile.loaded && wavfile.data) {
        free(wavfile.data);
    }
    wavfile.loaded = 0;
    wavfile.data = nullptr;
    wavfile.pos = 0;
    wavfile.length = 0;
}

#endif
