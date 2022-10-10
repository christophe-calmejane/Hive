#include "QtMate/flow/flowInput.hpp"
#include "QtMate/flow/flowConnection.hpp"
#include "QtMate/flow/flowStyle.hpp"

#include <QPainter>

namespace qtMate::flow
{
FlowInput::~FlowInput()
{
	if (_connection)
	{
		_connection->setInput(nullptr);
		_connection = nullptr;
	}
}

int FlowInput::type() const
{
	return Type;
}

QRectF FlowInput::boundingRect() const
{
	auto const availableWidth = parentItem()->boundingRect().width();
	return QRectF{ 0.f, 0.f, availableWidth * inputRatio(_node), NODE_LINE_HEIGHT };
}

bool FlowInput::isConnected() const
{
	return _connection != nullptr;
}

QRectF FlowInput::hotSpotBoundingRect() const
{
	auto const r = boundingRect();
	return QRectF{ 0.f, 0.f, NODE_SOCKET_BOUNDING_SIZE, r.height() };
}

FlowConnection* FlowInput::connection() const
{
	return _connection;
}

void FlowInput::setConnection(FlowConnection* connection)
{
	if (connection != _connection)
	{
		_connection = connection;
		update();
	}
}

void FlowInput::updateConnection()
{
	if (_connection)
	{
		_connection->updatePath();
	}
}

void FlowInput::paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget)
{
	auto const hotSpot = hotSpotBoundingRect().center();
	drawInputHotSpot(painter, hotSpot, _color, isConnected());

	auto const nameBoundingRect = boundingRect().adjusted(NODE_SOCKET_BOUNDING_SIZE, 0.f, 0.f, 0.f);

	painter->setPen(NODE_TEXT_COLOR);
	painter->setBrush(Qt::NoBrush);
	drawElidedText(painter, nameBoundingRect, Qt::AlignVCenter | Qt::AlignLeft, Qt::TextElideMode::ElideMiddle, _descriptor.name);
}

} // namespace qtMate::flow
