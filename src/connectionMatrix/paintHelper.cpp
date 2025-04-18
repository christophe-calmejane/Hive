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

#include "connectionMatrix/paintHelper.hpp"
#include <QtMate/material/color.hpp>

namespace color = qtMate::material::color;

namespace connectionMatrix
{
namespace paintHelper
{
static inline void drawSquare(QPainter* painter, QRect const& rect)
{
	painter->drawRect(rect.adjusted(2, 2, -3, -3));
}

static inline void drawDiamond(QPainter* painter, QRect const& rect)
{
	auto const r = rect.translated(-rect.center()).adjusted(3, 3, -4, -4);

	painter->save();
	painter->translate(rect.center());
	painter->rotate(45);
	painter->drawRect(r);
	painter->restore();
}

static inline void drawCircle(QPainter* painter, QRect const& rect)
{
	painter->drawEllipse(rect.adjusted(2, 2, -3, -3));
}

static inline void drawCenterDot(QPainter* painter, QRect const& rect, QColor const& col, qreal const penWidth = 1)
{
	painter->setBrush(QBrush{ col, Qt::SolidPattern });
	painter->setPen(QPen{ col, penWidth });
	auto const center = rect.center();
	auto const insideRect = QRect{ center.x() - 1, center.y() - 1, 2, 2 };
	painter->drawEllipse(insideRect);
}

static inline QColor getMediaLockedBrushColor() noexcept
{
	return color::value(color::Name::Gray, color::Shade::ShadeA700);
}

static inline QColor getLatencyErrorBrushColor() noexcept
{
	return color::value(color::Name::Red, color::Shade::Shade800);
}

static inline QColor getEntitySummaryBrushColor(Model::IntersectionData::State const state, Model::IntersectionData::Flags const& flags)
{
	static auto const White = color::value(color::Name::Gray, color::Shade::Shade100);
	static auto const Green = color::value(color::Name::Green, color::Shade::Shade500);
	static auto const PaleGreen = color::value(color::Name::Green, color::Shade::Shade200);
	static auto const Red = color::value(color::Name::Red, color::Shade::Shade800);
	static auto const Yellow = color::value(color::Name::Amber, color::Shade::Shade400);
	static auto const Blue = color::value(color::Name::Blue, color::Shade::Shade500);
	static auto const Grey = color::value(color::Name::Gray, color::Shade::Shade600);

	auto brushColor = QColor{ White };

	auto const connected = state != Model::IntersectionData::State::NotConnected;
	auto const interfaceDown = flags.test(Model::IntersectionData::Flag::InterfaceDown);
	auto const wrongDomain = flags.test(Model::IntersectionData::Flag::WrongDomain);
	auto const wrongFormatPossible = flags.test(Model::IntersectionData::Flag::WrongFormatPossible);
	auto const wrongFormatImpossible = flags.test(Model::IntersectionData::Flag::WrongFormatImpossible);

	if (interfaceDown)
	{
		if (wrongDomain)
		{
			brushColor = Red;
		}
		else if (wrongFormatPossible)
		{
			brushColor = Yellow;
		}
		else if (wrongFormatImpossible)
		{
			brushColor = connected ? Yellow : Grey;
		}
		else
		{
			brushColor = Blue;
		}
	}
	else if (wrongFormatImpossible && !connected)
	{
		brushColor = Grey;
	}
	else if (wrongDomain)
	{
		brushColor = Red;
	}
	else if (wrongFormatPossible || wrongFormatImpossible)
	{
		brushColor = Yellow;
	}
	else
	{
		if (state == Model::IntersectionData::State::PartiallyConnected)
		{
			brushColor = PaleGreen;
		}
		else
		{
			brushColor = connected ? Green : White;
		}
	}

	brushColor.setAlphaF(connected ? 1.0 : 0.25);

	return brushColor;
}

static inline QColor getConnectionBrushColor(Model::IntersectionData::State const state, Model::IntersectionData::Flags const& flags, bool const wrongFormatHasPriorityOverInterfaceDown)
{
	static auto const White = color::value(color::Name::Gray, color::Shade::Shade300);
	static auto const Green = color::value(color::Name::Green, color::Shade::Shade500);
	static auto const Red = color::value(color::Name::Red, color::Shade::Shade800);
	static auto const Yellow = color::value(color::Name::Amber, color::Shade::Shade400);
	static auto const Blue = color::value(color::Name::Blue, color::Shade::Shade500);
	static auto const Purple = color::value(color::Name::Purple, color::Shade::Shade400);
	static auto const Grey = color::value(color::Name::Gray, color::Shade::Shade600);
	//static auto const Cyan = color::value(color::Name::Cyan, color::Shade::Shade400);
	//static auto const Orange = color::value(color::Name::Orange, color::Shade::Shade600);

	auto brushColor = QColor{ White };

	auto const connected = state != Model::IntersectionData::State::NotConnected;
	auto const interfaceDown = flags.test(Model::IntersectionData::Flag::InterfaceDown);
	auto const wrongDomain = flags.test(Model::IntersectionData::Flag::WrongDomain);
	auto const wrongFormatPossible = flags.test(Model::IntersectionData::Flag::WrongFormatPossible);
	auto const wrongFormatImpossible = flags.test(Model::IntersectionData::Flag::WrongFormatImpossible);

	if (interfaceDown)
	{
		if (wrongFormatPossible && wrongFormatHasPriorityOverInterfaceDown)
		{
			brushColor = Yellow;
		}
		else if (wrongFormatImpossible && wrongFormatHasPriorityOverInterfaceDown)
		{
			brushColor = connected ? Yellow : Grey;
		}
		else
		{
			brushColor = Blue;
		}
	}
	else if (wrongFormatImpossible && !connected)
	{
		brushColor = Grey;
	}
	else if (wrongDomain)
	{
		brushColor = Red;
	}
	else if (wrongFormatPossible || wrongFormatImpossible)
	{
		brushColor = Yellow;
	}
	else
	{
		if (state == Model::IntersectionData::State::PartiallyConnected)
		{
			brushColor = Purple;
		}
		else
		{
			brushColor = connected ? Green : White;
		}
	}

	brushColor.setAlphaF(connected ? 1.0 : 0.25);

	return brushColor;
}

QPainterPath buildHeaderArrowPath(QRect const& rect, Qt::Orientation const orientation, bool const isTransposed, bool const alwaysShowArrowTip, bool const alwaysShowArrowEnd, int const arrowOffset, int const arrowSize, int const width)
{
	auto path = QPainterPath{};

	auto const topLeft = rect.topLeft();
	auto const topRight = rect.topRight();
	auto const bottomLeft = rect.bottomLeft();
	auto const bottomRight = rect.bottomRight();

	if (orientation == Qt::Horizontal)
	{
		if (isTransposed)
		{
			// Arrow towards the matrix
			auto const minXPos = topLeft.x();
			auto const maxXPos = bottomRight.x();
			auto const intermediateXPos = minXPos + (maxXPos - minXPos) / 2;
			auto const minYPos = (width == 0) ? topLeft.y() : bottomLeft.y() - (arrowOffset + arrowSize + width);
			auto const maxYPos = bottomLeft.y() - arrowOffset;
			auto const intermediateYPos = maxYPos - arrowSize;

			path.moveTo(minXPos, minYPos);
			path.lineTo(minXPos, intermediateYPos);
			path.lineTo(intermediateXPos, maxYPos);
			path.lineTo(maxXPos, intermediateYPos);
			path.lineTo(maxXPos, minYPos);
			if (alwaysShowArrowEnd || width != 0)
			{
				path.lineTo(intermediateXPos, minYPos + arrowSize);
			}
		}
		else
		{
			// Arrow away from the matrix
			auto const minXPos = topLeft.x();
			auto const maxXPos = bottomRight.x();
			auto const intermediateXPos = minXPos + (maxXPos - minXPos) / 2;
			auto const minYPos = (width == 0) ? topLeft.y() + (alwaysShowArrowTip ? arrowSize : 0) : bottomLeft.y() - (arrowOffset + width);
			auto const maxYPos = bottomLeft.y() - arrowOffset;
			auto const intermediateYPos = maxYPos - arrowSize;

			path.moveTo(minXPos, minYPos);
			path.lineTo(minXPos, maxYPos);
			path.lineTo(intermediateXPos, intermediateYPos);
			path.lineTo(maxXPos, maxYPos);
			path.lineTo(maxXPos, minYPos);
			path.lineTo(intermediateXPos, minYPos - arrowSize);
		}
	}
	else
	{
		if (isTransposed)
		{
			// Arrow away from the matrix
			auto const minXPos = (width == 0) ? topLeft.x() + (alwaysShowArrowTip ? arrowSize : 0) : topRight.x() - (arrowOffset + width);
			auto const maxXPos = topRight.x() - arrowOffset;
			auto const intermediateXPos = maxXPos - arrowSize;
			auto const minYPos = topLeft.y();
			auto const maxYPos = bottomLeft.y();
			auto const intermediateYPos = minYPos + (maxYPos - minYPos) / 2;

			path.moveTo(minXPos, minYPos);
			path.lineTo(maxXPos, minYPos);
			path.lineTo(intermediateXPos, intermediateYPos);
			path.lineTo(maxXPos, maxYPos);
			path.lineTo(minXPos, maxYPos);
			path.lineTo(minXPos - arrowSize, intermediateYPos);
		}
		else
		{
			// Arrow towards the matrix
			auto const minXPos = (width == 0) ? topLeft.x() : topRight.x() - (arrowOffset + arrowSize + width);
			auto const maxXPos = topRight.x() - arrowOffset;
			auto const intermediateXPos = maxXPos - arrowSize;
			auto const minYPos = topLeft.y();
			auto const maxYPos = bottomLeft.y();
			auto const intermediateYPos = minYPos + (maxYPos - minYPos) / 2;

			path.moveTo(minXPos, minYPos);
			path.lineTo(intermediateXPos, minYPos);
			path.lineTo(maxXPos, intermediateYPos);
			path.lineTo(intermediateXPos, maxYPos);
			path.lineTo(minXPos, maxYPos);
			if (alwaysShowArrowEnd || width != 0)
			{
				path.lineTo(minXPos + arrowSize, intermediateYPos);
			}
		}
	}

	return path;
}

void drawCapabilities(QPainter* painter, QRect const& rect, Model::IntersectionData::Type const type, Model::IntersectionData::State const state, Model::IntersectionData::Flags const& flags, bool const drawMediaLockedDot, bool const drawCRFAudioConnections, bool const drawEntitySummary)
{
	painter->setRenderHint(QPainter::Antialiasing);

	auto const connected = state != Model::IntersectionData::State::NotConnected;

	auto penColor = color::value(color::Name::Gray, connected ? color::Shade::Shade900 : color::Shade::Shade500);
	auto penWidth = qreal{ 1.5 };
	auto wrongFormatHasPriorityOverInterfaceDown = false;

	auto const drawInvalidIntersection = [painter, &rect]()
	{
		auto brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::isDarkColorScheme() ? qtMate::material::color::Shade::Shade800 : qtMate::material::color::Shade::Shade300);
		brush.setStyle(Qt::BrushStyle::BDiagPattern);
		painter->fillRect(rect, brush);
	};
	auto const drawEntitySummaryIntersection = [painter, &rect, state, flags, drawMediaLockedDot](auto const brush, auto const penColor, auto const penWidth)
	{
		painter->setBrush(brush);
		painter->setPen(QPen{ penColor, penWidth });
		drawSquare(painter, rect);

		if (flags.test(Model::IntersectionData::Flag::LatencyError))
		{
			drawCenterDot(painter, rect, getLatencyErrorBrushColor(), 3);
		}
		else if (drawMediaLockedDot)
		{
			if (flags.test(Model::IntersectionData::Flag::MediaLocked))
			{
				drawCenterDot(painter, rect, getMediaLockedBrushColor());
			}
		}
	};
	auto const drawSingleStreamIntersection = [painter, &rect, state, flags, drawMediaLockedDot](auto const brush, auto const penColor, auto const penWidth)
	{
		painter->setBrush(brush);
		painter->setPen(QPen{ penColor, penWidth });
		drawCircle(painter, rect);

		if (flags.test(Model::IntersectionData::Flag::LatencyError))
		{
			drawCenterDot(painter, rect, getLatencyErrorBrushColor(), 3);
		}
		else if (drawMediaLockedDot)
		{
			if (flags.test(Model::IntersectionData::Flag::MediaLocked))
			{
				drawCenterDot(painter, rect, getMediaLockedBrushColor());
			}
		}
	};
	auto const drawRedundantStreamIntersection = [painter, &rect, state, flags, drawMediaLockedDot](auto const brush, auto const penColor, auto const penWidth)
	{
		painter->setBrush(brush);
		painter->setPen(QPen{ penColor, penWidth });
		drawDiamond(painter, rect);

		if (flags.test(Model::IntersectionData::Flag::LatencyError))
		{
			drawCenterDot(painter, rect, getLatencyErrorBrushColor(), 3);
		}
		else if (drawMediaLockedDot)
		{
			if (flags.test(Model::IntersectionData::Flag::MediaLocked))
			{
				drawCenterDot(painter, rect, getMediaLockedBrushColor());
			}
		}
	};

