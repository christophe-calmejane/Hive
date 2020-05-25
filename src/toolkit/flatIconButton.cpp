/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "toolkit/flatIconButton.hpp"

namespace qt
{
namespace toolkit
{
FlatIconButton::FlatIconButton(QWidget* parent)
	: FlatIconButton{ QString::null, QString::null, parent }
{
	//auto font = QFont{ "Material Icons" };
	//font.setStyleStrategy(QFont::PreferQuality);
	//setFont(font);
}

FlatIconButton::FlatIconButton(QString const& fontFamily, QString const& icon, QWidget* parent)
	: QPushButton{ icon, parent }
{
	auto font = QFont{ fontFamily };
	font.setStyleStrategy(QFont::PreferQuality);
	setFont(font);

	setFlat(true);
}

} // namespace toolkit
} // namespace qt
