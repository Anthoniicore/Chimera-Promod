#pragma once
#include "command.h"

ChimeraCommandError voice_command(size_t argc, const char **argv) noexcept;
ChimeraCommandError voice_all_command(size_t argc, const char **argv) noexcept;
ChimeraCommandError voice_team_command(size_t argc, const char **argv) noexcept;
ChimeraCommandError voice_host_command(size_t argc, const char **argv) noexcept;
ChimeraCommandError voice_ptt_command(size_t argc, const char **argv) noexcept;
ChimeraCommandError voice_speakers_command(size_t argc, const char **argv) noexcept;
ChimeraCommandError voice_volume_command(size_t argc, const char **argv) noexcept;
ChimeraCommandError voice_status_command(size_t argc, const char **argv) noexcept;
void set_up_voice_connection_watcher() noexcept;
