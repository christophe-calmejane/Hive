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
