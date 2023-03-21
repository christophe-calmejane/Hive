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

#include <QPainter>
#include <QtMate/flow/flowDefs.hpp>

namespace qtMate::flow
{
//
inline const auto NODE_BACKGROUND_COLOR = QColor{ 0x222222 };
inline const auto NODE_TEXT_COLOR = QColor{ 0xf4f4f4 };
inline const auto NODE_HEADER_COLOR = QColor{ 0x272927 };
inline const auto NODE_SELECTED_HEADER_COLOR = QColor{ 0x334f3a };
inline const auto NODE_SOCKET_AREA_COLOR = QColor{ 0x484848 };
inline const auto NODE_INPUT_DEFAULT_COLOR = QColor{ 0x1f80ff };
inline const auto NODE_OUTPUT_DEFAULT_COLOR = QColor{ 0x80ff01 };

//
constexpr auto NODE_BORDER_RADIUS = 8.f;
constexpr auto NODE_HEADER_HEIGHT = 32.f;
constexpr auto NODE_HEADER_SEPARATOR_HEIGHT = 2.f;
constexpr auto NODE_SOCKET_AREA_INSET_TOP = 2.f;
constexpr auto NODE_SOCKET_AREA_INSET_BOTTOM = 2.f;
constexpr auto NODE_LINE_HEIGHT = 32.f;
constexpr auto NODE_SEPARATOR_THICKNESS = 2.f;
constexpr auto NODE_WIDTH = 380.f;

//
constexpr auto NODE_PADDING = 4.f;

//
constexpr auto NODE_SOCKET_RADIUS = 8.f;
constexpr auto NODE_SOCKET_PADDING = 4.f;
constexpr auto NODE_SOCKET_BOUNDING_SIZE = (NODE_SOCKET_RADIUS + NODE_SOCKET_PADDING) * 2;

//
inline const auto NODE_CONNECTION_PEN = QPen{ QColor{ 0xb0bec5 }, 2.f, Qt::PenStyle::DotLine };

//
constexpr auto TopLeft = Qt::Edge::TopEdge | Qt::Edge::LeftEdge;
constexpr auto TopRight = Qt::Edge::TopEdge | Qt::Edge::RightEdge;
constexpr auto BottomRight = Qt::Edge::BottomEdge | Qt::Edge::RightEdge;
constexpr auto BottomLeft = Qt::Edge::BottomEdge | Qt::Edge::LeftEdge;

void drawRoundedRect(QPainter* painter, float x, float y, float width, float height, Qt::Edges const edges, float radius);
void drawRoundedRect(QPainter* painter, QRectF const& r, Qt::Edges const edges, float radius);

void drawElidedText(QPainter* painter, QRectF const& r, int flags, Qt::TextElideMode mode, QString const& text);

void drawOutputHotSpot(QPainter* painter, QPointF const& hotSpot, QColor const& color, bool connected);
void drawInputHotSpot(QPainter* painter, QPointF const& hotSpot, QColor const& color, bool connected);

} // namespace qtMate::flow
