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

#include "QtMate/flow/flowScene.hpp"
#include "QtMate/flow/flowSceneDelegate.hpp"
#include "QtMate/flow/flowNode.hpp"
#include "QtMate/flow/flowConnection.hpp"
#include "QtMate/flow/flowInput.hpp"
#include "QtMate/flow/flowOutput.hpp"
#include "QtMate/flow/flowStyle.hpp"

namespace qtMate::flow
{
namespace
{
void ancestors(FlowNode* node, QSet<FlowNode*>& set)
{
	for (auto* input : node->inputs())
	{
		if (auto* connection = input->connection())
		{
			if (connection->output())
			{
				auto* parent = connection->output()->node();
				if (!set.contains(parent))
				{
					set.insert(parent);
					ancestors(parent, set);
				}
			}
		}
	}
}

void descendants(FlowNode* node, QSet<FlowNode*>& set)
{
	for (auto* output : node->outputs())
	{
		if (output->isConnected())
		{
			for (auto* connection : output->connections())
			{
				if (connection->input())
				{
					auto* child = connection->input()->node();
					if (!set.contains(child))
					{
						set.insert(child);
						descendants(child, set);
					}
				}
			}
		}
	}
}

} // namespace

FlowScene::FlowScene(FlowSceneDelegate* delegate, QObject* parent)
	: QGraphicsScene{ parent }
	, _delegate{ delegate }
{
	Q_ASSERT(_delegate && "FlowSceneDelegate is required");
}

FlowScene::~FlowScene() = default;

FlowNode* FlowScene::createNode(FlowNodeUid const& uid, FlowNodeDescriptor const& descriptor)
{
	if (_nodes.contains(uid))
	{
		return nullptr;
	}

	auto* node = new FlowNode{ _delegate, uid, descriptor };
	_nodes.insert(uid, node);

	addItem(node);

	emit nodeCreated(uid);

	return node;
}

void FlowScene::destroyNode(FlowNodeUid const& uid)
{
	if (auto* node = _nodes.value(uid))
	{
		for (auto* input : node->inputs())
		{
			if (input->isConnected())
			{
				destroyConnection(input->connection()->descriptor());
			}
		}

		for (auto* output : node->outputs())
		{
			if (output->isConnected())
			{
				for (auto* connection : output->connections())
				{
					destroyConnection(connection->descriptor());
				}
			}
		}

		removeItem(node);

		_nodes.remove(uid);
		delete node;

		emit nodeDestroyed(uid);
	}
}

FlowConnection* FlowScene::createConnection(FlowConnectionDescriptor const& descriptor)
{
	if (_connections.contains(descriptor))
	{
		return nullptr;
	}

	auto [source, sink] = sockets(descriptor);

	if (!canConnect(source, sink))
	{
		qWarning() << "invalid connection";
		return nullptr;
	}

	if (sink->isConnected())
	{
		destroyConnection(sink->connection()->descriptor());
	}

	auto* connection = new FlowConnection{};

	connection->setPen(NODE_CONNECTION_PEN);
	connection->setOutput(source);
	connection->setInput(sink);

	_connections.insert(descriptor, connection);

	addItem(connection);

	emit connectionCreated(descriptor);

	return connection;
}

void FlowScene::destroyConnection(FlowConnectionDescriptor const& descriptor)
{
	if (auto* connection = _connections.value(descriptor))
	{
		removeItem(connection);
		delete connection;

		_connections.remove(descriptor);

		emit connectionDestroyed(descriptor);
	}
}

QVector<FlowNode*> FlowScene::nodes() const
{
	return _nodes.values();
}

QVector<FlowConnection*> FlowScene::connections() const
{
	return _connections.values();
}

FlowNode* FlowScene::node(FlowNodeUid const& uid) const
{
	return _nodes.value(uid);
}

FlowInput* FlowScene::input(FlowSocketSlot const& slot) const
{
	if (auto* n = node(slot.first))
	{
		return n->input(slot.second);
	}
	return nullptr;
}

FlowOutput* FlowScene::output(FlowSocketSlot const& slot) const
{
	if (auto* n = node(slot.first))
	{
		return n->output(slot.second);
	}
	return nullptr;
}

FlowConnection* FlowScene::connection(FlowConnectionDescriptor const& descriptor) const
{
	return _connections.value(descriptor);
}

bool FlowScene::canConnect(FlowOutput* output, FlowInput* input) const
{
	if (!output || !input)
	{
		return false;
	}

	if (output->node() == input->node())
	{
		return false;
	}

	auto outputHierarchy = QSet<FlowNode*>{};
	ancestors(output->node(), outputHierarchy);
	if (outputHierarchy.contains(input->node()))
	{
		return false;
	}

	auto inputHierarchy = QSet<FlowNode*>{};
	descendants(input->node(), inputHierarchy);
	if (inputHierarchy.contains(output->node()))
	{
		return false;
	}

	return _delegate->canConnect(output, input);
}

bool FlowScene::canConnect(FlowConnectionDescriptor const& descriptor) const
{
	auto [source, sink] = sockets(descriptor);
	return canConnect(source, sink);
}

QColor FlowScene::socketTypeColor(FlowSocketType type) const
{
	return _delegate->socketTypeColor(type);
}

std::pair<FlowOutput*, FlowInput*> FlowScene::sockets(FlowConnectionDescriptor const& descriptor) const
{
	auto* source = output(descriptor.first);
	if (!source)
	{
		qWarning() << "invalid source";
	}

	auto* sink = input(descriptor.second);
	if (!sink)
	{
		qWarning() << "invalid sink";
	}

	return std::make_pair(source, sink);
}

} // namespace qtMate::flow
