# GeneralsX Engine Features - "QoL core" batch

Engine-side features added on top of the extended-veterancy work (see
`VETERANCY8.md`). All changes are **GeneralsMD (Zero Hour) only** - the primary
`z_generals` target - so the `Generals` tree / `g_generals` is untouched. No
shared `Core/` files were modified in this batch.

Each feature is a separate commit. INI contracts below are the authoritative
spec for a future data layer; unless stated otherwise, every feature is inert /
backward-compatible with no INI edits (opt-in or default-neutral).

---

## 0. Crash fix: null deref in `onDisabledEdge` during container-death rider cleanup

Not a feature - a defensive fix for a recurring mid-skirmish `SIGSEGV`
(symbolicated, `far=0x0`).

**Root cause (confirmed: null `m_behaviors`, not a dangling module):** when a
container (garrisoned building / transport / tank bay) dies,
`Object::onDie -> OpenContain::onDie -> processDamageToContained ->
TransportContain::onRemoving` calls `rider->clearDisabled(DISABLED_HELD)`, which
can cross a disabled edge into `Object::onDisabledEdge`. If that rider was **also
destroyed in the same damage event** (dense garrison + Shock Trooper AoE /
chain-lightning + SUBDUAL), its destructor already ran
`delete[] m_behaviors; m_behaviors = nullptr`, so the loop `for(module =
m_behaviors; *module; ...)` faulted reading address 0 (a dangling module would
fault at a garbage vtable address instead - the `far=0x0` proves it was the
array pointer itself).

**Fix:** guard the iteration on non-null `m_behaviors`
(`Object::onDisabledEdge`). A live or merely effectively-dead object always has a
valid `m_behaviors` and runs exactly as before; only a fully torn-down rider
no-ops. Deterministic - no behavior change for the normal case.

Files: `Source/GameLogic/Object/Object.cpp` (`Object::onDisabledEdge`).

---

## 1. Resolution-scaled floating health bars & veterancy readout

