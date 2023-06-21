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

#include "QtMate/flow/flowNode.hpp"
#include "QtMate/flow/flowInput.hpp"
#include "QtMate/flow/flowOutput.hpp"
#include "QtMate/flow/flowConnection.hpp"
#include "QtMate/flow/flowSceneDelegate.hpp"
#include "QtMate/flow/flowStyle.hpp"

#include <QGraphicsSceneMouseEvent>

namespace qtMate::flow
{
class FlowNodeHeader : public QGraphicsItem
{
public:
	FlowNodeHeader(QString const& name, QGraphicsItem* parent = nullptr)
		: QGraphicsItem{ parent }
		, _name{ name }
	{
	}

	virtual QRectF boundingRect() const override
	{
		return QRectF{ 0.f, 0.f, NODE_WIDTH, NODE_HEADER_HEIGHT };
	}

	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget) override
	{
		auto const r = boundingRect();

		painter->setPen(NODE_TEXT_COLOR);
		painter->setBrush(Qt::NoBrush);
		drawElidedText(painter, r, Qt::AlignCenter, Qt::ElideMiddle, _name);
	}

private:
	QString _name{};
};


FlowNode::FlowNode(FlowSceneDelegate* delegate, FlowNodeUid const& uid, FlowNodeDescriptor const& descriptor, QGraphicsItem* parent)
	: QGraphicsItem{ parent }
	, _delegate{ delegate }
	, _uid{ uid }
	, _name{ descriptor.name }
	, _header{ new FlowNodeHeader{ _name, this } }
{
	Q_ASSERT(_delegate && "FlowSceneDelegate is required");

	setFlag(QGraphicsItem::ItemIsMovable, true);
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
	setFlag(QGraphicsItem::ItemDoesntPropagateOpacityToChildren, true);
	setOpacity(0.65f);
	setAcceptHoverEvents(true);
	setZValue(0.f);


	for (auto const& descriptor : descriptor.inputs)
	{
		auto const index = static_cast<FlowSocketIndex>(_inputs.size());
		auto* input = _inputs.emplace_back(new FlowInput{ this, index, descriptor });
		input->setColor(_delegate->socketTypeColor(input->descriptor().type));
	}

	for (auto const& descriptor : descriptor.outputs)
	{
		auto const index = static_cast<FlowSocketIndex>(_outputs.size());
		auto* output = _outputs.emplace_back(new FlowOutput{ this, index, descriptor });
		output->setColor(_delegate->socketTypeColor(output->descriptor().type));
	}

	// collapse animation configuration
	_collapseAnimation.setDuration(350);
	_collapseAnimation.setStartValue(1.f);
	_collapseAnimation.setEndValue(0.f);
	_collapseAnimation.setEasingCurve(QEasingCurve::Type::OutQuart);

	QObject::connect(&_collapseAnimation, &QVariantAnimation::valueChanged,
		[this](QVariant const& value)
		{
			_collapseRatio = value.toFloat();
			updateSockets();
			prepareGeometryChange();
		});

	updateSockets();
}

FlowNode::~FlowNode() = default;

FlowNodeUid const& FlowNode::uid() const
{
	return _uid;
}

QString const& FlowNode::name() const
{
	return _name;
}

FlowInputs const& FlowNode::inputs() const
{
	return _inputs;
}

FlowOutputs const& FlowNode::outputs() const
{
	return _outputs;
}

bool FlowNode::isCollapsed() const {
	return _collapsed;
}


FlowInput* FlowNode::input(FlowSocketIndex index) const
{
	if (index < 0 || index >= _inputs.size())
	{
		return nullptr;
	}
	return _inputs[index];
}

FlowOutput* FlowNode::output(FlowSocketIndex index) const
{
	if (index < 0 || index >= _outputs.size())
	{
		return nullptr;
	}
	return _outputs[index];
}

bool FlowNode::hasConnectedInput() const
{
	for (auto* input : _inputs)
	{
		if (input->isConnected())
		{
			return true;
		}
	}

	return false;
}

bool FlowNode::hasConnectedOutput() const
{
	for (auto* output : _outputs)
	{
		if (output->isConnected())
		{
			return true;
		}
	}

	return false;
}

int FlowNode::type() const
{
	return Type;
}

QRectF FlowNode::boundingRect() const
{
	auto const n = std::max(_inputs.size(), _outputs.size());
	return QRectF{ 0.f, 0.f, NODE_WIDTH, NODE_HEADER_HEIGHT + _collapseRatio * (NODE_HEADER_SEPARATOR_HEIGHT + NODE_SOCKET_AREA_INSET_TOP + n * NODE_LINE_HEIGHT + NODE_SOCKET_AREA_INSET_BOTTOM) };
}

