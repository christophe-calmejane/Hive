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

#pragma once

#include <cstdint>

namespace utils
{
template<typename Tag, typename PID>
struct ProcessHelperBase
{
	using ProcessID = PID;

	static ProcessID getCurrentProcessID() noexcept
	{
		return {};
	}
	static bool isProcessRunning(ProcessID const pid) noexcept
	{
		return {};
	}
};

#ifdef _WIN32

struct ProcessHelper : public ProcessHelperBase<struct WindowsTag, std::uint32_t>
{
	static ProcessID getCurrentProcessID() noexcept;
	static bool isProcessRunning(ProcessID const pid) noexcept;
};

#elif __APPLE__

struct ProcessHelper : public ProcessHelperBase<struct macOSTag, std::int32_t>
{
	static ProcessID getCurrentProcessID() noexcept;
	static bool isProcessRunning(ProcessID const pid) noexcept;
};

#elif __unix__

struct ProcessHelper : public ProcessHelperBase<struct UnixTag, std::uint32_t>
{
	static ProcessID getCurrentProcessID() noexcept;
	static bool isProcessRunning(ProcessID const pid) noexcept;
};

#else

struct ProcessHelper : public ProcessHelperBase<struct DummyTag, std::uint32_t>
{
	static ProcessID getCurrentProcessID() noexcept
	{
		return static_cast<ProcessHelper::ProcessID>(0);
	}
	static bool isProcessRunning(ProcessID const /*pid*/) noexcept
	{
		return false;
	}
};

#endif // _WIN32

} // namespace utils
