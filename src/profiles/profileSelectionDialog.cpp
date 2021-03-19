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

#include "profileSelectionDialog.hpp"
#include "profileWidget.hpp"

#include <QApplication>

namespace profiles
{
ProfileSelectionDialog::ProfileSelectionDialog(QWidget* parent)
	: QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
{
	setWindowTitle("Please Choose The Default User Profile");
	_layout.setSpacing(20);

	auto const addProfile = [this](QString const& title, QString const& description, QString const& icon, ProfileType const profile)
	{
		auto* widget = new ProfileWidget{ title, description, icon, this };
		_layout.addWidget(widget);

		connect(widget, &ProfileWidget::clicked, this,
			[this, profile]()
			{
				_selectedProfile = profile;
				accept();
			});
	};

	// Describe and add profiles
	addProfile("Standard (Default)", "Intended for standard users, application engineers.\nChoose this if in doubt.", "face", ProfileType::Standard);
	addProfile("Advanced", "Intended for advanced users and developers.", "school", ProfileType::Developer);
}

ProfileType ProfileSelectionDialog::selectedProfile() const
{
	return _selectedProfile;
}

} // namespace profiles
