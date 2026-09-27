#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace Chimera {
    constexpr size_t VOICE_OPUS_FRAME_SAMPLES = 960;
    constexpr size_t VOICE_OPUS_MAX_PACKET_BYTES = 1275;

    bool initialize_voice_codec() noexcept;
    void shutdown_voice_codec() noexcept;
    bool voice_codec_initialized() noexcept;
    bool encode_voice_audio_packet(const int16_t *pcm, size_t samples, std::vector<uint8_t> &packet) noexcept;
    bool decode_voice_audio_packet(const uint8_t *packet, size_t packet_size, std::vector<int16_t> &pcm) noexcept;
}
