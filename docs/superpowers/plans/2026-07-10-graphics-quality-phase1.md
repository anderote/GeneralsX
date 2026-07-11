# Graphics Quality Phase 1 (Easy Wins) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Denser/juicier explosions, dynamic light pulses on big FX, forced 16× anisotropic filtering, and raised particle ceilings — all client-side, replay-safe.

**Architecture:** Two small engine edits in the `generalsx-gfx` worktree (particle caps, dxvk.conf), plus a data layer built in the separate `generalsx-mods` repo that transforms the ShockWaveSPE stack's effective `ParticleSystem.ini`/`FXList.ini` and packages the result as a late-loading `.big` following the existing layer convention.

**Tech Stack:** C++ (engine), Python 3 (INI transform + BIG packing via existing `hotkey-addon/bigfile.py`), DXVK config.

## Global Constraints

- NEVER modify anything under `GameLogic/` or anything reading `GameLogicRandom` (replay/multiplayer determinism).
- FX INI edits may only touch visual nuggets: `ParticleSystem`, `LightPulse`, existing nugget parameter values. Never add/remove/modify Debris, OCL, or object-creating nuggets; never touch weapon/object INIs in this plan.
- Engine changes to `GeneralsMD/` must be mirrored to the `Generals/` copy of the same file (AGENTS.md backport rule). `Core/` files are shared — do not duplicate.
- The INI parser hard-crashes on malformed lines (known: trailing-space `Object` names). The transformer must be surgical: modify only whole known-key value lines, preserve all other bytes exactly.
- No git commits, pushes, or PRs — the user decides commit boundaries.
- Engine worktree: `/Users/andrewcote/Documents/software/generalsx-gfx` (branch `feature/graphics-quality`). Mod repo: `/Users/andrewcote/Documents/software/generalsx-mods` (its own git repo).
- Build: `VULKAN_SDK=$HOME/VulkanSDK/1.4.350.1/macOS PATH="$VULKAN_SDK/bin:$PATH" ./scripts/build/macos/build-macos-zh.sh --build-only` from the worktree root.

---

### Task 1: Raise particle rendering caps (engine)

**Files:**
- Modify: `GeneralsMD/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DParticleSys.h:53`
- Modify: `Generals/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DParticleSys.h:53`
- Modify: `GeneralsMD/Code/GameEngine/Source/Common/GameLOD.cpp:255`
- Modify: `Generals/Code/GameEngine/Source/Common/GameLOD.cpp:257`

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces: engine renders up to 2048 particles per system per frame; Very High LOD preset allows 10000 total particles. Task 3's INI layer relies on these ceilings.

- [ ] **Step 1: Bump MAX_POINTS_PER_GROUP in both game copies**

In BOTH `GeneralsMD/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DParticleSys.h` and `Generals/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DParticleSys.h` (line 53):

```cpp
// old
	enum { MAX_POINTS_PER_GROUP = 512 };
// new
	enum { MAX_POINTS_PER_GROUP = 2048 };
```

Do NOT touch the identically named enum in `Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DSmudge.h:51` — that one is the smudge batch size, unrelated.

Sanity check while editing: `W3DParticleSys.cpp:135` declares `unsigned int personalities[MAX_POINTS_PER_GROUP]` on the stack — at 2048 that is 8 KB, fine. The shared point/vertex buffers in the same file are sized from the same macro and scale automatically.

- [ ] **Step 2: Bump Very High LOD particle cap in both game copies**

`GeneralsMD/Code/GameEngine/Source/Common/GameLOD.cpp:255` and `Generals/Code/GameEngine/Source/Common/GameLOD.cpp:257`:

```cpp
// old
	veryhigh.m_maxParticleCount = 5000;
// new
	veryhigh.m_maxParticleCount = 10000;
```

Leave the base default (`m_maxParticleCount=2500`) alone — lower LOD presets keep retail behavior.

- [ ] **Step 3: Build Zero Hour**

Run from worktree root:
```bash
VULKAN_SDK=$HOME/VulkanSDK/1.4.350.1/macOS PATH="$VULKAN_SDK/bin:$PATH" ./scripts/build/macos/build-macos-zh.sh --build-only
```
Expected: build completes, `build/macos-vulkan/GeneralsMD/GeneralsXZH` exists with a fresh timestamp.

- [ ] **Step 4: Compile-check the Generals base game copy**

```bash
VULKAN_SDK=$HOME/VulkanSDK/1.4.350.1/macOS PATH="$VULKAN_SDK/bin:$PATH" cmake --build build/macos-vulkan --target g_generals 2>&1 | tail -5
```
Expected: success (or, if the target name differs, find it with `cmake --build build/macos-vulkan --target help | grep -i generals`).

---

### Task 2: Force 16× anisotropic filtering via DXVK config (engine)

**Files:**
- Modify: `resources/dxvk/dxvk.conf`
- Possibly modify: `scripts/build/macos/deploy-macos-zh.sh` (only if it does not already copy dxvk.conf)

