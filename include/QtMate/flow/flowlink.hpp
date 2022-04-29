#pragma once

#include <QGraphicsPathItem>

namespace qtMate::flow {

class FlowLink : public QGraphicsPathItem {
public:
	FlowLink(QGraphicsItem* parent = nullptr);
	virtual ~FlowLink() override;

	void setStart(QPointF const& start);
	void setStop(QPointF const& stop);

private:
	void updatePainterPath();

private:
	QPointF _start{};
	QPointF _stop{};
};

} // namespace qtMate::flow
