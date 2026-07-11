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

#include <QFontMetricsF>
#include <QGraphicsScene>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace qtMate::graph
{
// Z ordering of the graph items: edge lines at the bottom, then edge labels, then nodes (default Z of 0)
static constexpr auto EdgeLineZValue = -2.0;
static constexpr auto EdgeLabelZValue = -1.0;

/**
* @brief Label of a GraphEdgeItem, as a dedicated top-level scene item.
* @details Drawn above all the edge lines (see Z ordering) so the text is never covered by nearby edges.
*          The text uses the palette text color over a translucent plate of the palette window color,
*          making it readable over any line in both light and dark themes.
*          Lifetime is managed by the owning GraphEdgeItem, with mutual detach notifications since a
*          QGraphicsScene destroys its items in an undefined order.
*/
class GraphEdgeLabelItem : public QGraphicsItem
{
public:
	GraphEdgeLabelItem(GraphEdgeItem* edge)
		: _edge{ edge }
	{
		setZValue(EdgeLabelZValue);
		// Let mouse interactions go through to the underlying items
		setAcceptedMouseButtons(Qt::NoButton);
	}

	virtual ~GraphEdgeLabelItem() override
	{
		if (_edge)
		{
			_edge->_labelItem = nullptr;
		}
	}

	void detachEdge()
	{
		_edge = nullptr;
	}

	void setText(QString const& text)
	{
		if (text != _text)
		{
			prepareGeometryChange();
			_text = text;
			update();
		}
	}

	virtual QRectF boundingRect() const override
	{
		return QRectF{ -90.0, -18.0, 180.0, 36.0 };
	}

	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* /*widget*/) override
	{
		if (_text.isEmpty())
		{
			return;
		}

		painter->setRenderHint(QPainter::Antialiasing);
		auto font = painter->font();
		font.setPointSizeF(font.pointSizeF() * 0.85);
		painter->setFont(font);

		// Translucent plate of the background color, so the text detaches from the lines it crosses
		auto const textRect = QFontMetricsF{ font }.boundingRect(boundingRect(), Qt::AlignCenter, _text);
		auto plateColor = option->palette.color(QPalette::Window);
		plateColor.setAlpha(200);
		painter->setPen(Qt::NoPen);
		painter->setBrush(plateColor);
		painter->drawRoundedRect(textRect.adjusted(-4.0, -1.0, 4.0, 1.0), 3.0, 3.0);

		// Palette text color: black on light theme, white on dark theme
		painter->setPen(option->palette.color(QPalette::WindowText));
		painter->drawText(boundingRect(), Qt::AlignCenter, _text);
	}

private:
	GraphEdgeItem* _edge{ nullptr };
	QString _text{};
};

GraphEdgeItem::GraphEdgeItem(GraphNodeItem* upstreamNode, GraphNodeItem* downstreamNode, QGraphicsItem* parent)
	: QGraphicsPathItem{ parent }
	, _upstreamNode{ upstreamNode }
	, _downstreamNode{ downstreamNode }
{
	setZValue(EdgeLineZValue);
	_labelItem = new GraphEdgeLabelItem{ this };

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
	// The label might already have been destroyed by the scene (items are destroyed in an undefined order)
	if (_labelItem)
	{
		_labelItem->detachEdge();
		delete _labelItem;
	}
}

void GraphEdgeItem::setLabel(QString const& label)
{
	if (_labelItem)
	{
		_labelItem->setText(label);
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

	if (_labelItem)
	{
		_labelItem->setPos(path.pointAtPercent(0.5));
	}
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

QPainterPath GraphEdgeItem::shape() const
{
	// Widen the interactive area (hover/click) around the line, a stroked thin curve would be nearly impossible to hit
	auto stroker = QPainterPathStroker{};
	stroker.setWidth(12.0);
	return stroker.createStroke(path());
}

void GraphEdgeItem::paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget)
{
	painter->setRenderHint(QPainter::Antialiasing);
	QGraphicsPathItem::paint(painter, option, widget);
}

QVariant GraphEdgeItem::itemChange(GraphicsItemChange change, QVariant const& value)
{
	// The label is a top-level item, it must follow the edge in and out of the scene
	if (change == QGraphicsItem::ItemSceneHasChanged && _labelItem)
	{
		if (auto* const currentScene = scene())
		{
			currentScene->addItem(_labelItem);
			_labelItem->setPos(path().isEmpty() ? QPointF{} : path().pointAtPercent(0.5));
		}
		else if (auto* const labelScene = _labelItem->scene())
		{
			labelScene->removeItem(_labelItem);
		}
	}
	return QGraphicsPathItem::itemChange(change, value);
}

} // namespace qtMate::graph
