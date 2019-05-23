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

#include "profileSelection/profileSelectionDialog.hpp"
#include "profileSelection/profileWidget.hpp"

#include <QApplication>

ProfileSelectionDialog::ProfileSelectionDialog(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(QCoreApplication::applicationName() + " Profile Selection");
	_layout.setSpacing(20);

	// Handle profile widget mapping
	connect(&_signalMapper, SIGNAL(mapped(int)), this, SLOT(onProfileSelected(int)));

	auto const addProfile = [this](QString const& title, QString const& description, QString const& icon, Profile const profile)
	{
		auto* widget = new ProfileWidget{ title, description, icon, this };
		_layout.addWidget(widget);

		connect(widget, SIGNAL(clicked()), &_signalMapper, SLOT(map()));
		_signalMapper.setMapping(widget, static_cast<int>(profile));
	};

	// Describe and add profiles
	addProfile("Standard", "Standard profile description", "face", Profile::Standard);
	addProfile("Developer", "Developer user profile description", "bug_report", Profile::Developer);
}

ProfileSelectionDialog::Profile ProfileSelectionDialog::selectedProfile() const
{
	return _selectedProfile;
}

void ProfileSelectionDialog::onProfileSelected(int profile)
{
	_selectedProfile = static_cast<Profile>(profile);
	accept();
}
