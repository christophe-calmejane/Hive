#pragma once

#include <QGraphicsItem>
#include <QVariantAnimation>
#include <QtMate/flow/flowDefs.hpp>

namespace qtMate::flow
{

class FlowNodeHeader;

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
	
	bool hasConnectedInput() const;
	bool hasConnectedOutput() const;

	enum
	{
		Type = UserType + 1
	};
	virtual int type() const override;

	virtual QRectF boundingRect() const override;
	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget = nullptr) override;

protected:
	virtual QVariant itemChange(GraphicsItemChange change, QVariant const& value) override;
	virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
	void toggleCollapsed();
	void setCollapsed(bool collapsed);
	void handleItemPositionHasChanged();
	void handleItemSelectionHasChanged();
	void updateSockets();

private:
	FlowSceneDelegate* _delegate{};
	FlowNodeUid _uid{};
	QString _name{};
	FlowNodeHeader* _header{};
	FlowInputs _inputs{};
	FlowOutputs _outputs{};
	bool _collapsed{};
	float _collapseRatio{1.f};
	QVariantAnimation _collapseAnimation{};
};

} // namespace qtMate::flow
