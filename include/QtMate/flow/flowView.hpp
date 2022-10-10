#pragma once

#include <QGraphicsView>
#include <QElapsedTimer>
#include <QVariantAnimation>
#include <QtMate/flow/flowDefs.hpp>

namespace qtMate::flow
{
class FlowView : public QGraphicsView
{
	using QGraphicsView::setScene;

public:
	FlowView(FlowScene* scene, QWidget* parent = nullptr);
	virtual ~FlowView() override;

	void animatedCenterOn(QPointF const& scenePos);

protected:
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void mouseReleaseEvent(QMouseEvent* event) override;
	virtual void wheelEvent(QWheelEvent* event) override;
	virtual void timerEvent(QTimerEvent* event) override;

private:
	bool handleMousePressEvent(QMouseEvent* event);
	bool handleMouseMoveEvent(QMouseEvent* event);
	bool handleMouseReleaseEvent(QMouseEvent* event);
	bool handleWheelEvent(QWheelEvent* event);
	bool handleTimerEvent(QTimerEvent* event);

	FlowSocket* socketAt(QPointF const& scenePos) const;
	FlowLink* createLinkFromConnection(FlowConnection* connection);
	bool canConnect(FlowSocket* socket) const;

private:
	FlowScene* _scene{};

	QElapsedTimer _elapsedTimer{};
	int _animationTimerId{ -1 };

	QVariantAnimation* _centerOnAnimation{};

	enum class ConnectionMode
	{
		Undefined,
		ConnectToInput,
		ConnectToOutput,
		ChangeInput,
		ChangeOutput,
	};

	// current connection mode
	ConnectionMode _mode{ ConnectionMode::Undefined };

	// this connection is used to temporarily mark a socket as connected
	// when creating a new connection
	QScopedPointer<FlowConnection> _tmpConnection{};
	FlowSocketType _tmpSocketType{};

	// the list of slots on the other hand of the manipulated link
	FlowSocketSlots _slots{};

	// list of existing connections
	FlowConnections _connections{};

	// list of volatile links
	FlowLinks _links{};
};

} // namespace qtMate::flow
