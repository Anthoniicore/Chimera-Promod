#include "autoaim_width_fix.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <utility>

#include "../halo_data/tag_data.h"
#include "../messaging/messaging.h"
#include "../client_signature.h" // for write helpers if needed later

// Confirmed by Devieth hit_reg_fix.lua and community usage.
// Biped tag data + 0x458 = autoaim_width (float, world units).
static constexpr std::uintptr_t AUTOAIM_WIDTH_OFFSET = 0x458;

// Recommended values from the classic script:
//   stock-ish ~0.08
//   recommended  0.05
//   minimum safe ~0.045 (below ~0.03 headshots break)
static constexpr float DEFAULT_TARGET_WIDTH = 0.05f;

struct SavedWidth {
    char *tag_data;
    float original;
};

static std::vector<SavedWidth> saved_widths;
static bool currently_applied = false;
static float current_target = DEFAULT_TARGET_WIDTH;

void apply_autoaim_width_fix(bool enable) noexcept {
    // Always restore first if we have saved state.
    if (!saved_widths.empty()) {
        for (const auto &s : saved_widths) {
            if (s.tag_data) {
                *reinterpret_cast<float *>(s.tag_data + AUTOAIM_WIDTH_OFFSET) = s.original;
            }
        }
        saved_widths.clear();
        currently_applied = false;
    }

    if (!enable) {
        return;
    }

    // Tag array base and count (same addresses used by Chimera and Devieth script).
    auto **tag_array_ptr = reinterpret_cast<HaloTag **>(0x40440000);
    auto *tag_count_ptr  = reinterpret_cast<uint32_t *>(0x4044000C);

    if (!tag_array_ptr || !*tag_array_ptr || !tag_count_ptr) {
        return;
    }

    const uint32_t count = *tag_count_ptr;
    HaloTag *tags = *tag_array_ptr;

    for (uint32_t i = 0; i < count; ++i) {
        HaloTag &tag = tags[i];

        // Match biped class. The fourCC in the tag header is stored in a specific byte order.
        // Community Lua does: string.reverse(string.sub(read_string(tag), 1, 4)) == "bipd"
        char class_buf[5] = {};
        std::memcpy(class_buf, &tag.tag_class, 4);
        // Reverse the 4 bytes to get human-readable "bipd"
        char rev[5] = { class_buf[3], class_buf[2], class_buf[1], class_buf[0], 0 };
        if (std::strcmp(rev, "bipd") != 0) {
            continue;
        }

        if (!tag.data) {
            continue;
        }

        float *width_ptr = reinterpret_cast<float *>(tag.data + AUTOAIM_WIDTH_OFFSET);
        float original = *width_ptr;

        // Only touch reasonable values (sanity).
        if (original > 0.0f && original < 2.0f) {
            saved_widths.push_back({ tag.data, original });
            *width_ptr = current_target;
        }
    }

    currently_applied = !saved_widths.empty();
}

ChimeraCommandError hitreg_autoaim_width_command(size_t argc, const char **argv) noexcept {
    if (argc == 0) {
        if (currently_applied) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "true (%.3f) - %zu bipeds patched", static_cast<double>(current_target), saved_widths.size());
            console_out(buf);
        } else {
            console_out("false");
        }
        return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    if (std::strcmp(argv[0], "false") == 0 || std::strcmp(argv[0], "0") == 0 || std::strcmp(argv[0], "off") == 0) {
        apply_autoaim_width_fix(false);
        console_out("autoaim_width fix disabled (stock values restored)");
        return CHIMERA_COMMAND_ERROR_SUCCESS;
    }

    float new_value = static_cast<float>(std::atof(argv[0]));
    if (new_value < 0.03f) {
        console_out("Warning: values below ~0.03 can break headshots. Clamping to 0.045");
        new_value = 0.045f;
    }
    if (new_value > 0.15f) {
        console_out("Warning: values above ~0.08 usually make hitreg worse. Proceeding anyway.");
    }

    current_target = new_value;
    apply_autoaim_width_fix(true);

    char buf[128];
    std::snprintf(buf, sizeof(buf), "autoaim_width set to %.3f on %zu biped tags", static_cast<double>(current_target), saved_widths.size());
    console_out(buf);
    return CHIMERA_COMMAND_ERROR_SUCCESS;
}
