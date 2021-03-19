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

#include "sparkleHelper.hpp"

#include <winsparkle.h>
#include <Windows.h>

void Sparkle::init(std::string const& signature) noexcept
{
	try
	{
		// Set Language ID
		win_sparkle_set_langid(::GetThreadUILanguage());
		// Set our DSA public key
		win_sparkle_set_dsa_pub_pem(signature.c_str());

		// Set callbacks to handle application shutdown when an update is starting
		win_sparkle_set_can_shutdown_callback(
			[]() -> int
			{
				auto const& sparkle = getInstance();
				if (sparkle._isShutdownAllowedHandler)
				{
					return static_cast<int>(sparkle._isShutdownAllowedHandler());
				}
				return 1;
			});
		win_sparkle_set_shutdown_request_callback(
			[]()
			{
				auto const& sparkle = getInstance();
				if (sparkle._shutdownRequestHandler)
				{
					sparkle._shutdownRequestHandler();
				}
			});
		win_sparkle_set_did_find_update_callback(
			[]()
			{
				auto const& sparkle = getInstance();
				if (sparkle._logHandler)
				{
					sparkle._logHandler("A new update has been found", LogLevel::Info);
				}
			});
		win_sparkle_set_error_callback(
			[]()
			{
				auto const& sparkle = getInstance();
				if (sparkle._logHandler)
				{
					sparkle._logHandler("Automatic update failed", LogLevel::Warn);
				}
			});

		// Get current Check For Updates value
		_checkForUpdates = static_cast<bool>(win_sparkle_get_automatic_check_for_updates());

		_initialized = true;
	}
	catch (...)
	{
	}
}

void Sparkle::start() noexcept
{
	if (!_initialized)
	{
		return;
	}

	win_sparkle_init();

	_started = true;
}

void Sparkle::setAutomaticCheckForUpdates(bool const checkForUpdates) noexcept
{
	if (!_initialized)
	{
		return;
	}

	try
	{
		// Set Automatic Check For Updates
		win_sparkle_set_automatic_check_for_updates(static_cast<int>(checkForUpdates));

		// If switching to CheckForUpdates, check right now
		if (checkForUpdates && _started)
		{
			win_sparkle_check_update_without_ui();
		}

		_checkForUpdates = checkForUpdates;
	}
	catch (...)
	{
	}
}

void Sparkle::setAppcastUrl(std::string const& appcastUrl) noexcept
{
	if (!_initialized)
	{
		return;
	}

	try
	{
		// Set Appcast URL
		win_sparkle_set_appcast_url(appcastUrl.c_str());
		if (appcastUrl != _appcastUrl && _started && _checkForUpdates)
		{
			win_sparkle_check_update_without_ui();
		}

		_appcastUrl = appcastUrl;
	}
	catch (...)
	{
	}
}

void Sparkle::manualCheckForUpdate() noexcept
{
	if (!_initialized)
	{
		return;
	}

	if (_started)
	{
		win_sparkle_check_update_with_ui();
	}
}

Sparkle::~Sparkle() noexcept
{
	win_sparkle_cleanup();
}
