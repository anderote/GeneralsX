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

// FILE: MapRevealUpgrade.h ///////////////////////////////////////////////////////////////////////
// GeneralsX @feature MapRevealUpgrade: when the TriggeredBy upgrade completes (intended usage: a
// PLAYER research on the China Propaganda Center), permanently reveal the ENTIRE map for the
// object's controlling player.  Uses the exact engine primitive the campaign script action
// "reveal map ... permanently" uses (PartitionManager::revealMapForPlayerPermanently), so the
// behavior matches scripts bit for bit.
//
// Expected INI usage (data ships in a data layer, not this repo):
//
//   Behavior = MapRevealUpgrade ModuleTag_MapReveal
//     TriggeredBy = Upgrade_ChinaSatelliteUplink   ; any player/object upgrade
//   End
//
// Save/load: the reveal itself lives in per-cell partition shroud state, which is xfer'd, and
// the upgrade-mux "executed" flag is xfer'd by UpgradeModule, so loading a save neither loses
// nor double-applies the reveal.  Note the primitive adds one looker per call to every cell;
// with multiple objects carrying this module (or capture-recapture research shenanigans) the
// map simply stays revealed - only the debug "undo reveal" script would need matching counts.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/UpgradeModule.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Thing;

//-------------------------------------------------------------------------------------------------
class MapRevealUpgradeModuleData : public UpgradeModuleData
{
public:

	MapRevealUpgradeModuleData()
	{
	}

	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
/** GeneralsX @feature Permanently reveal the whole map for the controlling player on upgrade */
//-------------------------------------------------------------------------------------------------
class MapRevealUpgrade : public UpgradeModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( MapRevealUpgrade, "MapRevealUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( MapRevealUpgrade, MapRevealUpgradeModuleData );

public:

	MapRevealUpgrade( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

protected:

	virtual void upgradeImplementation() override; ///< Here's the actual work of Upgrading
	virtual Bool isSubObjectsUpgrade() override { return false; }

};
