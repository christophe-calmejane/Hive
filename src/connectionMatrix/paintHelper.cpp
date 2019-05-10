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
#include "toolkit/material/color.hpp"

namespace color = qt::toolkit::material::color;

namespace connectionMatrix
{
namespace paintHelper
{
static inline void drawSquare(QPainter* painter, QRect const& rect)
{
	painter->drawRect(rect.adjusted(2, 2, -2, -2));
}

static inline void drawDiamond(QPainter* painter, QRect const& rect)
{
	auto const r = rect.translated(-rect.center()).adjusted(3, 3, -3, -3);

	painter->save();
	painter->translate(rect.center());
	painter->rotate(45);
	painter->drawRect(r);
	painter->restore();
}

static inline void drawCircle(QPainter* painter, QRect const& rect)
{
	painter->drawEllipse(rect.adjusted(2, 2, -2, -2));
}

static inline QColor getConnectionBrushColor(Model::IntersectionData::State const state, Model::IntersectionData::Flags const& flags)
{
	static auto const White = color::value(color::Name::Gray, color::Shade::Shade300);
	static auto const Green = color::value(color::Name::Green, color::Shade::Shade500);
	static auto const Red = color::value(color::Name::Red, color::Shade::Shade800);
	static auto const Yellow = color::value(color::Name::Amber, color::Shade::Shade400);
	static auto const Blue = color::value(color::Name::Blue, color::Shade::Shade500);
	static auto const Purple = color::value(color::Name::Purple, color::Shade::Shade400);
	//static auto const Orange = color::value(color::Name::Orange, color::Shade::Shade600);

	auto brushColor = QColor{ White };

	auto const connected = state != Model::IntersectionData::State::NotConnected;
	auto const interfaceDown = flags.test(Model::IntersectionData::Flag::InterfaceDown);
	auto const wrongDomain = flags.test(Model::IntersectionData::Flag::WrongDomain);
	auto const wrongFormat = flags.test(Model::IntersectionData::Flag::WrongFormat);

	if (state == Model::IntersectionData::State::PartiallyConnected)
	{
		brushColor = Purple;
	}
	else
	{
		if (interfaceDown)
		{
			brushColor = Blue;
		}
		else if (wrongDomain)
		{
			brushColor = Red;
		}
		else if (wrongFormat)
		{
			brushColor = Yellow;
		}
		else
		{
			brushColor = connected ? Green : White;
		}
	}

	brushColor.setAlphaF(connected ? 1.0 : 0.25);

	return brushColor;
}

void drawCapabilities(QPainter* painter, QRect const& rect, Model::IntersectionData::Type const type, Model::IntersectionData::State const state, Model::IntersectionData::Flags const& flags)
{
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setRenderHint(QPainter::HighQualityAntialiasing);

	auto const connected = state != Model::IntersectionData::State::NotConnected;

	auto const penColor = color::value(color::Name::Gray, connected ? color::Shade::Shade900 : color::Shade::Shade500);
	auto penWidth = qreal{ 1.5 };

	switch (type)
	{
		case Model::IntersectionData::Type::Entity_Entity:
		case Model::IntersectionData::Type::Entity_Redundant:
		case Model::IntersectionData::Type::Entity_RedundantStream:
		case Model::IntersectionData::Type::Entity_SingleStream:
			painter->setBrush(QColor{ color::value(color::Name::Gray, color::Shade::Shade200) });
			painter->setPen(QPen{ penColor, penWidth });
			drawSquare(painter, rect);
			break;
		case Model::IntersectionData::Type::Redundant_RedundantStream:
		case Model::IntersectionData::Type::RedundantStream_RedundantStream:
			painter->setBrush(getConnectionBrushColor(state, flags));
			penWidth = qreal{ 1.0 };
			painter->setPen(QPen{ penColor, penWidth });
			drawDiamond(painter, rect);
			break;
		case Model::IntersectionData::Type::RedundantStream_RedundantStream_Forbidden:
			// Nominal case, not connected (since it's forbidden by Milan)
			if (state == Model::IntersectionData::State::NotConnected)
			{
				painter->fillRect(rect, QBrush{ 0xE1E1E1, Qt::BDiagPattern });
			}
			else
			{
				// But if the connection is made using another controller, we might have a connection we want to kill
				painter->setBrush(getConnectionBrushColor(state, flags));
				penWidth = qreal{ 1.0 };
				painter->setPen(QPen{ penColor, penWidth });
				drawDiamond(painter, rect);
			}
			break;
		case Model::IntersectionData::Type::Redundant_Redundant:
			penWidth = qreal{ 2.0 };
			[[fallthrough]];
		case Model::IntersectionData::Type::RedundantStream_SingleStream:
		case Model::IntersectionData::Type::Redundant_SingleStream:
		case Model::IntersectionData::Type::SingleStream_SingleStream:
			painter->setBrush(getConnectionBrushColor(state, flags));
			painter->setPen(QPen{ penColor, penWidth });
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
