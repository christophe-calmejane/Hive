/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <unistd.h>
#include <limits.h>

#include <fstream>

namespace hive
{
namespace modelsLibrary
{
namespace helper
{
QString getComputerName() noexcept
{
	char hostname[HOST_NAME_MAX];
	gethostname(hostname, HOST_NAME_MAX);
	return QString{ hostname };
}

std::uint64_t getProcessResidentMemory() noexcept
{
	// /proc/self/statm: total program size then resident set size, in pages
	try
	{
		auto file = std::ifstream{ "/proc/self/statm" };
		auto size = std::uint64_t{ 0u };
		auto resident = std::uint64_t{ 0u };
		if (file >> size >> resident)
		{
			auto const pageSize = sysconf(_SC_PAGESIZE);
			if (pageSize > 0)
			{
				return resident * static_cast<std::uint64_t>(pageSize);
			}
		}
	}
	catch (...)
	{
	}
	return 0u;
}

} // namespace helper
} // namespace modelsLibrary
} // namespace hive
