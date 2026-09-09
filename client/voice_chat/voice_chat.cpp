#include "voice_chat.h"
#include "audio_capture.h"
#include "audio_playback.h"
#include "voice_codec.h"
#include "voice_packet.h"
#include "voice_transport.h"
#include "../halo_data/table.h"
#include <chrono>
#include <unordered_map>

namespace Chimera {
    static bool g_voice_chat_initialized = false;
    static bool g_voice_chat_enabled = false;
    namespace {
        std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> g_last_heard_from;
        uint32_t g_packets_sent = 0, g_packets_received = 0, g_packets_wrong_room = 0, g_room_id = 0;
        VoiceChatChannel g_voice_chat_channel = VoiceChatChannel::ALL;

        uint8_t player_team_by_machine(uint32_t machine_id) noexcept {
            auto &pt = get_player_table();
            for(uint16_t i=0; i<pt.size && i<pt.max_count; ++i) {
                auto *data = reinterpret_cast<char *>(pt.first) + static_cast<size_t>(i) * pt.index_size;
                if(*reinterpret_cast<uint16_t *>(data) == 0xFFFF) continue;
                if(*reinterpret_cast<uint8_t *>(data + 100) == static_cast<uint8_t>(machine_id)) return *reinterpret_cast<uint8_t *>(data + 32);
            }
            return 0xFF;
        }
        uint8_t local_team() noexcept {
            const uint32_t index = client_player_index() & 0xFFFF;
            auto &pt = get_player_table();
            if(index >= pt.size || index >= pt.max_count) return 0xFF;
            auto *data = reinterpret_cast<char *>(pt.first) + static_cast<size_t>(index) * pt.index_size;
            if(*reinterpret_cast<uint16_t *>(data) == 0xFFFF) return 0xFF;
            return *reinterpret_cast<uint8_t *>(data + 32);
        }
        bool sender_audible(uint32_t sender_id) noexcept {
            if(g_voice_chat_channel == VoiceChatChannel::ALL) return true;
            const uint8_t local = local_team();
            if(local == 0xFF) return true;
            const uint8_t sender = player_team_by_machine(sender_id);
            return sender != 0xFF && sender == local;
        }
    }

    void set_voice_chat_room(uint32_t room_id) noexcept { if(room_id != g_room_id) g_last_heard_from.clear(); g_room_id = room_id; }
    uint32_t voice_chat_room() noexcept { return g_room_id; }
    VoiceChatChannel voice_chat_channel() noexcept { return g_voice_chat_channel; }
    void set_voice_chat_channel(VoiceChatChannel channel) noexcept { if(g_voice_chat_channel == channel) return; g_last_heard_from.clear(); g_voice_chat_channel = channel; }
    void initialize_voice_chat() noexcept { g_voice_chat_initialized = true; }

    void shutdown_voice_chat() noexcept {
        if(!g_voice_chat_initialized) return;
        g_voice_chat_enabled = false;
        stop_voice_audio_capture(); shutdown_voice_audio_playback(); shutdown_voice_codec(); shutdown_voice_transport();
        g_voice_chat_initialized = false; g_room_id = 0; g_last_heard_from.clear();
    }
    bool voice_chat_initialized() noexcept { return g_voice_chat_initialized; }
    bool voice_chat_enabled() noexcept { return g_voice_chat_initialized && g_voice_chat_enabled; }

    void set_voice_chat_enabled(bool enabled) noexcept {
        if(!g_voice_chat_initialized) initialize_voice_chat();
        if(enabled) {
            if(!initialize_voice_codec() || !start_voice_audio_capture() || !initialize_voice_audio_playback()) {
                stop_voice_audio_capture(); shutdown_voice_audio_playback(); shutdown_voice_codec(); g_voice_chat_enabled = false; return;
            }
            g_voice_chat_enabled = true;
        } else { g_voice_chat_enabled = false; stop_voice_audio_capture(); }
    }

    bool set_voice_chat_transport(const std::string &host, uint16_t port) noexcept { if(!g_voice_chat_initialized) initialize_voice_chat(); return set_voice_transport_destination(host, port); }

    bool consume_serialized_voice_packet(uint32_t sender_id, uint32_t &sequence, uint32_t timestamp, std::vector<uint8_t> &packet) noexcept {
        std::vector<int16_t> pcm; if(!consume_voice_audio_packet(pcm)) return false;
        std::vector<uint8_t> opus_packet; if(!encode_voice_audio_packet(pcm.data(), pcm.size(), opus_packet)) return false;
        if(!build_voice_packet(g_room_id, sender_id, sequence, timestamp, opus_packet.data(), opus_packet.size(), packet)) return false;
        ++sequence; return true;
    }

    bool send_pending_voice_packet(uint32_t sender_id, uint32_t &sequence, uint32_t timestamp) noexcept {
        if(!voice_chat_enabled() || !voice_transport_has_destination() || g_room_id == 0) return false;
        std::vector<uint8_t> packet; const uint32_t old_sequence = sequence;
        if(!consume_serialized_voice_packet(sender_id, sequence, timestamp, packet)) return false;
        if(send_voice_transport_packet(packet.data(), packet.size())) { ++g_packets_sent; return true; }
        sequence = old_sequence; return false;
    }

    bool send_voice_keepalive_packet(uint32_t sender_id) noexcept {
        if(!voice_chat_enabled() || !voice_transport_has_destination() || g_room_id == 0) return false;
        std::vector<uint8_t> packet; if(!build_voice_keepalive_packet(g_room_id, sender_id, packet)) return false;
        return send_voice_transport_packet(packet.data(), packet.size());
    }

    void process_received_voice_packets() noexcept {
        if(!voice_chat_enabled()) return;
        for(;;) {
            std::vector<uint8_t> packet; if(!receive_voice_transport_packet(packet)) break;
            VoicePacketHeader header{}; const uint8_t *payload = nullptr;
            if(!parse_voice_packet(packet.data(), packet.size(), header, payload)) continue;
            if(header.room_id != g_room_id) { ++g_packets_wrong_room; continue; }
            if(header.flags & VOICE_PACKET_FLAG_KEEPALIVE) continue;
            if(!sender_audible(header.sender_id)) continue;
            std::vector<int16_t> pcm; if(!decode_voice_audio_packet(payload, header.payload_size, pcm)) continue;
            if(queue_voice_audio_playback(pcm.data(), pcm.size())) { g_last_heard_from[header.sender_id] = std::chrono::steady_clock::now(); ++g_packets_received; }
        }
    }

    uint32_t voice_packets_sent_count() noexcept { return g_packets_sent; }
    uint32_t voice_packets_received_count() noexcept { return g_packets_received; }
    uint32_t voice_packets_wrong_room_count() noexcept { return g_packets_wrong_room; }
    std::vector<uint32_t> get_active_voice_speakers(uint32_t max_age_ms) noexcept {
        std::vector<uint32_t> speakers; const auto now = std::chrono::steady_clock::now();
        for(auto &entry : g_last_heard_from) {
            const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.second).count();
            if(age >= 0 && static_cast<uint32_t>(age) <= max_age_ms) speakers.push_back(entry.first);
        }
        return speakers;
    }

    namespace { struct VoiceChatLifecycle { VoiceChatLifecycle() noexcept { initialize_voice_chat(); } ~VoiceChatLifecycle() { shutdown_voice_chat(); } }; static VoiceChatLifecycle g_voice_chat_lifecycle; }
}
