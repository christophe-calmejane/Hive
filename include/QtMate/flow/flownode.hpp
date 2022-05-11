#pragma once

#include <QGraphicsItem>
#include <QtMate/flow/flowdefs.hpp>

namespace qtMate::flow
{
class FlowNode : public QGraphicsItem
{
public:
	FlowNode(FlowSceneDelegate* delegate, FlowNodeUid const& uid, FlowNodeDescriptor const& descriptor, QGraphicsItem* parent = nullptr);
	virtual ~FlowNode() override;

	FlowNodeUid const& uid() const;
	QString const& name() const;
	FlowInputs const& inputs() const;
	FlowOutputs const& outputs() const;

	FlowInput* input(FlowSocketIndex index) const;
	FlowOutput* output(FlowSocketIndex index) const;

	enum
	{
		Type = UserType + 1
	};
	virtual int type() const override;

	virtual QRectF boundingRect() const override;
	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget = nullptr) override;

protected:
	virtual QVariant itemChange(GraphicsItemChange change, QVariant const& value) override;

private:
	void handleItemPositionHasChanged();
	void handleItemSelectionHasChanged();

private:
	FlowSceneDelegate* _delegate{};
	FlowNodeUid _uid{};
	QString _name{};
	FlowInputs _inputs{};
	FlowOutputs _outputs{};
};

} // namespace qtMate::flow