	// Do not draw incompatible format types if not connected
	if (!drawCRFAudioConnections && flags.test(Model::IntersectionData::Flag::WrongFormatType) && state == Model::IntersectionData::State::NotConnected)
	{
		drawInvalidIntersection();
		return;
	}

	switch (type)
	{
		// Offline Streams
		case Model::IntersectionData::Type::OfflineOutputStream_RedundantStream:
		{
			// Only draw something if it's connected
			if (state == Model::IntersectionData::State::Connected)
			{
				auto const brush = QBrush{ color::value(color::Name::Orange, color::Shade::Shade600), Qt::SolidPattern };
				penColor = color::value(color::Name::Gray, color::Shade::Shade900);
				penWidth = qreal{ 1.0 };
				drawRedundantStreamIntersection(brush, penColor, penWidth);
			}
			else
			{
				drawInvalidIntersection();
			}
			break;
		}
		case Model::IntersectionData::Type::OfflineOutputStream_Redundant:
		{
			penWidth = qreal{ 2.0 };
			wrongFormatHasPriorityOverInterfaceDown = true; // For summary, we want the WrongFormat error flag to have priority over InterfaceDown (more meaningful information)
			[[fallthrough]];
		}
		case Model::IntersectionData::Type::OfflineOutputStream_SingleStream:
		{
			// Only draw something if it's connected
			if (state == Model::IntersectionData::State::Connected)
			{
				auto const brush = QBrush{ color::value(color::Name::Orange, color::Shade::Shade600), Qt::SolidPattern };
				penColor = color::value(color::Name::Gray, color::Shade::Shade900);
				drawSingleStreamIntersection(brush, penColor, penWidth);
			}
			else
			{
				drawInvalidIntersection();
			}
			break;
		}
		case Model::IntersectionData::Type::Entity_Entity:
		{
			if (drawEntitySummary)
			{
				auto const brush = QBrush{ getEntitySummaryBrushColor(state, flags) };
				penColor = color::value(color::Name::Gray, connected ? color::Shade::Shade900 : color::Shade::Shade600);
				drawEntitySummaryIntersection(brush, penColor, penWidth * 1.5f);
			}
			else
			{
				painter->setBrush(QColor{ color::value(color::Name::Gray, color::Shade::Shade100) });
				painter->setPen(QPen{ color::value(color::Name::Gray, color::Shade::Shade500), penWidth * 1.5f });
				drawSquare(painter, rect);
			}
			break;
		}
		case Model::IntersectionData::Type::Entity_Redundant:
		case Model::IntersectionData::Type::Entity_RedundantStream:
		case Model::IntersectionData::Type::Entity_SingleStream:
		case Model::IntersectionData::Type::Entity_SingleChannel:
		{
			if (drawEntitySummary)
			{
				auto const brush = QBrush{ getEntitySummaryBrushColor(state, flags) };
				drawEntitySummaryIntersection(brush, penColor, penWidth);
			}
			else
			{
				painter->setBrush(QColor{ color::value(color::Name::Gray, color::Shade::Shade100) });
				painter->setPen(QPen{ color::value(color::Name::Gray, color::Shade::Shade500), penWidth });
				drawSquare(painter, rect);
			}
			break;
		}
		case Model::IntersectionData::Type::Redundant_RedundantStream:
		case Model::IntersectionData::Type::RedundantStream_RedundantStream:
		{
			auto const brush = QBrush{ getConnectionBrushColor(state, flags, wrongFormatHasPriorityOverInterfaceDown) };
			penWidth = qreal{ 1.0 };
			drawRedundantStreamIntersection(brush, penColor, penWidth);
			break;
		}
		case Model::IntersectionData::Type::RedundantStream_RedundantStream_Forbidden:
		{
			// Nominal case, not connected (since it's forbidden by Milan)
			if (state == Model::IntersectionData::State::NotConnected)
			{
				drawInvalidIntersection();
			}
			else
			{
				// But if the connection is made using another controller, we might have a connection we want to kill
				auto const brush = QBrush{ getConnectionBrushColor(state, flags, wrongFormatHasPriorityOverInterfaceDown) };
				penWidth = qreal{ 1.0 };
				drawRedundantStreamIntersection(brush, penColor, penWidth);
			}
			break;
		}
		case Model::IntersectionData::Type::Redundant_Redundant:
		{
			penWidth = qreal{ 2.0 };
			wrongFormatHasPriorityOverInterfaceDown = true; // For summary, we want the WrongFormat error flag to have priority over InterfaceDown (more meaningful information)
			[[fallthrough]];
		}
		case Model::IntersectionData::Type::RedundantStream_SingleStream:
		case Model::IntersectionData::Type::Redundant_SingleStream:
		case Model::IntersectionData::Type::SingleStream_SingleStream:
		case Model::IntersectionData::Type::SingleChannel_SingleChannel:
		{
			auto const brush = QBrush{ getConnectionBrushColor(state, flags, wrongFormatHasPriorityOverInterfaceDown) };
			drawSingleStreamIntersection(brush, penColor, penWidth);
			break;
		}
		case Model::IntersectionData::Type::None:
		default:
			drawInvalidIntersection();
			break;
	}
}

} // namespace paintHelper
} // namespace connectionMatrix
