/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QGraphicsItem>

namespace qtMate
{
namespace graph
{
class SocketItem : public QGraphicsItem
{
public:
	SocketItem(int nodeId, int index, QString const& text, QGraphicsItem* parent = nullptr);

	// Associated node
	int nodeId() const;
	// Socket index in this node
	int index() const;

	QString const& text() const;
	QSizeF size() const;

	virtual bool isConnected() const = 0;
	virtual QRectF boundingRect() const override;

	bool isOver(QPointF const& pos) const;

protected:
	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget) override;
	virtual void updateGeometry() = 0;

private:
	int _nodeId{ -1 };
	int _index{ -1 };

	QString _text{};
	QSize _size{};
};

} // namespace graph
} // namespace qtMate
