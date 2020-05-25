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

#include "outputSocket.hpp"
#include "connection.hpp"
#include "type.hpp"

namespace graph
{
OutputSocketItem::~OutputSocketItem()
{
	clearConnections();
}

int OutputSocketItem::type() const
{
	return ItemType::Output;
}

void OutputSocketItem::updateGeometry()
{
	for (auto& connection : _connections)
	{
		connection->setStart(mapToScene(0, 0));
	}
}

bool OutputSocketItem::isConnected() const
{
	return !_connections.empty();
}

void OutputSocketItem::addConnection(ConnectionItem* connection)
{
	_connections.insert(connection);
	updateGeometry();
}

void OutputSocketItem::removeConnection(ConnectionItem* connection)
{
	_connections.erase(connection);
}

void OutputSocketItem::clearConnections()
{
	auto localConnections{ _connections };
	for (auto& connection : localConnections)
	{
		connection->disconnect();
	}
}

ConnectionItems const& OutputSocketItem::connections() const
{
	return _connections;
}

} // namespace graph
