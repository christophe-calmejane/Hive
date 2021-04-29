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

#include <string>
#include <functional>

class Sparkle final
{
public:
	enum class LogLevel
	{
		Info = 0,
		Warn = 1,
		Error = 2,
	};

	using IsShutdownAllowedHandler = std::function<bool()>;
	using ShutdownRequestHandler = std::function<void()>;
	using LogHandler = std::function<void(std::string const& message, LogLevel const level)>;

	static Sparkle& getInstance() noexcept
	{
		static auto s_Instance = Sparkle{};
		return s_Instance;
	}

	/* Initialization methods */
	// Must be called before any other methods, and as soon as possible
	void init(std::string const& internalNumber, std::string const& signature) noexcept;
	// Must be called to start the background check process, but not before the UI is visible and configuration methods have been called
	void start() noexcept;

	/* Configuration methods */
	void setAutomaticCheckForUpdates(bool const checkForUpdates) noexcept;
	void setAppcastUrl(std::string const& appcastUrl) noexcept;
	void setIsShutdownAllowedHandler(IsShutdownAllowedHandler const& isShutdownAllowedHandler) noexcept
	{
		_isShutdownAllowedHandler = isShutdownAllowedHandler;
	}
	void setShutdownRequestHandler(ShutdownRequestHandler const& shutdownRequestHandler) noexcept
	{
		_shutdownRequestHandler = shutdownRequestHandler;
	}
	void setLogHandler(LogHandler const& logHandler) noexcept
	{
		_logHandler = logHandler;
	}
	LogHandler const& getLogHandler() const noexcept
	{
		return _logHandler;
	}

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
	ShutdownRequestHandler _shutdownRequestHandler{ nullptr };
	LogHandler _logHandler{ nullptr };
};
