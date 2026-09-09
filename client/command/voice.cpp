#include "voice.h"
#include "../client_signature.h"
#include "../halo_data/server.h"
#include "../halo_data/table.h"
#include "../hooks/frame.h"
#include "../hooks/tick.h"
#include "../messaging/messaging.h"
#include "../voice_chat/audio_capture.h"
#include "../voice_chat/voice_chat.h"
#include "../voice_chat/voice_transport.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {
    constexpr uint32_t VOICE_NO_PLAYER_SENDER_ID = 0xFFFFFFFFu;
    constexpr uint16_t DEFAULT_VOICE_PORT = 30777;
    constexpr uint16_t DEFAULT_HALO_PORT = 2302;
    constexpr DWORD KEEPALIVE_INTERVAL_MS = 20000;
    bool g_voice_frame_registered = false;
    unsigned int g_push_to_talk_key = 'V';
    uint32_t g_sequence = 0;
    bool g_manual_transport_override = false;
    bool g_manual_voice_channel_override = false;
    DWORD g_last_voice_send_tick = 0;

    std::string player_name(uint32_t player_index) {
        auto &pt = get_player_table();
        if(player_index >= pt.size || player_index >= pt.max_count) return "Unknown";
        auto *data = reinterpret_cast<char *>(pt.first) + static_cast<size_t>(player_index) * pt.index_size;
        if(*reinterpret_cast<uint16_t *>(data) == 0xFFFF) return "Unknown";
        char buffer[64]{};
        if(WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t *>(data + 4), -1, buffer, sizeof(buffer) - 1, nullptr, nullptr) == 0) return "Unknown";
        return buffer[0] ? std::string(buffer) : "Unknown";
    }

    std::string voice_sender_display_name(uint32_t sender_id) {
        if(sender_id == VOICE_NO_PLAYER_SENDER_ID) return "Unknown";
        auto &pt = get_player_table();
        for(uint16_t i = 0; i < pt.size && i < pt.max_count; ++i) {
            auto *data = reinterpret_cast<char *>(pt.first) + static_cast<size_t>(i) * pt.index_size;
            if(*reinterpret_cast<uint16_t *>(data) == 0xFFFF) continue;
            if(*reinterpret_cast<uint8_t *>(data + 100) == static_cast<uint8_t>(sender_id)) return player_name(i);
        }
        return "Unknown";
    }

    uint32_t voice_sender_id() noexcept {
        const uint32_t index = client_player_index() & 0xFFFF;
        auto &pt = get_player_table();
        if(index >= pt.size || index >= pt.max_count) return VOICE_NO_PLAYER_SENDER_ID;
        auto *data = reinterpret_cast<char *>(pt.first) + static_cast<size_t>(index) * pt.index_size;
        if(*reinterpret_cast<uint16_t *>(data) == 0xFFFF) return VOICE_NO_PLAYER_SENDER_ID;
        return static_cast<uint32_t>(*reinterpret_cast<uint8_t *>(data + 100));
    }

    uint32_t voice_timestamp() noexcept { return static_cast<uint32_t>(GetTickCount()); }

    void update_voice_channel_from_game() noexcept {
        if(g_manual_voice_channel_override || server_type() == SERVER_NONE) return;
        Chimera::set_voice_chat_channel(is_team() ? Chimera::VoiceChatChannel::TEAM : Chimera::VoiceChatChannel::ALL);
    }

    uint32_t fnv1a(const std::string &text) noexcept {
        uint32_t hash = 0x811C9DC5u;
        for(unsigned char c : text) { hash ^= c; hash *= 0x01000193u; }
        return hash;
    }

    uint32_t voice_room_id_for_server(const std::string &host, uint16_t port) noexcept {
        std::string key = host + ":" + std::to_string(port);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        const uint32_t room = fnv1a(key);
        return room == 0 ? 1 : room;
    }

    bool get_current_server_ip(std::string &host) noexcept {
        try {
            auto address = get_signature("create_server_ip_text_sig").address();
            if(!address) return false;
            const uintptr_t global_address = *reinterpret_cast<uintptr_t *>(address + 2);
            if(!global_address) return false;
            const uint32_t ip = *reinterpret_cast<uint32_t *>(global_address);
            in_addr addr{};
            addr.s_addr = ip;
            char buffer[INET_ADDRSTRLEN]{};
            if(!inet_ntop(AF_INET, &addr, buffer, sizeof(buffer))) return false;
            host = buffer;
            return true;
        }
        catch(...) { return false; }
    }

    void voice_frame_update() noexcept {
        if(!Chimera::voice_chat_enabled()) return;
        update_voice_channel_from_game();
        Chimera::process_received_voice_packets();
        const bool talking = (GetAsyncKeyState(static_cast<int>(g_push_to_talk_key)) & 0x8000) != 0;
        if(talking) {
            bool sent_any = false;
            while(Chimera::send_pending_voice_packet(voice_sender_id(), g_sequence, voice_timestamp())) sent_any = true;
            if(sent_any) g_last_voice_send_tick = GetTickCount();
        } else {
            std::vector<int16_t> discarded;
            while(Chimera::consume_voice_audio_packet(discarded)) {}
            const DWORD now = GetTickCount();
            if(now - g_last_voice_send_tick >= KEEPALIVE_INTERVAL_MS) {
                if(Chimera::send_voice_keepalive_packet(voice_sender_id())) g_last_voice_send_tick = now;
            }
        }
    }

    void update_voice_frame_registration() noexcept {
        if(Chimera::voice_chat_enabled() && !g_voice_frame_registered) {
            add_preframe_event(voice_frame_update, EVENT_PRIORITY_FINAL);
            g_voice_frame_registered = true;
        } else if(!Chimera::voice_chat_enabled() && g_voice_frame_registered) {
            remove_preframe_event(voice_frame_update);
            g_voice_frame_registered = false;
        }
    }

    bool parse_port(const char *text, uint16_t &port) noexcept {
        if(!text || !*text) return false;
        char *end = nullptr;
        const unsigned long value = std::strtoul(text, &end, 10);
        if(end == text || *end != '\0' || value == 0 || value > std::numeric_limits<uint16_t>::max()) return false;
        port = static_cast<uint16_t>(value); return true;
    }

    bool parse_virtual_key(const char *text, unsigned int &key) noexcept {
        if(!text || !*text) return false;
        if(std::strlen(text) == 1) { key = static_cast<unsigned char>(text[0]); return true; }
        char *end = nullptr;
        const unsigned long value = std::strtoul(text, &end, 0);
        if(end == text || *end != '\0' || value > 255) return false;
        key = static_cast<unsigned int>(value); return true;
    }

    void voice_connection_watch() noexcept {
        static ServerType last_server_type = SERVER_NONE;
        const ServerType current = server_type();
        if(current != SERVER_NONE && last_server_type == SERVER_NONE) {
            std::string host;
            if(get_current_server_ip(host)) {
                Chimera::set_voice_chat_room(voice_room_id_for_server(host, DEFAULT_HALO_PORT));
                if(!g_manual_transport_override) Chimera::set_voice_chat_transport(host, DEFAULT_VOICE_PORT);
            }
            g_manual_voice_channel_override = false;
            if(!Chimera::voice_chat_enabled()) Chimera::set_voice_chat_enabled(true);
            update_voice_frame_registration();
        } else if(current == SERVER_NONE && last_server_type != SERVER_NONE) {
            Chimera::set_voice_chat_enabled(false);
            Chimera::shutdown_voice_transport();
            Chimera::set_voice_chat_room(0);
            g_manual_voice_channel_override = false;
            Chimera::set_voice_chat_channel(Chimera::VoiceChatChannel::ALL);
            update_voice_frame_registration();
        }
        last_server_type = current;
    }
}

