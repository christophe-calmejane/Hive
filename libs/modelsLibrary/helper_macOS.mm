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

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include <mach/mach.h>

namespace hive
{
namespace modelsLibrary
{
namespace helper
{
QString getComputerName() noexcept
{
	if (auto* processInfo = [NSProcessInfo processInfo])
	{
		if ([processInfo respondsToSelector:@selector(hostName)])
		{
			return QString::fromStdString([[processInfo hostName] UTF8String]);
		}
	}

	return {};
}

std::uint64_t getProcessResidentMemory() noexcept
{
	auto info = mach_task_basic_info{};
	auto count = mach_msg_type_number_t{ MACH_TASK_BASIC_INFO_COUNT };
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
	{
		return static_cast<std::uint64_t>(info.resident_size);
	}
	return 0u;
}

} // namespace helper
} // namespace modelsLibrary
} // namespace hive
