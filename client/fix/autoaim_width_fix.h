#pragma once

#include "../command/command.h"

/// Apply (or restore) reduced autoaim_width on all biped tags.
/// This is the classic Devieth-style hitreg improvement.
void apply_autoaim_width_fix(bool enable) noexcept;

/// Command: chimera_hitreg_autoaim_width [value|false]
/// value = target autoaim_width (recommended 0.05). false = restore stock.
ChimeraCommandError hitreg_autoaim_width_command(size_t argc, const char **argv) noexcept;
