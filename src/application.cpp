/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "application.hpp"

#include <QGuiApplication>
#include <QStyleHints>
#include <QStyle>
#include <QFileOpenEvent>

HiveApplication::HiveApplication(int& argc, char** argv)
	: QApplication(argc, argv)
{
	// Get the current style (before a stylesheet is applied, which will cause the name to be lost)
	if (auto const* const style = QApplication::style(); style)
	{
		auto const styleName = style->name();
		checkStyleName(styleName);
	}

	// Set color scheme based on the current system color scheme
	setIsDarkColorScheme(QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);

	// Color Scheme changed
	connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
		[this](Qt::ColorScheme scheme)
		{
			setIsDarkColorScheme(scheme == Qt::ColorScheme::Dark);
		});

	// Install the event filter to catch macOS FileOpen events
	installEventFilter(this);

	// Process the events immediately so we can catch the FileOpen event before the MainWindow is created
	processEvents();

	// Note: the event filter is kept installed, so FileOpen events received before the MainWindow installs its own filter
	// (eg. while the splashscreen is displayed) are buffered instead of being lost. Once the MainWindow filter is
	// installed it is activated first (installed last) and consumes the events, so this filter no longer sees them.
}

void HiveApplication::addFileToLoad(QString const& filePath)
{
	_filesToLoad.append(filePath);
}

QStringList const& HiveApplication::getFilesToLoad() const
{
	return _filesToLoad;
}

QStringList HiveApplication::takeFilesToLoad()
{
	auto files = std::move(_filesToLoad);
	_filesToLoad.clear();
	return files;
}

bool HiveApplication::isDarkColorScheme() const noexcept
{
	return _isDarkColorScheme;
}

bool HiveApplication::isDarkPaletteSupported() const noexcept
{
	return _isDarkPaletteSupported;
}

void HiveApplication::setStyleName(QString const& styleName) noexcept
{
	checkStyleName(styleName);
	setIsDarkColorScheme(QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
	setStyle(styleName);
}

bool HiveApplication::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::FileOpen)
	{
		auto const* const fileEvent = static_cast<QFileOpenEvent const*>(event);
		addFileToLoad(fileEvent->file());
		return true;
	}
	return false;
}

void HiveApplication::setIsDarkColorScheme(bool const isDarkColorScheme)
{
	if (_isDarkPaletteSupported)
	{
		_isDarkColorScheme = isDarkColorScheme;
	}
	else
	{
		_isDarkColorScheme = false;
	}
	setProperty("isDarkColorScheme", _isDarkColorScheme);
}

void HiveApplication::checkStyleName(QString const& styleName) noexcept
{
	_isDarkPaletteSupported = true;
	if (styleName == "windowsvista")
	{
		_isDarkPaletteSupported = false;
	}
}
