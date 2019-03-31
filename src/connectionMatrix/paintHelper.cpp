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
#include "toolkit/materialPalette.hpp"
#include <QPainter>

namespace connectionMatrix
{
namespace paintHelper
{

static inline void drawSquare(QPainter* painter, QRect const& rect)
{
	painter->drawRect(rect.adjusted(3, 3, -3, -3));
}

static inline void drawDiamond(QPainter* painter, QRect const& rect)
{
	painter->save();
	painter->translate(-rect.center());
	painter->rotate(45);
	drawSquare(painter, rect);
	painter->restore();
}

static inline void drawCircle(QPainter* painter, QRect const& rect)
{
	painter->drawEllipse(rect.adjusted(3, 3, -3, -3));
}

static inline void configurePenAndBrush(QPainter* painter, Model::IntersectionData::Capabilities const& capabilities)
{
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setRenderHint(QPainter::HighQualityAntialiasing);

	auto const interfaceDown = capabilities.test(Model::IntersectionData::Capability::InterfaceDown);
	auto const wrongDomain = capabilities.test(Model::IntersectionData::Capability::WrongDomain);
	auto const wrongFormat = capabilities.test(Model::IntersectionData::Capability::WrongFormat);
	auto const connected = capabilities.test(Model::IntersectionData::Capability::Connected);
	auto const fastConnecting = capabilities.test(Model::IntersectionData::Capability::FastConnecting);
	auto const partiallyConnected = capabilities.test(Model::IntersectionData::Capability::PartiallyConnected);

	painter->setPen(QPen{ connected ? Qt::black : Qt::gray, 2 });

	static auto const White = qt::toolkit::materialPalette::color(qt::toolkit::materialPalette::Name::Gray, qt::toolkit::materialPalette::Shade::Shade100);
	static auto const Green = qt::toolkit::materialPalette::color(qt::toolkit::materialPalette::Name::Green, qt::toolkit::materialPalette::Shade::Shade500);
	static auto const Red = qt::toolkit::materialPalette::color(qt::toolkit::materialPalette::Name::Red, qt::toolkit::materialPalette::Shade::Shade500);
	static auto const Yellow = qt::toolkit::materialPalette::color(qt::toolkit::materialPalette::Name::Yellow, qt::toolkit::materialPalette::Shade::Shade500);
	static auto const Blue = qt::toolkit::materialPalette::color(qt::toolkit::materialPalette::Name::Blue, qt::toolkit::materialPalette::Shade::Shade300);

	QColor brushColor{ White };

	if (connected)
	{
		if (wrongDomain)
		{
			brushColor = Red;
		}
		else if (wrongFormat)
		{
			brushColor = Yellow;
		}
		else if (interfaceDown)
		{
			brushColor = Blue;
		}
		else
		{
			brushColor = Green;
		}
	}
	else if (fastConnecting)
	{
	}
	else if (partiallyConnected)
	{
	}
	else // Not connected
	{
	}

	brushColor.setAlphaF(connected ? 1.0 : 0.5);
	painter->setBrush(brushColor);
}

void drawEntityConnectionSummary(QPainter* painter, QRect const& rect, Model::IntersectionData::Capabilities const& capabilities)
{
	painter->save();
	configurePenAndBrush(painter, capabilities);
	drawSquare(painter, rect);
	painter->restore();
}

void drawStreamConnectionStatus(QPainter* painter, QRect const& rect, Model::IntersectionData::Capabilities const& capabilities)
{
	painter->save();
	configurePenAndBrush(painter, capabilities);
	drawCircle(painter, rect);
	painter->restore();
}

void drawIndividualRedundantStreamStatus(QPainter* painter, QRect const& rect, Model::IntersectionData::Capabilities const& capabilities)
{
	painter->save();
	configurePenAndBrush(painter, capabilities);
	drawDiamond(painter, rect);
	painter->restore();
}

} // namespace paintHelper
} // namespace connectionMatrix
