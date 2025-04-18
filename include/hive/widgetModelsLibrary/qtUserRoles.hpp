/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <Qt>
#include <QVector>

namespace hive
{
namespace widgetModelsLibrary
{
enum class QtUserRoles : int
{
	LightImageRole = Qt::UserRole + 1001, /**< Role used for Image representation as Light mode */
	DarkImageRole, /**< Role used for Image representation as Dark mode */
	ErrorRole, /**< Role used for Error representation */
	SelectedEntityRole, /**< Role used for Entity Selection representation */
	IdentificationRole, /**< Role used for Entity Identification representation */
	SubscribedUnsolRole, /**< Role used for Unsolicited Notifications Subscription representation */
	UnsolSupportedRole, /**< Role used for Supported Unsolicited Notifications representation */
	IsVirtualRole, /**< Role used for Virtual Entity representation */
	ActiveRole, /**< Role used for active item representation */
};
using RolesList = QVector<int>;

} // namespace widgetModelsLibrary
} // namespace hive
