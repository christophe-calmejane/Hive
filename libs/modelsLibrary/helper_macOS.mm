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


#include "hive/modelsLibrary/helper.hpp"

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

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

} // namespace helper
} // namespace modelsLibrary
} // namespace hive
