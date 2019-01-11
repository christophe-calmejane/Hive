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

#include "updater.hpp"
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <atomic>
#include <cstdint>
#include "internals/config.hpp"

static QString BaseUrlPath{ "http://www.kikisoft.com/Hive" };
#if defined(Q_OS_WIN)
static QString VersionUrlPath{ BaseUrlPath + "/windows/LatestVersion-windows.txt" };
static QString DownloadUrlPath{ BaseUrlPath + "/windows/" };
#elif defined(Q_OS_MACX)
static QString VersionUrlPath{ BaseUrlPath + "/macOS/LatestVersion-macOS.txt" };
static QString DownloadUrlPath{ BaseUrlPath + "/macOS/" };
#else
static QString VersionUrlPath{};
static QString DownloadUrlPath{};
#endif

class UpdaterImpl final : public Updater
{
public:
	UpdaterImpl() noexcept
	{
		connect(&_webCtrl, &QNetworkAccessManager::finished, this,
			[this](QNetworkReply* const reply)
			{
				if (reply->error() == QNetworkReply::NoError)
				{
					auto const newVersionString = QString(reply->readAll()).trimmed();
					auto const currentVersion = packVersionString(hive::internals::versionString);
					auto const newVersion = packVersionString(newVersionString);
					if (newVersion > currentVersion)
					{
						emit newVersionAvailable(newVersionString, DownloadUrlPath);
					}
				}
				else
				{
					emit checkFailed(reply->errorString());
				}
				reply->deleteLater();
				_checkInProgress = false;
			});
	}

	~UpdaterImpl() noexcept {}

private:
	// Updater overrides
	virtual void checkForNewVersion() noexcept override
	{
		if (!_checkInProgress && !VersionUrlPath.isEmpty())
		{
			QNetworkRequest request{ QUrl{ VersionUrlPath } };
			_webCtrl.get(request);
		}
	}

	// Private methods
	std::uint64_t packVersionString(QString const& versionString)
	{
		std::uint64_t packed{ 0u };
		auto const tokens = versionString.split(".");
		auto const length = tokens.length();

		for (auto idx = 0; idx < 4; ++idx)
		{
			std::uint8_t value{ 0u };
			if (idx < length)
			{
				value = tokens[idx].toInt();
			}
			packed <<= 8;
			packed += value;
		}

		return packed;
	}

	// Private members
	std::atomic_bool _checkInProgress{ false };
	QNetworkAccessManager _webCtrl{};
};

Updater& Updater::getInstance() noexcept
{
	static UpdaterImpl s_updater{};

	return s_updater;
}
