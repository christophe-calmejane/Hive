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

#include "settingsManager/settings.hpp"
#include "internals/config.hpp"
#include "updater.hpp"
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <atomic>
#include <optional>
#include <cstdint>

static QString BaseUrlPath{ "http://www.kikisoft.com/Hive" };
#if defined(Q_OS_WIN)
static QString VersionUrlPath{ BaseUrlPath + "/windows/LatestVersion-windows.txt" };
static QString BetaVersionUrlPath{ BaseUrlPath + "/windows/LatestVersion-beta-windows.txt" };
static QString ReleaseDownloadUrlPath{ BaseUrlPath + "/windows/" };
static QString BetaDownloadUrlPath{ BaseUrlPath + "/temp/" };
#elif defined(Q_OS_MACX)
static QString VersionUrlPath{ BaseUrlPath + "/macOS/LatestVersion-macOS.txt" };
static QString BetaVersionUrlPath{ BaseUrlPath + "/macOS/LatestVersion-beta-macOS.txt" };
static QString ReleaseDownloadUrlPath{ BaseUrlPath + "/macOS/" };
static QString BetaDownloadUrlPath{ BaseUrlPath + "/temp/" };
#else
static QString VersionUrlPath{};
static QString BetaVersionUrlPath{};
static QString ReleaseDownloadUrlPath{};
static QString BetaDownloadUrlPath{};
#endif

/** Class to handle versioning. Version number should be x.y.z[.w], with 'w' being pre-release tag. */
class Version final
{
public:
	Version(QString const& versionString)
	{
		// No version string, empty version (== 0)
		if (versionString.isEmpty())
		{
			return;
		}

		auto const tokens = versionString.split(".");
		auto const length = tokens.length();

		if (length < 3 || length > 4)
			throw std::invalid_argument("Passed versionString is not in the form x.y.z[.w]");

		// Split x.y.z
		for (auto idx = 0; idx < 3; ++idx)
		{
			auto const value = static_cast<std::uint16_t>(tokens[idx].toInt());
			_packedVersion <<= sizeof(decltype(value)) * 8;
			_packedVersion += value;
		}

		// Get pre-release tag
		if (length == 4)
			_preleaseTag = static_cast<std::uint16_t>(tokens[3].toInt());
	}

	friend inline bool operator<(Version const& lhs, Version const& rhs)
	{
		// Only case we need to deeply check, is if packedVersion is equal for both
		if (lhs._packedVersion == rhs._packedVersion)
		{
			// Pre-release tag on lhs but not on rhs (means rhs is a release version)
			if (lhs._preleaseTag.has_value() && !rhs._preleaseTag.has_value())
				return true;
			// Both have a pre-release tag, but lhs is < to rhs
			if (lhs._preleaseTag && rhs._preleaseTag && *lhs._preleaseTag < *rhs._preleaseTag)
				return true;

			return false;
		}

		return lhs._packedVersion < rhs._packedVersion;
	}

private:
	std::uint64_t _packedVersion{ 0ull };
	std::optional<std::uint16_t> _preleaseTag{ std::nullopt };
};

class UpdaterImpl final : public Updater, private settings::SettingsManager::Observer
{
public:
	UpdaterImpl() noexcept
	{
		// Connect signals
		connect(&_webCtrlRelease, &QNetworkAccessManager::finished, this,
			[this](QNetworkReply* const reply)
			{
				if (reply->error() == QNetworkReply::NoError)
				{
					_newReleaseVersionString = QString(reply->readAll()).trimmed();
					if (_checkBetaVersion)
					{
						QNetworkRequest request{ QUrl{ BetaVersionUrlPath } };
						_webCtrlBeta.get(request);
					}
					else
					{
						compareVersions();
					}
				}
				else
				{
					emit checkFailed(reply->errorString());
				}
				reply->deleteLater();
			});

		connect(&_webCtrlBeta, &QNetworkAccessManager::finished, this,
			[this](QNetworkReply* const reply)
			{
				if (reply->error() == QNetworkReply::NoError)
				{
					_newBetaVersionString = QString(reply->readAll()).trimmed();
				}
				compareVersions();
				reply->deleteLater();
			});

		// Register to settings::SettingsManager
		auto& settings = settings::SettingsManager::getInstance();
		settings.registerSettingObserver(settings::General_AutomaticCheckForUpdates.name, this, false);
		settings.registerSettingObserver(settings::General_CheckForBetaVersions.name, this, false);
		_automaticCheckNewVersion = settings.getValue(settings::General_AutomaticCheckForUpdates.name).toBool();
	}

	~UpdaterImpl() noexcept
	{
		// Remove settings observers
		auto& settings = settings::SettingsManager::getInstance();
		settings.unregisterSettingObserver(settings::General_AutomaticCheckForUpdates.name, this);
		settings.unregisterSettingObserver(settings::General_CheckForBetaVersions.name, this);
	}

private:
	// settings::SettingsManager overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override
	{
		if (name == settings::General_AutomaticCheckForUpdates.name)
		{
			_automaticCheckNewVersion = value.toBool();
			if (_automaticCheckNewVersion)
			{
				checkForNewVersion();
			}
		}
		else if (name == settings::General_CheckForBetaVersions.name)
		{
			if (_automaticCheckNewVersion)
			{
				checkForNewVersion();
			}
		}
	}

	// Updater overrides
	virtual void checkForNewVersion() noexcept override
	{
		if (!_checkInProgress && !VersionUrlPath.isEmpty())
		{
			auto& settings = settings::SettingsManager::getInstance();
			_checkBetaVersion = settings.getValue(settings::General_CheckForBetaVersions.name).toBool();
			_newReleaseVersionString = "";
			_newBetaVersionString = "";

			QNetworkRequest request{ QUrl{ VersionUrlPath } };
			_checkInProgress = true;
			_webCtrlRelease.get(request);
		}
	}

	virtual bool isAutomaticCheckForNewVersion() const noexcept override
	{
		return _automaticCheckNewVersion;
	}

	// Private methods
	void compareVersions() noexcept
	{
		try
		{
			auto const currentVersion = Version{ hive::internals::cmakeVersionString };
			auto const newReleaseVersion = Version{ _newReleaseVersionString };
			auto const newBetaVersion = Version{ _newBetaVersionString };

			if (currentVersion < newReleaseVersion || currentVersion < newBetaVersion)
			{
				if (newReleaseVersion < newBetaVersion)
				{
					emit newBetaVersionAvailable(_newBetaVersionString, BetaDownloadUrlPath);
				}
				else
				{
					emit newReleaseVersionAvailable(_newReleaseVersionString, ReleaseDownloadUrlPath);
				}
			}
		}
		catch (...)
		{
			// One of the version is invalid, don't run the version check
		}
		_checkInProgress = false;
	}

	// Private members
	std::atomic_bool _checkInProgress{ false };
	QNetworkAccessManager _webCtrlRelease{};
	QNetworkAccessManager _webCtrlBeta{};
	bool _automaticCheckNewVersion{ false };
	bool _checkBetaVersion{ false };
	QString _newReleaseVersionString{};
	QString _newBetaVersionString{};
};

Updater& Updater::getInstance() noexcept
{
	static UpdaterImpl s_updater{};

	return s_updater;
}
