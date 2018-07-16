/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

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

class SettingsManagerImpl : public SettingsManager
{
private:
	virtual void registerSetting(SettingDefault const& setting) noexcept override
	{
		if (!_settings.contains(setting.name))
		{
			_settings.setValue(setting.name, setting.initialValue);
		}
	}

	virtual void setValue(Setting const& name, QVariant const& value, Observer const* const dontNotifyObserver) noexcept override
	{
		_settings.setValue(name, value);

		// Notify observers
		auto const observersIt = _observers.find(name);
		if (observersIt != _observers.end())
		{
			observersIt->second.notifyObservers<Observer>([dontNotifyObserver, &name, &value](Observer* const obs)
			{
				if (obs != dontNotifyObserver)
				{
					obs->onSettingChanged(name, value);
				}
			});
		}
	}

	virtual QVariant getValue(Setting const& name) const noexcept override
	{
		return _settings.value(name);
	}

	virtual void registerSettingObserver(Setting const& name, Observer* const observer) noexcept override
	{
		if (AVDECC_ASSERT_WITH_RET(_settings.contains(name), "registerSettingObserver not allowed for a Setting without initial Value"))
		{
			auto& observers = _observers[name];
			try
			{
				observers.registerObserver(observer);
				auto const value = _settings.value(name);
				la::avdecc::invokeProtectedMethod(&Observer::onSettingChanged, observer, name, value);
			}
			catch (...)
			{
			}
		}
	}

	virtual void unregisterSettingObserver(Setting const& name, Observer* const observer) noexcept override
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

	virtual void triggerSettingObserver(Setting const& name, Observer* const observer) noexcept override
	{
		if (AVDECC_ASSERT_WITH_RET(_settings.contains(name), "triggerSettingObserver not allowed for a Setting without initial Value"))
		{
			auto const observersIt = _observers.find(name);
			if (observersIt != _observers.end())
			{
				if (observersIt->second.isObserverRegistered(observer))
				{
					la::avdecc::invokeProtectedMethod(&Observer::onSettingChanged, observer, name, _settings.value(name));
				}
			}
		}
	}

	// Private Members
	QSettings _settings{};
	std::unordered_map<QString, Subject, QStringHash> _observers{};
};

SettingsManager& SettingsManager::getInstance() noexcept
{
	static SettingsManagerImpl s_Manager{};

	return s_Manager;
}

} // namespace settings
