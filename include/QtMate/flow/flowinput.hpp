#pragma once

#include <QtMate/flow/flowsocket.hpp>

namespace qtMate::flow {

class FlowInput : public FlowSocket {
public:
	using FlowSocket::FlowSocket;

	virtual ~FlowInput() override;

	enum { Type = UserType + 2 };
	virtual int type() const override;
	virtual QRectF boundingRect() const override;

	virtual bool isConnected() const override;
	virtual QRectF hotSpotBoundingRect() const override;

	FlowConnection* connection() const;
	void setConnection(FlowConnection* connection);

	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget = nullptr) override;

private:
	FlowConnection* _connection{};
};

} // namespace qtMate::flow