**Interfaces:**
- Consumes: nothing.
- Produces: `~/GeneralsX/GeneralsZH/dxvk.conf` containing the anisotropy override at deploy time.

- [ ] **Step 1: Add the override to `resources/dxvk/dxvk.conf`**

Append (keeping the existing `d3d9.forceSamplerTypeSpecConstants = False` line and its comment intact):

```ini
# GeneralsX graphics-quality: force anisotropic filtering on all samplers.
# The game's own option tops out per-stage; this overrides at the DXVK layer.
d3d9.samplerAnisotropy = 16
```

(The d3d8 frontend layers on d3d9, so `d3d9.*` sampler options apply.)

- [ ] **Step 2: Verify the conf reaches the runtime dir**

```bash
grep -rn "dxvk.conf" scripts/build/macos/ cmake/ | head
ls ~/GeneralsX/GeneralsZH/dxvk.conf 2>/dev/null
```
If the deploy script copies it: done. If NOT, add a copy step to `scripts/build/macos/deploy-macos-zh.sh` mirroring how other resources are copied, e.g.:

```bash
cp "${PROJECT_ROOT}/resources/dxvk/dxvk.conf" "${GAME_DIR}/dxvk.conf"
```

Also confirm DXVK reads it from the game dir: DXVK-native looks for `dxvk.conf` in the executable's working directory or `DXVK_CONFIG_FILE`. The run script `run-macos-zh.sh` cds into/launches from the game dir — verify by reading it; if the CWD is not the game dir, export `DXVK_CONFIG_FILE="${GAME_DIR}/dxvk.conf"` in the run script instead.

- [ ] **Step 3: Deploy and verify file contents**

```bash
./scripts/build/macos/deploy-macos-zh.sh
grep samplerAnisotropy ~/GeneralsX/GeneralsZH/dxvk.conf
```
Expected: the new line present in the deployed file.

---

### Task 3: Explosion/FX enhancement layer (generalsx-mods repo)

**Files:**
- Create: `/Users/andrewcote/Documents/software/generalsx-mods/fx-enhance/build.py`
- Create: `/Users/andrewcote/Documents/software/generalsx-mods/fx-enhance/README.md`
- Output artifacts (not committed to the engine repo):
  - `/Users/andrewcote/GeneralsX/mods/ShockWave/zzzz_FXEnhance.big`
  - `/Users/andrewcote/GeneralsX/mods/ShockWaveSPE/zzzz_FXEnhance.big`
  - `/Users/andrewcote/GeneralsX/GeneralsZH/000_FXEnhanceZH.big` (vanilla variant)

**Interfaces:**
- Consumes: `bigfile.py` from `/Users/andrewcote/Documents/software/generalsx-mods/hotkey-addon/bigfile.py` (`read_big(path) -> List[BigEntry]`, and its write counterpart — read the file for the exact writer signature). Assumes Task 1's 2048/system ceiling.
- Produces: installed `.big` layers; `build.py` is rerunnable (idempotent regeneration).

- [ ] **Step 1: Resolve the effective source INIs**

In `build.py`, implement source resolution:

```python
import sys, re, fnmatch
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "hotkey-addon"))
from bigfile import read_big  # and the writer (check its name in the file)

GAME_DIR = Path.home() / "GeneralsX/GeneralsZH"
MOD_DIRS = [Path.home() / "GeneralsX/mods/ShockWave",
            Path.home() / "GeneralsX/mods/ShockWaveSPE"]
TARGETS = ["Data\\INI\\ParticleSystem.ini", "Data\\INI\\FXList.ini"]

def effective_ini(mod_dir: Path, target: str) -> bytes:
    """Inside a -mod dir, LATER-alphabetical archive wins (prepend semantics)."""
    winner = None
    for big in sorted(mod_dir.glob("*.big")):          # ascending; keep last hit
        for e in read_big(big):
            if e.path.lower() == target.lower():
                winner = (big.name, e.data)
    if winner is None:                                  # fall back to game dir:
        for big in sorted(GAME_DIR.glob("*.big")):      # EARLIER wins -> keep first hit
            for e in read_big(big):
                if e.path.lower() == target.lower():
                    return e.data
        raise FileNotFoundError(target)
    return winner[1]
```

Also assert no existing custom layer (`zz*` bigs) already contains the two target INIs — if one does, print which and stop (layer-ordering decision needed).

- [ ] **Step 2: Verify baseline extraction**

Run a dry extraction that just prints source archive names and sizes for both targets for ShockWaveSPE and vanilla. Expected: ParticleSystem.ini and FXList.ini found (ShockWave's own for the mod dirs; `INIZH.big`'s for vanilla), each > 100 KB, first line plausibly a comment/`ParticleSystem` block.

- [ ] **Step 3: Implement the surgical INI transformer**

Rules, applied line-by-line preserving every untouched byte (files use `\r\n` and `;` comments — preserve both; match keys case-insensitively with `^\s*Key\s*=`):

