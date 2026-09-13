# Hitreg Research Branch

**Goal:** Improve hit registration in Halo CE without touching object interpolation.

---

## Phase 1 — DONE (`autoaim_width`)

- `chimera_hitreg_autoaim_width` patches biped tag data **+0x458**

## Phase 3 — DONE (net)

- `chimera_hitreg_action_queue_ticks` / `chimera_hitreg_allow_client_projectiles`

---

## Phase 4 — Projectile bend: LOCATED

### Mechanism (c20 + binary)

When a biped autoaim pill is inside the weapon autoaim cone, the engine computes an **adjusted aim direction** and writes that 3-float vector out. Fire path uses it so projectile initial direction is pulled toward the pill.

### Call graph

```
Fire path @ 0x78370
  └─ call autoaim_eval @ 0x15D9C0     (e.g. 0x78521, 0x785BE, 0x78759)
        ├─ pill builder @ 0x15D850     (reads biped +0x458 width)
        ├─ vector math @ 0x15DBE0–0x15DCC8  (direction toward pill / combine)
        └─ COMMIT blend @ 0x15DE9C–0x15DEAB  ★ output direction write
              writes (x,y,z) to caller buffer; sets success flag
  └─ on success: unit aim vectors @ object+0x224 / +0x230 (0x7878B+)
```

### ★ Blend commit site (primary hook target)

**File offset `0x15DE9C`** inside `autoaim_eval` (`0x15D9C0`–`0x15DECF`):

```asm
; eax = output direction pointer (arg; may be null)
8B 84 24 54 05 00 00    mov  eax, [esp+0x554]
85 C0                   test eax, eax
74 14                   jz   skip
8B 4C 24 14             mov  ecx, [esp+0x14]   ; X
8B 54 24 18             mov  edx, [esp+0x18]   ; Y
89 08                   mov  [eax], ecx        ; ★ write X
8B 4C 24 1C             mov  ecx, [esp+0x1C]   ; Z
89 50 04                mov  [eax+4], edx      ; ★ write Y
89 48 08                mov  [eax+8], ecx      ; ★ write Z
C6 44 24 12 01          mov  byte ptr [esp+0x12], 1  ; success = true
```

**Unique signature (recommended):**
```
8B 4C 24 14 8B 54 24 18 89 08 8B 4C 24 1C 89 50 04 89 48 08 C6 44 24 12 01
```

This is the moment the autoaim-adjusted direction is published. Hooking here lets you:
- log the bent direction
- scale/replace the vector (stronger/weaker bend)
- force success/failure for tests

Secondary write of the same vector: `0x15DE58` → `[ebp+0x5C]`.

### Vector math region (builds [esp+0x14/18/1C])

`0x15DBE0`–`0x15DCC8`: FPU chain (`fmul` / `fadd` / `fstp`) producing the three components stored at `[esp+0x14]`, `[esp+0x18]`, `[esp+0x1C]`. Includes scales from `[ebp+0x80/84/88]` (aim basis) and table at `0x5FCC68`.

Not a single “lerp t” immediate — combination of pill-relative direction with firing basis.

### Downstream consumers

| Site | Role |
|------|------|
| `0x7878B` | `lea edx, [ecx+0x224]` then store 3 floats — unit aim vector |
| `0x7879E` | `lea edi, [ecx+0x230]` — related aim/look vector |
| Fire path `bl`/`al` from eval | `test bl,bl` / `jnz` after calls at `0x78521` etc. |

### Pill width (already used by Phase 1)

```
0x15D91C / 0x15D95E / 0x15D9B0:  8B 85 58 04 00 00   ; autoaim_width
```

### Still open (non-blocking)

- Exact weapon-tag byte offsets for `autoaim angle` / `autoaim range` in this build
- Where `player_autoaim` HS global is read (generic HS accessor, not hard-coded `0x68CD80`)
- In-flight projectile steering (if any); evidence points to **fire-time direction** bend only

### Practical hooks (no interpolation)

| Priority | Target | Use |
|----------|--------|-----|
| Done | biped +0x458 | `chimera_hitreg_autoaim_width` |
| Ready | sig @ blend commit `0x15DE9C` | log / scale bent direction |
| Optional | unit +0x224 write `0x7878B` | observe final aim vector |

---

## Non-goals

- Object / camera interpolation changes
- Tick rate changes
- Hitscan conversion

---

*Branch: `hitreg`*
*Blend site locked: 2026-09-13 — primary sig at file `0x15DE9C`*
