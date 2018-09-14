/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "connectionMatrix/view.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/headerView.hpp"
#include "connectionMatrix/itemDelegate.hpp"
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"

#include <QMouseEvent>
#include <QMenu>

namespace connectionMatrix
{

View::View(QWidget* parent)
	: QTableView{parent}
	, _model{std::make_unique<Model>()}
	, _verticalHeaderView{std::make_unique<HeaderView>(Qt::Vertical, this)}
	, _horizontalHeaderView{std::make_unique<HeaderView>(Qt::Horizontal, this)}
	, _itemDelegate{std::make_unique<ItemDelegate>()}
{
	setModel(_model.get());
	setVerticalHeader(_verticalHeaderView.get());
	setHorizontalHeader(_horizontalHeaderView.get());
	setItemDelegate(_itemDelegate.get());
	
	setSelectionMode(QAbstractItemView::NoSelection);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setCornerButtonEnabled(false);
	setMouseTracking(true);
	
	// Configure highlight color
	auto p = palette();
	p.setColor(QPalette::Highlight, 0xf3e5f5);
	setPalette(p);
	
	connect(this, &QTableView::clicked, this, &View::onClicked);
	
	verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(verticalHeader(), &QHeaderView::customContextMenuRequested, this, &View::onHeaderCustomContextMenuRequested);
	
	horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &View::onHeaderCustomContextMenuRequested);
}

void View::mouseMoveEvent(QMouseEvent* event)
{
	auto const index = indexAt(static_cast<QMouseEvent*>(event)->pos());
	selectionModel()->select(index, QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows|QItemSelectionModel::Columns);
	
	QTableView::mouseMoveEvent(event);
}

void View::onClicked(QModelIndex const& index)
{

#if 0
	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();

		auto const* const model = static_cast<ConnectionMatrixModel const*>(index.model());
		auto const& talkerNode = model->nodeAtRow(index.row());
		auto const& listenerNode = model->nodeAtColumn(index.column());
		auto const& talkerData = talkerNode->userData.value<UserData>();
		auto const& listenerData = listenerNode->userData.value<UserData>();

		if ((talkerData.type == UserData::Type::OutputStreamNode && listenerData.type == UserData::Type::InputStreamNode)
				|| (talkerData.type == UserData::Type::RedundantOutputStreamNode && listenerData.type == UserData::Type::RedundantInputStreamNode))
		{
			auto const caps = model->d_ptr->connectionCapabilities(talkerData, listenerData);

			if (la::avdecc::hasFlag(caps, ConnectionCapabilities::Connectable))
			{
				if (la::avdecc::hasFlag(caps, ConnectionCapabilities::Connected))
				{
					manager.disconnectStream(talkerData.entityID, talkerData.streamIndex, listenerData.entityID, listenerData.streamIndex);
				}
				else
				{
					manager.connectStream(talkerData.entityID, talkerData.streamIndex, listenerData.entityID, listenerData.streamIndex);
				}
			}
		}
		else if (talkerData.type == UserData::Type::RedundantOutputNode && listenerData.type == UserData::Type::RedundantInputNode)
		{
			auto const caps = model->d_ptr->connectionCapabilities(talkerData, listenerData);

			bool doConnect{ false };
			bool doDisconnect{ false };

			if (la::avdecc::hasFlag(caps, ConnectionCapabilities::Connectable))
			{
				if (la::avdecc::hasFlag(caps, ConnectionCapabilities::Connected))
					doDisconnect = true;
				else
					doConnect = true;
			}

			auto talkerEntity = manager.getControlledEntity(talkerData.entityID);
			auto listenerEntity = manager.getControlledEntity(listenerData.entityID);
			if (talkerEntity && listenerEntity)
			{
				auto const& talkerEntityNode = talkerEntity->getEntityNode();
				auto const& talkerEntityInfo = talkerEntity->getEntity();
				auto const& listenerEntityNode = listenerEntity->getEntityNode();
				auto const& listenerEntityInfo = listenerEntity->getEntity();

				auto const& talkerRedundantNode = talkerEntity->getRedundantStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerData.redundantIndex);
				auto const& listenerRedundantNode = listenerEntity->getRedundantStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerData.redundantIndex);
				// TODO: Maybe someday handle the case for more than 2 streams for redundancy
				AVDECC_ASSERT(talkerRedundantNode.redundantStreams.size() == listenerRedundantNode.redundantStreams.size(), "More than 2 redundant streams in the set");
				auto talkerIt = talkerRedundantNode.redundantStreams.begin();
				auto listenerIt = listenerRedundantNode.redundantStreams.begin();
				auto atLeastOneConnected{ false };
				auto allConnected{ true };
				auto allCompatibleFormat{ true };
				auto allDomainCompatible{ true };
				for (auto idx = 0u; idx < talkerRedundantNode.redundantStreams.size(); ++idx)
				{
					auto const* const talkerStreamNode = static_cast<la::avdecc::controller::model::StreamOutputNode const*>(talkerIt->second);
					auto const* const listenerStreamNode = static_cast<la::avdecc::controller::model::StreamInputNode const*>(listenerIt->second);
					auto const areConnected = model->d_ptr->isStreamConnected(talkerData.entityID, talkerStreamNode, listenerStreamNode);
					if (doConnect && !areConnected)
					{
						manager.connectStream(talkerData.entityID, talkerStreamNode->descriptorIndex, listenerData.entityID, listenerStreamNode->descriptorIndex);
					}
					else if (doDisconnect && areConnected)
					{
						manager.disconnectStream(talkerData.entityID, talkerStreamNode->descriptorIndex, listenerData.entityID, listenerStreamNode->descriptorIndex);
					}
					++talkerIt;
					++listenerIt;
				}
			}
		}
	}
	catch (...)
	{
	}
