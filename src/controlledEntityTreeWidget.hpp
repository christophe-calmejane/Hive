/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QTreeWidget>
#include <la/avdecc/internals/uniqueIdentifier.hpp>
#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>

class ControlledEntityTreeWidgetPrivate;
class ControlledEntityTreeWidget : public QTreeWidget
{
	Q_OBJECT
public:
	ControlledEntityTreeWidget(QWidget* parent = nullptr);
	~ControlledEntityTreeWidget();

	void setControlledEntityID(la::avdecc::UniqueIdentifier const entityID);
	la::avdecc::UniqueIdentifier controlledEntityID() const;

private:
	ControlledEntityTreeWidgetPrivate* d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(ControlledEntityTreeWidget)
};
