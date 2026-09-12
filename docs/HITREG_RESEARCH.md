# Hitreg Research Branch

**Goal:** Improve hit registration in Halo CE without touching object interpolation.

Focus areas (from RE plan):
1. Application of autoaim to projectiles
2. Reading / modifying `autoaim_width` and related magnetism fields
3. Client ↔ server hit reconciliation code

## Known Facts (community + RE)

### Autoaim system
- **Autoaim** ("red reticle" / bullet magnetism): When a biped's **autoaim pill** is inside the weapon's `autoaim angle` + `autoaim range`, the projectile trajectory is bent toward the pill.
- Controlled primarily by:
  - **Weapon tag**: `autoaim angle` (float, the cone), `autoaim range` (world units).
  - **Biped tag**: `autoaim width` (float, world units) — controls the radius/thickness of the autoaim pill.
- Debug globals present in the executable:
  - `player_autoaim`
  - `player_magnetism`
  - `debug_objects_biped_autoaim_pills`
  - `debug_objects_biped_physics_pills`

### Proven community improvement
Devieth's classic hit-reg script (SAPP / Chimera Lua):
- Loops all `bipd` tags.
- Writes a smaller value to `autoaim_width` (recommended ~0.05, stock is often ~0.08).
- Result: when the reticle is red, bullets curve more aggressively into the target → server registers more hits.
- Side effect if used only client-side: visual mismatch on servers that don't run the same change.

### Current Chimera-Promod state
- `client/fix/magnetism_fix.cpp` only handles **controller stick magnetism** (view stickiness), not bullet autoaim.
- `movement_info_sig` is currently misused for that purpose (FIXES.md notes it is a no-op for true magnetism scalar).
- No existing code path modifies biped `autoaim_width` or weapon autoaim fields at runtime.

## RE Targets in `haloce.exe`

Binary provided: Halo Custom Edition 1.10 style PE32.

Useful strings already confirmed:
```
player_autoaim
player_magnetism
ai_render_projectile_aiming
ai_render_aiming_vectors
ai_render_aiming_validity
debug_objects_biped_autoaim_pills
allow_client_side_weapon_projectiles
projectile_incremental_rate
```

### Priority signatures to locate
1. Code that reads biped tag `autoaim width` when building / testing autoaim pills.
2. Code that applies the trajectory bend to a projectile (the actual "magnetism" of the bullet).
3. Network / action queue paths that decide final hit validity (client prediction vs server authority).
4. Any global or player flag that gates `player_autoaim`.

### Tag data layout (reference)
- Tag array base commonly at `0x40440000` (CE).
- Biped tag (`bipd`) has `autoaim width` as a float (see c20.reclaimers biped page).
- Exact offset inside the biped structure needs confirmation via RE or cross-reference with known tag dumps.

## Implementation Plan on this branch

### Phase 1 – Safe, high-value client-side change (no interpolation touch)
- On map load / tag load: iterate biped tags and optionally reduce `autoaim_width` to a configurable value (default 0.05).
- Expose command: `chimera_hitreg_autoaim_width <value|false>`.
- Keep it opt-in and documented that best results require server-side equivalent.

### Phase 2 – Deeper RE
- Locate the exact instruction sequences that:
  - Read `autoaim_width` from biped tag data.
  - Perform the projectile direction correction.
- Add signatures + optional hooks for logging / fine control.
- Investigate client prediction of hits vs server finalization (action queue tick limits, etc.).

### Phase 3 – Reconciliation improvements
- Only after Phase 1+2 are solid. Look at remote player action queues and any client-side hit validation that can be made more aggressive without desync.

## Files to create / modify
- `client/fix/autoaim_width_fix.cpp` + `.h` (Phase 1)
- Additional signatures in `client/client_signature.cpp`
- This document (updated as findings appear)
- Optional Lua helper for servers that still use SAPP

## Non-goals
- Changing object interpolation or camera interpolation.
- Raising simulation tick rate.
- Pure hitscan conversion (breaks vanilla compatibility and feel).

---
*Branch created: hitreg*
*Starting point: Chimera-Promod main @ af822019*
