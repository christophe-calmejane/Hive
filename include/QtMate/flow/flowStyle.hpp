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
inline auto const NODE_BACKGROUND_COLOR = QColor{ 0x222222 };
inline auto const NODE_TEXT_COLOR = QColor{ 0xf4f4f4 };
inline auto const NODE_HEADER_COLOR = QColor{ 0x272927 };
inline auto const NODE_SELECTED_HEADER_COLOR = QColor{ 0x334f3a };
inline auto const NODE_INPUT_BACKGROUND_COLOR = QColor{ 0x363636 };
inline auto const NODE_OUTPUT_BACKGROUND_COLOR = QColor{ 0x282828 };
inline auto const NODE_INPUT_DEFAULT_COLOR = QColor{ 0x1f80ff };
inline auto const NODE_OUTPUT_DEFAULT_COLOR = QColor{ 0x80ff01 };

//
inline auto const NODE_BORDER_RADIUS = 8.f;
inline auto const NODE_LINE_HEIGHT = 32.f;
inline auto const NODE_SEPARATOR_THICKNESS = 2.f;
inline auto const NODE_INPUT_RATIO = 4.f / 7.f;
inline auto const NODE_OUTPUT_RATIO = 3.f / 7.f;
inline auto const NODE_WIDTH = 380.f;

//
inline auto const NODE_PADDING = 4.f;

//
inline auto const NODE_SOCKET_RADIUS = 8.f;
inline auto const NODE_SOCKET_PADDING = 4.f;
inline auto const NODE_SOCKET_BOUNDING_SIZE = (NODE_SOCKET_RADIUS + NODE_SOCKET_PADDING) * 2;

//
inline auto const NODE_CONNECTION_PEN = QPen{ QColor{ 0xb0bec5 }, 2.f, Qt::PenStyle::DotLine };

//
inline auto const TopLeft = Qt::Edge::TopEdge | Qt::Edge::LeftEdge;
inline auto const TopRight = Qt::Edge::TopEdge | Qt::Edge::RightEdge;
inline auto const BottomRight = Qt::Edge::BottomEdge | Qt::Edge::RightEdge;
inline auto const BottomLeft = Qt::Edge::BottomEdge | Qt::Edge::LeftEdge;

void drawRoundedRect(QPainter* painter, float x, float y, float width, float height, Qt::Edges const edges, float radius);
void drawRoundedRect(QPainter* painter, QRectF const& r, Qt::Edges const edges, float radius);

void drawElidedText(QPainter* painter, QRectF const& r, int flags, Qt::TextElideMode mode, QString const& text);

float outputRatio(FlowNode* node);
float inputRatio(FlowNode* node);

void drawOutputHotSpot(QPainter* painter, QPointF const& hotSpot, QColor const& color, bool connected);
void drawInputHotSpot(QPainter* painter, QPointF const& hotSpot, QColor const& color, bool connected);

} // namespace qtMate::flow
