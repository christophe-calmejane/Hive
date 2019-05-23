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

#pragma once

#include <QDialog>
#include <QGridLayout>
#include <QSignalMapper>

class ProfileWidget;

class ProfileSelectionDialog : public QDialog
{
	Q_OBJECT
public:
	enum class Profile
	{
		Standard,
		Developer,

		Default = Standard,
	};

	ProfileSelectionDialog(QWidget* parent = nullptr);

	Profile selectedProfile() const;

private Q_SLOTS:
	void onProfileSelected(int profile);

private:
	QGridLayout _layout{ this };
	Profile _selectedProfile{ Profile::Default };
	QSignalMapper _signalMapper{};
};
