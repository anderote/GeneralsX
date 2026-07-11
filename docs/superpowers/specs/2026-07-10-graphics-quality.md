# GeneralsX Graphics Quality Improvements — Design Spec

**Date:** 2026-07-10
**Branch:** `feature/graphics-quality` (worktree `generalsx-gfx`)
**Status:** Approved (design reviewed inline in session)

## Goal

Improve the visual quality of GeneralsX (Zero Hour, macOS/Vulkan path first) — explosions,
particles, texture filtering, lighting — without touching the deterministic 30 Hz game
simulation and without breaking retail replay/multiplayer compatibility.

## Background (from codebase research)

- Renderer is the original W3D/WW3D2 fixed-function DX8 pipeline, translated by a pinned
  DXVK fork → Vulkan → MoltenVK. All draws funnel through `DX8Wrapper`
  (`Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp`).
- Effects (explosions, weapon FX) are almost fully data-driven via `FXList.ini` +
  `ParticleSystem.ini` (parsed in `Core/GameEngine/Source/GameClient/FXList.cpp`,
  `.../System/ParticleSys.cpp`).
- Particle rendering caps: per-system hard cap `MAX_POINTS_PER_GROUP = 512`
  (`GeneralsMD/.../W3DParticleSys.h:53`); global cap from GameLOD presets
  (`GameLOD.cpp` — Very High = 5000).
- Loose files override `.big` archives (`FileSystem.cpp:175`) — INI and texture
  replacements need no code changes. Game install: `~/GeneralsX/GeneralsZH`
  (all INI currently inside `INIZH.big`; no loose `Data/INI` yet).
- Object lighting: fixed-function, 4 lights/object (`lightenvironment.h:117 MAX_LIGHTS`);
  terrain: 20 CPU-relit dynamic lights (`BaseHeightMap.h:39`). `LightPulseFXNugget`
  already lets INI attach dynamic lights to effects.
- Heat-haze ("smudge") system exists (`W3DSmudge.cpp`) but self-disables if DXVK
  mishandles its framebuffer-readback self-test.
- Physics: ALL debris/topple/collapse motion is deterministic GameLogic fed by the synced
  RNG. Only client-side systems (particles, `GameClientRandom` consumers) may change.

## Hard constraints

1. **Never touch the deterministic sim**: no changes to `PhysicsUpdate.cpp`, projectile
   code, OCL debris tossing, topple/collapse timing, `m_gravity`, or anything drawing from
   `GameLogicRandom`. No new draws from the logic RNG stream.
2. **Client-only enhancements**: visual changes must live in GameClient / W3DDevice /
   WW3D2 / INI FX data. FX INI edits restricted to visual nuggets (particle systems,
   light pulses); do not add/modify Debris or object-creating nuggets.
3. **Backport rule** (AGENTS.md): code improvements to GeneralsMD (Zero Hour) must be
   mirrored to the Generals base game copies where the file is duplicated.
4. **No pushes/PRs/commits without explicit user request.**
5. Modified retail INI files are **not committed upstream-style**; they live in a local
   `mods/` directory (user's own game data derivative, deployed by symlink).

## Delivery model (updated after environment review)

The user's default play mode is **ShockWave + SPE + Control Bar Pro** via
`-mod ~/GeneralsX/mods/ShockWaveSPE` (see project memory). ShockWave ships its own
`ParticleSystem.ini`/`FXList.ini`, and INI resolution is whole-file, first-match:

- Loose `Data/INI/` files in the game dir would override the MOD's effect INIs and break
  ShockWave. **Do not use loose INI overrides.**
- Enhancements ship as `.big` layers using the established `generalsx-mods/` convention
  (each layer a directory with `build.py`, using `hotkey-addon/bigfile.py` for BIG I/O).
  Inside a `-mod` dir, later-alphabetical wins; layer must sort after all existing
  INI-bearing layers (existing custom layers touch Object INIs, not ParticleSystem.ini —
  verify at build time).
- Engine code changes (caps, dxvk.conf, Phase 2) live in the `generalsx-gfx` worktree.
- Hi-res textures: already proven viable — the installed ZHE Enhanced mod is a working
  HD texture pack. No PoC needed; drop from scope.

## Phase 1 — Easy wins (this plan)

| Item | Mechanism | Where |
|---|---|---|
| E1. Particle cap raises | `MAX_POINTS_PER_GROUP` 512→2048; GameLOD Very High particle cap 5000→10000 | engine worktree |
| E2. Forced anisotropic filtering | `resources/dxvk/dxvk.conf` `d3d9.samplerAnisotropy = 16` (+ verify it reaches the runtime dir) | engine worktree |
| E3. Explosion/FX enhancement layer | Python transformer over the ShockWaveSPE effective `ParticleSystem.ini`/`FXList.ini`: bigger bursts, added `LightPulse` nuggets, richer smoke/fire for the most common explosion FXLists; packaged as a late `.big` layer (plus a vanilla-game variant) | generalsx-mods repo |

## Phase 2 — Medium items (planned after Phase 1 verification)

- M1. More dynamic lights: object cap 4→8 (`lightenvironment.h`, `dx8wrapper.h Lights[]`),
  terrain cap tuning.
- M2. LDR bloom via the pillarbox render-target hook + `W3DShaderManager` filter framework
  (precompiled ps.1.x shaders — runtime assembler is stubbed on the port).
- M3. Heat-haze: runtime-verify `W3DSmudge` self-test under DXVK; fix readback path or
  force the backbuffer-copy fallback.
- M4. Terrain atlas 2048→4096 (`TileData.h TEXTURE_WIDTH`).
- M5. Supersampling/render-scale via pillarbox offscreen RT (moved from "easy" — the
  pillarbox path is only active when backbuffer ≠ game resolution; needs design).
- M6. Client-only cosmetic debris layer (own manager, `GameClientRandom`, never xfer'd).

Phase 2 gets its own plan once Phase 1 is verified in-game (heat-haze status and particle
perf inform its scope).

## Verification strategy

- Code changes: project builds via `./scripts/build/macos/build-macos-zh.sh`.
- INI changes: game boots without INI parse errors (they hard-fail loudly), effects fire.
- Visual quality: user runs their normal ShockWaveSPE launch and A/B compares
  (removing the effects `.big` layer from the mod dir restores prior visuals instantly).
- Determinism guard: no modified file under `GameLogic/`; grep diff for
  `GameLogicRandom` usage additions (must be zero).
