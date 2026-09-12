# Hitreg Research Branch

**Goal:** Improve hit registration in Halo CE without touching object interpolation.

Focus areas:
1. Application of autoaim to projectiles
2. Reading / modifying `autoaim_width` and related magnetism fields
3. Client ↔ server hit reconciliation code

---

## Phase 1 status — DONE

- `client/fix/autoaim_width_fix.cpp` patches biped `autoaim_width` at **tag data + 0x458**
- Command: `chimera_hitreg_autoaim_width [value|false]`
- Offset confirmed by Devieth `hit_reg_fix.lua` + c20 biped field order

---

## Phase 2 / Deep RE findings (`haloce.exe`)

Binary: Halo Custom Edition style PE32, ImageBase `0x400000`.

### 1. HS globals (autoaim / magnetism / projectiles / net queues)

Globals are registered in tables (name ptr, type, value addr). Type `5` = boolean, type `8` = long/int.

| Global | Type | Runtime value address | Notes |
|--------|------|----------------------|-------|
| `player_autoaim` | bool (5) | `0x68CD80` | Gates bullet autoaim. **No direct code xref found** — only HS table. Likely read via generic HS global accessor. |
| `player_magnetism` | bool (5) | `0x68CD81` | Controller view stickiness. Used by `magnetism_fix` (`A0 81 CD 68 00` @ file `0x7410F`). |
| `allow_client_side_weapon_projectiles` | bool (5) | `0x624AA4` | Client-side projectile simulation gate. Code refs @ `0xC78F7`, `0xC790C`. |
| `projectile_incremental_rate` | long (8) | `0x624AAC` | Related to projectile update rate. |
| `weapon_incremental_rate` | long (8) | `0x624AA8` | |
| `sv_client_action_queue_limit` | long (8) | `0x623D74` | Server-side client action queue size. |
| `sv_client_action_queue_tick_limit` | long (8) | `0x623D78` | **Hit reconciliation relevant.** Code ref @ `0x79549`. |
| `cl_remote_player_action_queue_limit` | long (8) | `0x623D7C` | Client remote player queue size. |
| `cl_remote_player_action_queue_tick_limit` | long (8) | `0x623D80` | **Hit reconciliation relevant.** Code ref @ `0x799BB`. |

String file offsets (for signature anchors):
```
player_magnetism                              0x205AA0
player_autoaim                                0x205AB4
ai_render_projectile_aiming                   0x206608
debug_objects_biped_autoaim_pills             0x2073B0
cl_remote_player_action_queue_tick_limit      0x20780C
sv_client_action_queue_tick_limit             0x20785C
multiplayer_hit_sound_volume                  0x2078A0
projectile_incremental_rate                   0x208A6C
allow_client_side_weapon_projectiles          0x208ABC
```

### 2. Existing Chimera magnetism signatures (verified in this binary)

| Signature | File offset | Role |
|-----------|-------------|------|
| `magnetism_sig` pattern | `0x73F52` | Controller magnetism path (`84 C0 0F 84 ... 8A ... 84 ... 8A`) |
| `player_magnetism_enabled_sig` | `0x7410F` | `A0 81 CD 68 00 83 C4 10 84 C0 0F 84` — reads `player_magnetism` |

These are **controller aim-stick magnetism**, not bullet trajectory autoaim.

### 3. Biped `autoaim_width` (offset 0x458) code usage

Three sites load a dword from `[reg+0x458]` in `.text`:

| File offset | Bytes | Interpretation |
|-------------|-------|----------------|
| `0x15D91C` | `8B 85 58 04 00 00` | `mov eax, [ebp+0x458]` |
| `0x15D95E` | `8B 85 58 04 00 00` | same |
| `0x15D9B0` | `8B 85 58 04 00 00` | same |

Surrounding code copies vectors / does FP ops (`fld`, `fmul`) — consistent with building autoaim pill geometry from biped tag fields (width + head/pelvis nodes). This is **read-side** (pill construction), not the projectile bend itself.