void FlowNode::paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget)
{
	auto const r = boundingRect();

	// header
	auto headerBoundingRect = r;
	headerBoundingRect.setHeight(NODE_HEADER_HEIGHT);

	painter->setPen(Qt::NoPen);

	if (isSelected())
	{
		painter->setBrush(NODE_SELECTED_HEADER_COLOR);
	}
	else
	{
		painter->setBrush(NODE_HEADER_COLOR);
	}

	drawRoundedRect(painter, headerBoundingRect, TopLeft | TopRight, NODE_BORDER_RADIUS);

	auto headerHotSpotColor = _delegate->socketTypeColor(0); // FIXME
	headerHotSpotColor.setAlphaF(1.f - _collapseRatio);

	// inputs
	if (!_inputs.empty())
	{
		auto const inputHotSpotCenter = QRectF{ 0.f, 0.f, NODE_SOCKET_BOUNDING_SIZE, headerBoundingRect.height() }.center();
		drawOutputHotSpot(painter, inputHotSpotCenter, headerHotSpotColor, hasConnectedInput());
	}

	// outputs
	if (!_outputs.empty())
	{
		auto const outputHotSpotCenter = QRectF{ headerBoundingRect.right() - NODE_SOCKET_BOUNDING_SIZE, 0.f, NODE_SOCKET_BOUNDING_SIZE, headerBoundingRect.height() }.center();
		drawOutputHotSpot(painter, outputHotSpotCenter, headerHotSpotColor, hasConnectedOutput());
	}

	// Draw socket area
	auto color = NODE_SOCKET_AREA_COLOR;
	color.setAlphaF(_collapseRatio);
	painter->setPen(Qt::NoPen);
	painter->setBrush(color);
	auto socketAreaBoundingRect = r;
	auto const socketAreaTopPosition = NODE_HEADER_HEIGHT + NODE_HEADER_SEPARATOR_HEIGHT;
	socketAreaBoundingRect.moveTop(socketAreaTopPosition);
	socketAreaBoundingRect.setHeight(r.height() - socketAreaTopPosition);
	drawRoundedRect(painter, socketAreaBoundingRect, BottomLeft | BottomRight, NODE_BORDER_RADIUS);
}

QVariant FlowNode::itemChange(GraphicsItemChange change, QVariant const& value)
{
	if (change == GraphicsItemChange::ItemPositionHasChanged)
	{
		handleItemPositionHasChanged();
	}
	else if (change == GraphicsItemChange::ItemSelectedHasChanged)
	{
		handleItemSelectionHasChanged();
	}

	return QGraphicsItem::itemChange(change, value);
}

void FlowNode::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
	if (_header->contains(event->pos()))
	{
		toggleCollapsed();
	}

	QGraphicsItem::mouseDoubleClickEvent(event);
}

void FlowNode::toggleCollapsed()
{
	setCollapsed(!_collapsed);
}

void FlowNode::setCollapsed(bool collapsed)
{
	if (collapsed == _collapsed)
	{
		return;
	}

	_collapsed = collapsed;

	_collapseAnimation.stop();
	_collapseAnimation.setStartValue(_collapseRatio);

	if (_collapsed)
	{
		_collapseAnimation.setEndValue(0.f);
	}
	else
	{
		_collapseAnimation.setEndValue(1.f);
	}

	_collapseAnimation.start(QAbstractAnimation::DeletionPolicy::KeepWhenStopped);

	emit collapsedChanged();
}

void FlowNode::handleItemPositionHasChanged()
{
	for (auto* input : _inputs)
	{
		input->updateConnection();
	}

	for (auto* output : _outputs)
	{
		output->updateConnections();
	}
}

void FlowNode::handleItemSelectionHasChanged()
{
	setZValue(isSelected() ? 1.f : 0.f);
}

void FlowNode::updateSockets()
{
	auto const firstSocketTopPosition = NODE_HEADER_HEIGHT + NODE_HEADER_SEPARATOR_HEIGHT + NODE_SOCKET_AREA_INSET_TOP;
	for (auto* input : _inputs)
	{
		auto const index = input->index();
		input->setPos(0, _collapseRatio * (firstSocketTopPosition + index * NODE_LINE_HEIGHT));
		input->setOpacity(_collapseRatio);
	}

	for (auto* output : _outputs)
	{
		auto const index = output->index();
		output->setPos(0, _collapseRatio * (firstSocketTopPosition + index * NODE_LINE_HEIGHT));
		output->setOpacity(_collapseRatio);
	}

	handleItemPositionHasChanged();
}

} // namespace qtMate::flow
