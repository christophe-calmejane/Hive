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

#include "controlledEntityTreeWidget.hpp"
#include "nodeTreeWidget.hpp"
#include "errorItemDelegate.hpp"

#include <QSplitter>
#include <QLayout>

class EntityInspector : public QWidget
{
	Q_OBJECT
public:
	enum class RoleInfo
	{
		NodeType = Qt::UserRole,
		ErrorRole = ErrorItemDelegate::ErrorRole,
		IsActiveConfiguration,
	};

	EntityInspector(QWidget* parent = nullptr);

	void setControlledEntityID(la::avdecc::UniqueIdentifier const entityID);
	la::avdecc::UniqueIdentifier controlledEntityID() const;

	QByteArray saveState(int version = 0) const;
	bool restoreState(QByteArray const& state, int version = 0);

	Q_SIGNAL void stateChanged();

private:
	Q_SLOT void controllerOffline();
	Q_SLOT void entityOnline(la::avdecc::UniqueIdentifier const entityID);
	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID);
	Q_SLOT void entityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& name);

	void configureWindowTitle();

private:
	QHBoxLayout _layout{ this };
	QSplitter _splitter{ Qt::Vertical, this };
	ControlledEntityTreeWidget _controlledEntityTreeWiget{ this };
	NodeTreeWidget _nodeTreeWiget{ this };
	ErrorItemDelegate _itemDelegate{ this };
};
