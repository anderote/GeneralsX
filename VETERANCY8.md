# Extended Veterancy: 8 Levels (VETERANCY8)

This branch extends the veterancy system from 4 to 8 levels in both game targets
(GeneralsMD / Zero Hour is the primary target; Generals receives the identical mirrored
changes because the `VeterancyLevel` enum lives in shared `Core` code).

## Levels

| Index | Internal enum   | INI name  | Health bonus default | Notes                       |
|-------|-----------------|-----------|----------------------|-----------------------------|
| 0     | `LEVEL_REGULAR` | `REGULAR` | 100% (fixed)         | unchanged                   |
| 1     | `LEVEL_VETERAN` | `VETERAN` | INI (vanilla 120%)   | unchanged                   |
| 2     | `LEVEL_ELITE`   | `ELITE`   | INI (vanilla 130%)   | unchanged                   |
| 3     | `LEVEL_HEROIC`  | `HEROIC`  | INI (vanilla 150%)   | unchanged                   |
| 4     | `LEVEL_HEROIC2` | `HEROIC2` | 170%                 | new                         |
| 5     | `LEVEL_HEROIC3` | `HEROIC3` | 190%                 | new                         |
| 6     | `LEVEL_HEROIC4` | `HEROIC4` | 210%                 | new                         |
| 7     | `LEVEL_HEROIC5` | `HEROIC5` | 230%                 | new, `LEVEL_LAST`           |

The INI names (`HEROIC2` .. `HEROIC5`) work everywhere veterancy names are parsed:
crate `VeterancyLevel`, `StartingLevel` (VeterancyGainCreate), `IntialVeterancy`
(PlayerTemplate), per-veterancy weapon FX keys, DamageFX veterancy keys, DieModule
`VeterancyLevel` +/- flag lists, etc. (they all read `TheVeterancyNames`).

## New INI keys and defaults

### GameData (GlobalData)

- `HealthBonus_Heroic2` (default 170%)
- `HealthBonus_Heroic3` (default 190%)
- `HealthBonus_Heroic4` (default 210%)
- `HealthBonus_Heroic5` (default 230%)

Defaults are hard-coded extrapolations of the vanilla 100/120/130/150 curve
(+20% of base health per rank). They are code defaults, not INI-file additions, so
shipped `GameData.ini` needs no edits. Caveat: if a mod changes `HealthBonus_Heroic`
to something far from 150%, it should also override the `_Heroic2..5` keys to keep the
curve monotonic.

### WeaponBonus conditions

Four new `WeaponBonusConditionType` entries with INI names `HERO2`, `HERO3`, `HERO4`,
`HERO5` (usable in `GameData.ini` / weapon `WeaponBonus =` overrides, e.g.
`WeaponBonus = HERO3 DAMAGE 125%`).

Semantics are **cumulative**: at `HEROIC3` the object has `HERO`, `HERO2` and `HERO3`
conditions set simultaneously, so each level's `WeaponBonus` entry expresses the
*marginal* bonus stacked on top of the previous ranks. With no INI overrides the new
conditions contribute nothing, so a HEROIC5 unit fires exactly like a vanilla HEROIC
unit (this is the safe backward-compatible default).

The new values are appended at the end of the enum, so existing save files (raw 32-bit
flag masks) remain valid. `WeaponBonusConditionFlags` is a 32-bit `UnsignedInt`; the
count is now 31 of 32 bits used - only 1 bit remains for future conditions.

### Object experience lists (`ExperienceValue`, `ExperienceRequired`, `SkillPointValue`)

The parser (`ThingTemplate::parseVeterancyIntList`) now accepts **1 to 8** values.
Every shipped object INI provides exactly 4; the missing high ranks are extrapolated:

- **`ExperienceRequired`** (thresholds): each missing rank costs 1.75x the previous
  rank's increment:
  `t[i] = t[i-1] + round(1.75 * (t[i-1] - t[i-2]))`
  Example vanilla `0 100 200 400` extends to `0 100 200 400 750 1362 2433 4307`.
- **`ExperienceValue` / `SkillPointValue`** (per-level worth): the last increment is
  continued linearly and never downward:
  `v[i] = v[i-1] + max(0, v[i-1] - v[i-2])`
  Example `20 40 80 160` extends to `20 40 80 160 240 320 400 480`.
  (Sentinel `-1` SkillPointValue lists are preserved unchanged: increment is 0.)

