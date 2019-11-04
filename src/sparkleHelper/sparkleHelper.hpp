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

#include <string>
#include <functional>

class Sparkle final
{
public:
	using IsShutdownAllowedHandler = std::function<bool()>;

	static Sparkle& getInstance() noexcept
	{
		static auto s_Instance = Sparkle{};
		return s_Instance;
	}

	/* Initialization methods */
	void init(std::string const& signature) noexcept;
	void start() noexcept;

	/* Configuration methods */
	void setAutomaticCheckForUpdates(bool const checkForUpdates) noexcept;
	void setAppcastUrl(std::string const& appcastUrl) noexcept;
	void setIsShutdownAllowedHandler(IsShutdownAllowedHandler const& isShutdownAllowedHandler) noexcept;

	/* Requests methods */
	void manualCheckForUpdate() noexcept;

	// Deleted compiler auto-generated methods
	Sparkle(Sparkle&&) = delete;
	Sparkle(Sparkle const&) = delete;
	Sparkle& operator=(Sparkle const&) = delete;
	Sparkle& operator=(Sparkle&&) = delete;

private:
	/** Constructor */
	Sparkle() noexcept = default;

	/** Destructor */
	~Sparkle() noexcept;

	// Private members
	bool _initialized{ false };
	bool _started{ false };
	bool _checkForUpdates{ false };
	std::string _appcastUrl{};
	IsShutdownAllowedHandler _isShutdownAllowedHandler{ nullptr };
};
