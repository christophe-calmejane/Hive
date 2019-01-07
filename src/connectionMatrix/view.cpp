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
#include "connectionMatrix/legend.hpp"
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/hiveLogItems.hpp"

#include <QMouseEvent>
#include <QMenu>

Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)

namespace connectionMatrix
{

// We use a custom SortFilterProxy that does not filter anything through the filter, instead it just hides the filtered rows/columns
class Filter : public QSortFilterProxyModel
{
public:
	Filter(View& view)
	: _view{view} {
	}

protected:
	virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
	{
		if (sourceModel()->headerData(sourceRow, Qt::Vertical, Model::NodeTypeRole).value<Model::NodeType>() == Model::NodeType::Entity)
		{
			auto const matches = sourceModel()->headerData(sourceRow, Qt::Vertical, Qt::DisplayRole).toString().contains(filterRegExp());
			auto const childrenCount = sourceModel()->headerData(sourceRow, Qt::Vertical, Model::ChildrenCountRole).toInt();
			for (auto row = sourceRow; row <= sourceRow + childrenCount; row++)
			{
				_view.setRowHidden(row, !matches);
			}
		}

		return true;
	}

	virtual bool filterAcceptsColumn(int sourceColumn, const QModelIndex& sourceParent) const override
	{
		if (sourceModel()->headerData(sourceColumn, Qt::Horizontal, Model::NodeTypeRole).value<Model::NodeType>() == Model::NodeType::Entity)
		{
			auto const matches = sourceModel()->headerData(sourceColumn, Qt::Horizontal, Qt::DisplayRole).toString().contains(filterRegExp());
			auto const childrenCount = sourceModel()->headerData(sourceColumn, Qt::Horizontal, Model::ChildrenCountRole).toInt();
			for (auto column = sourceColumn; column <= sourceColumn + childrenCount; column++)
			{
				_view.setColumnHidden(column, !matches);
			}
		}

		return true;
	}

private:
	View& _view;
};

View::View(QWidget* parent)
	: QTableView{ parent }
	, _model{ std::make_unique<Model>() }
	, _verticalHeaderView{ std::make_unique<HeaderView>(Qt::Vertical, this) }
	, _horizontalHeaderView{ std::make_unique<HeaderView>(Qt::Horizontal, this) }
	, _itemDelegate{ std::make_unique<ItemDelegate>() }
	, _legend{ std::make_unique<Legend>(this) }
	, _filterProxy{ std::make_unique<Filter>(*this) }
{
	_proxy.connectToModel(_model.get());

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
	p.setColor(QPalette::HighlightedText, Qt::black);
	setPalette(p);

	connect(this, &QTableView::clicked, this, &View::onClicked);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QTableView::customContextMenuRequested, this, &View::onCustomContextMenuRequested);

	verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(verticalHeader(), &QHeaderView::customContextMenuRequested, this, &View::onHeaderCustomContextMenuRequested);
	connect(verticalHeader(), &QHeaderView::geometriesChanged, this, &View::onLegendGeometryChanged);

	horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &View::onHeaderCustomContextMenuRequested);
	connect(horizontalHeader(), &QHeaderView::geometriesChanged, this, &View::onLegendGeometryChanged);

	connect(_legend.get(), &Legend::filterChanged, this,
		[this](QString const& filter)
		{
			_filterProxy->setFilterRegExp(filter);
		});

	// Configure settings observers
	auto& settings = settings::SettingsManager::getInstance();
	settings.registerSettingObserver(settings::TransposeConnectionMatrix.name, this);
}

View::~View()
{
	// Configure settings observers
	auto& settings = settings::SettingsManager::getInstance();
	settings.unregisterSettingObserver(settings::TransposeConnectionMatrix.name, this);
}

void View::mouseMoveEvent(QMouseEvent* event)
{
	auto const index = indexAt(static_cast<QMouseEvent*>(event)->pos());
	selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Columns);

	QTableView::mouseMoveEvent(event);
}

