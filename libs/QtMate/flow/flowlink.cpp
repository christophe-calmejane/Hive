#include "QtMate/flow/flowlink.hpp"
#include "QtMate/flow/flowstyle.hpp"

#include <QPainter>

namespace qtMate::flow
{
FlowLink::FlowLink(QGraphicsItem* parent)
	: QGraphicsPathItem{ parent }
{
	setZValue(-1.f);
	setPen(NODE_CONNECTION_PEN);
}

FlowLink::~FlowLink() = default;

void FlowLink::setStart(QPointF const& start)
{
	_start = start;
	updatePainterPath();
}

void FlowLink::setStop(QPointF const& stop)
{
	_stop = stop;
	updatePainterPath();
}

void FlowLink::updatePainterPath()
{
	auto dist = _start.x() - _stop.x();

	auto ratio = 0.5f;
	auto offset = 0.f;

	if (dist > 0)
	{
		offset = -dist;
	}

	dist = std::abs(dist);

	auto const c1 = QPointF{ _start.x() + dist * ratio, _start.y() + offset };
	auto const c2 = QPointF{ _stop.x() - dist * ratio, _stop.y() + offset };

	auto painterPath = QPainterPath{ _start };
	painterPath.cubicTo(c1, c2, _stop);
	setPath(painterPath);
}

} // namespace qtMate::flow
