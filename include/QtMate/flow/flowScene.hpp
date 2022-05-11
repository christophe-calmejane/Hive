#pragma once

#include <QGraphicsScene>
#include <QtMate/flow/flowDefs.hpp>

namespace qtMate::flow
{
class FlowScene : public QGraphicsScene
{
	Q_OBJECT
public:
	FlowScene(FlowSceneDelegate* delegate, QObject* parent = nullptr);
	virtual ~FlowScene() override;

	FlowNode* createNode(FlowNodeUid const& uid, FlowNodeDescriptor const& descriptor);
	void destroyNode(FlowNodeUid const& uid);

	FlowConnection* createConnection(FlowConnectionDescriptor const& descriptor);
	void destroyConnection(FlowConnectionDescriptor const& descriptor);

	QVector<FlowNode*> nodes() const;
	QVector<FlowConnection*> connections() const;

	FlowNode* node(FlowNodeUid const& uid) const;

	FlowInput* input(FlowSocketSlot const& slot) const;
	FlowOutput* output(FlowSocketSlot const& slot) const;

	FlowConnection* connection(FlowConnectionDescriptor const& descriptor) const;

	// do some sanity checks before returning the value of FlowSceneDelegate::canConnect
	bool canConnect(FlowOutput* output, FlowInput* input) const;
	bool canConnect(FlowConnectionDescriptor const& descriptor) const;

	// shortcut to FlowSceneDelegate::socketTypeColor
	QColor socketTypeColor(FlowSocketType type) const;

	std::pair<FlowOutput*, FlowInput*> sockets(FlowConnectionDescriptor const& descriptor) const;

signals:
	void nodeCreated(FlowNodeUid const& uid);
	void nodeDestroyed(FlowNodeUid const& uid);
	void connectionCreated(FlowConnectionDescriptor const& descriptor);
	void connectionDestroyed(FlowConnectionDescriptor const& descriptor);

private:
	FlowSceneDelegate* _delegate{};

	QHash<FlowNodeUid, FlowNode*> _nodes{};
	QHash<FlowConnectionDescriptor, FlowConnection*> _connections{};
};

} // namespace qtMate::flow