Providing all 8 values explicitly in an INI is fully supported for future data, and any
count in between works (the remainder is extrapolated from the last two provided values).

## PointDefenseLaserUpdate: `VeterancyBoost` (opt-in)

`PointDefenseLaserUpdate` (the anti-missile laser module, e.g. the USA Avenger) stock
behavior computes its interception weapon's range and shot delay with a **cleared**
`WeaponBonus`, so veterancy never affects it. A new module-data field makes it honor
the owner's weapon bonus conditions:

```ini
Behavior = PointDefenseLaserUpdate ModuleTag_PDL
  WeaponTemplate                 = AvengerPointDefenseLaser
  PrimaryTargetTypes             = BALLISTIC_MISSILE SMALL_MISSILE
  ScanRate                       = 0
  ScanRange                      = 150.0  ; must stay > base (unboosted) weapon range
  PredictTargetVelocityFactor    = 2.0
  VeterancyBoost                 = Yes    ; NEW; default No (stock behavior)
End
```

Semantics when `VeterancyBoost = Yes`:

- The interception `WeaponBonus` is computed from the owning object's **current weapon
  bonus condition flags** via the new public `WeaponTemplate::computeBonus()` (the exact
  same computation `Weapon::computeBonus()` performs when a normal weapon fires, and
  `Weapon::computeBonus` now delegates to it). That means `VETERAN`/`ELITE`/`HERO` and
  this branch's `HERO2..HERO5` conditions apply, and any other active conditions
  (e.g. `GARRISONED`, ZH container-passed bonuses) come along for the ride by design.
- **RANGE** bonus multiplies the interception firing range, and the scan/acquisition
  radius (`ScanRange`) is scaled by the same factor so acquisition keeps pace.
- **RATE_OF_FIRE** bonus shortens the effective delay between interception shots. The
  module throttles with its own frame counter (`m_nextShotAvailableInFrames`, refilled
  from `WeaponTemplate::getDelayBetweenShots(bonus)` after each shot -- it allocates,
  fires and deletes a fresh `Weapon` per shot, so the counter, not weapon readiness, is
  the throttle); the real bonus is now passed into that refill.
- **DAMAGE** already scaled with the owner's bonuses in stock code (the per-shot
  `Weapon::fireWeapon` path computes its own bonus internally); this is unchanged.
- With `VeterancyBoost = No` (or omitted) the bonus stays cleared: bit-for-bit stock
  behavior, including for saves/replays.

