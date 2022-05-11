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
inline auto const NODE_WIDTH = 250.f;

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

} // namespace qtMate::flow
