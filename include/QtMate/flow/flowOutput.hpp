/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QtMate/flow/flowSocket.hpp>

namespace qtMate::flow
{
class FlowOutput : public FlowSocket
{
public:
	using FlowSocket::FlowSocket;

	virtual ~FlowOutput() override;

	enum
	{
		Type = UserType + 3
	};
	virtual int type() const override;
	virtual QRectF boundingRect() const override;

	virtual bool isConnected() const override;
	virtual QRectF hotSpotBoundingRect() const override;

	void addConnection(FlowConnection* connection);
	void removeConnection(FlowConnection* connection);
	FlowConnections const& connections() const;

	void updateConnections();

	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget = nullptr) override;

private:
	FlowConnections _connections{};
};

} // namespace qtMate::flow
