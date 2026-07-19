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

// FILE: ContainCapacityUpgrade.h /////////////////////////////////////////////////////////////////
// GeneralsX @feature ContainCapacityUpgrade: when the TriggeredBy upgrade completes, the object's
// contain module gains AddSlots extra passenger capacity at runtime.  The bonus is applied via
// ContainModuleInterface::addContainBonusSlots (stored in OpenContain::m_bonusSlots), so every
// capacity check that funnels through getContainMax() - isValidContainerFor, control-bar pips,
// script queries - sees the enlarged capacity.
//
// Expected INI usage (data ships in a data layer, not this repo):
//
//   Behavior = ContainCapacityUpgrade ModuleTag_ExtraSeats
//     TriggeredBy = Upgrade_AmericaAdvancedTraining   ; any player/object upgrade
//     AddSlots    = 2
//   End
//
// Save/load: the bonus itself is xfer'd by OpenContain (version 3), and the upgrade-mux
// "executed" flag is xfer'd by UpgradeModule, so loading a save neither loses nor
// double-applies the bonus.  Note: containers whose capacity lives outside the object
// (TunnelContain/CaveContain use a player-wide tracker) ignore the bonus.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "GameLogic/Module/UpgradeModule.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Thing;

//-------------------------------------------------------------------------------------------------
class ContainCapacityUpgradeModuleData : public UpgradeModuleData
{
public:

	Int m_addSlots;		///< extra passenger slots granted when the upgrade completes

	ContainCapacityUpgradeModuleData()
	{
		m_addSlots = 0;
	}

	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
/** GeneralsX @feature Grant the object's contain module extra passenger slots on upgrade */
//-------------------------------------------------------------------------------------------------
class ContainCapacityUpgrade : public UpgradeModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ContainCapacityUpgrade, "ContainCapacityUpgrade" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ContainCapacityUpgrade, ContainCapacityUpgradeModuleData );

public:

	ContainCapacityUpgrade( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

protected:

	virtual void upgradeImplementation() override; ///< Here's the actual work of Upgrading
	virtual Bool isSubObjectsUpgrade() override { return false; }

};
