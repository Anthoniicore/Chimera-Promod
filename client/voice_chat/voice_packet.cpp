#include "voice_packet.h"
#include "voice_codec.h"

namespace Chimera {
    namespace {
        void write_u16(std::vector<uint8_t> &out, uint16_t value) { out.push_back(static_cast<uint8_t>(value)); out.push_back(static_cast<uint8_t>(value >> 8)); }
        void write_u32(std::vector<uint8_t> &out, uint32_t value) { for(unsigned i=0;i<4;++i) out.push_back(static_cast<uint8_t>(value >> (8*i))); }
        uint16_t read_u16(const uint8_t *in) { return static_cast<uint16_t>(in[0]) | static_cast<uint16_t>(in[1] << 8); }
        uint32_t read_u32(const uint8_t *in) { return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) | (static_cast<uint32_t>(in[2]) << 16) | (static_cast<uint32_t>(in[3]) << 24); }
    }

    bool build_voice_packet(uint32_t room_id, uint32_t sender_id, uint32_t sequence, uint32_t timestamp, const uint8_t *payload, size_t payload_size, std::vector<uint8_t> &packet) noexcept {
        packet.clear();
        if(!payload || payload_size == 0 || payload_size > VOICE_OPUS_MAX_PACKET_BYTES || payload_size > 0xFFFFu) return false;
        packet.reserve(VOICE_PACKET_HEADER_SIZE + payload_size);
        write_u32(packet, VOICE_PACKET_MAGIC); packet.push_back(VOICE_PACKET_VERSION); packet.push_back(0); write_u16(packet, static_cast<uint16_t>(payload_size));
        write_u32(packet, room_id); write_u32(packet, sender_id); write_u32(packet, sequence); write_u32(packet, timestamp);
        packet.insert(packet.end(), payload, payload + payload_size); return true;
    }

    bool build_voice_keepalive_packet(uint32_t room_id, uint32_t sender_id, std::vector<uint8_t> &packet) noexcept {
        packet.clear(); packet.reserve(VOICE_PACKET_HEADER_SIZE + 1);
        write_u32(packet, VOICE_PACKET_MAGIC); packet.push_back(VOICE_PACKET_VERSION); packet.push_back(VOICE_PACKET_FLAG_KEEPALIVE); write_u16(packet, 1);
        write_u32(packet, room_id); write_u32(packet, sender_id); write_u32(packet, 0); write_u32(packet, 0); packet.push_back(0); return true;
    }

    bool parse_voice_packet(const uint8_t *packet, size_t packet_size, VoicePacketHeader &header, const uint8_t *&payload) noexcept {
        payload = nullptr;
        if(!packet || packet_size < VOICE_PACKET_HEADER_SIZE) return false;
        header.magic = read_u32(packet); header.version = packet[4]; header.flags = packet[5]; header.payload_size = read_u16(packet + 6);
        header.room_id = read_u32(packet + 8); header.sender_id = read_u32(packet + 12); header.sequence = read_u32(packet + 16); header.timestamp = read_u32(packet + 20);
        if(header.magic != VOICE_PACKET_MAGIC || header.version != VOICE_PACKET_VERSION) return false;
        if(header.payload_size == 0 || header.payload_size > VOICE_OPUS_MAX_PACKET_BYTES) return false;
        if(packet_size != VOICE_PACKET_HEADER_SIZE + static_cast<size_t>(header.payload_size)) return false;
        payload = packet + VOICE_PACKET_HEADER_SIZE; return true;
    }
}