The floating health box and the single-unit stats readout are sized in raw
screen pixels, so at high resolutions (e.g. 3440x1440) they shrank relative to
the larger-rendered units (upstream #108 / #867 / #1607). A modest UI scale
factor is derived from the display height vs a configurable baseline, clamped,
and applied to the health-box width/height and the readout font.

**INI contract** (`GameData` / `GlobalData`):

```ini
UIFloatingScaleReferenceHeight = 600    ; baseline vertical resolution (default 600)
UIFloatingScaleMax             = 2.5     ; clamp on the scale factor  (default 2.5)
```

Scale = `clamp(displayHeight / UIFloatingScaleReferenceHeight, 1.0,
UIFloatingScaleMax)`. It never shrinks below the authored size. Set
`UIFloatingScaleReferenceHeight <= 0` to disable scaling entirely (bit-for-bit
stock sizing).

Files: `Include/Common/GlobalData.h`, `Source/Common/GlobalData.cpp`,
`Source/GameClient/Drawable.cpp` (`ResolveFloatingUIScale`, `computeHealthRegion`,
`ResolveVeterancyProgressFont`).

---

## 2. Per-unit kill counter

`ExperienceTracker` tracks a lifetime enemy-kill tally per object
(`getKillCount()` / `addKill()`). `Object::scoreTheKill()` credits one kill per
destroyed enemy, gated by the same enemy / own-unit guards the score keeper uses,
and counts **even for non-trainable killers** so the tally is independent of
veterancy eligibility. Surfaced in the single-unit stats panel (feature 3).

Save/replay: `ExperienceTracker` xfer version bumped **2 -> 3** (append-only);
old saves load with count 0. No INI.

Files: `Include/GameLogic/ExperienceTracker.h`,
`Source/GameLogic/Object/ExperienceTracker.cpp`,
`Source/GameLogic/Object/Object.cpp` (`scoreTheKill`).

---

## 3. Single-unit stats panel

Replaces the single-line veterancy XP readout with a compact multi-line block,
drawn under the health bar **only when exactly one experience-capable unit is
selected** (multi-select shows nothing, unchanged gate). Lines:

1. rank + XP to next (or rank-only at max / for non-trainable ranked units)
2. current / max health
3. current weapon damage & range, **with the unit's veterancy / upgrade /
   garrison `WeaponBonus` applied** via `WeaponTemplate::computeBonus()` - the
   same path the PDL `VeterancyBoost` uses, so the numbers match what the unit
   actually fires
4. movement speed (mobile units, honoring current damage state)
5. lifetime kill count (feature 2)

`Drawable::m_veterancyProgressString` is now an array
(`MAX_UNIT_STAT_LINES = 6`), one cached `DisplayString` per line; the font
re-resolves on text change so the resolution scale (feature 1) stays in sync.
Rank names keep their localizable labels + English fallbacks
(`GUI:VeterancyRegular` .. `GUI:VeterancyHeroic5`, `GUI:VeterancyProgress`,
`GUI:VeterancyRankOnly`); the extra stat lines are plain formatted text, so **no
CSF additions are required**.

Files: `Include/GameClient/Drawable.h`, `Source/GameClient/Drawable.cpp`
(`drawVeterancyProgressText`).

---

## 4. Shift-click queues 5 units

Holding **Shift** while clicking a build cameo (`GUI_COMMAND_UNIT_BUILD`)
enqueues a batch of **5** instead of 1. Applies to any
`ProductionUpdateInterface` factory, so it covers **infantry barracks and vehicle
war factories** (both use this command path). Implemented as N ordinary
`MSG_QUEUE_UNIT_CREATE` messages (each with its own unique production id), so it
is fully **deterministic and network-safe**; the logic layer re-validates money /
queue space / per-player caps per message at execution, so extras that can't be
afforded or don't fit are simply rejected there. Shift state via
`TheKeyboard->isShift()`. No INI (batch size is the constant
`SHIFT_QUEUE_COUNT = 5`).

Files: `Source/GameClient/GUI/ControlBar/ControlBarCommandProcessing.cpp`
(`GUI_COMMAND_UNIT_BUILD`).

---

## 5. Hackers hack anywhere

Extends the Internet Center's "start hacking on enter" behavior to **every**
container type: `OpenContain::onContaining` auto-starts Internet hacking for any
`KINDOF_MONEY_HACKER` passenger, so a hacker earns money while garrisoned in a
building, riding a transport, in a tank bay (Overlord / Helix) or a bunker
(Tunnel / Cave). Because every contain module chains to
`OpenContain::onContaining`, this single hook covers them all.

**Income-rate semantics:** `HackInternetAIUpdate::getCashUpdateDelay()` already
returns the fast delay (`m_cashUpdateDelayFast`) whenever the hacker has *any*
container, so hacking in any container pays at the contained "fast" rate for
consistency - no rate change was needed.

The dedicated `InternetHackContain` already triggers hacking for its riders, so
it overrides `isDedicatedHackContain() -> TRUE` to suppress the base
auto-trigger and avoid starting hacking twice for the same rider. No INI (gate
is the `KINDOF_MONEY_HACKER` kindof the hacker already has).

Files: `Include/GameLogic/Module/OpenContain.h`,
`Include/GameLogic/Module/InternetHackContain.h`,
`Source/GameLogic/Object/Contain/OpenContain.cpp`.

---

## 6. `RequiredUpgrade` prerequisites on `UpgradeTemplate`

An optional `RequiredUpgrade` field on Upgrade definitions. The player must own
**every** listed upgrade (completed) before the dependent upgrade can be
purchased; its cameo greys out until then. Obsoletes the data-side
`CommandSetUpgrade` enumeration hack.

Implemented in `UpgradeCenter::canAffordUpgrade` (the long-standing prereq
`TODO`), which is the **single funnel** for both the client cameo greying
(`ControlBarCommand`) and the logic-side purchase gate (`ProductionUpdate` /
command processing), so a prereq-blocked upgrade both greys out **and** cannot be
researched. Also naturally covers AI purchase decisions
(`AIPlayer` / `AIGroup` call the same check).

**INI contract** (`Upgrade` block):

```ini
Upgrade Upgrade_Foo
  ; ...
  RequiredUpgrade = Upgrade_A Upgrade_B   ; player must own ALL of these first
End
```

Empty / omitted = no prerequisite (stock behavior). An unknown prereq name fails
safe (blocks + `DEBUG_CRASH`). On a blocked **user-initiated** purchase the game
shows `GUI:UpgradePrerequisiteNotMet` (data should add this CSF label; the cameo
greying itself needs no label). Prereqs are **player-scoped** via
`Player::hasUpgradeComplete`.

Files: `Include/Common/Upgrade.h`, `Source/Common/System/Upgrade.cpp`.

---

## 7. Opt-in vision scales with veterancy

A unit that sets `VisionBonusFromVeterancy = Yes` grows its effective sight and
shroud-clearing range with veterancy rank. Applied at **rank-change time** in
`Object::onVeterancyLevelChanged` (not per-frame, so it stays off the hot shroud
path), recomputed from the **template base** each time so it is idempotent and
never compounds across promotions / demotions. Sight range and shroud-clearing
range scale by the same per-rank factor so acquisition and reveal stay in step.

**INI contract:**

```ini
; per object (ThingTemplate): opt in
VisionBonusFromVeterancy = Yes            ; default No

; per-rank multiplier (GameData / GlobalData), overridable:
VisionBonus_Veteran = 110%
VisionBonus_Elite   = 120%
VisionBonus_Heroic  = 130%
VisionBonus_Heroic2 = 140%
VisionBonus_Heroic3 = 150%
VisionBonus_Heroic4 = 160%
VisionBonus_Heroic5 = 170%
```

Defaults are a modest +10%/rank curve (Veteran 110% .. Heroic5 170%); Regular is
always 100%. **Caveat:** scaling is relative to the template's
`VisionRange`/`ShroudClearingRange`, so a per-map object override of vision is
superseded once the unit changes rank.

Files: `Include/Common/GlobalData.h`, `Source/Common/GlobalData.cpp`,
`Include/Common/ThingTemplate.h`, `Source/Common/Thing/ThingTemplate.cpp`,
`Source/GameLogic/Object/Object.cpp` (`onVeterancyLevelChanged`).

---

## 8. Rank-gated command abilities (`RequiredVeterancy` on `CommandButton`)

The enabler: a general engine mechanism to gate a `CommandButton` on the owning
unit's veterancy rank. `CommandButton` gains an optional `RequiredVeterancy`
field (parsed via `TheVeterancyNames`, same as elsewhere);
`ControlBar::getCommandAvailability` - the single funnel for cameo availability -
greys the button (`COMMAND_RESTRICTED`) whenever the unit's veterancy level is
below the threshold (`>=` comparison). `LEVEL_REGULAR` (default) means no gate,
so all existing buttons are unaffected.

This is the **mechanism only** - no specific ability is wired here. Future data
can, e.g., give an Elite tank a speed-boost order by adding `RequiredVeterancy`
to that button's `CommandButton` block.

**INI contract** (`CommandButton` block):

```ini
CommandButton Command_EliteSpeedBoost
  ; ...
  RequiredVeterancy = ELITE   ; usable only at ELITE rank or higher
End
```

Accepts `REGULAR` / `VETERAN` / `ELITE` / `HEROIC` / `HEROIC2` .. `HEROIC5`.
**Note:** this gates client cameo *availability*; per-command *execution*
enforcement can be layered on when a concrete rank-gated order is added.

Files: `Include/GameClient/ControlBar.h`,
`Source/GameClient/GUI/ControlBar/ControlBar.cpp` (parse table + ctor),
`Source/GameClient/GUI/ControlBar/ControlBarCommand.cpp`
(`getCommandAvailability`).
