#include "QtMate/flow/flowstyle.hpp"
#include "QtMate/flow/flownode.hpp"

#include <QPainter>
#include <QPainterPath>

namespace qtMate::flow {

void drawRoundedRect(QPainter* painter, float x, float y, float width, float height, Qt::Edges const edges, float radius) {
	QPolygonF polygon;

	if (edges.testFlags(TopLeft)) {
		polygon << QPointF{x, y + radius}
						<< QPointF{x + radius, y + radius}
						<< QPointF{x + radius, y};
	} else {
		polygon << QPointF{x, y};
	}

	if (edges.testFlags(TopRight)) {
		polygon << QPointF{x + width - radius, y}
						<< QPointF{x + width - radius, y + radius }
						<< QPointF{x + width, y + radius };
	} else {
		polygon << QPointF{x + width, y};
	}

	if (edges.testFlags(BottomRight)) {
		polygon << QPointF{x + width, y + height - radius}
						<< QPointF{x + width - radius, y + height - radius}
						<< QPointF{x + width - radius, y + height};
	} else {
		polygon << QPointF{x + width, y + height};
	}

	if (edges.testFlags(BottomLeft)) {
		polygon << QPointF{x + radius, y + height}
						<< QPointF{x + radius, y + height - radius}
						<< QPointF{x, y + height - radius};
	} else {
		polygon << QPointF{x, y+height};
	}

	QPainterPath painterPath;
	painterPath.addPolygon(polygon);
	painter->drawPath(painterPath);

	auto const d = 2 * radius;
	auto const step = 16 * 90;

	if (edges.testFlags(TopLeft)) {
		painter->drawPie(x, y, d, d, step, step);
	}

	if (edges.testFlags(TopRight)) {
		painter->drawPie(x + width - d, y, d, d, 4 * step, step);
	}

	if (edges.testFlags(BottomRight)) {
		painter->drawPie(x + width - d, y + height - d, d, d, 3 * step, step);
	}

	if (edges.testFlags(BottomLeft)) {
		painter->drawPie(x, y + height - d, d, d, 2 * step, step);
	}
}

void drawRoundedRect(QPainter* painter, QRectF const& r, Qt::Edges const edges, float radius) {
	drawRoundedRect(painter, r.x(), r.y(), r.width(), r.height(), edges, radius);
}

void drawElidedText(QPainter* painter, QRectF const& r, int flags, Qt::TextElideMode mode, QString const& text) {
	auto const fontMetrics = QFontMetrics{painter->font()};
	auto const elidedText = fontMetrics.elidedText(text, mode, r.width(), flags);
	painter->drawText(r, flags, elidedText);
}

float outputRatio(FlowNode* node) {
	return node->inputs().empty() ? 1.f : (node->outputs().empty() ? 0.f : NODE_OUTPUT_RATIO);
}

float inputRatio(FlowNode* node) {
	return node->outputs().empty() ? 1.f : (node->inputs().empty() ? 0.f : NODE_INPUT_RATIO);
}

} // namespace qtMate::flow
