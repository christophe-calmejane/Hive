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

#include "internals/config.hpp"
#include "settingsManager.hpp"
#include <la/avdecc/utils.hpp>
#include <QSettings>
#include <QHash>
#include <unordered_map>

struct QStringHash
{
	std::size_t operator()(QString const& s) const
	{
		return qHash(s);
	}
};

namespace settings
{
class SettingsManagerImpl final : public SettingsManager
{
public:
	SettingsManagerImpl(std::optional<QString> const& iniFilePath) noexcept
	{
		if (iniFilePath)
		{
			_settings = std::make_unique<QSettings>(*iniFilePath, QSettings::Format::IniFormat);
		}
		else
		{
			auto appName = hive::internals::applicationShortName;
			// Starting with Hive > 1.2, the settings file path is built using the marketing version number
			// But any beta version (key digits > 2) also uses the marketing version number
			{
				auto const tokens = la::avdecc::utils::tokenizeString(hive::internals::cmakeVersionString.toStdString(), '.', false);
				if (AVDECC_ASSERT_WITH_RET(tokens.size() >= 3, "cmake version tokens should always be 3 or 4"))
				{
					if (hive::internals::marketingDigits > 2u || (hive::internals::marketingDigits == 2u && (tokens[0] > "1" || tokens[1] > "2")))
					{
						appName = hive::internals::applicationShortName + "-" + hive::internals::marketingVersion;
					}
				}
			}
			_settings = std::make_unique<QSettings>(QSettings::Format::IniFormat, QSettings::Scope::UserScope, hive::internals::companyName, appName);
		}
	}

	virtual void destroy() noexcept override
	{
		delete this;
	}

private:
	virtual void registerSetting(SettingDefault const& setting) noexcept override
	{
		if (!_settings->contains(setting.name))
		{
			_settings->setValue(setting.name, setting.initialValue);
		}
	}

	virtual void setValueInternal(Setting const& name, QVariant const& value, Observer const* const dontNotifyObserver) noexcept override
	{
		_settings->setValue(name, value);

		// Notify observers
		auto const observersIt = _observers.find(name);
		if (observersIt != _observers.end())
		{
			observersIt->second.notifyObservers<Observer>(
				[dontNotifyObserver, &name, &value](Observer* const obs)
				{
					if (obs != dontNotifyObserver)
					{
						obs->onSettingChanged(name, value);
					}
				});
		}
	}

	virtual QVariant getValueInternal(Setting const& name) const noexcept override
	{
		return _settings->value(name);
	}

	virtual void registerSettingObserver(Setting const& name, Observer* const observer, bool const triggerFirstNotification) const noexcept override
	{
		if (AVDECC_ASSERT_WITH_RET(_settings->contains(name), "registerSettingObserver not allowed for a Setting without initial Value"))
		{
			auto& observers = _observers[name];
			try
			{
				observers.registerObserver(observer);
				if (triggerFirstNotification)
				{
					auto const value = _settings->value(name);
					la::avdecc::utils::invokeProtectedMethod(&Observer::onSettingChanged, observer, name, value);
				}
			}
			catch (...)
			{
			}
		}
	}

	virtual void unregisterSettingObserver(Setting const& name, Observer* const observer) const noexcept override
	{
		auto const observersIt = _observers.find(name);
		if (observersIt != _observers.end())
		{
			try
			{
				observersIt->second.unregisterObserver(observer);
			}
			catch (...)
			{
			}
		}
	}

	virtual void triggerSettingObserver(Setting const& name, Observer* const observer) const noexcept override
	{
		if (AVDECC_ASSERT_WITH_RET(_settings->contains(name), "triggerSettingObserver not allowed for a Setting without initial Value"))
		{
			auto const observersIt = _observers.find(name);
			if (observersIt != _observers.end())
			{
				if (observersIt->second.isObserverRegistered(observer))
				{
					la::avdecc::utils::invokeProtectedMethod(&Observer::onSettingChanged, observer, name, _settings->value(name));
				}
			}
		}
	}

	virtual QString getFilePath() const noexcept override
	{
		return _settings->fileName();
	}

	// Private Members
	std::unique_ptr<QSettings> _settings{ nullptr };
	mutable std::unordered_map<QString, Subject, QStringHash> _observers{};
};

/** SettingsManager Entry point */
SettingsManager* SettingsManager::createRawSettingsManager(std::optional<QString> const& iniFilePath)
{
	return new SettingsManagerImpl(iniFilePath);
}

} // namespace settings
