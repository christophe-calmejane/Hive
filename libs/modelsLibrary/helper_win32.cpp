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


#include "hive/modelsLibrary/helper.hpp"

#include <la/networkInterfaceHelper/windowsHelper.hpp>

#include <Windows.h>

namespace hive
{
namespace modelsLibrary
{
namespace helper
{
QString getComputerName() noexcept
{
	WCHAR buffer[MAX_COMPUTERNAME_LENGTH + 1];
	auto size = DWORD{ MAX_COMPUTERNAME_LENGTH + 1 };

	if (GetComputerNameW(buffer, &size))
	{
		try
		{
			return QString::fromStdString(la::networkInterface::windows::wideCharToUtf8(buffer));
		}
		catch (std::invalid_argument const&)
		{
		}
	}

	return {};
}

} // namespace helper
} // namespace modelsLibrary
} // namespace hive
