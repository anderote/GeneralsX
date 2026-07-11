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

---

# GeneralsX Engine Features - "Command & Control" batch

Order-system features on top of the QoL batch. GeneralsMD (Zero Hour / `z_generals`)
is the target; shared `Core/` order dispatch is touched where required and the
`Generals` tree gets the minimal mirror needed to keep `g_generals` compiling.
Each feature is a separate commit; INI/order contracts below are authoritative for a
future data layer. All features are inert / backward-compatible with no INI edits.

## C0. Crash fix family: container-death rider ejection

A recurring mid-skirmish `SIGSEGV` when a container (vehicle / garrisoned building)
dies while carrying infantry that was **also** killed by the same damage event
(dense garrison + AoE / chain damage). Same family as the earlier `onDisabledEdge`
fix (`bc51e34`).

**Root cause:** `OpenContain::onDie` runs `killRidersWhoAreNotFreeToExit()` /
`processDamageToContained()`, which call `destroyObject()`/`kill()` on riders.
`destroyObject()` sets `OBJECT_STATUS_DESTROYED` immediately but **defers** teardown,
so already-destroyed riders remain in `m_containList`. `removeAllContained()` then
ejects them via `removeFromContainViaIterator`, which re-places them in the world
(`addOrRemoveObjFromWorld` / `setPosition` -> `handlePartitionCellMaintenance` ->
`handleShroud` -> `unlook`; and `onRemoving` -> `clearDisabled` -> `onDisabledEdge`),
dereferencing partition / behavior state already freed & nulled in `~Object`
(`m_partitionLastLook` etc.) -> `SIGSEGV far=0x1c`.

**Fix (deterministic, only the crashing edge changes):**
- **Root-cause guard** at the chokepoint `OpenContain::removeFromContainViaIterator`:
  `rider->isDestroyed()` (reliable tombstone - `OBJECT_STATUS_DESTROYED` is set early
  and never cleared) -> keep list/count bookkeeping but **skip all live-object
  placement & notification**; the rider is reaped normally on the destruction pass.
  `isDestroyed()==false` guarantees partition state is intact, so the live path is
  byte-identical; merely-dead-but-not-destroyed riders still eject normally.
- **Belt-and-suspenders** null guards on `Object::handlePartitionCellMaintenance`
  and `Object::unlook` (`m_partitionLastLook == nullptr`).

Audited the full call tree (`removeFromContainViaIterator`, `handlePartitionCellMaintenance`,
`unlook`/`handleShroud`/`look`/value/threat, `onDisabledEdge`, `processDamageToContained`,
`killRidersWhoAreNotFreeToExit`, `onDelete`/`onCollide`, `SightingInfo::isInvalid`).
No INI.

Files: `Source/GameLogic/Object/Contain/OpenContain.cpp`,
`Source/GameLogic/Object/Object.cpp`.

## C1. Multi-select buildings + bulk build order

Selecting several production buildings and clicking a build cameo now queues the
unit at **every** selected compatible factory, not just the primary (e.g. select 5
Barracks, click Ranger -> all 5 queue it). Composes with the QoL shift-x5 (5 each
across N buildings). Deterministic & network-safe: still N ordinary
`MSG_QUEUE_UNIT_CREATE` messages, each now carrying an **explicit producer objectID**
(3rd arg). The logic layer validates that object is owned by the message player
(anti-exploit) and routes the queue there; single-arg senders keep the retail
single-selection path. No INI.

Files: `Source/GameClient/GUI/ControlBar/ControlBarCommandProcessing.cpp`
(`GUI_COMMAND_UNIT_BUILD` fan-out), `Core/.../GameLogicDispatch.cpp`
(`onQueueUnitCreate` explicit-producer path).

## C2. Combat stances (per-unit posture)

A per-unit `UnitStance` state, player-settable, that modulates auto-target-acquisition
and pursuit. Distinct from `AttitudeType`/the mood matrix, which only governs
**AI-controlled** units (`getMoodMatrixActionAdjustment` returns `Action_Ok` for human
players) - stances work for human player units.

Stances (enum `UnitStance` in `AI.h`; INI names in parens):

