/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#ifdef _WIN32

namespace npf
{
enum class Status
{
	Unknown = 0,
	NotInstalled = 1,
	NotStarted = 2,
	StartedManually = 3,
	StartedAutomatically = 4,
};

Status getStatus() noexcept;
void startService() noexcept;
void setServiceAutoStart() noexcept;

} // namespace npf

#endif // _WIN32
