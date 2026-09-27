#pragma once
#include "command.h"

namespace Chimera {
    ChimeraCommandError voice_command(size_t argc, const char **argv) noexcept;
    ChimeraCommandError voice_all_command(size_t argc, const char **argv) noexcept;
    ChimeraCommandError voice_team_command(size_t argc, const char **argv) noexcept;
    ChimeraCommandError voice_host_command(size_t argc, const char **argv) noexcept;
    ChimeraCommandError voice_ptt_command(size_t argc, const char **argv) noexcept;
    ChimeraCommandError voice_speakers_command(size_t argc, const char **argv) noexcept;
    ChimeraCommandError voice_volume_command(size_t argc, const char **argv) noexcept;
    ChimeraCommandError voice_status_command(size_t argc, const char **argv) noexcept;
    void set_up_voice_connection_watcher() noexcept;
}

using Chimera::voice_command;
using Chimera::voice_all_command;
using Chimera::voice_team_command;
using Chimera::voice_host_command;
using Chimera::voice_ptt_command;
using Chimera::voice_speakers_command;
using Chimera::voice_volume_command;
using Chimera::voice_status_command;
using Chimera::set_up_voice_connection_watcher;
