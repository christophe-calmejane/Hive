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

#pragma once

#include <QGraphicsItem>
#include <QtMate/flow/flowDefs.hpp>

namespace qtMate::flow
{
class FlowSocket : public QGraphicsItem
{
public:
	FlowSocket(FlowNode* node, FlowSocketIndex index, FlowSocketDescriptor const& descriptor);
	virtual ~FlowSocket() override;

	FlowNode* node() const;
	FlowSocketIndex index() const;
	FlowSocketDescriptor const& descriptor() const;
	FlowSocketSlot slot() const;

	QColor const& color() const;
	void setColor(QColor const& color);

	bool hit(QPointF const& scenePos) const;

	QPointF hotSpotSceneCenter() const;

	virtual bool isConnected() const = 0;
	virtual QRectF hotSpotBoundingRect() const = 0;

protected:
	FlowNode* _node{};
	FlowSocketIndex _index{};
	FlowSocketDescriptor _descriptor{};

	QColor _color{ Qt::darkGray };
};

} // namespace qtMate::flow
