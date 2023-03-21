/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "QtMate/flow/flowInput.hpp"
#include "QtMate/flow/flowConnection.hpp"
#include "QtMate/flow/flowStyle.hpp"

#include <QPainter>

namespace qtMate::flow
{
FlowInput::~FlowInput()
{
	if (_connection)
	{
		_connection->setInput(nullptr);
		_connection = nullptr;
	}
}

int FlowInput::type() const
{
	return Type;
}

QRectF FlowInput::boundingRect() const
{
	auto const availableWidth = parentItem()->boundingRect().width();
	return QRectF{ 0.f, 0.f, availableWidth, NODE_LINE_HEIGHT };
}

bool FlowInput::isConnected() const
{
	return _connection != nullptr;
}

QRectF FlowInput::hotSpotBoundingRect() const
{
	auto const r = boundingRect();
	return QRectF{ 0.f, 0.f, NODE_SOCKET_BOUNDING_SIZE, r.height() };
}

FlowConnection* FlowInput::connection() const
{
	return _connection;
}

void FlowInput::setConnection(FlowConnection* connection)
{
	if (connection != _connection)
	{
		_connection = connection;
		update();
	}
}

void FlowInput::updateConnection()
{
	if (_connection)
	{
		_connection->updatePath();
	}
}

void FlowInput::paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget)
{
	auto const hotSpot = hotSpotBoundingRect().center();
	drawInputHotSpot(painter, hotSpot, _color, isConnected());

	auto const nameBoundingRect = boundingRect().adjusted(NODE_SOCKET_BOUNDING_SIZE, 0.f, 0.f, 0.f);

	painter->setPen(NODE_TEXT_COLOR);
	painter->setBrush(Qt::NoBrush);
	drawElidedText(painter, nameBoundingRect, Qt::AlignVCenter | Qt::AlignLeft, Qt::TextElideMode::ElideMiddle, _descriptor.name);
}

} // namespace qtMate::flow
