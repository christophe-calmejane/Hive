#pragma once

#include <QtMate/flow/flowSocket.hpp>

namespace qtMate::flow
{
class FlowOutput : public FlowSocket
{
public:
	using FlowSocket::FlowSocket;

	virtual ~FlowOutput() override;

	enum
	{
		Type = UserType + 3
	};
	virtual int type() const override;
	virtual QRectF boundingRect() const override;

	virtual bool isConnected() const override;
	virtual QRectF hotSpotBoundingRect() const override;

	void addConnection(FlowConnection* connection);
	void removeConnection(FlowConnection* connection);
	FlowConnections const& connections() const;

	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget = nullptr) override;

private:
	FlowConnections _connections{};
};

} // namespace qtMate::flow
