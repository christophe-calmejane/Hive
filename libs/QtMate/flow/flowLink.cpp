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

#include "QtMate/flow/flowLink.hpp"
#include "QtMate/flow/flowStyle.hpp"

#include <QPainter>

namespace qtMate::flow
{
FlowLink::FlowLink(QGraphicsItem* parent)
	: QGraphicsPathItem{ parent }
{
	setZValue(-1.f);
	setPen(NODE_CONNECTION_PEN);
}

FlowLink::~FlowLink() = default;

void FlowLink::setStart(QPointF const& start)
{
	_start = start;
	updatePainterPath();
}

void FlowLink::setStop(QPointF const& stop)
{
	_stop = stop;
	updatePainterPath();
}

void FlowLink::updatePainterPath()
{
	auto dist = _start.x() - _stop.x();

	auto ratio = 0.5f;
	auto offset = 0.f;

	if (dist > 0)
	{
		offset = -dist;
	}

	dist = std::abs(dist);

	auto const c1 = QPointF{ _start.x() + dist * ratio, _start.y() + offset };
	auto const c2 = QPointF{ _stop.x() - dist * ratio, _stop.y() + offset };

	auto painterPath = QPainterPath{ _start };
	painterPath.cubicTo(c1, c2, _stop);
	setPath(painterPath);
}

} // namespace qtMate::flow