| Stance | INI name | Behavior |
|--------|----------|----------|
| `STANCE_AGGRESSIVE` (0, default) | `AGGRESSIVE` | auto-acquire **and pursue** (vanilla) |
| `STANCE_DEFENSIVE` (1) | `DEFENSIVE` | auto-acquire, fire in range, **no pursuit** |
| `STANCE_HOLD_POSITION` (2) | `HOLD_POSITION` | fire in range, no pursuit (never move for AI reasons) |
| `STANCE_HOLD_FIRE` (3) | `HOLD_FIRE` | **never auto-fire** (explicit orders still fire) |

**Mechanism.** `AIUpdateInterface::setStance()` stores the stance and derives pursuit:
`setAllowedToChase(stance == AGGRESSIVE)`. `m_allowedToChase` is consulted only for
`CMD_FROM_AI` (auto-acquired) attacks in the attack state machine, so an explicit
player attack / force-fire order pursues regardless of stance. `HOLD_FIRE` is enforced
in `getNextMoodTarget()` (the single funnel for idle-scan + retaliation auto-fire),
which returns null - explicit orders bypass it. Default `AGGRESSIVE == 0` so a
zero-initialized / pre-feature unit is bit-for-bit vanilla. Xfer version bumped
**5 -> 6** (non-retail; retail-compatible builds cap at v4 and simply don't persist
stance, loading as `AGGRESSIVE`).

**Command / order contract (for the data layer):**
- New `GUICommandType` **`GUI_COMMAND_SET_STANCE`** with a `CommandButton` field
  **`Stance = DEFENSIVE`** (`AGGRESSIVE`/`DEFENSIVE`/`HOLD_POSITION`/`HOLD_FIRE`).
  Clicking issues the networked **`MSG_SET_UNIT_STANCE`** (int stance).
- Data adds 4 `CommandButton` blocks (one per stance) and wires them onto unit
  command sets. Example:
  ```ini
  CommandButton Command_StanceDefensive
    Command = SET_STANCE
    Stance  = DEFENSIVE
    ButtonImage = SNMoveOrder   ; placeholder - pick real art in the data layer
  End
  ```
- Dispatch: `MSG_SET_UNIT_STANCE` -> `AIGroup::groupSetStance(stance)` fans out to
  every selected member's AI. Deterministic (int arg only). No-op stub in the
  `Generals` tree (behavior is ZH-only).

**Per-building default stance (INI contract, `ProductionUpdate` block):**
```ini
Behavior = ProductionUpdate ModuleTag_xx
  DefaultUnitStance = DEFENSIVE   ; units roll off this factory in this stance
  ; ...                          ; omitted / -1 = leave AGGRESSIVE (vanilla)
End
```
Applied to each produced unit as it exits the factory. This is the intended fix for
"artillery won't shoot / tanks roll off too aggressive": ship artillery factories
with `DefaultUnitStance = DEFENSIVE` (or `HOLD_POSITION`) so artillery holds ground.

**Note on target-type preference (separate from stance):** "tanks ignore buildings /
artillery won't shoot units" is partly a **target-selection preference** problem
(which target the unit picks among several), NOT posture. Stance only controls
*whether* the unit acquires/pursues, not *which* target it prefers. A separate fix
(weapon `AntiMask` / target-type priority tuning, or a target-preference field) is
needed for the preference half; this feature does not address it.

**Deferred / notes:** `DEFENSIVE` and `HOLD_POSITION` currently share the same combat
behavior (fire in range, no pursuit); the finer "`HOLD_POSITION` also refuses
get-out-of-way / scatter / repulsor moves" distinction is not yet wired (the enum,
command and INI contract preserve it for a future refinement). All four stances,
the command mechanism, and the per-building default ship and are deterministic.

Files: `Include/GameLogic/AI.h` (`UnitStance` enum + `groupSetStance` decl),
`Include/GameLogic/Module/AIUpdate.h` + `Source/GameLogic/Object/Update/AIUpdate.cpp`
(`m_stance`, `setStance`, `getNextMoodTarget` gate, xfer v6),
`Source/GameLogic/AI/AIGroup.cpp` (`groupSetStance`),
`Include/GameClient/ControlBar.h` + `Source/GameClient/GUI/ControlBar/ControlBar.cpp`
(`GUI_COMMAND_SET_STANCE` + `Stance` field), `.../ControlBarCommandProcessing.cpp`
(issue message), `Include/Common/MessageStream.h` + `Source/Common/MessageStream.cpp`
(`MSG_SET_UNIT_STANCE`), `Include/GameLogic/Module/ProductionUpdate.h` +
`Source/GameLogic/Object/Update/ProductionUpdate.cpp` (`DefaultUnitStance`),
`Core/.../GameLogicDispatch.cpp` (dispatch). Generals tree: `MessageStream.*`, `AI.h`,
`AIGroup.cpp` minimal mirror (no-op `groupSetStance`).

