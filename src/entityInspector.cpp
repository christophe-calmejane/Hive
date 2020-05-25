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

#include "entityInspector.hpp"
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"

#include <QHeaderView>

Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)

EntityInspector::EntityInspector(QWidget* parent)
	: QWidget(parent)
{
	_layout.setContentsMargins(0, 0, 0, 0);
	_layout.addWidget(&_splitter);

	_controlledEntityTreeWiget.setItemDelegate(&_itemDelegate);
	_nodeTreeWiget.setItemDelegate(&_itemDelegate);

	_splitter.addWidget(&_controlledEntityTreeWiget);
	_splitter.addWidget(&_nodeTreeWiget);
	_splitter.setStretchFactor(0, 0); // Descriptors List has less weight than
	_splitter.setStretchFactor(1, 1); // the Descriptor itself, as far as expand is concerned

	_nodeTreeWiget.setColumnCount(2);
	_nodeTreeWiget.setHeaderLabels({ "", "" });

	connect(_controlledEntityTreeWiget.selectionModel(), &QItemSelectionModel::currentChanged, this,
		[this](QModelIndex const& index, QModelIndex const&)
		{
			auto const entityID = _controlledEntityTreeWiget.controlledEntityID();
			auto const anyNode = index.data(la::avdecc::utils::to_integral(EntityInspector::RoleInfo::NodeType)).value<AnyNode>();
			auto const isActiveConfiguration = index.data(la::avdecc::utils::to_integral(EntityInspector::RoleInfo::IsActiveConfiguration)).toBool();

			_nodeTreeWiget.setNode(entityID, isActiveConfiguration, anyNode);
		});

	connect(&_splitter, &QSplitter::splitterMoved, this, &EntityInspector::stateChanged);
	connect(_nodeTreeWiget.header(), &QHeaderView::sectionResized, this, &EntityInspector::stateChanged);

	auto& controllerManager = avdecc::ControllerManager::getInstance();

	connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &EntityInspector::controllerOffline);
	connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &EntityInspector::entityOnline);
	connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &EntityInspector::entityOffline);
	connect(&controllerManager, &avdecc::ControllerManager::entityNameChanged, this, &EntityInspector::entityNameChanged);
}

void EntityInspector::setControlledEntityID(la::avdecc::UniqueIdentifier const entityID)
{
	_controlledEntityTreeWiget.setControlledEntityID(entityID);
	setEnabled(true);
	configureWindowTitle();
}

la::avdecc::UniqueIdentifier EntityInspector::controlledEntityID() const
{
	return _controlledEntityTreeWiget.controlledEntityID();
}

QByteArray EntityInspector::saveState(int /*version*/) const
{
	QMap<int, QByteArray> map;
	map[0] = _splitter.saveState();
	map[1] = _nodeTreeWiget.header()->saveState();

	QByteArray buffer;
	QDataStream stream(&buffer, QIODevice::WriteOnly);
	stream << map;

	return buffer;
}

bool EntityInspector::restoreState(QByteArray const& state, int /*version*/)
{
	QMap<int, QByteArray> map;
	QByteArray buffer{ state };

	QDataStream stream(&buffer, QIODevice::ReadOnly);
	stream >> map;

	_splitter.restoreState(map[0]);
	_nodeTreeWiget.header()->restoreState(map[1]);

	return true;
}

void EntityInspector::controllerOffline()
{
	if (isWindow())
	{
		window()->close();
	}
}

void EntityInspector::entityOnline(la::avdecc::UniqueIdentifier const entityID)
{
	if (entityID == _controlledEntityTreeWiget.controlledEntityID())
	{
		setEnabled(true);
		configureWindowTitle();
	}
}

void EntityInspector::entityOffline(la::avdecc::UniqueIdentifier const entityID)
{
	if (entityID == _controlledEntityTreeWiget.controlledEntityID())
	{
		setEnabled(false);
		setWindowTitle(windowTitle() + " (Offline)");
	}
}

void EntityInspector::entityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& /*name*/)
{
	if (entityID == _controlledEntityTreeWiget.controlledEntityID())
	{
		configureWindowTitle();
	}
}

void EntityInspector::configureWindowTitle()
{
	auto& manager = avdecc::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(_controlledEntityTreeWiget.controlledEntityID());
	if (controlledEntity)
	{
		setWindowTitle(avdecc::helper::smartEntityName(*controlledEntity));
	}
}
