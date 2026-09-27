#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace Chimera {
    enum class VoiceChatChannel : uint8_t { ALL = 0, TEAM = 1 };
    void initialize_voice_chat() noexcept;
    void shutdown_voice_chat() noexcept;
    bool voice_chat_initialized() noexcept;
    bool voice_chat_enabled() noexcept;
    void set_voice_chat_enabled(bool enabled) noexcept;
    bool set_voice_chat_transport(const std::string &host, uint16_t port) noexcept;
    void set_voice_chat_room(uint32_t room_id) noexcept;
    uint32_t voice_chat_room() noexcept;
    VoiceChatChannel voice_chat_channel() noexcept;
    void set_voice_chat_channel(VoiceChatChannel channel) noexcept;
    bool send_pending_voice_packet(uint32_t sender_id, uint32_t &sequence, uint32_t timestamp) noexcept;
    bool send_voice_keepalive_packet(uint32_t sender_id) noexcept;
    void process_received_voice_packets() noexcept;
    std::vector<uint32_t> get_active_voice_speakers(uint32_t max_age_ms = 500) noexcept;
    uint32_t voice_packets_sent_count() noexcept;
    uint32_t voice_packets_received_count() noexcept;
    uint32_t voice_packets_wrong_room_count() noexcept;
    float get_voice_audio_playback_volume() noexcept;
    void set_voice_audio_playback_volume(float volume) noexcept;
}
