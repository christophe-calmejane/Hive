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

#include "profileWidget.hpp"

namespace profiles
{
ProfileWidget::ProfileWidget(QString const& title, QString const& description, QString const& icon, QWidget* parent)
	: QFrame{ parent }
{
	_layout.addWidget(&_title, 0, 1);
	_layout.addWidget(&_description, 1, 1);
	_layout.addWidget(&_icon, 0, 0, -1, 1);

	_title.setText(title);
	_title.setStyleSheet("font-size: 20px; font-weight: bold;");

	_description.setText(description);

	_icon.setStyleSheet("font-size: 40px; font-family: 'Material Icons';");
	_icon.setText(icon);
	_icon.setFixedWidth(60);

	setStyleSheet(R"(
		profiles--ProfileWidget {
			background-color: #ddd;
		}
		profiles--ProfileWidget:hover {
			background-color: #ccc;
		}
	)");
}

void ProfileWidget::mouseReleaseEvent(QMouseEvent* event)
{
	emit clicked();
	QWidget::mouseReleaseEvent(event);
}
} // namespace profiles