void View::onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept
{
	if (name == settings::TransposeConnectionMatrix.name)
	{
		auto const verticalSectionState = _verticalHeaderView->saveSectionState();
		auto const horizontalSectionState = _horizontalHeaderView->saveSectionState();

		_isTransposed = value.toBool();

		_itemDelegate->setTransposed(_isTransposed);
		_legend->setTransposed(_isTransposed);

		// Force a repaint while there is no model, this fixes a refresh issue when switching between transpose states
		setModel(nullptr);
		repaint();

		if (_isTransposed)
		{
			_filterProxy->setSourceModel(&_proxy);
		}
		else
		{
			_filterProxy->setSourceModel(_model.get());
		}

		setModel(_filterProxy.get());

		_verticalHeaderView->restoreSectionState(horizontalSectionState);
		_horizontalHeaderView->restoreSectionState(verticalSectionState);

		repaint();
	}
}

void View::onClicked(QModelIndex const& index)
{
	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();

		auto talkerNodeType = talkerData(index, Model::NodeTypeRole).value<Model::NodeType>();
		auto listenerNodeType = listenerData(index, Model::NodeTypeRole).value<Model::NodeType>();

		auto talkerID = talkerData(index, Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>();
		auto listenerID = listenerData(index, Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>();

		// Simple Stream or Single Stream of a redundant pair: connect the stream
		if ((talkerNodeType == Model::NodeType::OutputStream && listenerNodeType == Model::NodeType::InputStream) || (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInputStream))
		{
			auto const caps = index.data(Model::ConnectionCapabilitiesRole).value<Model::ConnectionCapabilities>();

			if (la::avdecc::utils::hasFlag(caps, Model::ConnectionCapabilities::Connectable))
			{
				auto const talkerStreamIndex = talkerData(index, Model::StreamIndexRole).value<la::avdecc::entity::model::StreamIndex>();
				auto const listenerStreamIndex = listenerData(index, Model::StreamIndexRole).value<la::avdecc::entity::model::StreamIndex>();

				if (la::avdecc::utils::hasFlag(caps, Model::ConnectionCapabilities::Connected))
				{
					manager.disconnectStream(talkerID, talkerStreamIndex, listenerID, listenerStreamIndex);
				}
				else
				{
					manager.connectStream(talkerID, talkerStreamIndex, listenerID, listenerStreamIndex);
				}
			}
		}

		// One redundant node and one redundant stream: connect the only possible stream (diagonal)
		else if ((talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInput) || (talkerNodeType == Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInputStream))
		{
			LOG_HIVE_INFO("TODO: Connect the only possible stream (the one in diagonal)");
		}

		// Both redundant nodes: connect both streams
		else if (talkerNodeType == Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInput)
		{
			auto const caps = index.data(Model::ConnectionCapabilitiesRole).value<Model::ConnectionCapabilities>();

			bool doConnect{ false };
			bool doDisconnect{ false };

			if (la::avdecc::utils::hasFlag(caps, Model::ConnectionCapabilities::Connectable))
			{
				if (la::avdecc::utils::hasFlag(caps, Model::ConnectionCapabilities::Connected))
					doDisconnect = true;
				else
					doConnect = true;
			}

			auto talkerEntity = manager.getControlledEntity(talkerID);
			auto listenerEntity = manager.getControlledEntity(listenerID);

			if (talkerEntity && listenerEntity)
			{
				auto const& talkerEntityNode = talkerEntity->getEntityNode();
				auto const& talkerEntityInfo = talkerEntity->getEntity();
				auto const& listenerEntityNode = listenerEntity->getEntityNode();
				auto const& listenerEntityInfo = listenerEntity->getEntity();

				auto const talkerRedundantIndex = talkerData(index, Model::RedundantIndexRole).value<la::avdecc::controller::model::VirtualIndex>();
				auto const listenerRedundantIndex = listenerData(index, Model::RedundantIndexRole).value<la::avdecc::controller::model::VirtualIndex>();

				auto const& talkerRedundantNode = talkerEntity->getRedundantStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerRedundantIndex);
				auto const& listenerRedundantNode = listenerEntity->getRedundantStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerRedundantIndex);
				// TODO: Maybe someday handle the case for more than 2 streams for redundancy
				AVDECC_ASSERT(talkerRedundantNode.redundantStreams.size() == listenerRedundantNode.redundantStreams.size(), "More than 2 redundant streams in the set");
				auto talkerIt = talkerRedundantNode.redundantStreams.begin();
				auto listenerIt = listenerRedundantNode.redundantStreams.begin();
				for (auto idx = 0u; idx < talkerRedundantNode.redundantStreams.size(); ++idx)
				{
					auto const* const talkerStreamNode = static_cast<la::avdecc::controller::model::StreamOutputNode const*>(talkerIt->second);
					auto const* const listenerStreamNode = static_cast<la::avdecc::controller::model::StreamInputNode const*>(listenerIt->second);
					auto const areConnected = avdecc::helper::isStreamConnected(talkerID, talkerStreamNode, listenerStreamNode);
					if (doConnect && !areConnected)
					{
						manager.connectStream(talkerID, talkerStreamNode->descriptorIndex, listenerID, listenerStreamNode->descriptorIndex);
					}
					else if (doDisconnect && areConnected)
					{
						manager.disconnectStream(talkerID, talkerStreamNode->descriptorIndex, listenerID, listenerStreamNode->descriptorIndex);
					}
					else
					{
						LOG_HIVE_TRACE(QString("connectionMatrix::View::onClicked: Neither connecting nor disconnecting: doConnect=%1 doDisconnect=%2 areConnected=%3").arg(doConnect).arg(doDisconnect).arg(areConnected));
					}
					++talkerIt;
					++listenerIt;
				}
			}
		}

		// One non-redundant stream and one redundant stream: connect the stream
		else if ((talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::InputStream) || (talkerNodeType == Model::NodeType::OutputStream && listenerNodeType == Model::NodeType::RedundantInputStream))
		{
			auto const caps = index.data(Model::ConnectionCapabilitiesRole).value<Model::ConnectionCapabilities>();

			if (la::avdecc::utils::hasFlag(caps, Model::ConnectionCapabilities::Connectable))
			{
				auto const talkerStreamIndex = talkerData(index, Model::StreamIndexRole).value<la::avdecc::entity::model::StreamIndex>();
				auto const listenerStreamIndex = listenerData(index, Model::StreamIndexRole).value<la::avdecc::entity::model::StreamIndex>();

				if (la::avdecc::utils::hasFlag(caps, Model::ConnectionCapabilities::Connected))
				{
					manager.disconnectStream(talkerID, talkerStreamIndex, listenerID, listenerStreamIndex);
				}
				else
				{
					manager.connectStream(talkerID, talkerStreamIndex, listenerID, listenerStreamIndex);
				}
			}
		}

		// One non-redundant stream and one redundant node: connect the stream
		else if ((talkerNodeType == Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::InputStream) || (talkerNodeType == Model::NodeType::OutputStream && listenerNodeType == Model::NodeType::RedundantInput))
		{
			LOG_HIVE_INFO("TODO: Connect the non-redundant stream to the redundant stream on the same domain.");
			// Print a warning if no domain matches
		}
	}
	catch (...)
	{
		LOG_HIVE_DEBUG(QString("connectionMatrix::View::onClicked: Ignoring click due to an Exception"));
	}
}

