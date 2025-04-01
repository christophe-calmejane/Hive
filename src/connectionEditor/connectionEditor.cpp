/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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
#include "connectionEditor.hpp"
#include "nodeOrganizer.hpp"
#include "nodeListModel.hpp"
#include "nodeListView.hpp"
#include "connectionWorkspace.hpp"

#include <QtMate/flow/flowScene.hpp>
#include <QtMate/flow/flowSceneDelegate.hpp>
#include <QtMate/flow/flowNode.hpp>
#include <QtMate/flow/flowSocket.hpp>
#include <QtMate/flow/flowView.hpp>

#include <QHBoxLayout>
#include <QListView>

namespace
{
QPixmap render(qtMate::flow::FlowSceneDelegate* delegate, qtMate::flow::FlowNodeUid const& uid, qtMate::flow::FlowNodeDescriptor const& descriptor)
{
	auto node = qtMate::flow::FlowNode{ delegate, uid, descriptor };

	auto const boundingRect = node.boundingRect();
	auto const width = static_cast<int>(boundingRect.width() + 1);
	auto const height = static_cast<int>(boundingRect.height() + 1);
	auto pixmap = QPixmap{ width, height };
	pixmap.fill(Qt::transparent);

	auto painter = QPainter{ &pixmap };
	node.paint(&painter, nullptr, nullptr);

	auto const options = QStyleOptionGraphicsItem{};

	// we also need to render the children ourself
	for (auto* child : node.childItems())
	{
		auto transform = QTransform{};
		transform.translate(child->pos().x(), child->pos().y());
		painter.setTransform(transform);
		child->paint(&painter, &options, nullptr);
	}

	return pixmap;
}

} // namespace

class ConnectionEditorDelegate : public qtMate::flow::FlowSceneDelegate
{
public:
	using FlowSceneDelegate::FlowSceneDelegate;

	virtual QColor socketTypeColor(qtMate::flow::FlowSocketType type) const override
	{
		switch (type)
		{
			case 0x0:
				return 0x673AB7;
			case 0x1:
				return 0x009688;
			case 0x2:
				return 0x7CB342;
			default:
				return Qt::darkGray;
		}
	}
};

ConnectionEditor::ConnectionEditor(qtMate::flow::FlowNodeDescriptorMap const& nodes, qtMate::flow::FlowConnectionDescriptors const& connections, QWidget* parent)
	: QWidget{ parent }
{
	auto* delegate = new ConnectionEditorDelegate{ this };

	auto* scene = new qtMate::flow::FlowScene{ delegate, this };

	auto* organizer = new NodeOrganizer{ scene, this };

	auto* model = new NodeListModel{ this };

	connect(scene, &qtMate::flow::FlowScene::nodeCreated, this,
		[=](qtMate::flow::FlowNodeUid const& uid)
		{
			model->setEnabled(uid, false);
		});

	connect(scene, &qtMate::flow::FlowScene::nodeDestroyed, this,
		[=](qtMate::flow::FlowNodeUid const& uid)
		{
			model->setEnabled(uid, true);
		});

	connect(scene, &qtMate::flow::FlowScene::connectionCreated, this,
		[=](qtMate::flow::FlowConnectionDescriptor const& descriptor)
		{
			_connections.insert(descriptor);
		});

	connect(scene, &qtMate::flow::FlowScene::connectionDestroyed, this,
		[=](qtMate::flow::FlowConnectionDescriptor const& descriptor)
		{
			_connections.remove(descriptor);
		});

	auto* list = new NodeListView{ this };
	list->setModel(model);

	auto* workspace = new ConnectionWorkspace{ scene, this };

	// selecting a node in the view makes it active in the list and vice-versa
	connect(scene, &QGraphicsScene::selectionChanged, this,
		[=]()
		{
			for (auto* item : scene->selectedItems())
			{
				if (auto* node = qgraphicsitem_cast<qtMate::flow::FlowNode*>(item))
				{
					if (auto const index = model->nodeIndex(node->uid()); index.isValid())
					{
						list->selectionModel()->select(index, QItemSelectionModel::SelectionFlag::ClearAndSelect);
					}
				}
			}
		});

	connect(list, &QListView::clicked, this,
		[=](QModelIndex const& index)
		{
			auto const uid = index.data(Qt::UserRole).value<qtMate::flow::FlowNodeUid>();
			if (auto* node = scene->node(uid))
			{
				scene->clearSelection();
				node->setSelected(true);
			}
		});

	// double-clicking a node in the list focuses it in the view
	connect(list, &NodeListView::doubleClicked, this,
		[=](QModelIndex const& index)
		{
			auto const uid = index.data(Qt::UserRole).value<qtMate::flow::FlowNodeUid>();
			if (auto* node = scene->node(uid))
			{
				workspace->animatedCenterOn(node->sceneBoundingRect().center());
			}
		});

	// node dropped means we want to add it to the workspace
	connect(list, &NodeListView::dropped, this,
		[=](QModelIndex const& index)
		{
			auto const uid = index.data(Qt::UserRole).value<qtMate::flow::FlowNodeUid>();
			if (auto* descriptor = model->descriptor(uid))
			{
				scene->createNode(uid, *descriptor);
			}
		});

	// add all nodes to the list
	auto const uids = nodes.keys();
	for (auto const& uid : uids)
	{
		auto const& descriptor = nodes[uid];
		auto const pixmap = ::render(delegate, uid, descriptor);
		model->createItem(uid, descriptor, pixmap);
	}

	// gather the list of nodes that are connected
	QSet<qtMate::flow::FlowNodeUid> listOfNodeUidToCreate{};
	for (auto const& descriptor : connections)
	{
		listOfNodeUidToCreate.insert(descriptor.first.first);
		listOfNodeUidToCreate.insert(descriptor.second.first);
	}

	// add all these nodes to the scene
	for (auto const& uid : listOfNodeUidToCreate)
	{
		scene->createNode(uid, nodes[uid]);
	}

	// finally, create all connections
	for (auto const& descriptor : connections)
	{
		scene->createConnection(descriptor);
	}

	auto* layout = new QHBoxLayout{ this };
	layout->addWidget(list);
	layout->addWidget(workspace);

	workspace->animatedCenterOn(scene->itemsBoundingRect().bottomRight());
}

ConnectionEditor::~ConnectionEditor() = default;

qtMate::flow::FlowConnectionDescriptors const& ConnectionEditor::connections() const
{
	return _connections;
}
