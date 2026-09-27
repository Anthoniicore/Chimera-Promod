#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace Chimera {
    constexpr uint32_t VOICE_AUDIO_SAMPLE_RATE = 48000;
    constexpr uint16_t VOICE_AUDIO_CHANNELS = 1;
    constexpr uint32_t VOICE_AUDIO_PACKET_DURATION_MS = 20;
    constexpr size_t VOICE_AUDIO_PACKET_SAMPLES = static_cast<size_t>(VOICE_AUDIO_SAMPLE_RATE) * VOICE_AUDIO_PACKET_DURATION_MS / 1000;

    bool start_voice_audio_capture() noexcept;
    void stop_voice_audio_capture() noexcept;
    bool voice_audio_capture_running() noexcept;
    size_t voice_audio_buffered_samples() noexcept;
    std::vector<int16_t> consume_voice_audio_samples(size_t maximum_samples);
    bool consume_voice_audio_packet(std::vector<int16_t> &packet);
}
