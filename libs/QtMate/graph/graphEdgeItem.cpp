/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "QtMate/graph/graphEdgeItem.hpp"
#include "QtMate/graph/graphNodeItem.hpp"

#include <QPainter>

namespace qtMate::graph
{
GraphEdgeItem::GraphEdgeItem(GraphNodeItem* upstreamNode, GraphNodeItem* downstreamNode, QGraphicsItem* parent)
	: QGraphicsPathItem{ parent }
	, _upstreamNode{ upstreamNode }
	, _downstreamNode{ downstreamNode }
{
	// Draw edges behind nodes
	setZValue(-1.0);

	_upstreamNode->registerEdge(this);
	_downstreamNode->registerEdge(this);

	updatePath();
}

GraphEdgeItem::~GraphEdgeItem()
{
	if (_upstreamNode)
	{
		_upstreamNode->unregisterEdge(this);
	}
	if (_downstreamNode)
	{
		_downstreamNode->unregisterEdge(this);
	}
}

void GraphEdgeItem::setLabel(QString const& label)
{
	if (label != _label)
	{
		prepareGeometryChange();
		_label = label;
		update();
	}
}

void GraphEdgeItem::setLinePen(QPen const& pen)
{
	setPen(pen);
}

void GraphEdgeItem::updatePath()
{
	if (!_upstreamNode || !_downstreamNode)
	{
		return;
	}

	auto const start = _upstreamNode->bottomAnchorScenePos();
	auto const end = _downstreamNode->topAnchorScenePos();

	// Vertical cubic curve, control points at one third and two thirds of the vertical span
	auto const dy = (end.y() - start.y()) / 3.0;
	auto path = QPainterPath{ start };
	path.cubicTo(QPointF{ start.x(), start.y() + dy }, QPointF{ end.x(), end.y() - dy }, end);

	setPath(path);
}

void GraphEdgeItem::detachNode(GraphNodeItem* node)
{
	if (_upstreamNode == node)
	{
		_upstreamNode = nullptr;
	}
	if (_downstreamNode == node)
	{
		_downstreamNode = nullptr;
	}
}

int GraphEdgeItem::type() const
{
	return Type;
}

QRectF GraphEdgeItem::boundingRect() const
{
	// Extend the path bounding rect so the label always fits
	return QGraphicsPathItem::boundingRect().adjusted(-50.0, -10.0, 50.0, 10.0);
}

void GraphEdgeItem::paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget)
{
	painter->setRenderHint(QPainter::Antialiasing);
	QGraphicsPathItem::paint(painter, option, widget);

	if (!_label.isEmpty())
	{
		auto const center = path().pointAtPercent(0.5);
		auto font = painter->font();
		font.setPointSizeF(font.pointSizeF() * 0.85);
		painter->setFont(font);
		painter->setPen(pen().color());
		auto const textRect = QRectF{ center.x() - 50.0, center.y() - 8.0, 100.0, 16.0 };
		painter->drawText(textRect, Qt::AlignCenter, _label);
	}
}

} // namespace qtMate::graph
