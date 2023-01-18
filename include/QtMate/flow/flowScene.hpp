/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

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
