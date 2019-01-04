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

#include "inputSocket.hpp"
#include "connection.hpp"
#include "type.hpp"

namespace graph
{
InputSocketItem::~InputSocketItem()
{
	if (_connection)
	{
		_connection->disconnect();
	}
}

int InputSocketItem::type() const
{
	return ItemType::Input;
}

void InputSocketItem::updateGeometry()
{
	if (_connection)
	{
		_connection->setStop(mapToScene(0, 0));
	}
}

bool InputSocketItem::isConnected() const
{
	return _connection != nullptr;
}

void InputSocketItem::setConnection(ConnectionItem* connection)
{
	_connection = connection;
	updateGeometry();
}

ConnectionItem* InputSocketItem::connection() const
{
	return _connection;
}

} // namespace graph
