#pragma once

#include <QGraphicsItem>
#include <QtMate/flow/flowdefs.hpp>

namespace qtMate::flow
{
class FlowSocket : public QGraphicsItem
{
public:
	FlowSocket(FlowNode* node, FlowSocketIndex index, FlowSocketDescriptor const& descriptor);
	virtual ~FlowSocket() override;

	FlowNode* node() const;
	FlowSocketIndex index() const;
	FlowSocketDescriptor const& descriptor() const;
	FlowSocketSlot slot() const;

	QColor const& color() const;
	void setColor(QColor const& color);

	bool hit(QPointF const& scenePos) const;

	QPointF hotSpotSceneCenter() const;

	virtual bool isConnected() const = 0;
	virtual QRectF hotSpotBoundingRect() const = 0;

protected:
	FlowNode* _node{};
	FlowSocketIndex _index{};
	FlowSocketDescriptor _descriptor{};

	QColor _color{ Qt::darkGray };
};

} // namespace qtMate::flow
