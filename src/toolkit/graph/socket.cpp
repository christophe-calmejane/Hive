/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "socket.hpp"
#include "type.hpp"

#include <QPainter>
#include <cassert>

namespace graph
{
const float PEN_WIDTH = 1.f;
const float CIRCLE_RADIUS = 6.f;
const float MIN_WIDTH = 50.f;
const float MIN_HEIGHT = 15.f;
const float TEXT_OFFSET = 5.f;

SocketItem::SocketItem(int nodeId, int index, QString const& text, QGraphicsItem* parent)
	: QGraphicsItem(parent)
	, _nodeId(nodeId)
	, _index(index)
	, _text(text)
{
	QFont font;
	QFontMetrics fm(font);
	auto const textWidth = fm.width(_text);
	auto const textHeight = fm.height();
	auto const width = std::max(MIN_WIDTH, CIRCLE_RADIUS * 2 + TEXT_OFFSET + textWidth + PEN_WIDTH);
	auto const height = std::max(MIN_HEIGHT, textHeight + PEN_WIDTH);
	_size = QSize(width, height);
}

int SocketItem::nodeId() const
{
	return _nodeId;
}

int SocketItem::index() const
{
	return _index;
}

QString const& SocketItem::text() const
{
	return _text;
}

QSizeF SocketItem::size() const
{
	return _size;
}

QRectF SocketItem::boundingRect() const
{
	auto const s = size();

	auto const x = -CIRCLE_RADIUS - PEN_WIDTH / 2.f;
	auto const y = -s.height() / 2.f - PEN_WIDTH / 2.f;

	switch (type())
	{
		case ItemType::Input:
		{
			return QRectF(QPointF(x, y), s).normalized();
		}

		case ItemType::Output:
		{
			return QRectF(QPointF(-s.width() - x, y), s).normalized();
		}

		default:
			assert(true && "Invalid item type");
			return {};
	}
}

bool SocketItem::isOver(QPointF const& pos) const
{
	return pos.x() >= -CIRCLE_RADIUS && pos.x() <= CIRCLE_RADIUS && pos.y() >= -CIRCLE_RADIUS && pos.y() <= CIRCLE_RADIUS;
}

void SocketItem::paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget)
{
	QPen pen;
	pen.setWidth(2);
	painter->setPen(NodeItemColor);

	switch (type())
	{
		case ItemType::Input:
		{
			painter->setBrush(InputSocketColor);
			break;
		}
		case ItemType::Output:
		{
			painter->setBrush(OutputSocketColor);
			break;
		}
		default:
			assert(true && "Invalid item type");
	}

	painter->drawEllipse(-CIRCLE_RADIUS, -CIRCLE_RADIUS, CIRCLE_RADIUS * 2, CIRCLE_RADIUS * 2);

	if (isConnected())
	{
		painter->setPen(Qt::NoPen);
		painter->setBrush(NodeItemColor);
		painter->drawEllipse(-CIRCLE_RADIUS / 2, -CIRCLE_RADIUS / 2, CIRCLE_RADIUS, CIRCLE_RADIUS);
	}

	auto const size = 32767.f;

	int flags{ 0 };
	QPointF corner{ 0, 0 };

	switch (type())
	{
		case ItemType::Input:
		{
			flags = Qt::AlignVCenter | Qt::AlignLeft;

			corner.setX(CIRCLE_RADIUS + TEXT_OFFSET);
			corner.setY(-size);
			corner.ry() += size / 2.;
			break;
		}
		case ItemType::Output:
		{
			flags = Qt::AlignVCenter | Qt::AlignRight;

			corner.setX(-CIRCLE_RADIUS - TEXT_OFFSET);
			corner.setY(-size);
			corner.ry() += size / 2.;
			corner.rx() -= size;
			break;
		}
		default:
			assert(true && "Invalid item type");
	}

	QRectF rect(corner, QSizeF{ size, size });

	painter->setPen(TextColor);
	painter->drawText(rect.adjusted(0, -2, 0, -2), flags, _text, nullptr);
}

} // namespace graph
