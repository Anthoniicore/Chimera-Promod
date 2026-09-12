#pragma once

#include "../command/command.h"

/// chimera_hitreg_action_queue_ticks [sv|cl] [value]
/// Read or set action-queue tick limits used in client/server reconciliation.
ChimeraCommandError hitreg_action_queue_ticks_command(size_t argc, const char **argv) noexcept;

/// chimera_hitreg_allow_client_projectiles [true/false]
/// Toggle allow_client_side_weapon_projectiles HS global.
ChimeraCommandError hitreg_allow_client_projectiles_command(size_t argc, const char **argv) noexcept;
