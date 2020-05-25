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

#include <QMenu>

namespace qt
{
namespace toolkit
{
/* TickableMenu do not close the menu after an action has been triggered by a user click */
class TickableMenu : public QMenu
{
public:
	using QMenu::QMenu;

protected:
	virtual void mouseReleaseEvent(QMouseEvent* event) override;
};

} // namespace toolkit
} // namespace qt
