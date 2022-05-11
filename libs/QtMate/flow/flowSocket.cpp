#include "QtMate/flow/flowSocket.hpp"
#include "QtMate/flow/flowNode.hpp"

namespace qtMate::flow
{
FlowSocket::FlowSocket(FlowNode* node, FlowSocketIndex index, FlowSocketDescriptor const& descriptor)
	: QGraphicsItem{ node }
	, _node{ node }
	, _index{ index }
	, _descriptor{ descriptor }
{
}

FlowSocket::~FlowSocket() = default;

FlowNode* FlowSocket::node() const
{
	return _node;
}

FlowSocketIndex FlowSocket::index() const
{
	return _index;
}

FlowSocketDescriptor const& FlowSocket::descriptor() const
{
	return _descriptor;
}

FlowSocketSlot FlowSocket::slot() const
{
	return FlowSocketSlot{ _node->uid(), _index };
}

QColor const& FlowSocket::color() const
{
	return _color;
}

void FlowSocket::setColor(QColor const& color)
{
	if (color != _color)
	{
		_color = color;
		update();
	}
}

bool FlowSocket::hit(QPointF const& scenePos) const
{
	auto const localPos = mapFromScene(scenePos);
	return hotSpotBoundingRect().contains(localPos);
}

QPointF FlowSocket::hotSpotSceneCenter() const
{
	return mapToScene(hotSpotBoundingRect().center());
}

} // namespace qtMate::flow
