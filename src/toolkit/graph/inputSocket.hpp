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

#pragma once

#include "socket.hpp"

namespace graph
{
class ConnectionItem;
class InputSocketItem final : public SocketItem
{
public:
	using SocketItem::SocketItem;

	virtual ~InputSocketItem();

	virtual int type() const override;

	virtual void updateGeometry() override;

	virtual bool isConnected() const override;

	//

	void setConnection(ConnectionItem* connection);
	ConnectionItem* connection() const;

private:
	ConnectionItem* _connection{ nullptr };
};

} // namespace graph
