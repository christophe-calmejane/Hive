#include "QtMate/flow/flowNode.hpp"
#include "QtMate/flow/flowInput.hpp"
#include "QtMate/flow/flowOutput.hpp"
#include "QtMate/flow/flowConnection.hpp"
#include "QtMate/flow/flowSceneDelegate.hpp"
#include "QtMate/flow/flowStyle.hpp"

namespace qtMate::flow
{
namespace
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

} // namespace

FlowNode::FlowNode(FlowSceneDelegate* delegate, FlowNodeUid const& uid, FlowNodeDescriptor const& descriptor, QGraphicsItem* parent)
	: QGraphicsItem{ parent }
	, _delegate{ delegate }
	, _uid{ uid }
	, _name{ descriptor.name }
{
	Q_ASSERT(_delegate && "FlowSceneDelegate is required");

	setFlag(QGraphicsItem::ItemIsMovable, true);
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);
	setFlag(QGraphicsItem::ItemDoesntPropagateOpacityToChildren, true);
	setOpacity(0.65f);
	setAcceptHoverEvents(true);
	setZValue(0.f);

	new FlowNodeHeader{ _name, this };


	for (auto const& descriptor : descriptor.inputs)
	{
		auto const index = static_cast<FlowSocketIndex>(_inputs.size());
		auto* input = _inputs.emplace_back(new FlowInput{ this, index, descriptor });
		input->moveBy(0, NODE_SEPARATOR_THICKNESS + (index + 1) * NODE_LINE_HEIGHT);
		input->setColor(_delegate->socketTypeColor(input->descriptor().type));
	}

	for (auto const& descriptor : descriptor.outputs)
	{
		auto const index = static_cast<FlowSocketIndex>(_outputs.size());
		auto* output = _outputs.emplace_back(new FlowOutput{ this, index, descriptor });
		output->moveBy(0, NODE_SEPARATOR_THICKNESS + (index + 1) * NODE_LINE_HEIGHT);
		output->setColor(_delegate->socketTypeColor(output->descriptor().type));
	}
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

int FlowNode::type() const
{
	return Type;
}

QRectF FlowNode::boundingRect() const
{
	auto const n = 1 + std::max(_inputs.size(), _outputs.size());
	return QRectF{ 0.f, 0.f, NODE_WIDTH, n * NODE_LINE_HEIGHT };
}

void FlowNode::paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget)
{
	auto const r = boundingRect();

	// background
	painter->setPen(Qt::NoPen);
	painter->setBrush(NODE_BACKGROUND_COLOR);
	painter->drawRoundedRect(r, NODE_BORDER_RADIUS, NODE_BORDER_RADIUS);

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

	// inputs
	if (!_inputs.empty())
	{
		auto inputsBoundingRect = r;
		inputsBoundingRect.moveTop(NODE_LINE_HEIGHT + NODE_SEPARATOR_THICKNESS);
		inputsBoundingRect.setWidth(r.width() * inputRatio(this));
		inputsBoundingRect.setHeight(r.height() - NODE_LINE_HEIGHT - NODE_SEPARATOR_THICKNESS);
		painter->setPen(Qt::NoPen);
		painter->setBrush(NODE_INPUT_BACKGROUND_COLOR);
		drawRoundedRect(painter, inputsBoundingRect, _outputs.empty() ? (BottomLeft | BottomRight) : BottomLeft, NODE_BORDER_RADIUS);
	}

	// outputs
	if (!_outputs.empty())
	{
		auto ouputsBoundingRect = r;
		ouputsBoundingRect.moveTop(NODE_LINE_HEIGHT + NODE_SEPARATOR_THICKNESS);
		ouputsBoundingRect.moveLeft(r.width() * inputRatio(this) + NODE_SEPARATOR_THICKNESS);
		ouputsBoundingRect.setWidth(r.width() * outputRatio(this) - NODE_SEPARATOR_THICKNESS);
		ouputsBoundingRect.setHeight(r.height() - NODE_LINE_HEIGHT - NODE_SEPARATOR_THICKNESS);
		painter->setPen(Qt::NoPen);
		painter->setBrush(NODE_OUTPUT_BACKGROUND_COLOR);
		drawRoundedRect(painter, ouputsBoundingRect, _inputs.empty() ? (BottomLeft | BottomRight) : BottomLeft, NODE_BORDER_RADIUS);
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

void FlowNode::handleItemPositionHasChanged()
{
	for (auto* input : _inputs)
	{
		if (auto* connection = input->connection())
		{
			connection->updatePath();
		}
	}

	for (auto* output : _outputs)
	{
		for (auto* connection : output->connections())
		{
			connection->updatePath();
		}
	}
}

void FlowNode::handleItemSelectionHasChanged()
{
	setZValue(isSelected() ? 1.f : 0.f);
}

} // namespace qtMate::flow
