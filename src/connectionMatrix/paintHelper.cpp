/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "connectionMatrix/paintHelper.hpp"
#include <QPainter>

namespace connectionMatrix
{
namespace paintHelper
{

static inline void drawNothing(QPainter* painter, QRect const& rect)
{
	painter->fillRect(rect, QBrush{ 0xE1E1E1, Qt::BDiagPattern });
}

static inline void drawSquare(QPainter* painter, QRect const& rect)
{
	painter->drawRect(rect.adjusted(3, 3, -3, -3));
}

static inline void drawLozenge(QPainter* painter, QRect const& rect)
{
	painter->save();

	auto r = rect.adjusted(4, 4, -4, -4);
	painter->translate(r.center());
	r.moveCenter({});
	painter->rotate(45.0);
	painter->drawRect(r);

	painter->restore();
}

static inline void drawCircle(QPainter* painter, QRect const& rect)
{
	painter->drawEllipse(rect.adjusted(3, 3, -3, -3));
}

void drawEntityConnectionSummary(QPainter* painter, QRect const& rect, Model::IntersectionData::Capabilities const& capabilities)
{
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);
	drawSquare(painter, rect);
	painter->restore();
}

void drawStreamConnectionStatus(QPainter* painter, QRect const& rect, Model::IntersectionData::Capabilities const& capabilities)
{
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);
	drawCircle(painter, rect);
	painter->restore();
}

void drawIndividualRedundantStreamStatus(QPainter* painter, QRect const& rect, Model::IntersectionData::Capabilities const& capabilities)
{
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);
	drawLozenge(painter, rect);
	painter->restore();
}

} // namespace paintHelper
} // namespace connectionMatrix
