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

#pragma once

#include <QGraphicsPathItem>

namespace qtMate::flow
{
class FlowLink : public QGraphicsPathItem
{
public:
	FlowLink(QGraphicsItem* parent = nullptr);
	virtual ~FlowLink() override;

	void setStart(QPointF const& start);
	void setStop(QPointF const& stop);

private:
	void updatePainterPath();

private:
	QPointF _start{};
	QPointF _stop{};
};

} // namespace qtMate::flow
