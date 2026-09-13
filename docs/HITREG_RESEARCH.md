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

## Phase 3 status — DONE (net levers)

- `client/fix/hitreg_net_fix.cpp`
- `chimera_hitreg_action_queue_ticks [sv|cl] [1-128]`
- `chimera_hitreg_allow_client_projectiles [true/false]`
- Addresses: sv `0x623D78`, cl `0x623D80`, allow_client `0x624AA4`

---

## Phase 2 / Deep RE findings (`haloce.exe`)

Binary: Halo Custom Edition style PE32, ImageBase `0x400000`.

### 1. HS globals (autoaim / magnetism / projectiles / net queues)

| Global | Type | Runtime value address | Notes |
|--------|------|----------------------|-------|
| `player_autoaim` | bool (5) | `0x68CD80` | Gates bullet autoaim. No direct `.text` xref — HS accessor only. |
| `player_magnetism` | bool (5) | `0x68CD81` | Controller stick magnetism (`A0 81 CD 68 00` @ `0x7410F`). |
| `allow_client_side_weapon_projectiles` | bool (5) | `0x624AA4` | Refs @ `0xC78F7`, `0xC790C`. |
| `sv_client_action_queue_tick_limit` | long (8) | `0x623D78` | Ref @ `0x79549`. |
| `cl_remote_player_action_queue_tick_limit` | long (8) | `0x623D80` | Ref @ `0x799BB`. |

### 2. Controller magnetism (NOT bullet bend)

| Sig | File offset | Role |
|-----|-------------|------|
| `magnetism_sig` | `0x73F52` | Stick magnetism |
| `player_magnetism_enabled_sig` | `0x7410F` | Reads `player_magnetism` |

---

## Phase 4 — Projectile bend RE (in progress)

### Mechanism (confirmed by c20.reclaimers)

When a biped **autoaim pill** lies inside the weapon's **autoaim angle** + **autoaim range**, projectile paths are **pulled toward the pill**. That is the red-reticle / bullet-magnetism behaviour.

- **Weapon tag:** `autoaim angle`, `autoaim range` (and separate magnetism angle/range for controller stick).
- **Biped tag:** `autoaim width` (+ head/pelvis nodes for pill shape).
- Flag `aim assists only when zoomed` on weapon disables this when unzoomed (sniper).

### Call graph mapped in this binary

```
Fire / targeting path
  func @ 0x78370  (prologue 55 8B EC 83 EC 78)
    │
    ├─ calls autoaim evaluation @ 0x15D9C0   sites: 0x78521, 0x785BE, 0x78759, ...
    │     │
    │     └─ uses pill geometry builder
    │
    └─ on success (al/bl flag): continues toward projectile spawn / direction setup

Pill geometry (reads autoaim_width)
  func @ 0x15D850
    loads [ebp+0x458] at 0x15D91C / 0x15D95E / 0x15D9B0
    head/pelvis node vectors + width → pill capsule/sphere
    single external caller: 0x5AC08
```

### Key sites

| VA / file offset | Role |
|------------------|------|
| `0x78370` | Fire/targeting function that invokes autoaim test |
| `0x78521` | `call 0x15D9C0` — autoaim evaluation; result in `bl`/`al` |
| `0x15D9C0` | Large autoaim evaluation (object resolution, vector math, range tests) |
| `0x15D850` | Builds autoaim pill; **reads biped +0x458** |
| `0x5AC08` | Caller of pill builder |
| `0x15D91C` etc. | `mov eax, [ebp+0x458]` autoaim_width |

### Signature candidates (bend pipeline)

**A — Pill width read (stable):**
```
8B 85 58 04 00 00          ; mov eax, [ebp+0x458]
```
Prefer with surrounding unique bytes from `0x15D910`:
```
8B 49 08 89 4E 08 8B 85 58 04 00 00
```

**B — Autoaim eval call from fire path (`0x78521` region):**
```
6A 01 6A 00 6A 00 68 00 00 00 40 6A 00 57 52
; then E8 rel32 → 0x15D9C0
```

**C — Fire function prologue:**
```
55 8B EC 83 EC 78 8B 45 08
```

### What is NOT pinned yet

1. **Exact instruction that blends projectile forward** toward the pill center after a successful autoaim test (the true “bend”).
2. **Weapon tag byte offsets** for `autoaim angle` / `autoaim range` inside this binary’s in-memory layout (field order known from c20; numeric offsets still to lock).
3. **`player_autoaim` gate** in the fire path (likely via generic HS get, not `mov` from `0x68CD80`).

### Next RE steps (concrete)

1. From return of `0x15D9C0` at `0x78521`, follow the **true** branch and locate where a direction vector (3 floats) is written into projectile / firing data.
2. Cross-check with projectile creation routines (search for projectile tag class / spawn helpers used by weapon triggers).
3. Optional: enable `debug_objects_biped_autoaim_pills` in-game and breakpoint draw path to confirm live addresses.
4. Optional: pull function names from `halo-re/halo` or HCEA corpus for the same pipeline.

### Practical impact of bend RE

| Hook target | What we could do |
|-------------|------------------|
| Pill width (already have) | Stronger/weaker engage via `autoaim_width` |
| Autoaim test result | Log when bend is eligible (debug) |
| Direction blend | Scale bend strength, force on/off, diagnostics |

Until the blend site is signed, **tuning `autoaim_width` remains the main practical lever** for hitreg.

---

## Non-goals

- Changing object / camera interpolation
- Raising simulation tick rate
- Pure hitscan conversion

---

*Branch: `hitreg`*
*Projectile bend RE pass: 2026-09-13*
