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

// FILE: MapRevealUpgrade.cpp /////////////////////////////////////////////////////////////////////
// GeneralsX @feature Permanently reveal the entire map for the controlling player when the
// TriggeredBy upgrade completes.  See MapRevealUpgrade.h for the INI contract and notes.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameLogic/Module/MapRevealUpgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void MapRevealUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	// no module-specific fields: TriggeredBy etc. come from the standard upgrade mux
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
MapRevealUpgrade::MapRevealUpgrade( Thing *thing, const ModuleData* moduleData ) :
							UpgradeModule( thing, moduleData )
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
MapRevealUpgrade::~MapRevealUpgrade()
{

}

//-------------------------------------------------------------------------------------------------
/** The upgrade fired: permanently reveal the whole map for the controlling player.  This is the
	* exact primitive the "reveal map ... permanently" script action uses
	* (PartitionManager::revealMapForPlayerPermanently), so behavior matches campaign scripts.
	* UpgradeModule xfers its executed flag, so this does NOT re-fire after save/load, and the
	* per-cell shroud state it modifies is itself xfer'd with the partition manager. */
//-------------------------------------------------------------------------------------------------
void MapRevealUpgrade::upgradeImplementation()
{
	Player *player = getObject()->getControllingPlayer();
	if( player == nullptr )
		return;

	ThePartitionManager->revealMapForPlayerPermanently( player->getPlayerIndex() );
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void MapRevealUpgrade::crc( Xfer *xfer )
{

	// extend base class
	UpgradeModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void MapRevealUpgrade::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpgradeModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void MapRevealUpgrade::loadPostProcess()
{

	// extend base class
	UpgradeModule::loadPostProcess();

}
