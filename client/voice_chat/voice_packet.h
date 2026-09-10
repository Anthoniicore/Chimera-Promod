#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace Chimera {
    constexpr uint32_t VOICE_PACKET_MAGIC = 0x56434831u;
    constexpr uint8_t VOICE_PACKET_VERSION = 2;
    constexpr size_t VOICE_PACKET_HEADER_SIZE = 24;
    constexpr uint8_t VOICE_PACKET_FLAG_KEEPALIVE = 0x01;
    // The sender's transmission scope. TEAM is represented by no scope bit;
    // ALL is explicitly marked so receivers can make the decision from the
    // packet itself instead of from their own local voice setting.
    constexpr uint8_t VOICE_PACKET_FLAG_ALL = 0x02;

    struct VoicePacketHeader {
        uint32_t magic;
        uint8_t version;
        uint8_t flags;
        uint16_t payload_size;
        uint32_t room_id;
        uint32_t sender_id;
        uint32_t sequence;
        uint32_t timestamp;
    };

    bool build_voice_packet(uint32_t room_id, uint32_t sender_id, uint32_t sequence, uint32_t timestamp, uint8_t flags, const uint8_t *payload, size_t payload_size, std::vector<uint8_t> &packet) noexcept;
    bool build_voice_keepalive_packet(uint32_t room_id, uint32_t sender_id, uint8_t flags, std::vector<uint8_t> &packet) noexcept;
    bool parse_voice_packet(const uint8_t *packet, size_t packet_size, VoicePacketHeader &header, const uint8_t *&payload) noexcept;
}