## C3. Guard a moving unit (escort)

The guard cursor placed directly over a **friendly mobile unit** now issues a
guard-**object** order (escort) instead of the vanilla position-locked
guard-**position** snapshot: the guards follow and defend that unit as it moves
(artillery-with-escorts). The guard state machine already tracks a moving guardee -
`AIGuardIdleState::update` re-reads the target's position each scan and, when it has
moved past ~2 pathfind cells, transitions to `AIGuardReturnState` to catch up, while
the inner/outer/attack states re-read the live target position on entry - so this is
purely an **input** change that unlocks the existing engine capability.

Semantics: a friendly (own or allied, `getRelationship == ALLIES`) target that has an
`AIUpdateInterface` and is **not** a `KINDOF_STRUCTURE` triggers `MSG_DO_GUARD_OBJECT`
(objectID + guardMode). Guarding a building or empty ground still falls through to
`MSG_DO_GUARD_POSITION` (unchanged). Deterministic - the message carries the target's
objectID; all follow math runs in-sim. Works with any guard mode
(`NORMAL`/`WITHOUT_PURSUIT`/`FLYING_UNITS_ONLY`). No INI; no data changes required
(applies to the stock Guard command button).

Files: `Core/.../GUICommandTranslator.cpp` (`doGuardCommand` friendly-unit escort
path). Uses existing `AIGroup::groupGuardObject` / `AIUpdateInterface::privateGuardObject`
/ the `AIGuardMachine` object-tracking states (unchanged).

## C4. Line-move / formation-move (BAR-style)

A right-button **drag** in ALTERNATE mouse mode lays the selected group out evenly
along the dragged segment (start -> end), instead of clumping at a point - the
classic "formation move" from BAR / modern RTS. RMB-drag is otherwise a no-op in
alternate mode, so there is no conflict.

**Order / determinism:** new networked message **`MSG_DO_MOVETO_LINE`**
(location start, location end, int shiftDown), appended in the 1000-1999 range next
to `MSG_DO_MOVETO`. The message carries only the two endpoints + a shift bit; **all
fan-out math runs in-sim** in `AIGroup::groupMoveToLine`, so it is fully
deterministic / network-safe. Dispatch: `MSG_DO_MOVETO_LINE` ->
`groupMoveToLine(start, end, shiftQueue, CMD_FROM_PLAYER)`.

