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

// FILE: ContainCapacityUpgrade.cpp ///////////////////////////////////////////////////////////////
// GeneralsX @feature Grant the object's contain module AddSlots extra passenger capacity when the
// TriggeredBy upgrade completes.  See ContainCapacityUpgrade.h for the INI contract and notes.
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Xfer.h"
#include "GameLogic/Module/ContainCapacityUpgrade.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ContainCapacityUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "AddSlots",	INI::parseInt,		nullptr, offsetof( ContainCapacityUpgradeModuleData, m_addSlots ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ContainCapacityUpgrade::ContainCapacityUpgrade( Thing *thing, const ModuleData* moduleData ) :
							UpgradeModule( thing, moduleData )
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ContainCapacityUpgrade::~ContainCapacityUpgrade()
{

}

//-------------------------------------------------------------------------------------------------
/** The upgrade fired: grant the object's contain module the extra passenger slots.  The bonus
	* lives in OpenContain::m_bonusSlots and is xfer'd there, while UpgradeModule xfers its
	* executed flag, so this does NOT re-fire (or double-apply) after save/load. */
//-------------------------------------------------------------------------------------------------
void ContainCapacityUpgrade::upgradeImplementation()
{
	ContainModuleInterface *contain = getObject()->getContain();
	if( contain == nullptr )
		return;

	contain->addContainBonusSlots( getContainCapacityUpgradeModuleData()->m_addSlots );
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ContainCapacityUpgrade::crc( Xfer *xfer )
{

	// extend base class
	UpgradeModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ContainCapacityUpgrade::xfer( Xfer *xfer )
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
void ContainCapacityUpgrade::loadPostProcess()
{

	// extend base class
	UpgradeModule::loadPostProcess();

}