void View::onCustomContextMenuRequested(QPoint const& pos)
{
	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();

		auto const index = indexAt(pos);
		if (!index.isValid())
		{
			return;
		}

		auto talkerNodeType = talkerData(index, Model::NodeTypeRole).value<Model::NodeType>();
		auto listenerNodeType = listenerData(index, Model::NodeTypeRole).value<Model::NodeType>();

		auto talkerID = talkerData(index, Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>();
		auto listenerID = listenerData(index, Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>();

		if ((talkerNodeType == Model::NodeType::OutputStream && listenerNodeType == Model::NodeType::InputStream) || (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInputStream))
		{
			auto const caps = index.data(Model::ConnectionCapabilitiesRole).value<Model::ConnectionCapabilities>();

#pragma message("TODO: Call haveCompatibleFormats(talker, listener)")
			if (caps != Model::ConnectionCapabilities::None && la::avdecc::utils::hasFlag(caps, Model::ConnectionCapabilities::WrongFormat))
			{
				QMenu menu;

				auto* matchTalkerAction = menu.addAction("Match formats using Talker");
				auto* matchListenerAction = menu.addAction("Match formats using Listener");
				menu.addSeparator();
				menu.addAction("Cancel");

#pragma message("TODO: setEnabled() based on format compatibility -> If talker can be set from listener, and vice versa.")
				matchTalkerAction->setEnabled(true);
				matchListenerAction->setEnabled(true);

				if (auto* action = menu.exec(viewport()->mapToGlobal(pos)))
				{
					auto const talkerStreamIndex = talkerData(index, Model::StreamIndexRole).value<la::avdecc::entity::model::StreamIndex>();
					auto const listenerStreamIndex = listenerData(index, Model::StreamIndexRole).value<la::avdecc::entity::model::StreamIndex>();

					if (action == matchTalkerAction)
					{
						auto talkerEntity = manager.getControlledEntity(talkerID);
						if (talkerEntity)
						{
							auto const& talkerEntityNode = talkerEntity->getEntityNode();
							auto const& talkerStreamNode = talkerEntity->getStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerStreamIndex);
							manager.setStreamInputFormat(listenerID, listenerStreamIndex, talkerStreamNode.dynamicModel->currentFormat);
						}
					}
					else if (action == matchListenerAction)
					{
						auto listenerEntity = manager.getControlledEntity(listenerID);
						if (listenerEntity)
						{
							auto const& listenerEntityNode = listenerEntity->getEntityNode();
							auto const& listenerStreamNode = listenerEntity->getStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerStreamIndex);
							manager.setStreamOutputFormat(talkerID, talkerStreamIndex, listenerStreamNode.dynamicModel->currentFormat);
						}
					}
				}
			}
		}
		else if (talkerNodeType == Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInput)
		{
			// TODO
		}
	}
	catch (...)
	{
	}
}

