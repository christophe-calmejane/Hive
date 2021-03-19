/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "QtMate/graph/connection.hpp"
#include "QtMate/graph/inputSocket.hpp"
#include "QtMate/graph/outputSocket.hpp"
#include "QtMate/graph/type.hpp"

#include <QPainter>

namespace qtMate
{
namespace graph
{
ConnectionItem::ConnectionItem()
{
	setFlag(QGraphicsItem::ItemIsSelectable);
	setZValue(-1);
}

ConnectionItem::~ConnectionItem()
{
	disconnect();
}

void ConnectionItem::setStart(QPointF const& p)
{
	_start = p;
	updatePath();
}

void ConnectionItem::setStop(QPointF const& p)
{
	_stop = p;
	updatePath();
}

int ConnectionItem::type() const
{
	return ItemType::Connection;
}

void ConnectionItem::connectInput(InputSocketItem* input)
{
	disconnectInput();
	_input = input;
	if (_input)
	{
		_input->setConnection(this);
	}
}

InputSocketItem* ConnectionItem::input() const
{
	return _input;
}

void ConnectionItem::disconnectInput()
{
	if (_input)
	{
		_input->setConnection(nullptr);
		_input = nullptr;
	}
}

void ConnectionItem::connectOutput(OutputSocketItem* output)
{
	disconnectOutput();
	_output = output;
	if (_output)
	{
		_output->addConnection(this);
	}
}

OutputSocketItem* ConnectionItem::output() const
{
	return _output;
}

void ConnectionItem::disconnectOutput()
{
	if (_output)
	{
		_output->removeConnection(this);
		_output = nullptr;
	}
}

void ConnectionItem::disconnect()
{
	disconnectInput();
	disconnectOutput();
}

void ConnectionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
	QPen pen;

	auto const connected = _input && _output;
	pen.setStyle(connected ? Qt::SolidLine : Qt::DotLine);

	pen.setWidth(3);
	pen.setColor(NodeItemColor);
	painter->setPen(pen);
	painter->drawPath(path());
}

void ConnectionItem::updatePath()
{
	auto dist = _start.x() - _stop.x();

	auto ratio = 0.5f;
	auto offset = 0.f;

	if (dist > 0)
	{
		ratio = 0.9f;
		offset = -dist;
	}

	dist = std::abs(dist);

	QPointF c1(_start.x() + dist * ratio, _start.y() + offset);
	QPointF c2(_stop.x() - dist * ratio, _stop.y() + offset);

	QPainterPath path(_start);
	path.cubicTo(c1, c2, _stop);
	setPath(path);
}

} // namespace graph
} // namespace qtMate