**Slot assignment (`AIGroup::groupMoveToLine`):** collect movable members (same skip
filters as the per-member move loop: `DISABLED_HELD` occupants, `KINDOF_IMMOBILE`,
non-AI); project each unit onto the line direction and **sort ascending by that
projection** (sorted-projection -> assigned slots preserve left-to-right order, so
paths don't cross); slot i = `start + (i/(N-1))*(end-start)`, clamped to terrain via
`pathfinder()->adjustDestination`; then `aiMoveToPosition` (or `aiFollowPathAppend`
when the shift bit queues it). Straight-line v1 (curved formations deferred). Natural
per-unit facing (explicit perpendicular facing deferred). Degenerate (zero-length)
drag falls back to an ordinary `groupMoveToPosition`.

Input in alt mode also requires a controllable selection, no active GUI command, and
not waypoint mode. Shift queues the line as an appended order.

**Deferred:** the live drag **preview** (ghost line + N slot ticks while dragging) is
not yet drawn - it needs mirrored `InGameUI` state + a `W3DInGameUI::draw()` render in
both trees (display-only, non-deterministic). The order itself is fully functional;
the line is applied on button release. Curved formations and explicit facing are also
deferred.

Files: `Include/Common/MessageStream.h` + `Source/Common/MessageStream.cpp`
(`MSG_DO_MOVETO_LINE`, both trees), `Core/.../CommandXlat.cpp` (RMB-drag input),
`Core/.../GameLogicDispatch.cpp` (dispatch), `Include/GameLogic/AI.h` +
`Source/GameLogic/AI/AIGroup.cpp` (`groupMoveToLine`, both trees).

## C5. Waypoint / patrol upgrade  (DEFERRED - design only)

Not implemented in this batch. Deferred to keep both trees building clean and the
sim deterministic, because it requires determinism-critical changes to the movement
state machine + save/xfer versioning that were too large to land safely alongside the
rest of the batch. Recorded here so a follow-up (or the data layer) can pick it up.

**Current behavior (what already exists):**
- Player waypoints are built incrementally: waypoint mode -> `MSG_ADD_WAYPOINT` ->
  `AIGroup::groupMoveToPosition(pos, addWaypoint=TRUE)` -> per unit
  `aiFollowPathAppend` -> `AIStateMachine::addToGoalPath` (the `m_goalPath`
  `std::vector<Coord3D>`), consumed by `AIFollowPathState` (`AI_FOLLOW_PATH`). This
  is a plain move (no enemy engagement) and is **not** looped.
- The goal path lives on each unit's state machine, so it **already survives
  reselection at the sim level** (units keep moving); only the on-screen preview
  overlay is lost - sub-feature (3) is essentially a cosmetic gap.
- `aiAttackFollowWaypointPath` / `AI_ATTACKFOLLOW_WAYPOINT_PATH_*` (attack-move down a
  **map `Waypoint` chain**) already exists but is script-facing and consumes a
  `Waypoint` object, not the player's dynamic `m_goalPath`.

**Design to implement:**
1. **Attack-move along waypoints:** add an `m_engageWhileFollowing` flag to
   `AIFollowPathState` (or a parallel `AIAttackFollowPathState`) so that while
   traversing `m_goalPath` it runs the idle-scan / `getNextMoodTarget` engage logic
   (respecting the new combat stance, C2) and resumes the path after the fight.
   Expose via a group order `groupFollowPathAttack` + an input modifier (e.g. a
   "attack-waypoint" toggle or Alt while placing waypoints).
2. **Patrol loop:** store the completed path (copy `m_goalPath`) plus a
   `m_patrolLoop` bool on the state machine; when `AIFollowPathState` reaches the end
   and loop is set, re-seed `m_goalPath` from the stored copy (optionally reversed for
   ping-pong) and continue. Bump the relevant state-machine xfer version and persist
   the stored path + loop flag.
3. **Preview persistence:** redraw the queued waypoint path for the current selection
   from each unit's `m_goalPath` (display-only), so reselecting a patrolling group
   re-shows its route.

All three are deterministic (path math is in-sim); the only cross-network input is the
order + a couple of bits. Estimated scope is comparable to the combat-stances feature
(new state flags, a group order + message, xfer version bumps in both trees).

---

# GeneralsX Engine Features - "Graphics quality / patrol" batch

Three features on `feature/graphics-quality`, GeneralsMD (Zero Hour / `z_generals`) target.
Feature G1 completes the deferred C5 waypoint/patrol design above; G2 and G3 are the
persistent-wrecks and drawable-tracer readability features. Each is a separate focused edit
set. Determinism rule respected throughout: gameplay-affecting behavior only ever moves through
standard networked messages or through identically-read GlobalData; the two readability features
are display-only (or a hulks-only, config-gated sim tweak that stays bit-identical across peers).

## G1. Waypoint / patrol movement order  (implements the deferred C5 patrol loop)

Shift-click now lays a **multi-point waypoint path** (one queued leg per click) and a **patrol
toggle** makes the unit loop that path.

**Client-side shift-click accumulation.** In `CommandTranslator::issueMoveToLocationCommand`
(`Core/.../MessageStream/CommandXlat.cpp`) an ordinary move-click issued while **Shift** is held
(`TheKeyboard->isShift()`) now emits **`MSG_ADD_WAYPOINT`** instead of `MSG_DO_MOVETO` - exactly
the message the existing (Alt) waypoint mode uses. So each shift-click is one standard, networked,
per-leg queued-move message, dispatched in-sim by `AIGroup::groupMoveToPosition(addWaypoint=TRUE)`
-> `aiFollowPathAppend` -> `AIStateMachine::addToGoalPath`. The sim's `privateFollowPathAppend`
already does the right thing (first click while idle starts a fresh path; subsequent clicks append),
so no new client accumulation state is needed. **Deterministic:** reuses the pre-existing
`MSG_ADD_WAYPOINT` path; only the client's choice of message type changed.

**Patrol loop (sim).** `AIStateMachine` gains a `Bool m_patrolLoop` (accessors
`setPatrolLoop`/`getPatrolLoop`) that lives right next to the already-persisted `m_goalPath`.
`AIFollowPathState::update()` - at the "reached the end of the path" point - re-seeds the walk
index to 0 and keeps going when `m_patrolLoop` is set and the path has >= 2 legs (the goal path is
never consumed, only indexed, so looping is just an index reset). Any fresh order clears the flag
via `AIStateMachine::clear()`. Xfer version bumped **1 -> 2** (append-only; v1 saves load with
patrol off). Because `m_goalPath` was already xfer'd, the loop is fully replay/save-safe.

**Patrol order dispatch.** New networked message **`MSG_DO_PATROL`** (no arguments, toggle) ->
`AIGroup::groupDoPatrol(CMD_FROM_PLAYER)`. For each selected member with a real multi-leg goal path
it toggles the loop flag; when enabling on a unit that already finished its path it re-issues the
stored path from index 0 (via `aiFollowPath`, then sets the flag - `aiFollowPath` -> `clear()`
would otherwise wipe it) so patrolling starts immediately. All path/loop math runs in-sim; the only
thing crossing the wire is the toggle bit, so it is deterministic / network-safe. The client trigger
is a new `GUICommandType` **`GUI_COMMAND_TOGGLE_PATROL`** (mechanism only, mirrors the C2 stance
button pattern) which emits `MSG_DO_PATROL`; the data layer wires it onto a command button.

Access to the protected `getStateMachine()` is via new public friend wrappers on
`AIUpdateInterface` (`friend_getGoalPathSize` / `friend_getPatrolLoop` / `friend_setPatrolLoop`).

Files: `Core/.../MessageStream/CommandXlat.cpp` (shift-click -> MSG_ADD_WAYPOINT),
`Include/Common/MessageStream.h` + `Source/Common/MessageStream.cpp` (`MSG_DO_PATROL`, both trees),
`Core/.../GameLogicDispatch.cpp` (dispatch -> groupDoPatrol),
`Include/GameLogic/AIStateMachine.h` + `Source/GameLogic/AI/AIStates.cpp`
(`m_patrolLoop`, ctor/clear init, xfer v2, `AIFollowPathState::update` loop),
`Include/GameLogic/Module/AIUpdate.h` (friend patrol accessors),
`Include/GameLogic/AI.h` + `Source/GameLogic/AI/AIGroup.cpp` (`groupDoPatrol`),
`Include/GameClient/ControlBar.h` (`GUI_COMMAND_TOGGLE_PATROL` enum + name),
`Source/GameClient/GUI/ControlBar/ControlBarCommandProcessing.cpp` (emit `MSG_DO_PATROL`).
Generals tree mirror (keeps `g_generals` linking; behavior is ZH-only): `MessageStream.*`
(`MSG_DO_PATROL`), `AI.h` + `AIGroup.cpp` (no-op `groupDoPatrol`).

## G2. Persistent vehicle wrecks (config-driven hulk lifetime)

When a vehicle dies it becomes a **HULK** - a real `KINDOF_HULK` *GameLogic Object* whose
on-battlefield lifetime is owned by `LifetimeUpdate` (then `SlowDeathBehavior` sinks + destroys it).
There is no purely client-side lifetime knob on the hulk (the drawable is removed when the sim
destroys the object; the client `m_expirationDate` path is only valid for object-less drawables),
so a "keep wrecks longer" tweak necessarily touches logic. It is therefore implemented as a
**deterministically-gated** sim change rather than a client hack.

New GlobalData field **`WreckLifetimeScale`** (default `1.0` = stock). In the `LifetimeUpdate`
constructor, for `KINDOF_HULK` objects only (and only when no script `HulkMaxLifetimeOverride` is in
play), the computed lifetime `delay` is multiplied by `TheGlobalData->m_wreckLifetimeScale` when that
scale exceeds 1.0 (it never shortens a wreck), and `m_dieFrame` is updated to match.

**Why it's deterministic-safe:** the multiply is applied **after** the unchanged
`GameLogicRandomValue` draw, so the RNG stream stays aligned across all objects; the scale is read
identically from `GlobalData` on every peer (config is part of the synchronized deterministic input,
exactly like the stock `MaxLifetime` value itself); and the result flows through the already-xfer'd
`m_dieFrame`. No per-player input, no new network message, no RNG divergence -> the sim stays
bit-identical across clients and in replays. Gated to hulks so no other timed object
(mines / timed effects / OCL spawns) is affected.

**INI contract** (`GameData` / `GlobalData`):

```ini
WreckLifetimeScale = 1.0    ; multiplier on HULK on-battlefield lifetime (default 1.0 = stock)
                            ; e.g. 3.0 keeps wrecks around ~3x longer; values <= 1.0 never shorten
```

Files: `Include/Common/GlobalData.h`, `Source/Common/GlobalData.cpp` (field + parse `WreckLifetimeScale`
+ default), `Source/GameLogic/Object/Update/LifetimeUpdate.cpp` (hulk-gated scale in the ctor).

## G3. Drawable weapon tracers

A subtle, client-side tracer streak for direct-fire weapons whose FireFX has **no** tracer of its
own, config-toggleable and off by default. Tracers in this engine are already an FXList nugget
(`TracerFXNugget`) that spawns a self-expiring tracer `Drawable`; this feature reuses that machinery
for weapons that lack one.

New GlobalData bool **`ExtraTracers`** (default `FALSE`). The draw is added in the **client-side**
FX dispatch `W3DModelDraw::handleWeaponFireFX` (`Core/GameEngineDevice/...`, display-only): when
`TheGlobalData->m_extraTracers` is set, the weapon has a target position, and the weapon's FireFX
does **not** already contain a tracer (`fxl == nullptr || !fxl->hasTracer()`), it calls
`FXList::doSubtleTracer(muzzle, weaponSpeed, victimPos)`. That helper (co-located with
`TracerFXNugget` in `Core/.../FXList.cpp`) spawns one `GenericTracer` drawable from muzzle toward the
target with deliberately subtle parameters (thin, short, half-opacity, warm color, ~40% probability
roll) that self-expires after the flight time.

**Why it's determinism-safe:** everything is display-only. `doSubtleTracer` uses
`GameClientRandomValueReal` (the client RNG, never the logic RNG), creates only an object-less,
self-expiring `Drawable`, and writes **no** `Object`/logic state; `handleWeaponFireFX` is the
client FX hook, not the damage path (`dealDamageInternal` is untouched). Tracer presence detection
uses a new `FXNugget::isTracer()` virtual (default `FALSE`, overridden `TRUE` in `TracerFXNugget`)
surfaced through `FXList::hasTracer()`, so weapons that already show a tracer are skipped (no
doubling).

**INI contract** (`GameData` / `GlobalData`):

```ini
ExtraTracers = Yes     ; draw a subtle tracer for direct-fire weapons that lack one (default No)
```

Files: `Include/Common/GlobalData.h`, `Source/Common/GlobalData.cpp` (field + parse `ExtraTracers`
+ default), `Core/GameEngine/Include/GameClient/FXList.h` (`FXNugget::isTracer`, `FXList::hasTracer`,
`FXList::doSubtleTracer`), `Core/GameEngine/Source/GameClient/FXList.cpp` (`TracerFXNugget::isTracer`
override, `hasTracer` + `doSubtleTracer` impl),
`Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp`
(`handleWeaponFireFX` extra-tracer pass).

## Build

Every changed translation unit compiles clean under the `macos-vulkan` preset
(`RelWithDebInfo`, clang, from `compile_commands.json`): the 10 feature TUs plus the two
Generals-tree mirror TUs plus `ControlBar.cpp` (verifies the `GUI_COMMAND_TOGGLE_PATROL`
name-table `static_assert`). A full `z_generals` link could **not** be produced in this
environment because the `dxvk` dependency triggers a `meson` reconfigure that requires
`glslangValidator`, which is not installed here (pre-existing toolchain gap, unrelated to these
changes). The main session performs the full build/link/deploy.