namespace Chimera {
    void set_up_voice_connection_watcher() noexcept { add_tick_event(voice_connection_watch); }

    ChimeraCommandError voice_command(size_t argc, const char **argv) noexcept {
        if(argc == 1) {
            if(!std::strcmp(argv[0], "1") || !std::strcmp(argv[0], "true") || !std::strcmp(argv[0], "on")) set_voice_chat_enabled(true);
            else if(!std::strcmp(argv[0], "0") || !std::strcmp(argv[0], "false") || !std::strcmp(argv[0], "off")) set_voice_chat_enabled(false);
            else { console_out_error("Usage: chimera_voice [on|off]"); return CHIMERA_COMMAND_ERROR_FAILURE; }
        } else if(argc != 0) { console_out_error("Usage: chimera_voice [on|off]"); return CHIMERA_COMMAND_ERROR_TOO_MANY_ARGUMENTS; }
        update_voice_frame_registration();
        console_out(voice_chat_enabled() ? "true" : "false");
        return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    ChimeraCommandError voice_all_command(size_t argc, const char **argv) noexcept {
        (void)argv; if(argc != 0) return CHIMERA_COMMAND_ERROR_TOO_MANY_ARGUMENTS;
        g_manual_voice_channel_override = true; set_voice_chat_channel(VoiceChatChannel::ALL); console_out("Voice channel: ALL (manual override)"); return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    ChimeraCommandError voice_team_command(size_t argc, const char **argv) noexcept {
        (void)argv; if(argc != 0) return CHIMERA_COMMAND_ERROR_TOO_MANY_ARGUMENTS;
        g_manual_voice_channel_override = true; set_voice_chat_channel(VoiceChatChannel::TEAM); console_out("Voice channel: TEAM (manual override)"); return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    ChimeraCommandError voice_host_command(size_t argc, const char **argv) noexcept {
        if(argc != 2) { console_out_error("Usage: chimera_voice_host <host> <port>"); return argc < 2 ? CHIMERA_COMMAND_ERROR_NOT_ENOUGH_ARGUMENTS : CHIMERA_COMMAND_ERROR_TOO_MANY_ARGUMENTS; }
        uint16_t port = 0;
        if(!parse_port(argv[1], port) || !set_voice_chat_transport(argv[0], port)) { console_out_error("Unable to configure voice relay."); return CHIMERA_COMMAND_ERROR_FAILURE; }
        g_manual_transport_override = true;
        std::string message = std::string("Voice relay configured: ") + argv[0] + ":" + std::to_string(port) + " (manual override)";
        console_out(message); return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    ChimeraCommandError voice_ptt_command(size_t argc, const char **argv) noexcept {
        if(argc == 1) { unsigned int key = 0; if(!parse_virtual_key(argv[0], key)) { console_out_error("Invalid virtual key."); return CHIMERA_COMMAND_ERROR_FAILURE; } g_push_to_talk_key = key; }
        else if(argc != 0) { console_out_error("Usage: chimera_voice_ptt [key|vk_code]"); return argc > 1 ? CHIMERA_COMMAND_ERROR_TOO_MANY_ARGUMENTS : CHIMERA_COMMAND_ERROR_NOT_ENOUGH_ARGUMENTS; }
        char message[128]{}; std::snprintf(message, sizeof(message), "PTT key/VK: %u", g_push_to_talk_key); console_out(message); return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    ChimeraCommandError voice_speakers_command(size_t argc, const char **argv) noexcept {
        (void)argv; if(argc != 0) return CHIMERA_COMMAND_ERROR_TOO_MANY_ARGUMENTS;
        const auto speakers = get_active_voice_speakers();
        if(speakers.empty()) { console_out("No one is talking right now."); return CHIMERA_COMMAND_ERROR_SUCCESS; }
        for(auto id : speakers) console_out(std::string(voice_sender_display_name(id)) + " is talking.");
        return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    ChimeraCommandError voice_volume_command(size_t argc, const char **argv) noexcept {
        if(argc == 0) { char message[128]{}; std::snprintf(message, sizeof(message), "Voice playback volume: %.2f", get_voice_audio_playback_volume()); console_out(message); return CHIMERA_COMMAND_ERROR_SUCCESS; }
        if(argc != 1) return CHIMERA_COMMAND_ERROR_TOO_MANY_ARGUMENTS;
        char *end = nullptr; const float value = std::strtof(argv[0], &end);
        if(end == argv[0] || *end != '\0') { console_out_error("Usage: chimera_voice_volume [0.0-4.0]"); return CHIMERA_COMMAND_ERROR_FAILURE; }
        set_voice_audio_playback_volume(value);
        char message[128]{}; std::snprintf(message, sizeof(message), "Voice playback volume: %.2f", get_voice_audio_playback_volume()); console_out(message); return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    ChimeraCommandError voice_status_command(size_t argc, const char **argv) noexcept {
        (void)argv; if(argc != 0) return CHIMERA_COMMAND_ERROR_TOO_MANY_ARGUMENTS;
        char message[256]{};
        std::snprintf(message, sizeof(message), "Voice: %s | Channel: %s%s | Room: 0x%08X | Sent: %u | Received: %u | Wrong room: %u",
            voice_chat_enabled() ? "ON" : "OFF", voice_chat_channel() == VoiceChatChannel::TEAM ? "TEAM" : "ALL", g_manual_voice_channel_override ? " (manual)" : "", static_cast<unsigned int>(voice_chat_room()), static_cast<unsigned int>(voice_packets_sent_count()), static_cast<unsigned int>(voice_packets_received_count()), static_cast<unsigned int>(voice_packets_wrong_room_count()));
        console_out(message); return CHIMERA_COMMAND_ERROR_SUCCESS;
    }
}