void View::onHeaderCustomContextMenuRequested(QPoint const& pos)
{
	auto* header = static_cast<QHeaderView*>(sender());

	auto const logicalIndex = header->logicalIndexAt(pos);
	if (logicalIndex < 0)
	{
		return;
	}

	// Highlight this section
	if (header->orientation() == Qt::Horizontal)
	{
		selectionModel()->select(model()->index(0, logicalIndex), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Columns);
	}
	else
	{
		selectionModel()->select(model()->index(logicalIndex, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
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
			la::avdecc::controller::model::StreamNode const* streamNode{ nullptr };

			QString streamName;
			bool isStreamRunning{ false };

			auto const isInputStreamKind{ header->orientation() == (_isTransposed ? Qt::Vertical : Qt::Horizontal) };

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

void View::onLegendGeometryChanged()
{
	_legend->setGeometry(0, 0, verticalHeader()->width(), horizontalHeader()->height());
}

QVariant View::talkerData(QModelIndex const& index, int role) const
{
	if (_isTransposed)
	{
		return model()->headerData(index.column(), Qt::Horizontal, role);
	}
	else
	{
		return model()->headerData(index.row(), Qt::Vertical, role);
	}
}

QVariant View::listenerData(QModelIndex const& index, int role) const
{
	if (_isTransposed)
	{
		return model()->headerData(index.row(), Qt::Vertical, role);
	}
	else
	{
		return model()->headerData(index.column(), Qt::Horizontal, role);
	}
}

} // namespace connectionMatrix
