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

#include "QtMate/flow/flowSocket.hpp"
#include "QtMate/flow/flowNode.hpp"

namespace qtMate::flow
{
FlowSocket::FlowSocket(FlowNode* node, FlowSocketIndex index, FlowSocketDescriptor const& descriptor)
	: QGraphicsItem{ node }
	, _node{ node }
	, _index{ index }
	, _descriptor{ descriptor }
{
}

FlowSocket::~FlowSocket() = default;

FlowNode* FlowSocket::node() const
{
	return _node;
}

FlowSocketIndex FlowSocket::index() const
{
	return _index;
}

FlowSocketDescriptor const& FlowSocket::descriptor() const
{
	return _descriptor;
}

FlowSocketSlot FlowSocket::slot() const
{
	return FlowSocketSlot{ _node->uid(), _index };
}

QColor const& FlowSocket::color() const
{
	return _color;
}

void FlowSocket::setColor(QColor const& color)
{
	if (color != _color)
	{
		_color = color;
		update();
	}
}

bool FlowSocket::hit(QPointF const& scenePos) const
{
	auto const localPos = mapFromScene(scenePos);
	return boundingRect().contains(localPos);
}

QPointF FlowSocket::hotSpotSceneCenter() const
{
	return mapToScene(hotSpotBoundingRect().center());
}

} // namespace qtMate::flow