1. **Parse FXList.ini into blocks** (`FXList <name>` … `End` at nesting depth 0; nuggets are nested blocks). A block is an "explosion FXList" if it contains a `ViewShake` nugget (screen shake ⇒ big boom) — this is mod-name-agnostic, works for ShockWave too.
2. **LightPulse injection:** for each explosion FXList that does NOT already contain a `LightPulse` nugget, insert before its final `End`:

```
  LightPulse
    Color = R:255 G:156 B:64
    Radius = 80
    RadiusAsPercentOfObjectSize = 0
    IncreaseTime = 80
    DecreaseTime = 600
  End
```

   First, grep the source FXList.ini for an existing `LightPulse` nugget and copy ITS exact field spelling/order as the template (field names above are from `FXList.cpp`'s parse table — verify against real data before generating; if a field is absent in real data, drop it).
3. **Collect enhanced particle-system names:** every `ParticleSystem = <name>` / `AttachedSystem = <name>` value inside explosion FXLists.
4. **ParticleSystem.ini scaling:** for each collected system (and, additionally, any system whose name matches `(?i)explosion|fireball|flame` that is referenced by any collected system's `SlaveSystem`): parse its block; then
   - `BurstCount` keyframe values ×1.75, rounded, per-value cap 120
   - `Size` start/end values ×1.25
   - `Lifetime` values ×1.3 ONLY if the system's `ParticleName` contains `smoke` (case-insensitive) or name matches `(?i)smoke`
   - never scale a value on a line containing `%` or that fails float parse — skip and log
   - only touch numeric parameters on lines whose key is exactly `BurstCount`, `Size`, or `Lifetime`; ParticleSystem.ini expresses these as `Key = min max` pairs — scale both numbers, preserve spacing style
5. **Safety caps:** if a system's max `BurstCount × (Lifetime_max / BurstDelay_min)` estimate exceeds 2000, reduce the multiplier for that system to stay under (log it). Skip systems with `Type = VOLUME_PARTICLE` (6× fill cost) and any system whose `Priority` is `WEAPON_TRAIL` (continuous emitters).
6. **Log a summary:** N FXLists enhanced, M light pulses injected, K systems scaled, list of skips.

- [ ] **Step 4: Round-trip and diff verification**

- Re-run the transformer on its own output: second pass must be detected as already-enhanced (marker comment `; FXEnhance v1` prepended to each file) and refuse to double-scale.
- `diff` original vs transformed: every changed line must match one of the allowed patterns (`BurstCount`, `Size`, `Lifetime`, injected LightPulse blocks, marker comment). Implement this as an automated check inside `build.py` that fails loudly otherwise.
- Line-ending check: transformed file has the same `\r\n` convention and same total block count (`^End` count + injected blocks).

- [ ] **Step 5: Package and install layers**

- Mod variant: pack transformed ShockWaveSPE-derived INIs as `zzzz_FXEnhance.big` (paths `Data\INI\ParticleSystem.ini`, `Data\INI\FXList.ini`) and copy to BOTH `~/GeneralsX/mods/ShockWave/` and `~/GeneralsX/mods/ShockWaveSPE/`. `zzzz_` sorts after every existing `zz*`/`zzz*` layer (4th char `z` > `_`), so it wins inside the mod dir.
- Vanilla variant: transform the `INIZH.big` versions, pack as `000_FXEnhanceZH.big`, copy to `~/GeneralsX/GeneralsZH/` (game dir: EARLIER-alphabetical wins; `000_` precedent exists with `000_ShowHotkeysZH.big`).
- Print install summary.

- [ ] **Step 6: README**

`fx-enhance/README.md`: what it does, the transformation rules and caps, how to rebuild (`python3 build.py`), how to uninstall (delete the three `.big` files), and the determinism rationale (visual-only nuggets; no Debris/OCL/weapon changes).

---

### Task 4: End-to-end verification checklist (user-facing)

**Files:** none (verification only)

- [ ] **Step 1: Confirm builds/artifacts**

```bash
ls -la /Users/andrewcote/Documents/software/generalsx-gfx/build/macos-vulkan/GeneralsMD/GeneralsXZH
ls -la ~/GeneralsX/mods/ShockWaveSPE/zzzz_FXEnhance.big ~/GeneralsX/GeneralsZH/000_FXEnhanceZH.big
grep samplerAnisotropy ~/GeneralsX/GeneralsZH/dxvk.conf
```

- [ ] **Step 2: Deploy the new binary**

```bash
cd /Users/andrewcote/Documents/software/generalsx-gfx && ./scripts/build/macos/deploy-macos-zh.sh
```

- [ ] **Step 3: Hand off to user for visual A/B**

User runs their normal ShockWaveSPE launch; checks: explosions denser + emit light pulses on terrain, no parse crash at startup, FPS acceptable in a big fight, Options → particle cap slider still works. A/B: temporarily remove `zzzz_FXEnhance.big` to compare. Report any INI crash with the log tail from `~/Library/Application Support/GeneralsX/GeneralsZH/`.