Note: vanilla `GameData.ini` `WeaponBonus` grants no RANGE/RATE_OF_FIRE to the
veterancy conditions (VETERAN/ELITE/HERO give DAMAGE + ROF only in ZH -- check your
`WeaponBonus` table), so a mod enabling this should also define the desired
`WeaponBonus = HERO RANGE 120%`-style entries (globally in `GameData.ini` or per-weapon
via the weapon's `WeaponBonus =` override) for the conditions it wants to matter.

Touched files: `Include/GameLogic/Weapon.h`, `Source/GameLogic/Object/Weapon.cpp`
(computeBonus hoisted to `WeaponTemplate`), `Include/GameLogic/Module/PointDefenseLaserUpdate.h`,
`Source/GameLogic/Object/Update/PointDefenseLaserUpdate.cpp` -- mirrored in both trees.

## Insignia / rank UI

- World-space insignia (`Drawable::s_veterancyImage`, `initStaticImages()`): levels 4-7
  look up the **new MappedImage names `SCVeter4` .. `SCVeter7`**; each rank falls back to
  the previous rank's image (ultimately the HEROIC `SCVeter3` chevron) when the art is not
  shipped, so the art is **data-optional**. `drawVeterancy()` also null-checks, so a
  missing image can never crash.
- Control bar rank overlay (`ControlBar::calculateVeterancyOverlayForObject/Thing`):
  levels 4-7 look up **`SSChevron4L` .. `SSChevron7L`** (`m_rankHeroic2Icon` ..
  `m_rankHeroic5Icon`, loaded next to the existing `SSChevron1L..3L`), with the same
  previous-rank fallback chain ending at the heroic icon.
- The art itself ships in a **data layer**, not this repo:
  `generalsx-mods/veterancy-insignia/` builds `zzz-ZZZZZZZVetInsignia.big` containing
  `Art\Textures\ZZVetInsignia.tga` (256x256 32-bit TGA atlas) and
  `Data\INI\MappedImages\HandCreated\VeterancyInsignia.INI` (8 new MappedImage blocks;
  all names and paths are new, nothing shared is overridden). Insignia scheme, composed
  pixel-exact from the shipped rank art (SCVeter chevrons; the ShockWave-stack SNS cameo
  chevrons) plus procedurally generated gold stars in the matching palettes:
  - HEROIC2: gold star above 1 chevron (`SCVeter4` 9x13 px, `SSChevron4L` 120x96 cell)
  - HEROIC3: gold star above 2 chevrons
  - HEROIC4: gold star above 3 chevrons
  - HEROIC5: **double** gold star above 3 chevrons (`SCVeter7` 19x19 px)
- Promotion audio: ranks above HEROIC reuse `getSoundPromotedHero()`.
- Promotion FX animation (`m_levelGainAnimationName`) already generic - fires on every
  level-up including the new ones.

## Selected-unit rank/XP readout

When exactly **one** experience-capable unit is selected, the engine draws its rank name
and raw XP progress against the next-rank threshold (e.g. `Elite 320/750 XP`) underneath
the health bar (`Drawable::drawVeterancyProgressText`, driven through the same
text-bearing-drawable path as group-number text). Multi-select shows nothing by design.
At max rank - or for units that hold a rank but can no longer gain XP - only the rank
name is shown. Strings go through `TheGameText->fetchOrSubstitute(Format)` with the
labels `GUI:VeterancyRegular` .. `GUI:VeterancyHeroic5`, `GUI:VeterancyProgress`
(format `%ls %d/%d XP`) and `GUI:VeterancyRankOnly`, falling back to hardcoded English
when the labels are absent from the string file - localizable, but no CSF additions are
required.

## "Edge of Tomorrow" respawn (engine half)

New engine capability, mirrored in both trees: `RespawnAtBuildingDie` (a die module) plus
its runtime companion `RespawnMarkerUpdate` (an update module for the invisible countdown
proxy - the RebuildHole idiom, since the dying object cannot host its own timer).

Semantics: when a unit carrying the die module is killed while (a) its player - or the
object itself - has the `TriggeredBy` upgrade and (b) its veterancy is at least
`RequiredVeterancy`, then `Delay` ms later the same object template is recreated at the
**nearest friendly `RespawnAtKindOf` building** (nearest to the death spot), exiting via
the building's production door / rally point when it has an exit interface (else placed at
the building's edge), with veterancy level **and exact XP** restored
(`PreserveExperience`) and full health (`FullHealth`).

Edge cases handled:

- **No qualifying building alive when the timer expires**: no respawn (checked at respawn
  time, so a building finished during the delay still counts; one under construction or
  being sold does not).
- **Death inside a transport/garrison**: works - `onDie` fires normally and nothing
  depends on the dying object being in the open.
- **Respawn loop**: the respawned unit keeps the module and can die and respawn again -
  intentional (that's the movie). Each death spawns exactly one one-shot marker.
- **Missing data**: if the `RespawnMarkerName` template is not defined (data layer not
  installed) or lacks `RespawnMarkerUpdate`, the module is inert - no crash.
- **Save/load**: the marker xfers all of its runtime state (template name, kindof mask,
  frame, XP, level, flags), so a save during the delay window restores the pending respawn.

The DATA half (PropCenter upgrade + wiring on China infantry) is a later data layer; it
must ship exactly this:

```ini
; on each respawn-capable unit (e.g. China infantry):
Behavior = RespawnAtBuildingDie ModuleTag_EdgeOfTomorrow
  TriggeredBy        = Upgrade_ChinaEdgeOfTomorrow ; the PropCenter-researched upgrade
  RequiredVeterancy  = HEROIC5                     ; minimum rank at death (default REGULAR)
  RespawnAtKindOf    = COMMANDCENTER               ; nearest friendly building of this kind
  Delay              = 10000                       ; ms from death to respawn
  PreserveExperience = Yes                         ; restore rank + exact XP (default Yes)
  FullHealth         = Yes                         ; force full health (default Yes)
  RespawnMarkerName  = VeterancyRespawnMarker      ; countdown proxy template (below)
End

; the invisible countdown proxy (define once):
Object VeterancyRespawnMarker
  KindOf = INERT IMMOBILE UNATTACKABLE
  Body = InactiveBody ModuleTag_Body
  End
  Behavior = RespawnMarkerUpdate ModuleTag_Respawn
  End
  Geometry = SPHERE
  GeometryMajorRadius = 1.0
  GeometryIsSmall = Yes
End

; the upgrade itself (PropCenter, ZH China):
Upgrade Upgrade_ChinaEdgeOfTomorrow
  DisplayName        = UPGRADE:ChinaEdgeOfTomorrow
  Type               = PLAYER
  BuildTime          = 60.0
  BuildCost          = 2000
  ButtonImage        = SNPCInternet ; placeholder - pick real art in the data layer
End
; plus a CommandButton (PLAYER_UPGRADE) added to the PropagandaCenter CommandSet.
```

Notes for the data layer: `TriggeredBy` is checked live at die time via
`Player::hasUpgradeComplete` / `Object::hasUpgrade` (single upgrade name, no UpgradeMux
activation state), so OBJECT-type upgrades on the unit also satisfy the gate. The usual
`DieMuxData` filters (`DeathTypes`, `ExemptStatus`, ...) apply to the module as with any
die module. `RequiredVeterancy` is a minimum (>=), parsed from `TheVeterancyNames`, so
`HEROIC5` = only max-rank units return.

Files: `Include/GameLogic/Module/RespawnAtBuildingDie.h`,
`Source/GameLogic/Object/Die/RespawnAtBuildingDie.cpp` (both modules), registered in
`Source/Common/Thing/ModuleFactory.cpp` - mirrored in both trees.

## Veterancy-gated behavior decisions

- **Weapon sets**: units with `WeaponSet ... HERO` variants keep `WEAPONSET_HERO` set
  at all levels >= HEROIC (there are no new per-level weapon sets). Same for
  `ARMORSET_HERO`.
- **Upgrade_Veterancy_***: upgrades now exist for every level
  (`Upgrade_Veterancy_VETERAN` .. `Upgrade_Veterancy_HEROIC5`), so `TriggeredBy`
  behaviors (e.g. AutoHealBehavior self-heal at VETERAN) keep working unchanged and
  modders can trigger on the new ranks. Veterancy upgrades accumulate on promotion, so
  any `TriggeredBy Upgrade_Veterancy_HEROIC` module stays satisfied at HEROIC2+.
- **Salvage crates** (`SalvageCrateCollide::eligibleForLevel`): "max level" is now
  `LEVEL_LAST` (HEROIC5) instead of HEROIC, so salvage crates can level units through
  the new ranks.
- **Veterancy crates / gainExpForLevel / cheats**: all use `LEVEL_LAST` / `LEVEL_COUNT`
  and extend automatically (the Ctrl-veterancy debug cheat now climbs to HEROIC5).
- **Hack Internet cash**: ranks above HEROIC earn the `HeroicCashAmount`.
- **Per-veterancy weapon FX / OCL / exhaust** (`VeterancyFireFX = HEROIC ...` etc.) and
  **DamageFX veterancy entries**: an entry naming HEROIC **or higher** now fills through
  `LEVEL_LAST`, so the heroic red-tracer FX etc. carry into the new ranks. Entries below
  HEROIC keep exact-level vanilla semantics, so behavior for levels 0-3 is unchanged.
- **HelicopterSlowDeath pilot ejection** (`> LEVEL_REGULAR`), **hijacker/rider level
  transfer** (`MAX` of levels), **skill points** (indexed by victim level): all correct
  automatically.

## Save / replay / network compatibility

- `ExperienceTracker` serializes `m_currentLevel` as a raw 32-bit enum; values 0-3 are
  unchanged, so **old save files load fine**. New saves that contain levels 4-7 or the
  strings `Upgrade_Veterancy_HEROIC2..5` will **not** load on older builds.
- Weapon bonus condition flags are appended at the enum tail, so old raw masks stay valid.
- Upgrade masks are saved **by name**, so the 4 extra built-in veterancy upgrades do not
  break save files. However, in `XFER_CRC` mode the raw upgrade bit mask is CRC'd and all
  INI-defined upgrades shift by 4 bits, so **multiplayer / replay CRC compatibility with
  unpatched builds is broken** (a desync-on-purpose situation typical for gameplay forks;
  patched<->patched play is consistent and deterministic).
- Old replays: deterministic only against the executable that recorded them; this build
  will mismatch retail replays that involve veterancy XP thresholds (extrapolated arrays
  do not change levels 0-3 thresholds, but salvage-crate leveling past HEROIC changes sim
  behavior).

## Upstream sync friction

- `Core/GameEngine/Include/Common/GameCommon.h` (enum) and the two `Object.cpp` /
  `ActiveBody.cpp` switch-to-predicate rewrites are the most likely merge-conflict
  points against TheSuperHackers upstream.
- `RETAIL_COMPATIBLE_CRC` builds of this branch are no longer retail-CRC-compatible
  (see above); if upstream ever gates on that, this feature must become opt-in.

## Touched files

Core (shared):
- `Core/GameEngine/Include/Common/GameCommon.h` - enum `VeterancyLevel` + `LEVEL_LAST`
- `Core/GameEngine/Source/Common/System/GameCommon.cpp` - `TheVeterancyNames`

GeneralsMD (Zero Hour) and Generals (identical mirrored edits in both trees):
- `Include/GameLogic/Weapon.h` - `WEAPONBONUSCONDITION_HERO2..5` + INI names
- `Source/GameLogic/Object/Object.cpp` - `onVeterancyLevelChanged` weapon set/bonus flags
- `Source/GameLogic/Object/Body/ActiveBody.cpp` - armor set flags, promotion sound
- `Source/Common/GlobalData.cpp` - `HealthBonus_Heroic2..5` keys + defaults
- `Include/Common/ThingTemplate.h`, `Source/Common/Thing/ThingTemplate.cpp` -
  `parseVeterancyIntList` (4-value backward compat + extrapolation)
- `Source/Common/System/Upgrade.cpp` - veterancy upgrades for all 8 levels
- `Source/Common/DamageFX.cpp` - HEROIC+ entries extend to `LEVEL_LAST`
- `Source/GameLogic/Object/Weapon.cpp` - per-vet FX/OCL/exhaust parsers extend HEROIC+
- `Source/GameClient/Drawable.cpp` - chevron images for levels 4-7 (reuse `SCVeter3`)
- `Source/GameClient/GUI/ControlBar/ControlBarCommand.cpp` - rank overlay for new levels
- `Source/GameLogic/Object/Update/AIUpdate/HackInternetAIUpdate.cpp` - heroic cash at 5+
- `Source/GameLogic/Object/Collide/CrateCollide/SalvageCrateCollide.cpp` - max level cap

## In-game test plan (manual, main session)

1. **Debug promotion cheat**: select a unit, use the debug "give veterancy" meta-key
   (`MSG_META_DEBUG_GIVE_VETERANCY`) repeatedly. Expect: 7 promotions REGULAR->HEROIC5,
   heroic chevron shown for ranks 4+ (no crash), promotion sound/FX each time, health
   bar max grows each rank (150% -> 170% -> ... -> 230%), demotion cheat walks back down.
2. **XP grinding**: give a Ranger `ExperienceScalar`-style XP via kills or the science
   cheat; confirm levels 5-8 require the 1.75x-increment thresholds (log or observe
   promotion cadence) and units killed at rank 5+ award the extrapolated
   `ExperienceValue` to the killer.
3. **Hero weapon retention**: promote a unit with a HERO weapon set (e.g. ZH ranger
   flashbang variants / any `WeaponSet ... HERO` unit) to HEROIC3+; confirm it still
   uses its HEROIC weapon and heroic red-tracer fire FX.
4. **INI overrides**: in a map `map.ini` or SagePatch-style override, add:
   - `GameData` block: `HealthBonus_Heroic2 = 200%` - confirm rank-5 health jump.
   - `Weapon` block for a test weapon: `WeaponBonus = HERO2 DAMAGE 150%` - confirm
     damage increases only at HEROIC2+.
   - An object with 8-value `ExperienceRequired = 0 50 100 200 300 400 500 600` -
     confirm explicit thresholds are honored.
5. **Salvage crates** (GLA): heroic scorpion picking up salvage keeps leveling to
   HEROIC5, then salvage reverts to money/weapon-upgrade behavior at max level.
6. **Hack Internet**: promote a hacker past HEROIC (crate or cheat); cash per hack stays
   at the heroic amount.
7. **Save/load**: save with a HEROIC4 unit, reload - level, health bonus, weapon bonus
   conditions and chevron all restored. Also load a pre-branch save (levels 0-3).
8. **Control bar**: select a rank-5+ unit - heroic rank overlay on the portrait, no UI
   overflow; also check contained-unit promotion marks UI dirty correctly.
9. **Crates/scripts**: place a `VeterancyCrateCollide` crate and a script granting
   veterancy (`StartingLevel = HEROIC3` VeterancyGainCreate object) - INI names parse.
