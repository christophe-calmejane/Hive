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

namespace palette = qt::toolkit::materialPalette;

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
	auto const r = rect.translated(-rect.center()).adjusted(4, 4, -4, -4);
	
	painter->save();
	painter->translate(rect.center());
	painter->rotate(45);
	painter->drawRect(r);
	painter->restore();
}

static inline void drawCircle(QPainter* painter, QRect const& rect)
{
	painter->drawEllipse(rect.adjusted(3, 3, -3, -3));
}

void drawCapabilities(QPainter* painter, QRect const& rect, Model::IntersectionData::Type const type, Model::IntersectionData::State const state, Model::IntersectionData::Flags const& flags)
{
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setRenderHint(QPainter::HighQualityAntialiasing);
	
	auto const connected = state != Model::IntersectionData::State::NotConnected;

	auto const interfaceDown = flags.test(Model::IntersectionData::Flag::InterfaceDown);
	auto const wrongDomain = flags.test(Model::IntersectionData::Flag::WrongDomain);
	auto const wrongFormat = flags.test(Model::IntersectionData::Flag::WrongFormat);


	auto penColor = palette::color(palette::Name::Gray, connected ? palette::Shade::Shade900 : palette::Shade::Shade500);
	painter->setPen(QPen{ penColor, 1.5 });

	static auto const White = palette::color(palette::Name::Gray, palette::Shade::Shade100);
	static auto const Green = palette::color(palette::Name::Green, palette::Shade::Shade500);
	static auto const Red = palette::color(palette::Name::Red, palette::Shade::Shade800);
	static auto const Yellow = palette::color(palette::Name::Yellow, palette::Shade::Shade600);
	static auto const Blue = palette::color(palette::Name::Blue, palette::Shade::Shade300);
	static auto const Purple = palette::color(palette::Name::Purple, palette::Shade::Shade400);
	static auto const Orange = palette::color(palette::Name::Orange, palette::Shade::Shade600);

	auto brushColor = QColor{ White };

	if (connected)
	{
		brushColor = Green;
	}

	if (wrongFormat)
	{
		brushColor = Yellow;
	}

	if (wrongDomain)
	{
		brushColor = Red;
	}

	if (interfaceDown)
	{
		brushColor = Blue;
	}

	// Special case
	if (type == Model::IntersectionData::Type::Redundant_Redundant)
	{
		if (state == Model::IntersectionData::State::PartiallyConnected)
		{
			brushColor = Orange;
		}
		else if (interfaceDown)
		{
			brushColor = Blue;
		}
		else if (wrongDomain || wrongFormat)
		{
			brushColor = Purple;
		}
	}

	brushColor.setAlphaF(connected ? 1.0 : 0.25);
	painter->setBrush(brushColor);

	switch (type)
	{
		case Model::IntersectionData::Type::Entity_Entity:
		case Model::IntersectionData::Type::Entity_Redundant:
		case Model::IntersectionData::Type::Entity_RedundantStream:
		case Model::IntersectionData::Type::Entity_SingleStream:
			drawSquare(painter, rect);
			break;
		case Model::IntersectionData::Type::Redundant_RedundantStream:
		case Model::IntersectionData::Type::RedundantStream_RedundantStream:
			drawDiamond(painter, rect);
			break;
		case Model::IntersectionData::Type::Redundant_Redundant:
		case Model::IntersectionData::Type::RedundantStream_SingleStream:
		case Model::IntersectionData::Type::Redundant_SingleStream:
		case Model::IntersectionData::Type::SingleStream_SingleStream:
			drawCircle(painter, rect);
			break;
		case Model::IntersectionData::Type::None:
		default:
			painter->fillRect(rect, QBrush{ 0xE1E1E1, Qt::BDiagPattern });
			break;
	}
}

} // namespace paintHelper
} // namespace connectionMatrix
