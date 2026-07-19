/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// FILE: RespawnAtBuildingDie.h ///////////////////////////////////////////////////////////////////
// GeneralsX @feature "Edge of Tomorrow" respawn.  When a unit carrying this die module is killed
// while (a) its player (or the object itself) has the TriggeredBy upgrade and (b) its veterancy
// is at least RequiredVeterancy, then Delay ms later the same object template is recreated at
// the nearest friendly RespawnAtKindOf building (rally-point exit if the building has one), with
// its veterancy level and exact experience restored and full health.
//
// Because the dying object is destroyed in the same frame, the countdown is carried by an
// invisible proxy object (the RebuildHole idiom): onDie spawns RespawnMarkerName on the dying
// unit's team at the death position and hands all runtime parameters to the marker's
// RespawnMarkerUpdate module, which counts down, performs the respawn and destroys itself.
// If no qualifying building is alive when the timer expires there is no respawn.  The respawned
// unit keeps this module, so it can die and respawn again by design.
//
// Expected INI usage (all data ships in a data layer, not this repo):
//
//   Behavior = RespawnAtBuildingDie ModuleTag_EdgeOfTomorrow
//     TriggeredBy        = Upgrade_ChinaEdgeOfTomorrow ; optional upgrade gate
//     RequiredVeterancy  = HEROIC5                     ; minimum rank at death (default REGULAR)
//     RespawnAtKindOf    = COMMANDCENTER               ; nearest friendly building of this kind
//     Delay              = 10000                       ; ms from death to respawn
//     PreserveExperience = Yes                         ; restore rank + exact XP (default Yes)
//     FullHealth         = Yes                         ; force full health (default Yes)
//     RespawnMarkerName  = VeterancyRespawnMarker      ; proxy template carrying RespawnMarkerUpdate
//   End
//
//   Object VeterancyRespawnMarker
//     KindOf = INERT IMMOBILE UNATTACKABLE
//     Body = InactiveBody ModuleTag_Body
//     End
//     Behavior = RespawnMarkerUpdate ModuleTag_Respawn
//     End
//     Geometry = SPHERE
//     GeometryMajorRadius = 1.0
//     GeometryIsSmall = Yes
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/INI.h"
#include "Common/KindOf.h"
#include "GameLogic/Module/DieModule.h"
#include "GameLogic/Module/UpdateModule.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Thing;

//-------------------------------------------------------------------------------------------------
class RespawnAtBuildingDieModuleData : public DieModuleData
{

public:

	AsciiString			m_triggeredBy;				///< optional player/object upgrade gate (empty = always on)
	VeterancyLevel	m_requiredVeterancy;	///< minimum veterancy at death for the respawn to trigger
	KindOfMaskType	m_respawnAtKindOf;		///< respawn at the nearest friendly building with these kindofs
	UnsignedInt			m_delayFrames;				///< frames between death and respawn (INI: Delay, in ms)
	Bool						m_preserveExperience;	///< restore veterancy level and exact experience points
	Bool						m_fullHealth;					///< force the respawned unit to full health
	AsciiString			m_respawnMarkerName;	///< template of the invisible countdown proxy object

	RespawnAtBuildingDieModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);

};

//-------------------------------------------------------------------------------------------------
/** GeneralsX @feature Upgrade- and veterancy-gated respawn at a friendly building on death */
//-------------------------------------------------------------------------------------------------
class RespawnAtBuildingDie : public DieModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( RespawnAtBuildingDie, "RespawnAtBuildingDie"  )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( RespawnAtBuildingDie, RespawnAtBuildingDieModuleData );

public:

	RespawnAtBuildingDie( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual void onDie( const DamageInfo *damageInfo ) override;

	// GeneralsX @feature Max-rank perk DEATH-DEFIANCE: global (module-less) respawn path for
	// max-rank infantry/vehicles, gated on GameData VeterancyMaxRankRespawn.  Reuses the
	// marker mechanics below; called from Object::onDie for objects WITHOUT this die module.
	static void maybeGlobalMaxRankRespawn( Object *obj );

};

//-------------------------------------------------------------------------------------------------
class RespawnMarkerUpdateModuleData : public UpdateModuleData
{
public:

	RespawnMarkerUpdateModuleData();

	static void buildFieldParse(MultiIniFieldParse& p);

};

//-------------------------------------------------------------------------------------------------
/** GeneralsX @feature Countdown proxy spawned by RespawnAtBuildingDie: waits out the delay on an
	* invisible marker object, then recreates the fallen unit at the nearest qualifying friendly
	* building and destroys itself.  All parameters are runtime state received via startRespawn()
	* (the marker template itself carries no INI configuration). */
//-------------------------------------------------------------------------------------------------
class RespawnMarkerUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( RespawnMarkerUpdate, "RespawnMarkerUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( RespawnMarkerUpdate, RespawnMarkerUpdateModuleData )

public:

	RespawnMarkerUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

	/// keep counting down even if something manages to disable the inert marker
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

	/// arm the countdown; called by RespawnAtBuildingDie::onDie with the dying unit's state.
	/// spawnHealthPercent applies when fullHealth is FALSE: 0 < pct < 1 sets the respawned
	/// unit's health to that fraction of max (GeneralsX max-rank perk DEATH-DEFIANCE).
	void startRespawn( const AsciiString& templateName, VeterancyLevel level, Int experience,
										 const KindOfMaskType& respawnAtKindOf, UnsignedInt delayFrames,
										 Bool preserveExperience, Bool fullHealth, Real spawnHealthPercent );

protected:

	Object* findRespawnBuilding() const;	///< nearest alive friendly building matching the kindof mask
	void doRespawn();											///< recreate the unit (no-op when no qualifying building lives)

	AsciiString			m_respawnTemplateName;	///< template of the unit to recreate
	KindOfMaskType	m_respawnAtKindOf;			///< building kindof filter
	UnsignedInt			m_respawnFrame;					///< frame at which to respawn
	Int							m_experience;						///< exact experience points at death
	VeterancyLevel	m_veterancyLevel;				///< veterancy level at death
	Real						m_spawnHealthPercent;		///< respawn health fraction when not FullHealth (1.0 = untouched)
	Bool						m_armed;								///< startRespawn received
	Bool						m_preserveExperience;
	Bool						m_fullHealth;

};
