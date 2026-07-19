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

// FILE: RespawnAtBuildingDie.cpp /////////////////////////////////////////////////////////////////
// GeneralsX @feature "Edge of Tomorrow" respawn: upgrade- and veterancy-gated recreation of a
// fallen unit at the nearest friendly building, with rank and exact experience restored.
// See RespawnAtBuildingDie.h for the INI contract and design notes.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/RespawnAtBuildingDie.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
RespawnAtBuildingDieModuleData::RespawnAtBuildingDieModuleData()
{
	m_requiredVeterancy = LEVEL_REGULAR;
	m_respawnAtKindOf = MAKE_KINDOF_MASK( KINDOF_COMMANDCENTER );
	m_delayFrames = 0;
	m_preserveExperience = TRUE;
	m_fullHealth = TRUE;
	m_respawnMarkerName = "VeterancyRespawnMarker";
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ void RespawnAtBuildingDieModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	DieModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "TriggeredBy",				INI::parseAsciiString,						nullptr,						offsetof( RespawnAtBuildingDieModuleData, m_triggeredBy ) },
		{ "RequiredVeterancy",	INI::parseIndexList,							TheVeterancyNames,	offsetof( RespawnAtBuildingDieModuleData, m_requiredVeterancy ) },
		{ "RespawnAtKindOf",		KindOfMaskType::parseFromINI,			nullptr,						offsetof( RespawnAtBuildingDieModuleData, m_respawnAtKindOf ) },
		{ "Delay",							INI::parseDurationUnsignedInt,		nullptr,						offsetof( RespawnAtBuildingDieModuleData, m_delayFrames ) },
		{ "PreserveExperience",	INI::parseBool,										nullptr,						offsetof( RespawnAtBuildingDieModuleData, m_preserveExperience ) },
		{ "FullHealth",					INI::parseBool,										nullptr,						offsetof( RespawnAtBuildingDieModuleData, m_fullHealth ) },
		{ "RespawnMarkerName",	INI::parseAsciiString,						nullptr,						offsetof( RespawnAtBuildingDieModuleData, m_respawnMarkerName ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
RespawnAtBuildingDie::RespawnAtBuildingDie( Thing *thing, const ModuleData* moduleData ) : DieModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
RespawnAtBuildingDie::~RespawnAtBuildingDie()
{
}

//-------------------------------------------------------------------------------------------------
/** The die callback.  Note this also fires when the unit dies inside a transport or garrison;
	* nothing here depends on the dying object being in the open. */
//-------------------------------------------------------------------------------------------------
void RespawnAtBuildingDie::onDie( const DamageInfo *damageInfo )
{
	const RespawnAtBuildingDieModuleData *data = getRespawnAtBuildingDieModuleData();
	if (!isDieApplicable(damageInfo))
		return;

	Object *obj = getObject();

	// veterancy gate: RequiredVeterancy is a minimum, not an exact match
	if( obj->getVeterancyLevel() < data->m_requiredVeterancy )
		return;

	// upgrade gate: either the controlling player researched it, or the object carries it
	if( data->m_triggeredBy.isNotEmpty() )
	{
		const UpgradeTemplate *upgradeTemplate = TheUpgradeCenter->findUpgrade( data->m_triggeredBy );
		DEBUG_ASSERTCRASH( upgradeTemplate != nullptr,
			("RespawnAtBuildingDie for '%s': TriggeredBy upgrade '%s' not found",
			 obj->getTemplate()->getName().str(), data->m_triggeredBy.str()) );
		if( upgradeTemplate == nullptr )
			return;

		Player *player = obj->getControllingPlayer();
		Bool hasUpgrade = (player != nullptr && player->hasUpgradeComplete( upgradeTemplate ))
											|| obj->hasUpgrade( upgradeTemplate );
		if( !hasUpgrade )
			return;
	}

	if( obj->getTeam() == nullptr )
		return;

	// the countdown must outlive the dying object, so it is carried by an invisible proxy
	// (the RebuildHole idiom).  The marker template ships with the data half of the feature;
	// when it is absent the module is inert by design.
	const ThingTemplate *markerTemplate = TheThingFactory->findTemplate( data->m_respawnMarkerName, FALSE );
	if( markerTemplate == nullptr )
		return;

	Object *marker = TheThingFactory->newObject( markerTemplate, obj->getTeam() );
	if( marker == nullptr )
		return;

	// measure "nearest building" from the death spot
	marker->setPosition( obj->getPosition() );

	static NameKeyType key_RespawnMarkerUpdate = NAMEKEY( "RespawnMarkerUpdate" );
	RespawnMarkerUpdate *respawn = (RespawnMarkerUpdate *)marker->findUpdateModule( key_RespawnMarkerUpdate );
	if( respawn == nullptr )
	{
		DEBUG_ASSERTCRASH( FALSE, ("RespawnAtBuildingDie: marker template '%s' lacks a RespawnMarkerUpdate module",
			data->m_respawnMarkerName.str()) );
		TheGameLogic->destroyObject( marker );
		return;
	}

	const ExperienceTracker *xpTracker = obj->getExperienceTracker();
	respawn->startRespawn( obj->getTemplate()->getName(),
												 obj->getVeterancyLevel(),
												 xpTracker != nullptr ? xpTracker->getCurrentExperience() : 0,
												 data->m_respawnAtKindOf,
												 data->m_delayFrames,
												 data->m_preserveExperience,
												 data->m_fullHealth,
												 1.0f );
}

//-------------------------------------------------------------------------------------------------
/** GeneralsX @feature Max-rank perk DEATH-DEFIANCE.  Global respawn path, gated on the GameData
	* key VeterancyMaxRankRespawn (default No): when a max-rank (LEVEL_LAST) infantry/vehicle dies,
	* respawn it at the nearest friendly VeterancyMaxRankRespawnAtKindOf building with its exact
	* experience preserved and VeterancyMaxRankRespawnHealthPercent (default 50%) health.  This is
	* the same marker mechanism the module uses; objects that carry their own RespawnAtBuildingDie
	* module are skipped here so they never respawn twice.  Called from Object::onDie. */
//-------------------------------------------------------------------------------------------------
/*static*/ void RespawnAtBuildingDie::maybeGlobalMaxRankRespawn( Object *obj )
{
	if( obj == nullptr || TheGlobalData == nullptr || !TheGlobalData->m_veterancyMaxRankRespawn )
		return;

	// only max-rank units earn the perk
	if( obj->getVeterancyLevel() < LEVEL_LAST )
		return;

	// infantry and vehicles only; no drones, nothing under construction, nothing contained
	if( !obj->isKindOf( KINDOF_INFANTRY ) && !obj->isKindOf( KINDOF_VEHICLE ) )
		return;
	if( obj->isKindOf( KINDOF_DRONE ) )
		return;
	if( obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION ) )
		return;
	if( obj->getContainedBy() != nullptr )
		return;
	if( obj->getTeam() == nullptr )
		return;

	// NOTE: objects carrying their own RespawnAtBuildingDie module are filtered out by the
	// caller (Object::onDie checks findModule, which is protected there) so they never
	// respawn twice.

	// same marker idiom as the module path: inert when the data layer is absent
	const ThingTemplate *markerTemplate =
		TheThingFactory->findTemplate( TheGlobalData->m_veterancyMaxRankRespawnMarkerName, FALSE );
	if( markerTemplate == nullptr )
		return;

	Object *marker = TheThingFactory->newObject( markerTemplate, obj->getTeam() );
	if( marker == nullptr )
		return;

	marker->setPosition( obj->getPosition() );

	static NameKeyType key_RespawnMarkerUpdate = NAMEKEY( "RespawnMarkerUpdate" );
	RespawnMarkerUpdate *respawn = (RespawnMarkerUpdate *)marker->findUpdateModule( key_RespawnMarkerUpdate );
	if( respawn == nullptr )
	{
		DEBUG_ASSERTCRASH( FALSE, ("maybeGlobalMaxRankRespawn: marker template '%s' lacks a RespawnMarkerUpdate module",
			TheGlobalData->m_veterancyMaxRankRespawnMarkerName.str()) );
		TheGameLogic->destroyObject( marker );
		return;
	}

	const ExperienceTracker *xpTracker = obj->getExperienceTracker();
	respawn->startRespawn( obj->getTemplate()->getName(),
												 obj->getVeterancyLevel(),
												 xpTracker != nullptr ? xpTracker->getCurrentExperience() : 0,
												 TheGlobalData->m_veterancyMaxRankRespawnAtKindOf,
												 0,		// next frame; the marker enforces a 1-frame minimum
												 TRUE,	// preserve exact experience
												 FALSE,	// not full health...
												 TheGlobalData->m_veterancyMaxRankRespawnHealthPercent );
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void RespawnAtBuildingDie::crc( Xfer *xfer )
{

	// extend base class
	DieModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void RespawnAtBuildingDie::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	DieModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void RespawnAtBuildingDie::loadPostProcess()
{

	// extend base class
	DieModule::loadPostProcess();

}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
RespawnMarkerUpdateModuleData::RespawnMarkerUpdateModuleData()
{
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
/*static*/ void RespawnMarkerUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	// no INI fields: everything arrives at runtime via startRespawn()
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
RespawnMarkerUpdate::RespawnMarkerUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	m_respawnAtKindOf = KINDOFMASK_NONE;
	m_respawnFrame = 0;
	m_experience = 0;
	m_veterancyLevel = LEVEL_REGULAR;
	m_spawnHealthPercent = 1.0f;
	m_armed = FALSE;
	m_preserveExperience = TRUE;
	m_fullHealth = TRUE;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
RespawnMarkerUpdate::~RespawnMarkerUpdate()
{
}

//-------------------------------------------------------------------------------------------------
void RespawnMarkerUpdate::startRespawn( const AsciiString& templateName, VeterancyLevel level,
																				Int experience, const KindOfMaskType& respawnAtKindOf,
																				UnsignedInt delayFrames, Bool preserveExperience,
																				Bool fullHealth, Real spawnHealthPercent )
{
	m_respawnTemplateName = templateName;
	m_veterancyLevel = level;
	m_experience = experience;
	m_respawnAtKindOf = respawnAtKindOf;
	m_respawnFrame = TheGameLogic->getFrame() + (delayFrames > 0 ? delayFrames : 1);
	m_preserveExperience = preserveExperience;
	m_fullHealth = fullHealth;
	m_spawnHealthPercent = spawnHealthPercent;
	m_armed = TRUE;
}

//-------------------------------------------------------------------------------------------------
struct ClosestRespawnBuildingData
{
	const Object*		m_source;
	KindOfMaskType	m_kindOf;
	Object*					m_closest;
	Real						m_closestDistSq;
};

//-------------------------------------------------------------------------------------------------
static void closestRespawnBuildingProc( Object *obj, void *userData )
{
	ClosestRespawnBuildingData *data = (ClosestRespawnBuildingData *)userData;

	if( !obj->isKindOfMulti( data->m_kindOf, KINDOFMASK_NONE ) )
		return;

	// only respawn out of finished, living buildings
	if( obj->isEffectivelyDead()
			|| obj->testStatus( OBJECT_STATUS_UNDER_CONSTRUCTION )
			|| obj->testStatus( OBJECT_STATUS_SOLD ) )
		return;

	Real distSq = ThePartitionManager->getDistanceSquared( data->m_source, obj, FROM_CENTER_2D );
	if( distSq < data->m_closestDistSq )
	{
		data->m_closest = obj;
		data->m_closestDistSq = distSq;
	}
}

//-------------------------------------------------------------------------------------------------
/** Nearest alive friendly building matching the kindof filter, measured from the death spot */
//-------------------------------------------------------------------------------------------------
Object* RespawnMarkerUpdate::findRespawnBuilding() const
{
	const Player *player = getObject()->getControllingPlayer();
	if( player == nullptr )
		return nullptr;

	ClosestRespawnBuildingData data;
	data.m_source = getObject();
	data.m_kindOf = m_respawnAtKindOf;
	data.m_closest = nullptr;
	data.m_closestDistSq = FLT_MAX;

	player->iterateObjects( closestRespawnBuildingProc, &data );

	return data.m_closest;
}

//-------------------------------------------------------------------------------------------------
/** Recreate the fallen unit at the respawn building.  No qualifying building alive means no
	* respawn - the marker just goes away. */
//-------------------------------------------------------------------------------------------------
void RespawnMarkerUpdate::doRespawn()
{
	const ThingTemplate *unitTemplate = TheThingFactory->findTemplate( m_respawnTemplateName, FALSE );
	if( unitTemplate == nullptr )
		return;

	Object *building = findRespawnBuilding();
	if( building == nullptr )
		return;

	Player *player = building->getControllingPlayer();
	Team *team = player != nullptr ? player->getDefaultTeam() : getObject()->getTeam();
	if( team == nullptr )
		return;

	Object *newObj = TheThingFactory->newObject( unitTemplate, team );
	if( newObj == nullptr )
		return;

	newObj->setProducer( building );

	// restore veterancy and exact experience.  setVeterancyLevel works even for untrainable
	// units; setExperienceAndLevel then applies the exact XP (and recomputes the same level
	// from the template thresholds - by construction XP is always >= the level's threshold).
	ExperienceTracker *xpTracker = newObj->getExperienceTracker();
	if( xpTracker != nullptr && m_preserveExperience )
	{
		xpTracker->setVeterancyLevel( m_veterancyLevel, FALSE );
		xpTracker->setExperienceAndLevel( m_experience, FALSE );
	}

	if( m_fullHealth )
	{
		BodyModuleInterface *body = newObj->getBodyModule();
		if( body != nullptr && body->getHealth() < body->getMaxHealth() )
			body->internalChangeHealth( body->getMaxHealth() - body->getHealth() );
	}
	else if( m_spawnHealthPercent > 0.0f && m_spawnHealthPercent < 1.0f )
	{
		// GeneralsX @feature max-rank DEATH-DEFIANCE: come back wounded, not fresh
		BodyModuleInterface *body = newObj->getBodyModule();
		if( body != nullptr )
			body->internalChangeHealth( body->getMaxHealth() * m_spawnHealthPercent - body->getHealth() );
	}

	// place the unit like freshly-produced units: exit door + rally point when the building
	// has an exit interface, otherwise drop it at the building's edge
	Bool placed = FALSE;
	ExitInterface *exitInterface = building->getObjectExitInterface();
	if( exitInterface != nullptr )
	{
		ExitDoorType exitDoor = exitInterface->reserveDoorForExit( unitTemplate, newObj );
		if( exitDoor != DOOR_NONE_AVAILABLE )
		{
			exitInterface->exitObjectViaDoor( newObj, exitDoor );
			placed = TRUE;
		}
	}
	if( !placed )
	{
		Coord3D pos = *building->getPosition();
		Real offset = building->getGeometryInfo().getBoundingCircleRadius()
									+ newObj->getGeometryInfo().getBoundingCircleRadius();
		Real angle = building->getOrientation();
		pos.x += Cos( angle ) * offset;
		pos.y += Sin( angle ) * offset;
		newObj->setPosition( &pos );
		newObj->setOrientation( angle );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime RespawnMarkerUpdate::update()
{
	if( !m_armed )
		return UPDATE_SLEEP_NONE;

	if( TheGameLogic->getFrame() < m_respawnFrame )
		return UPDATE_SLEEP_NONE;

	doRespawn();

	// one shot: the marker's job is done either way
	m_armed = FALSE;
	TheGameLogic->destroyObject( getObject() );
	return UPDATE_SLEEP_FOREVER;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void RespawnMarkerUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void RespawnMarkerUpdate::xfer( Xfer *xfer )
{

	// version (2: added m_spawnHealthPercent for the max-rank DEATH-DEFIANCE perk)
	XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	xfer->xferAsciiString( &m_respawnTemplateName );
	m_respawnAtKindOf.xfer( xfer );
	xfer->xferUnsignedInt( &m_respawnFrame );
	xfer->xferInt( &m_experience );
	xfer->xferUser( &m_veterancyLevel, sizeof( m_veterancyLevel ) );
	xfer->xferBool( &m_armed );
	xfer->xferBool( &m_preserveExperience );
	xfer->xferBool( &m_fullHealth );
	if( version >= 2 )
		xfer->xferReal( &m_spawnHealthPercent );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void RespawnMarkerUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
