/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

namespace defaults
{
namespace ui
{
struct AdvancedView
{
	static constexpr int ColumnWidth_UniqueIdentifier = 160;
	static constexpr int ColumnWidth_Logo = 60;
	static constexpr int ColumnWidth_Compatibility = 50;
	static constexpr int ColumnWidth_Name = 180;
	static constexpr int ColumnWidth_ExclusiveAccessState = 80;
	static constexpr int ColumnWidth_Group = 80;
	static constexpr int ColumnWidth_GPTPDomain = 80;
	static constexpr int ColumnWidth_InterfaceIndex = 90;
	static constexpr int ColumnWidth_Firmware = 160;
};

} // namespace ui
} // namespace defaults
