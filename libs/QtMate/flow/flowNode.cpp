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
		return QRectF{ 0.f, 0.f, NODE_WIDTH, NODE_LINE_HEIGHT };
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
	
	QObject::connect(&_collapseAnimation, &QVariantAnimation::valueChanged, [this](QVariant const& value)
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
	for(auto* input : _inputs)
	{
		if(input->isConnected())
		{
			return true;
		}
	}
	
	return false;
}

bool FlowNode::hasConnectedOutput() const
{
	for(auto* output : _outputs)
	{
		if(output->isConnected())
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
	auto const n = /* header */ 1 + _collapseRatio * std::max(_inputs.size(), _outputs.size());
	return QRectF{ 0.f, 0.f, NODE_WIDTH, n * NODE_LINE_HEIGHT };
}

void FlowNode::paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget)
{
	auto const r = boundingRect();

	// header
	auto headerBoundingRect = r;
	headerBoundingRect.setHeight(NODE_LINE_HEIGHT);

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
		
		auto inputsBoundingRect = r;
		inputsBoundingRect.moveTop(NODE_LINE_HEIGHT + NODE_SEPARATOR_THICKNESS);
		inputsBoundingRect.setWidth(r.width() * inputRatio(this));
		inputsBoundingRect.setHeight(r.height() - NODE_LINE_HEIGHT - NODE_SEPARATOR_THICKNESS);
		painter->setPen(Qt::NoPen);
		
		auto color = QColor{NODE_INPUT_BACKGROUND_COLOR};
		color.setAlphaF(_collapseRatio);
		painter->setBrush(color);
		
		drawRoundedRect(painter, inputsBoundingRect, _outputs.empty() ? (BottomLeft | BottomRight) : BottomLeft, NODE_BORDER_RADIUS);
	}

	// outputs
	if (!_outputs.empty())
	{
		auto const outputHotSpotCenter = QRectF{ headerBoundingRect.right() - NODE_SOCKET_BOUNDING_SIZE, 0.f, NODE_SOCKET_BOUNDING_SIZE, headerBoundingRect.height() }.center();
		drawOutputHotSpot(painter, outputHotSpotCenter, headerHotSpotColor, hasConnectedOutput());
		
		auto ouputsBoundingRect = r;
		ouputsBoundingRect.moveTop(NODE_LINE_HEIGHT + NODE_SEPARATOR_THICKNESS);
		ouputsBoundingRect.moveLeft(r.width() * inputRatio(this) + NODE_SEPARATOR_THICKNESS);
		ouputsBoundingRect.setWidth(r.width() * outputRatio(this) - NODE_SEPARATOR_THICKNESS);
		ouputsBoundingRect.setHeight(r.height() - NODE_LINE_HEIGHT - NODE_SEPARATOR_THICKNESS);
		painter->setPen(Qt::NoPen);
		
		auto color = QColor{NODE_OUTPUT_BACKGROUND_COLOR};
		color.setAlphaF(_collapseRatio);
		painter->setBrush(color);

		drawRoundedRect(painter, ouputsBoundingRect, _inputs.empty() ? (BottomLeft | BottomRight) : BottomRight, NODE_BORDER_RADIUS);
	}
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

void FlowNode::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
	if(_header->contains(event->pos())) {
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
	_collapsed = collapsed;
	
	_collapseAnimation.stop();
	_collapseAnimation.setStartValue(_collapseRatio);
	
	if(_collapsed)
	{
		_collapseAnimation.setEndValue(0.f);
	}
	else
	{
		_collapseAnimation.setEndValue(1.f);
	}
	
	_collapseAnimation.start(QAbstractAnimation::DeletionPolicy::KeepWhenStopped);
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
	for (auto* input : _inputs)
	{
		auto const index = input->index();
		input->setPos(0, _collapseRatio * (NODE_SEPARATOR_THICKNESS + (index + 1) * NODE_LINE_HEIGHT));
		input->setOpacity(_collapseRatio);
	}

	for (auto* output : _outputs)
	{
		auto const index = output->index();
		output->setPos(0, _collapseRatio * (NODE_SEPARATOR_THICKNESS + (index + 1) * NODE_LINE_HEIGHT));
		output	->setOpacity(_collapseRatio);
	}
	
	handleItemPositionHasChanged();
}

} // namespace qtMate::flow
