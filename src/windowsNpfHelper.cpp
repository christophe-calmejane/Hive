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

#ifdef _WIN32

#	include "windowsNpfHelper.hpp"

namespace npf
{
//template<typename ComObject>
class HandleGuard final
{
public:
	HandleGuard(SC_HANDLE handle) noexcept
		: _handle(handle)
	{
	}
	~HandleGuard() noexcept
	{
		if (_handle)
		{
			CloseServiceHandle(_handle);
		}
	}

private:
	SC_HANDLE _handle{ nullptr };
};


Status getStatus(std::string const& serviceName) noexcept
{
	auto status = Status::Unknown;
	if (auto const managerHandle = OpenSCManager(nullptr, nullptr, GENERIC_READ))
	{
		auto const managerGuard = HandleGuard{ managerHandle };

		if (auto const serviceHandle = OpenService(managerHandle, serviceName.c_str(), GENERIC_READ))
		{
			auto const serviceGuard = HandleGuard{ serviceHandle };

			auto bytesNeeded = DWORD{};
			auto serviceStatus = SERVICE_STATUS_PROCESS{};
			if (QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatus, sizeof(serviceStatus), &bytesNeeded))
			{
				auto const isRunning = (serviceStatus.dwCurrentState == SERVICE_RUNNING);
				if (isRunning)
				{
					if (!QueryServiceConfig(serviceHandle, nullptr, 0, &bytesNeeded) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
					{
						auto serviceConfigBuffer = std::vector<char>(static_cast<std::vector<char>::size_type>(bytesNeeded)); // Brace-initialization constructor prevents the use of {}
						auto pServiceConfig = (QUERY_SERVICE_CONFIG*)(serviceConfigBuffer.data());
						if (QueryServiceConfig(serviceHandle, pServiceConfig, bytesNeeded, &bytesNeeded))
						{
							if ((pServiceConfig->dwStartType == SERVICE_DEMAND_START) || (pServiceConfig->dwStartType == SERVICE_DISABLED))
							{
								status = Status::StartedManually;
							}
							else
							{
								status = Status::StartedAutomatically;
							}
						}
					}
				}
				else
				{
					status = Status::NotStarted;
				}
			}
		}
		else
		{
			const auto error = GetLastError();
			if (error == ERROR_SERVICE_DOES_NOT_EXIST)
			{
				status = Status::NotInstalled;
			}
		}
	}

	return status;
}

void startService() noexcept
{
	auto sei = SHELLEXECUTEINFO{ sizeof(SHELLEXECUTEINFO) };

	sei.lpVerb = "runas";
	sei.lpFile = "sc.exe";
	sei.lpParameters = "start npf";
	sei.nShow = SW_NORMAL;

	ShellExecuteEx(&sei);
}

void setServiceAutoStart() noexcept
{
	auto sei = SHELLEXECUTEINFO{ sizeof(SHELLEXECUTEINFO) };

	sei.lpVerb = "runas";
	sei.lpFile = "sc.exe";
	sei.lpParameters = "config npf start=auto";
	sei.nShow = SW_NORMAL;

	ShellExecuteEx(&sei);
}

} // namespace npf

#endif // _WIN32
