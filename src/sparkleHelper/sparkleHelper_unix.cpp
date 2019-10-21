/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "sparkle.hpp"

void Sparkle::init(std::string const& /*signature*/) noexcept
{
}

void Sparkle::setAutomaticCheckForUpdates(bool const /*checkForUpdates*/) noexcept
{
		if (!_initialized)
		{
			return;
		}

			_checkForUpdates = checkForUpdates;
}

	void Sparkle::setAppcastUrl(std::string const& /*appcastUrl*/) noexcept
	{
		if (!_initialized)
		{
			return;
		}

			_appcastUrl = appcastUrl;
	}

	void Sparkle::start() noexcept
	{
		if (!_initialized)
		{
			return;
		}

			_started = true;
	}

	void Sparkle::manualCheckForUpdate() noexcept
	{
		if (!_initialized)
		{
			return;
		}

	}
	Sparkle::~Sparkle() noexcept
	{
	}
