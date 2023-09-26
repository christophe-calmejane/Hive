/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QApplication>

class HiveApplication : public QApplication
{
public:
	HiveApplication(int& argc, char** argv);

	void addFileToLoad(QString const& filePath);

	QStringList const& getFilesToLoad() const;

	bool isDarkColorScheme() const noexcept;

	bool isDarkPaletteSupported() const noexcept;

	void setStyleName(QString const& styleName) noexcept;

private:
	// QObject overrides
	bool eventFilter(QObject* watched, QEvent* event) override;

	// Private methods
	void setIsDarkColorScheme(bool const isDarkColorScheme);
	void checkStyleName(QString const& styleName) noexcept;

	// Private members
	QStringList _filesToLoad{};
	bool _isDarkColorScheme{ false };
	bool _isDarkPaletteSupported{ true };
};