#endif
}

void View::onHeaderCustomContextMenuRequested(QPoint const& pos)
{
	auto* header = static_cast<QHeaderView*>(sender());
	
	auto const logicalIndex = header->logicalIndexAt(pos);
	if (logicalIndex < 0)
	{
		return;
	}

	try
	{
		auto const entityID = model()->headerData(logicalIndex, header->orientation(), Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>();
		auto const streamIndex = model()->headerData(logicalIndex, header->orientation(), Model::StreamIndexRole).value<la::avdecc::entity::model::StreamIndex>();
		
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);
		if (controlledEntity)
		{
			auto const& entityNode = controlledEntity->getEntityNode();
			la::avdecc::controller::model::StreamNode const* streamNode{nullptr};
			
			QString streamName;
			bool isStreamRunning{false};
			
			auto const isInputStreamKind{header->orientation() == Qt::Horizontal};
			
			if (isInputStreamKind)
			{
				auto const& streamInputNode = controlledEntity->getStreamInputNode(entityNode.dynamicModel->currentConfiguration, streamIndex);
				streamName = avdecc::helper::objectName(controlledEntity.get(), streamInputNode);
				isStreamRunning = controlledEntity->isStreamInputRunning(entityNode.dynamicModel->currentConfiguration, streamIndex);
				streamNode = &streamInputNode;
			}
			else
			{
				auto const& streamOutputNode = controlledEntity->getStreamOutputNode(entityNode.dynamicModel->currentConfiguration, streamIndex);
				streamName = avdecc::helper::objectName(controlledEntity.get(), streamOutputNode);
				isStreamRunning = controlledEntity->isStreamOutputRunning(entityNode.dynamicModel->currentConfiguration, streamIndex);
				streamNode = &streamOutputNode;
			}
			
			auto addHeaderAction = [](QMenu& menu, QString const& text)
			{
				auto* action = menu.addAction(text);
				auto font = action->font();
				font.setBold(true);
				action->setFont(font);
				action->setEnabled(false);
				return action;
			};
			
			auto addAction = [](QMenu& menu, QString const& text, bool enabled)
			{
				auto* action = menu.addAction(text);
				action->setEnabled(enabled);
				return action;
			};
			
			QMenu menu;
			
			addHeaderAction(menu, "Entity: " + avdecc::helper::smartEntityName(*controlledEntity));
			addHeaderAction(menu, "Stream: " + streamName);
			menu.addSeparator();
			auto* startStreamingAction = addAction(menu, "Start Streaming", !isStreamRunning);
			auto* stopStreamingAction = addAction(menu, "Stop Streaming", isStreamRunning);
			menu.addSeparator();
			menu.addAction("Cancel");
			
			// Release the controlled entity before starting a long operation (menu.exec)
			controlledEntity.reset();
			
			if (auto* action = menu.exec(header->mapToGlobal(pos)))
			{
				if (action == startStreamingAction)
				{
					if (isInputStreamKind)
					{
						manager.startStreamInput(entityID, streamIndex);
					}
					else
					{
						manager.startStreamOutput(entityID, streamIndex);
					}
				}
				else if (action == stopStreamingAction)
				{
					if (isInputStreamKind)
					{
						manager.stopStreamInput(entityID, streamIndex);
					}
					else
					{
						manager.stopStreamOutput(entityID, streamIndex);
					}
				}
			}
			
		}
	}
	catch (...)
	{
	}
}

} // namespace connectionMatrix
