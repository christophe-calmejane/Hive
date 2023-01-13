/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

namespace hive
{
struct VisibilityDefaults
{
	// MainWindow widgets
	bool mainWindow_ControllerToolbar_Visible{ true };
	bool mainWindow_UtilitiesToolbar_Visible{ true };
	bool mainWindow_EntitiesList_Visible{ true };
	bool mainWindow_Inspector_Visible{ true };
	bool mainWindow_Logger_Visible{ true };

	// Controller Table View
	bool controllerTableView_EntityLogo_Visible{ true };
	bool controllerTableView_Compatibility_Visible{ true };
	bool controllerTableView_Name_Visible{ true };
	bool controllerTableView_Group_Visible{ true };
	bool controllerTableView_AcquireState_Visible{ true };
	bool controllerTableView_LockState_Visible{ true };
	bool controllerTableView_GrandmasterID_Visible{ true };
	bool controllerTableView_GptpDomain_Visible{ true };
	bool controllerTableView_InterfaceIndex_Visible{ true };
	bool controllerTableView_AssociationID_Visible{ true };
	bool controllerTableView_MediaClockMasterID_Visible{ true };
	bool controllerTableView_MediaClockMasterName_Visible{ true };
};

} // namespace hive
