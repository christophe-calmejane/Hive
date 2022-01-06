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

#pragma once

#include <la/avdecc/logger.hpp>
#include "la/avdecc/utils.hpp"
#include <QString>

namespace avdecc
{
namespace logger
{
class LogItemHive : public la::avdecc::logger::LogItem
{
public:
	LogItemHive(QString message)
		: LogItem(la::avdecc::logger::Layer::FirstUserLayer)
		, _message(message)
	{
	}

	virtual std::string getMessage() const noexcept override
	{
		return _message.toStdString();
	}

private:
	QString _message{};
};

/** Template to remove at compile time some of the most time-consuming log messages (Trace and Debug) - Forward arguments to the Logger */
template<la::avdecc::logger::Level LevelValue, class LogItemType, typename... Ts>
constexpr void log(Ts&&... params)
{
#ifndef DEBUG
	// In release, we don't want Trace nor Debug levels
	if constexpr (LevelValue == la::avdecc::logger::Level::Trace || LevelValue == la::avdecc::logger::Level::Debug)
	{
	}
	else
#endif // !DEBUG
	{
		auto const item = LogItemType{ std::forward<Ts>(params)... };
		la::avdecc::logger::Logger::getInstance().logItem(LevelValue, &item);
	}
}

} // namespace logger
} // namespace avdecc

/** Preprocessor defines to remove at compile time some of the most time-consuming log messages (Trace and Debug) - Creation of the arguments */
#define LOG_HIVE(LogLevel, Message) avdecc::logger::log<la::avdecc::logger::Level::LogLevel, avdecc::logger::LogItemHive>(Message)
#ifdef DEBUG
#	define LOG_HIVE_TRACE(Message) LOG_HIVE(Trace, Message)
#	define LOG_HIVE_DEBUG(Message) LOG_HIVE(Debug, Message)
#else // !DEBUG
#	define LOG_HIVE_TRACE(Message)
#	define LOG_HIVE_DEBUG(Message)
#endif // DEBUG
#define LOG_HIVE_INFO(Message) LOG_HIVE(Info, Message)
#define LOG_HIVE_WARN(Message) LOG_HIVE(Warn, Message)
#define LOG_HIVE_ERROR(Message) LOG_HIVE(Error, Message)
