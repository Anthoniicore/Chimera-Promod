#include "hitreg_net_fix.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "../messaging/messaging.h"

// Halo CE 1.10-class addresses recovered from RE of haloce.exe
// (HS global value pointers). Same approach as tag array @ 0x40440000.
//
// sv_client_action_queue_tick_limit      -> 0x623D78
// cl_remote_player_action_queue_tick_limit -> 0x623D80
// allow_client_side_weapon_projectiles   -> 0x624AA4 (bool)

static constexpr std::uintptr_t ADDR_SV_CLIENT_ACTION_QUEUE_TICK_LIMIT =
    0x623D78u;
static constexpr std::uintptr_t ADDR_CL_REMOTE_ACTION_QUEUE_TICK_LIMIT =
    0x623D80u;
static constexpr std::uintptr_t ADDR_ALLOW_CLIENT_SIDE_WEAPON_PROJECTILES =
    0x624AA4u;

// Conservative bounds for tick limits (engine default is typically small).
static constexpr int32_t TICK_LIMIT_MIN = 1;
static constexpr int32_t TICK_LIMIT_MAX = 128;

static int32_t *sv_tick_limit() noexcept {
    return reinterpret_cast<int32_t *>(ADDR_SV_CLIENT_ACTION_QUEUE_TICK_LIMIT);
}

static int32_t *cl_tick_limit() noexcept {
    return reinterpret_cast<int32_t *>(ADDR_CL_REMOTE_ACTION_QUEUE_TICK_LIMIT);
}

static uint8_t *allow_client_projectiles() noexcept {
    return reinterpret_cast<uint8_t *>(ADDR_ALLOW_CLIENT_SIDE_WEAPON_PROJECTILES);
}

static bool parse_bool_arg(const char *s, bool *out) noexcept {
    if (!s || !out) {
        return false;
    }
    if (std::strcmp(s, "true") == 0 || std::strcmp(s, "1") == 0 ||
        std::strcmp(s, "on") == 0) {
        *out = true;
        return true;
    }
    if (std::strcmp(s, "false") == 0 || std::strcmp(s, "0") == 0 ||
        std::strcmp(s, "off") == 0) {
        *out = false;
        return true;
    }
    return false;
}

ChimeraCommandError hitreg_action_queue_ticks_command(size_t argc,
                                                      const char **argv) noexcept {
    int32_t *sv = sv_tick_limit();
    int32_t *cl = cl_tick_limit();

    if (argc == 0) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "sv_client_action_queue_tick_limit=%d  "
                      "cl_remote_player_action_queue_tick_limit=%d",
                      *sv, *cl);
        console_out(buf);
        return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    if (argc == 1) {
        console_out("Usage: chimera_hitreg_action_queue_ticks <sv|cl> <value>");
        console_out("  value range: 1-128 (higher keeps more actions under lag)");
        return CHIMERA_COMMAND_ERROR_FAILURE;
    }

    // argc >= 2: <sv|cl> <value>
    const char *which = argv[0];
    int32_t value = static_cast<int32_t>(std::atoi(argv[1]));

    if (value < TICK_LIMIT_MIN) {
        value = TICK_LIMIT_MIN;
        console_out("Clamped to minimum 1");
    }
    if (value > TICK_LIMIT_MAX) {
        value = TICK_LIMIT_MAX;
        console_out("Clamped to maximum 128");
    }

    char buf[128];
    if (std::strcmp(which, "sv") == 0 || std::strcmp(which, "server") == 0) {
        *sv = value;
        std::snprintf(buf, sizeof(buf),
                      "sv_client_action_queue_tick_limit set to %d", value);
        console_out(buf);
        return CHIMERA_COMMAND_ERROR_SUCCESS;
    }
    if (std::strcmp(which, "cl") == 0 || std::strcmp(which, "client") == 0) {
        *cl = value;
        std::snprintf(buf, sizeof(buf),
                      "cl_remote_player_action_queue_tick_limit set to %d", value);
        console_out(buf);
        return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    console_out("First argument must be 'sv' or 'cl'");
    return CHIMERA_COMMAND_ERROR_FAILURE;
}

ChimeraCommandError hitreg_allow_client_projectiles_command(
    size_t argc, const char **argv) noexcept {
    uint8_t *flag = allow_client_projectiles();

    if (argc == 0) {
        console_out(*flag ? "true" : "false");
        return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    bool enable = false;
    if (!parse_bool_arg(argv[0], &enable)) {
        console_out("Expected true/false");
        return CHIMERA_COMMAND_ERROR_FAILURE;
    }

    *flag = enable ? 1 : 0;
    console_out(enable
                    ? "allow_client_side_weapon_projectiles = true"
                    : "allow_client_side_weapon_projectiles = false");
    return CHIMERA_COMMAND_ERROR_SUCCESS;
}
