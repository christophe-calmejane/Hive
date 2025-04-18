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

#include "processHelper.hpp"

#import <sys/sysctl.h>
#import <Foundation/Foundation.h>

#include <memory>
#include <functional>

namespace utils
{
ProcessHelper::ProcessID ProcessHelper::getCurrentProcessID() noexcept
{
	return [NSProcessInfo processInfo].processIdentifier;
}

bool ProcessHelper::isProcessRunning(ProcessID const pid) noexcept
{
	static auto maxArgumentSize = int{ 0 };
	if (maxArgumentSize == 0)
	{
		auto size = sizeof(maxArgumentSize);
		if (sysctl((int[]){ CTL_KERN, KERN_ARGMAX }, 2, &maxArgumentSize, &size, NULL, 0) == -1)
		{
			maxArgumentSize = 4096; // Default
		}
	}

	int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
	struct kinfo_proc* info;
	size_t length;
	int count;
	std::unique_ptr<struct kinfo_proc, std::function<void(struct kinfo_proc*)>> scopedInfo{ nullptr, [](struct kinfo_proc* ptr)
		{
			if (ptr != nullptr)
			{
				free(ptr);
			}
		} };

	if (sysctl(mib, 3, NULL, &length, NULL, 0) < 0)
	{
		return false;
	}
	if (!(info = (struct kinfo_proc*)malloc(length)))
	{
		return false;
	}
	scopedInfo.reset(info);
	if (sysctl(mib, 3, info, &length, NULL, 0) < 0)
	{
		return false;
	}
	count = static_cast<decltype(count)>(length / sizeof(struct kinfo_proc));
	for (auto i = 0; i < count; i++)
	{
		if (info[i].kp_proc.p_pid == pid)
		{
			return true;
		}
	}

	return false;
}

} // namespace utils
