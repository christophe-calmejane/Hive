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

#include "connectionMatrix/paintHelper.hpp"

namespace connectionMatrix
{

static inline void drawCircle(QPainter* painter, QRect const& rect)
{
	painter->drawEllipse(rect.adjusted(3, 3, -3, -3));
}

static inline void drawLozenge(QPainter* painter, QRect const& rect)
{
	auto r = rect.adjusted(4, 4, -4, -4);
	painter->translate(r.center());
	r.moveCenter({});
	painter->rotate(45.0);
	painter->drawRect(r);
}

static inline void drawSquare(QPainter* painter, QRect const& rect)
{
	painter->drawRect(rect.adjusted(3, 3, -3, -3));
}

static inline void drawEntitySummaryFigure(QPainter* painter, QRect const& rect, QColor const& color)
{
	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(QColor("#9E9E9E"), 1));
	painter->setBrush(color);
	drawSquare(painter, rect);

	painter->restore();
}

static inline void drawConnectedStreamFigure(QPainter* painter, QRect const& rect, QColor const& color)
{
	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(Qt::black, 2));
	painter->setBrush(color);
	drawCircle(painter, rect);

	painter->restore();
}

static inline void drawNotConnectedStreamFigure(QPainter* painter, QRect const& rect, QColor const& color)
{
	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(QColor("#9E9E9E"), 2));
	painter->setBrush(color);
	drawCircle(painter, rect);

	painter->restore();
}

static inline void drawFastConnectingStreamFigure(QPainter* painter, QRect const& rect, QColor const& colorConnected, QColor const& colorNotConnected)
{
	constexpr auto startAngle = 90;

	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(QColor("#9E9E9E"), 2));
	painter->setBrush(colorConnected);
	painter->drawPie(rect.adjusted(3, 3, -3, -3), startAngle * 16, 180 * 16);
	painter->setBrush(colorNotConnected);
	painter->drawPie(rect.adjusted(3, 3, -3, -3), (startAngle + 180) * 16, 180 * 16);

	painter->restore();
}

static inline void drawConnectedRedundantStreamFigure(QPainter* painter, QRect const& rect, QColor const& color)
{
	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(Qt::black, 1));
	painter->setBrush(color);
	drawLozenge(painter, rect);

	painter->restore();
}

static inline void drawNotConnectedRedundantStreamFigure(QPainter* painter, QRect const& rect, QColor const& color)
{
	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(QColor("#9E9E9E"), 1));
	painter->setBrush(color);
	drawLozenge(painter, rect);

	painter->restore();
}

static inline void drawFastConnectingRedundantStreamFigure(QPainter* painter, QRect const& rect, QColor const& colorConnected, QColor const& colorNotConnected)
{
	drawNotConnectedRedundantStreamFigure(painter, rect, colorNotConnected); // TODO: Try to draw a lozenge split in 2, like the circle
}

static inline QColor getConnectedColor()
{
	return QColor(0x4CAF50);
}
static inline QColor getConnectedWrongDomainColor()
{
	return QColor(0xB71C1C);
}

static inline QColor getConnectedWrongFormatColor()
{
	return QColor(0xFFD600);
}

static inline QColor getPartiallyConnectedColor()
{
	return QColor(0x2196F3);
}

static inline QColor getNotConnectedColor()
{
	return QColor(0xF5F5F5);
}

static inline QColor getNotConnectedWrongDomainColor()
{
	return QColor(0xFFCDD2);
}

static inline QColor getNotConnectedWrongFormatColor()
{
	return QColor(0xFFF9C4);
}

void drawConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
	{
		drawConnectedRedundantStreamFigure(painter, rect, getConnectedColor());
	}
	else
	{
		drawConnectedStreamFigure(painter, rect, getConnectedColor());
	}
}

void drawWrongDomainConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
	{
		drawConnectedRedundantStreamFigure(painter, rect, getConnectedWrongDomainColor());
	}
	else
	{
		drawConnectedStreamFigure(painter, rect, getConnectedWrongDomainColor());
	}
}

void drawWrongFormatConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
	{
		drawConnectedRedundantStreamFigure(painter, rect, getConnectedWrongFormatColor());
	}
	else
	{
		drawConnectedStreamFigure(painter, rect, getConnectedWrongFormatColor());
	}
}

void drawFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
	{
		drawFastConnectingRedundantStreamFigure(painter, rect, getConnectedColor(), getNotConnectedColor());
	}
	else
	{
		drawFastConnectingStreamFigure(painter, rect, getConnectedColor(), getNotConnectedColor());
	}
}

void drawWrongDomainFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
	{
		drawFastConnectingRedundantStreamFigure(painter, rect, getConnectedWrongDomainColor(), getNotConnectedWrongDomainColor());
	}
	else
	{
		drawFastConnectingStreamFigure(painter, rect, getConnectedWrongDomainColor(), getNotConnectedWrongDomainColor());
	}
}

void drawWrongFormatFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
	{
		drawFastConnectingRedundantStreamFigure(painter, rect, getConnectedWrongFormatColor(), getNotConnectedWrongFormatColor());
	}
	else
	{
		drawFastConnectingStreamFigure(painter, rect, getConnectedWrongFormatColor(), getNotConnectedWrongFormatColor());
	}
}

void drawNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
	{
		drawNotConnectedRedundantStreamFigure(painter, rect, getNotConnectedColor());
	}
	else
	{
		drawNotConnectedStreamFigure(painter, rect, getNotConnectedColor());
	}
}

void drawWrongDomainNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
	{
		drawNotConnectedRedundantStreamFigure(painter, rect, getNotConnectedWrongDomainColor());
	}
	else
	{
		drawNotConnectedStreamFigure(painter, rect, getNotConnectedWrongDomainColor());
	}
}

void drawWrongFormatNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
	{
		drawNotConnectedRedundantStreamFigure(painter, rect, getNotConnectedWrongFormatColor());
	}
	else
	{
		drawNotConnectedStreamFigure(painter, rect, getNotConnectedWrongFormatColor());
	}
}

void drawPartiallyConnectedRedundantNode(QPainter* painter, QRect const& rect, bool const)
{
	drawConnectedStreamFigure(painter, rect, getPartiallyConnectedColor());
}

void drawEntityNoConnection(QPainter* painter, QRect const& rect)
{
	drawEntitySummaryFigure(painter, rect, QColor("#EEEEEE"));
}

void drawNotApplicable(QPainter* painter, QRect const& rect)
{
	painter->fillRect(rect, QBrush{QColor("#E1E1E1"), Qt::BDiagPattern});
}

} // namespace connectionMatrix
