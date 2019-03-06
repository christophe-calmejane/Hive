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

#include "materialButton.hpp"

namespace qt
{
namespace toolkit
{
MaterialButton::MaterialButton(QWidget* parent)
	: MaterialButton{ QString::null, parent }
{
}

MaterialButton::MaterialButton(QString const& icon, QWidget* parent)
	: QPushButton{ icon, parent }
{
	QFont font{ "Material Icons" };
	font.setStyleStrategy(QFont::PreferQuality);
	setFont(font);

	setFlat(true);
}

} // namespace toolkit
} // namespace qt
