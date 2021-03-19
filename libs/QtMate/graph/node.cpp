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

#include "QtMate/graph/node.hpp"
#include "QtMate/graph/inputSocket.hpp"
#include "QtMate/graph/outputSocket.hpp"
#include "QtMate/graph/type.hpp"

#include <QPainter>

namespace qtMate
{
namespace graph
{
float const TITLE_HEIGHT = 20.f;
float const PADDING = 5.f;

NodeItem::NodeItem(int id, QString const& text, QGraphicsItem* parent)
	: QGraphicsItem(parent)
	, _id(id)
	, _text(text)
{
	setAcceptHoverEvents(true);

	updateGeometry();
}

int NodeItem::type() const
{
	return ItemType::Node;
}

void NodeItem::addInput(QString const& text)
{
	auto const socketIndex = static_cast<int>(_inputs.size());
	auto* item = new InputSocketItem(_id, socketIndex, text, this);
	_inputs.push_back(item);
	updateGeometry();
}

void NodeItem::addOutput(QString const& text)
{
	auto const socketIndex = static_cast<int>(_outputs.size());
	auto* item = new OutputSocketItem(_id, socketIndex, text, this);
	_outputs.push_back(item);
	updateGeometry();
}

InputSocketItem* NodeItem::inputAt(int index) const
{
	return _inputs.at(index);
}

OutputSocketItem* NodeItem::outputAt(int index) const
{
	return _outputs.at(index);
}

QRectF NodeItem::boundingRect() const
{
	return QRectF{ 0, 0, _width, _height };
}

void NodeItem::paint(QPainter* painter, QStyleOptionGraphicsItem const* /*option*/, QWidget* /*widget*/)
{
	painter->setPen(Qt::NoPen);
	painter->setBrush(NodeItemColor);

	painter->drawRoundedRect(boundingRect(), 5, 5);
	painter->drawRect(boundingRect().adjusted(0, TITLE_HEIGHT, 0, 0));

	painter->setBrush(NodeItemColor.lighter());
	painter->drawRect(boundingRect().adjusted(PADDING / 2, TITLE_HEIGHT, -PADDING / 2, -PADDING / 2));

	auto r = boundingRect().adjusted(PADDING, 0, -PADDING, 0);
	r.setHeight(TITLE_HEIGHT);

	painter->setPen(TextColor);
	painter->drawText(r, Qt::AlignHCenter | Qt::AlignVCenter, _text, nullptr);
}

QVariant NodeItem::itemChange(GraphicsItemChange change, QVariant const& value)
{
	if (change == QGraphicsItem::ItemSelectedChange)
	{
		setZValue(value.toBool() ? 1 : 0);
	}

	if (change == QGraphicsItem::ItemPositionChange || change == QGraphicsItem::ItemPositionHasChanged)
	{
		propagateChanges();
	}

	return QGraphicsItem::itemChange(change, value);
}

void NodeItem::updateGeometry()
{
	prepareGeometryChange();

	// Geometry

	auto maxInputWidth{ 0 };
	auto maxOutputWidth{ 0 };

	for (auto const& input : _inputs)
	{
		maxInputWidth = std::max(maxInputWidth, static_cast<int>(input->boundingRect().width()));
	}

	for (auto const& output : _outputs)
	{
		maxOutputWidth = std::max(maxOutputWidth, static_cast<int>(output->boundingRect().width()));
	}

	QFont font;
	QFontMetrics fm(font);
	auto const textHeight = static_cast<float>(fm.height());

	auto const max = std::max(1, std::max(_inputs.size(), _outputs.size()));
	auto const step = textHeight + PADDING * 2;

	_height = step * max;
	_height += TITLE_HEIGHT;

	_width = std::max(maxInputWidth + PADDING + maxOutputWidth, fm.horizontalAdvance(_text) + PADDING * 2);

	// Input & Outputs

	auto const ystart = TITLE_HEIGHT + PADDING;

	auto ypos1 = ystart;
	for (auto i = 0; i < static_cast<int>(_inputs.size()); ++i)
	{
		auto const size = _inputs[i]->size();
		_inputs[i]->setPos(0, ypos1 + size.height() / 2.0);
		ypos1 += step;
	}

	auto ypos2 = ystart;
	for (auto i = 0; i < static_cast<int>(_outputs.size()); ++i)
	{
		auto const size = _outputs[i]->size();
		_outputs[i]->setPos(boundingRect().width(), ypos2 + size.height() / 2.0);
		ypos2 += step;
	}

	//

	propagateChanges();
}

void NodeItem::propagateChanges()
{
	for (auto& input : _inputs)
	{
		input->updateGeometry();
	}
	for (auto& output : _outputs)
	{
		output->updateGeometry();
	}
}

} // namespace graph
} // namespace qtMate
