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

#include "settingsManager.hpp"

#include <QObject>
#include <QApplication>

/**
 * @brief Helper class to convert Settings Observer events to Qt Signals
*/
class SettingsSignaler : public QObject, public settings::SettingsManager::Observer
{
	Q_OBJECT
public:
	SettingsSignaler() noexcept {}

	~SettingsSignaler() noexcept
	{
		stop();
	}

	void start() noexcept
	{
		if (!_started)
		{
			auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
			settings->registerSettingObserver(settings::General_ThemeColorIndex.name, this);
			_started = true;
		}
	}

	void stop() noexcept
	{
		if (_started)
		{
			auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
			settings->unregisterSettingObserver(settings::General_ThemeColorIndex.name, this);
			_started = false;
		}
	}

	Q_SIGNAL void themeColorNameChanged(qtMate::material::color::Name const /*themeColorName*/);

private:
	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override
	{
		if (name == settings::General_ThemeColorIndex.name)
		{
			auto const colorName = qtMate::material::color::Palette::name(value.toInt());
			emit themeColorNameChanged(colorName);
		}
	}

	bool _started{ false };
};
