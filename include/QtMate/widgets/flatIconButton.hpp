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

#pragma once

#include <QPushButton>

namespace qtMate
{
namespace widgets
{
// Button that renders a flat icon from an icons font
class FlatIconButton : public QPushButton
{
	Q_OBJECT
public:
	FlatIconButton(QWidget* parent = nullptr);
	FlatIconButton(QString const& fontFamily, QString const& icon, QWidget* parent = nullptr);
};

} // namespace widgets
} // namespace qtMate
