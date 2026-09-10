#include "audio_playback.h"
#include "audio_capture.h"
#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <list>
#include <mutex>
#include <algorithm>

namespace Chimera {
    namespace {
        struct PlaybackState {
            HWAVEOUT wave = nullptr;
            std::mutex mutex;
            float volume = 1.5f;
            struct Buffer { WAVEHDR header{}; std::vector<int16_t> samples; };
            std::list<Buffer> buffers;
        };

        PlaybackState &playback_state() {
            static PlaybackState state;
            return state;
        }

        void apply_voice_volume(std::vector<int16_t> &samples) noexcept {
            auto &state = playback_state();
            for(auto &sample : samples) {
                const float amplified = static_cast<float>(sample) * state.volume;
                if(amplified > 32767.0f) sample = 32767;
                else if(amplified < -32768.0f) sample = -32768;
                else sample = static_cast<int16_t>(amplified);
            }
        }
    }

    bool initialize_voice_audio_playback() noexcept {
        auto &state = playback_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        if(state.wave) return true;
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = static_cast<WORD>(VOICE_AUDIO_CHANNELS);
        format.nSamplesPerSec = VOICE_AUDIO_SAMPLE_RATE;
        format.wBitsPerSample = 16;
        format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        return waveOutOpen(&state.wave, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR;
    }

    void shutdown_voice_audio_playback() noexcept {
        auto &state = playback_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        if(!state.wave) return;
        waveOutReset(state.wave);
        for(auto &buffer : state.buffers) if(buffer.header.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(state.wave, &buffer.header, sizeof(buffer.header));
        state.buffers.clear();
        waveOutClose(state.wave);
        state.wave = nullptr;
    }

    bool voice_audio_playback_initialized() noexcept { return playback_state().wave != nullptr; }

    bool queue_voice_audio_playback(const int16_t *pcm, size_t samples) noexcept {
        if(!pcm || samples == 0 || !initialize_voice_audio_playback()) return false;
        auto &state = playback_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        if(state.buffers.size() >= 32) {
            for(auto it = state.buffers.begin(); it != state.buffers.end(); ++it) {
                if(it->header.dwFlags & WHDR_DONE) {
                    waveOutUnprepareHeader(state.wave, &it->header, sizeof(it->header));
                    state.buffers.erase(it);
                    break;
                }
            }
            if(state.buffers.size() >= 32) return false;
        }
        state.buffers.emplace_back();
        auto it = state.buffers.end(); --it;
        auto &buffer = *it;
        buffer.samples.assign(pcm, pcm + samples);
        apply_voice_volume(buffer.samples);
        buffer.header.lpData = reinterpret_cast<LPSTR>(buffer.samples.data());
        buffer.header.dwBufferLength = static_cast<DWORD>(buffer.samples.size() * sizeof(int16_t));
        if(waveOutPrepareHeader(state.wave, &buffer.header, sizeof(buffer.header)) != MMSYSERR_NOERROR) { state.buffers.erase(it); return false; }
        if(waveOutWrite(state.wave, &buffer.header, sizeof(buffer.header)) != MMSYSERR_NOERROR) {
            waveOutUnprepareHeader(state.wave, &buffer.header, sizeof(buffer.header));
            state.buffers.erase(it);
            return false;
        }
        return true;
    }

    float get_voice_audio_playback_volume() noexcept {
        auto &state = playback_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        return state.volume;
    }

    void set_voice_audio_playback_volume(float volume) noexcept {
        auto &state = playback_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.volume = std::max(0.0f, std::min(volume, 4.0f));
    }
}