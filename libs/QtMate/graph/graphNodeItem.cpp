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

#include "QtMate/graph/graphNodeItem.hpp"
#include "QtMate/graph/graphEdgeItem.hpp"

#include <QPainter>

namespace qtMate::graph
{
GraphNodeItem::GraphNodeItem(QString const& label, QSizeF const& size, QGraphicsItem* parent)
	: QGraphicsItem{ parent }
	, _label{ label }
	, _size{ size }
{
	setFlag(QGraphicsItem::ItemIsMovable, true);
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

GraphNodeItem::~GraphNodeItem()
{
	// Detach all edges still referencing this node, so they don't end up with a dangling pointer (destruction order of QGraphicsScene items is undefined)
	for (auto* const edge : _edges)
	{
		edge->detachNode(this);
	}
}

QString const& GraphNodeItem::label() const noexcept
{
	return _label;
}

void GraphNodeItem::setLabel(QString const& label)
{
	if (label != _label)
	{
		_label = label;
		update();
	}
}

QSizeF const& GraphNodeItem::size() const noexcept
{
	return _size;
}

QPointF GraphNodeItem::topAnchorScenePos() const
{
	return mapToScene(QPointF{ _size.width() / 2.0, 0.0 });
}

QPointF GraphNodeItem::bottomAnchorScenePos() const
{
	return mapToScene(QPointF{ _size.width() / 2.0, _size.height() });
}

int GraphNodeItem::type() const
{
	return Type;
}

QRectF GraphNodeItem::boundingRect() const
{
	// Add a small margin so the pen is not clipped
	return QRectF{ QPointF{ 0.0, 0.0 }, _size }.adjusted(-1.0, -1.0, 1.0, 1.0);
}

void GraphNodeItem::paint(QPainter* painter, QStyleOptionGraphicsItem const* /*option*/, QWidget* /*widget*/)
{
	auto const rect = QRectF{ QPointF{ 0.0, 0.0 }, _size };

	painter->setRenderHint(QPainter::Antialiasing);
	painter->setPen(QPen{ isSelected() ? Qt::blue : Qt::black, 1.0 });
	painter->setBrush(Qt::white);
	painter->drawRoundedRect(rect, 4.0, 4.0);
	painter->setPen(Qt::black);
	painter->drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, _label);
}

QVariant GraphNodeItem::itemChange(GraphicsItemChange change, QVariant const& value)
{
	if (change == QGraphicsItem::ItemPositionHasChanged)
	{
		for (auto* const edge : _edges)
		{
			edge->updatePath();
		}
	}
	return QGraphicsItem::itemChange(change, value);
}

void GraphNodeItem::registerEdge(GraphEdgeItem* edge)
{
	_edges.insert(edge);
}

void GraphNodeItem::unregisterEdge(GraphEdgeItem* edge)
{
	_edges.erase(edge);
}

} // namespace qtMate::graph