**Signature candidate (pill build / autoaim width read):**
```
8B 85 58 04 00 00    ; mov eax, [ebp+0x458]  ; autoaim_width
```
Context differs slightly per site; prefer unique surrounding bytes for a Chimera sig.

### 4. Action queue / hit reconciliation

Halo CE is **server-authoritative** for hits. Clients send actions into queues; the server simulates and decides hits.

Relevant globals (live addresses):
- `sv_client_action_queue_tick_limit` @ `0x623D78` — how many ticks of client actions the server keeps
- `cl_remote_player_action_queue_tick_limit` @ `0x623D80` — client-side remote player action history

Code that references these:
- Server tick limit usage near file `0x79549`
- Client remote tick limit usage near file `0x799BB`

Also:
- `allow_client_side_weapon_projectiles` @ `0x624AA4` — when true, client simulates weapon projectiles locally (can affect perceived hitreg vs server).

**Practical lever:** Tuning tick limits (within safe ranges) can reduce action drops under lag. Aggressive values risk desync / rubber-banding. Needs in-game measurement.

### 5. What we still need for full projectile-autoaim RE

The actual **trajectory bend** (when reticle is red, projectiles curve toward the autoaim pill) was **not** isolated to a single signature yet. Likely path:

1. Weapon fire → create projectile with initial direction
2. Autoaim test: is any biped pill inside weapon `autoaim angle` + `autoaim range`?
3. If yes and `player_autoaim` enabled → blend projectile forward vector toward pill center
4. Projectile then simulates with (possibly bent) direction

Next RE steps:
1. Find writers/readers of weapon tag `autoaim angle` / `autoaim range` (c20 weapon fields).
2. Cross-ref from `debug_objects_biped_autoaim_pills` draw path (string @ `0x2073B0`, table xref @ `0x2258D8`) — debug draw often sits next to the real test.
3. Find projectile spawn / direction init and any `player_autoaim` check via HS global get (generic), not hard-coded `0x68CD80`.
4. Optional: compare with Xbox decomp (`halo-re/halo`) / HCEA symbol corpus for function names.

---

## Recommended next implementations (no interpolation touch)

### A. Already shipped
- `chimera_hitreg_autoaim_width` — primary practical hitreg win when server agrees.

### B. Safe experimental commands (proposed)
1. **`chimera_hitreg_action_queue_ticks <sv|cl> <n>`**  
   Read/write `sv_client_action_queue_tick_limit` / `cl_remote_player_action_queue_tick_limit` at runtime.  
   Addresses: `0x623D78` / `0x623D80` (validate with signature before write).

2. **`chimera_hitreg_allow_client_projectiles [true/false]`**  
   Toggle `allow_client_side_weapon_projectiles` @ `0x624AA4` for testing perceived vs actual hits.

3. **Signature for biped autoaim_width read** (logging/assert only) using the `0x15D91C` region.

### C. Deeper (later)
- Hook projectile direction finalization once located.
- Optional server Lua package documenting Devieth values for hosts.

---

## Signature sketches (for `client_signature.cpp`)

```cpp
// player_magnetism byte (already partially covered by magnetism_fix)
// A0 81 CD 68 00 83 C4 10 84 C0 0F 84
const short player_magnetism_enabled_sig[] = {
    0xA0, 0x81, 0xCD, 0x68, 0x00, 0x83, 0xC4, 0x10, 0x84, 0xC0, 0x0F, 0x84
};

// sv_client_action_queue_tick_limit usage (anchor near 0x79549)
// Need unique bytes — dump more context before committing.

// allow_client_side_weapon_projectiles (anchor near 0xC78F7)
// Need unique bytes — dump more context before committing.
```

Absolute addresses above are for this specific CE 1.10-class binary. Prefer byte signatures over hard-coded VAs when integrating into Chimera.

---

## Non-goals

- Changing object / camera interpolation
- Raising simulation tick rate
- Pure hitscan conversion

---

*Branch: `hitreg`*
*Deep RE pass: 2026-09-12 against provided `haloce.exe`*
