/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "connectionMatrix/view.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/node.hpp"
#include "connectionMatrix/headerView.hpp"
#include "connectionMatrix/itemDelegate.hpp"
#include "connectionMatrix/cornerWidget.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/hiveLogItems.hpp"

#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>

#include <QMouseEvent>
#include <QMessageBox>
#include <QMenu>
#include <QApplication>

namespace connectionMatrix
{
View::View(QWidget* parent)
	: QTableView{ parent }
	, _model{ std::make_unique<Model>() }
	, _horizontalHeaderView{ std::make_unique<HeaderView>(true, Qt::Horizontal, this) }
	, _verticalHeaderView{ std::make_unique<HeaderView>(false, Qt::Vertical, this) }
	, _itemDelegate{ std::make_unique<ItemDelegate>(this) }
	, _cornerWidget{ std::make_unique<CornerWidget>(this) }
{
	setModel(_model.get());
	setHorizontalHeader(_horizontalHeaderView.get());
	setVerticalHeader(_verticalHeaderView.get());
	setItemDelegate(_itemDelegate.get());

	setSelectionMode(QAbstractItemView::NoSelection);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setMouseTracking(true);

	// Take care of the top left corner widget
	setCornerButtonEnabled(false);
	stackUnder(_cornerWidget.get());

	// Apply filter when needed
	connect(_cornerWidget.get(), &CornerWidget::filterChanged, this, &View::onFilterChanged);

	connect(_cornerWidget.get(), &CornerWidget::horizontalExpandClicked, _horizontalHeaderView.get(), &HeaderView::expandAll);
	connect(_cornerWidget.get(), &CornerWidget::horizontalCollapseClicked, _horizontalHeaderView.get(), &HeaderView::collapseAll);

	connect(_cornerWidget.get(), &CornerWidget::verticalExpandClicked, _verticalHeaderView.get(), &HeaderView::expandAll);
	connect(_cornerWidget.get(), &CornerWidget::verticalCollapseClicked, _verticalHeaderView.get(), &HeaderView::collapseAll);

	// Make sure the corner widgets fits the available space
	auto const updateCornerWidgetGeometry = [this]()
	{
		_cornerWidget->setGeometry(0, 0, verticalHeader()->width(), horizontalHeader()->height());
	};

	connect(_verticalHeaderView.get(), &QHeaderView::geometriesChanged, this, updateCornerWidgetGeometry);
	connect(_horizontalHeaderView.get(), &QHeaderView::geometriesChanged, this, updateCornerWidgetGeometry);

	// Handle click on the table
	connect(this, &QTableView::clicked, this, &View::onIntersectionClicked);

	// Handle contextual menu
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QTableView::customContextMenuRequested, this, &View::onCustomContextMenuRequested);

	// Configure settings observers
	auto& settings = settings::SettingsManager::getInstance();
	settings.registerSettingObserver(settings::ConnectionMatrix_AlwaysShowArrowTip.name, this);
	settings.registerSettingObserver(settings::ConnectionMatrix_AlwaysShowArrowEnd.name, this);
	settings.registerSettingObserver(settings::ConnectionMatrix_Transpose.name, this);
	settings.registerSettingObserver(settings::ConnectionMatrix_ChannelMode.name, this);
	settings.registerSettingObserver(settings::General_ThemeColorIndex.name, this);

	// react on connection completed signals to show error messages.
	auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
	connect(&channelConnectionManager, &avdecc::ChannelConnectionManager::createChannelConnectionsFinished, this, &View::handleCreateChannelConnectionsFinished);
}

View::~View()
{
	// Remove settings observers
	auto& settings = settings::SettingsManager::getInstance();
	settings.unregisterSettingObserver(settings::ConnectionMatrix_AlwaysShowArrowTip.name, this);
	settings.unregisterSettingObserver(settings::ConnectionMatrix_AlwaysShowArrowEnd.name, this);
	settings.unregisterSettingObserver(settings::ConnectionMatrix_Transpose.name, this);
	settings.unregisterSettingObserver(settings::ConnectionMatrix_ChannelMode.name, this);
	settings.unregisterSettingObserver(settings::General_ThemeColorIndex.name, this);
}

void View::onIntersectionClicked(QModelIndex const& index)
{
	auto const& intersectionData = _model->intersectionData(index);

	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();

	auto const handleChannelCreationResult = [this](avdecc::ChannelConnectionManager::ChannelConnectResult channelConnectResult, bool allowTalkerMappingChanges, bool allowListenerMappingRemoval, std::function<void(bool, bool)> callbackTryAgainElevatedRights)
	{
		switch (channelConnectResult)
		{
			case avdecc::ChannelConnectionManager::ChannelConnectResult::RemovalOfListenerDynamicMappingsNecessary:
			{
				auto result = QMessageBox::question(this, "", "The connection is not possible with the currently existing listener mappings. Allow removing the currently unused dynamic mappings?");
				if (result == QMessageBox::StandardButton::Yes)
				{
					allowListenerMappingRemoval = true;
					callbackTryAgainElevatedRights(allowTalkerMappingChanges, allowListenerMappingRemoval);
				}
				break;
			}
			case avdecc::ChannelConnectionManager::ChannelConnectResult::NeedsTalkerMappingAdjustment:
			{
				auto result = QMessageBox::question(this, "", "To make the required changes it is necessary to temporarily disconnect streams which might lead to audio interruptions! Continue?");
				if (result == QMessageBox::StandardButton::Yes)
				{
					// yes was chosen, make call again, with force override flag
					allowTalkerMappingChanges = true;
					callbackTryAgainElevatedRights(allowTalkerMappingChanges, allowListenerMappingRemoval);
					break;
				}
				break;
			}
			case avdecc::ChannelConnectionManager::ChannelConnectResult::Impossible:
				QMessageBox::information(this, "", "The connection couldn't be created because all compatible streams are already occupied.");
				break;
			case avdecc::ChannelConnectionManager::ChannelConnectResult::Error:
				QMessageBox::information(this, "", "The connection couldn't be created. Unknown error occured.");
				break;
			case avdecc::ChannelConnectionManager::ChannelConnectResult::Unsupported:
				QMessageBox::information(this, "", "The connection couldn't be created. Unsupported device.");
				break;
			default:
				break;
		}
	};

	switch (intersectionData.type)
	{
		// Use SmartConnection algorithm
		case Model::IntersectionData::Type::RedundantStream_RedundantStream:
		case Model::IntersectionData::Type::RedundantStream_SingleStream:
		case Model::IntersectionData::Type::SingleStream_SingleStream:
		case Model::IntersectionData::Type::Redundant_Redundant:
		case Model::IntersectionData::Type::Redundant_RedundantStream:
		case Model::IntersectionData::Type::Redundant_SingleStream:
		{
			if (intersectionData.smartConnectableStreams.empty())
			{
				QMessageBox::information(this, "", "Couldn't detect a Stream of the Redundant Pair on the same AVB domain than the other Entity, cannot automatically change the stream connection.\n\nPlease expand the Redundant Pair and manually choose desired stream.");
			}
			else
			{
				auto doConnect{ false };
				auto doDisconnect{ false };

				if (intersectionData.state == Model::IntersectionData::State::NotConnected || intersectionData.state == Model::IntersectionData::State::PartiallyConnected)
				{
					doConnect = true;
				}
				else
				{
					doDisconnect = true;
				}

				auto* talker = static_cast<RedundantNode*>(intersectionData.talker);
				auto* listener = static_cast<RedundantNode*>(intersectionData.listener);

				for (auto const& connectableStream : intersectionData.smartConnectableStreams)
				{
					auto const areConnected = connectableStream.isConnected || connectableStream.isFastConnecting;

					if (doConnect && !areConnected)
					{
						manager.connectStream(connectableStream.talkerStream.entityID, connectableStream.talkerStream.streamIndex, connectableStream.listenerStream.entityID, connectableStream.listenerStream.streamIndex);
					}
					else if (doDisconnect && areConnected)
					{
						manager.disconnectStream(connectableStream.talkerStream.entityID, connectableStream.talkerStream.streamIndex, connectableStream.listenerStream.entityID, connectableStream.listenerStream.streamIndex);
					}
					else
					{
						LOG_HIVE_TRACE(QString("connectionMatrix::View::onClicked: Neither connecting nor disconnecting: doConnect=%1 doDisconnect=%2 areConnected=%3").arg(doConnect).arg(doDisconnect).arg(areConnected));
					}
				}
			}
			break;
		}

		case Model::IntersectionData::Type::RedundantStream_RedundantStream_Forbidden:
		{
			// The only allowed action on a forbidden connection, is disconnecting a stream that was connected using a non-milan controller
			if (intersectionData.state != Model::IntersectionData::State::NotConnected)
			{
				auto* talker = static_cast<RedundantNode*>(intersectionData.talker);
				auto* listener = static_cast<RedundantNode*>(intersectionData.listener);

				for (auto const& connectableStream : intersectionData.smartConnectableStreams)
				{
					auto const areConnected = connectableStream.isConnected || connectableStream.isFastConnecting;

					if (areConnected)
					{
						manager.disconnectStream(connectableStream.talkerStream.entityID, connectableStream.talkerStream.streamIndex, connectableStream.listenerStream.entityID, connectableStream.listenerStream.streamIndex);
					}
				}
			}
			break;
		}

		case Model::IntersectionData::Type::Entity_Entity:
		{
			if (_model->mode() == Model::Mode::Channel && QApplication::keyboardModifiers().testFlag(Qt::ControlModifier))
			{
				// establish diagonal connections
				// gather all connections to be made:
				auto const talkerID = intersectionData.talker->entityID();
				auto const listenerID = intersectionData.listener->entityID();
				auto talkerControlledEntity = manager.getControlledEntity(talkerID); // TODO: all getControlledEntity calls should be removed the this method. Any model related information should be gathered and cached from notification thread
				auto listenerControlledEntity = manager.getControlledEntity(listenerID);

				auto const& talkerConfiguration = talkerControlledEntity->getCurrentConfigurationNode();
				auto const& listenerConfiguration = listenerControlledEntity->getCurrentConfigurationNode();
				std::vector<avdecc::ChannelIdentification> talkerChannels;
				std::vector<avdecc::ChannelIdentification> listenerChannels;

				for (auto const& talkerAudioUnitKV : talkerConfiguration.audioUnits)
				{
					for (auto const& talkerStreamPortOutputKV : talkerAudioUnitKV.second.streamPortOutputs)
					{
						for (auto const& talkerAudioClusterKV : talkerStreamPortOutputKV.second.audioClusters)
						{
							for (uint16_t channel = 0; channel < talkerAudioClusterKV.second.staticModel->channelCount; channel++)
							{
								talkerChannels.emplace_back(avdecc::ChannelIdentification{ talkerConfiguration.descriptorIndex, talkerAudioClusterKV.first, channel, avdecc::ChannelConnectionDirection::OutputToInput, talkerAudioUnitKV.first, talkerStreamPortOutputKV.first, talkerStreamPortOutputKV.second.staticModel->baseCluster });
							}
						}
					}
				}

				for (auto const& listenerAudioUnitKV : listenerConfiguration.audioUnits)
				{
					for (auto const& listenerStreamPortOutputKV : listenerAudioUnitKV.second.streamPortInputs)
					{
						for (auto const& listenerAudioClusterKV : listenerStreamPortOutputKV.second.audioClusters)
						{
							for (uint16_t channel = 0; channel < listenerAudioClusterKV.second.staticModel->channelCount; channel++)
							{
								listenerChannels.emplace_back(avdecc::ChannelIdentification{ listenerConfiguration.descriptorIndex, listenerAudioClusterKV.first, channel, avdecc::ChannelConnectionDirection::InputToOutput, listenerAudioUnitKV.first, listenerStreamPortOutputKV.first, listenerStreamPortOutputKV.second.staticModel->baseCluster });
							}
						}
					}
				}

				auto talkerChannelIt = talkerChannels.begin();
				auto listenerChannelIt = listenerChannels.begin();
				std::vector<std::pair<avdecc::ChannelIdentification, avdecc::ChannelIdentification>> connectionsToCreate;

				while (talkerChannelIt != talkerChannels.end() && listenerChannelIt != listenerChannels.end())
				{
					connectionsToCreate.push_back(std::make_pair(*talkerChannelIt, *listenerChannelIt));
					talkerChannelIt++;
					listenerChannelIt++;
				}

				auto error = channelConnectionManager.createChannelConnections(talkerID, listenerID, connectionsToCreate, false, true);
				std::function<void(bool, bool)> elevatedRightsCallback = [&](bool allowTalkerMappingChanges, bool allowListenerMappingRemoval)
				{
					auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
					auto errorSecondTry = channelConnectionManager.createChannelConnections(talkerID, listenerID, connectionsToCreate, allowTalkerMappingChanges, allowListenerMappingRemoval);
					handleChannelCreationResult(errorSecondTry, allowTalkerMappingChanges, allowListenerMappingRemoval, elevatedRightsCallback);
				};
				handleChannelCreationResult(error, false, true, elevatedRightsCallback);
			}
			break;
		}

		case Model::IntersectionData::Type::Entity_SingleChannel:
		{
			if (_model->mode() == Model::Mode::Channel && QApplication::keyboardModifiers().testFlag(Qt::ControlModifier))
			{
				// check if a talker channel edge was clicked
				if (!intersectionData.talker->isChannelNode())
				{
					return;
				}

				auto* talkerChannelNode = static_cast<ChannelNode*>(intersectionData.talker);
				AVDECC_ASSERT(talkerChannelNode->type() == Node::Type::OutputChannel, "Invalid node type");

				// establish all connections in this row
				// gather all connections to be made:
				auto const& talkerChannelIdentification = talkerChannelNode->channelIdentification();

				auto const talkerID = intersectionData.talker->entityID();
				auto const listenerID = intersectionData.listener->entityID();
				auto talkerControlledEntity = manager.getControlledEntity(talkerID);
				auto listenerControlledEntity = manager.getControlledEntity(listenerID);

				auto const& talkerConfiguration = talkerControlledEntity->getCurrentConfigurationNode();
				auto const& listenerConfiguration = listenerControlledEntity->getCurrentConfigurationNode();
				std::vector<avdecc::ChannelIdentification> listenerChannels;

				for (auto const& listenerAudioUnitKV : listenerConfiguration.audioUnits)
				{
					for (auto const& listenerStreamPortOutputKV : listenerAudioUnitKV.second.streamPortInputs)
					{
						for (auto const& listenerAudioClusterKV : listenerStreamPortOutputKV.second.audioClusters)
						{
							for (uint16_t channel = 0; channel < listenerAudioClusterKV.second.staticModel->channelCount; channel++)
							{
								listenerChannels.emplace_back(avdecc::ChannelIdentification{ listenerConfiguration.descriptorIndex, listenerAudioClusterKV.first, channel, avdecc::ChannelConnectionDirection::InputToOutput, listenerAudioUnitKV.first, listenerStreamPortOutputKV.first, listenerStreamPortOutputKV.second.staticModel->baseCluster });
							}
						}
					}
				}

				auto listenerChannelIt = listenerChannels.begin();
				std::vector<std::pair<avdecc::ChannelIdentification, avdecc::ChannelIdentification>> connectionsToCreate;

				while (listenerChannelIt != listenerChannels.end())
				{
					connectionsToCreate.push_back(std::make_pair(talkerChannelIdentification, *listenerChannelIt));
					listenerChannelIt++;
				}

				auto error = channelConnectionManager.createChannelConnections(talkerID, listenerID, connectionsToCreate, false, true);
				std::function<void(bool, bool)> elevatedRightsCallback = [&](bool allowTalkerMappingChanges, bool allowListenerMappingRemoval)
				{
					auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
					auto errorSecondTry = channelConnectionManager.createChannelConnections(talkerID, listenerID, connectionsToCreate, allowTalkerMappingChanges, allowListenerMappingRemoval);
					handleChannelCreationResult(errorSecondTry, allowTalkerMappingChanges, allowListenerMappingRemoval, elevatedRightsCallback);
				};
				handleChannelCreationResult(error, false, true, elevatedRightsCallback);
			}
			break;
		}

		case Model::IntersectionData::Type::SingleChannel_SingleChannel:
		{
			auto const talkerID = intersectionData.talker->entityID();
			auto const listenerID = intersectionData.listener->entityID();
			auto const talkerChannelIdentification = static_cast<ChannelNode*>(intersectionData.talker)->channelIdentification();
			auto const listenerChannelIdentification = static_cast<ChannelNode*>(intersectionData.listener)->channelIdentification();

			if (intersectionData.state != Model::IntersectionData::State::NotConnected)
			{
				channelConnectionManager.removeChannelConnection(talkerID, *talkerChannelIdentification.audioUnitIndex, *talkerChannelIdentification.streamPortIndex, talkerChannelIdentification.clusterIndex, *talkerChannelIdentification.baseCluster, talkerChannelIdentification.clusterChannel, listenerID, *listenerChannelIdentification.audioUnitIndex, *listenerChannelIdentification.streamPortIndex, listenerChannelIdentification.clusterIndex, *listenerChannelIdentification.baseCluster, listenerChannelIdentification.clusterChannel);
			}
			else
			{
				auto error = channelConnectionManager.createChannelConnection(talkerID, listenerID, talkerChannelIdentification, listenerChannelIdentification, false, true);

				std::function<void(bool, bool)> elevatedRightsCallback = [&](bool allowTalkerMappingChanges, bool allowListenerMappingRemoval)
				{
					auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
					auto errorSecondTry = channelConnectionManager.createChannelConnection(talkerID, listenerID, talkerChannelIdentification, listenerChannelIdentification, allowTalkerMappingChanges, allowListenerMappingRemoval);
					handleChannelCreationResult(errorSecondTry, allowTalkerMappingChanges, allowListenerMappingRemoval, elevatedRightsCallback);
				};
				handleChannelCreationResult(error, false, true, elevatedRightsCallback);
			}
			break;
		}

		// Offline Streams
		case Model::IntersectionData::Type::OfflineOutputStream_Redundant:
		case Model::IntersectionData::Type::OfflineOutputStream_RedundantStream:
		case Model::IntersectionData::Type::OfflineOutputStream_SingleStream:
		{
			auto* talker = static_cast<RedundantNode*>(intersectionData.talker);
			auto* listener = static_cast<RedundantNode*>(intersectionData.listener);

			for (auto const& connectableStream : intersectionData.smartConnectableStreams)
			{
				auto const areConnected = connectableStream.isConnected || connectableStream.isFastConnecting;
				if (areConnected)
				{
					manager.disconnectStream(connectableStream.talkerStream.entityID, connectableStream.talkerStream.streamIndex, connectableStream.listenerStream.entityID, connectableStream.listenerStream.streamIndex);
				}
			}
			break;
		}

		default:
			break;
	}
}

void View::onCustomContextMenuRequested(QPoint const& pos)
{
	try
	{
		auto const index = indexAt(pos);
		if (!index.isValid())
		{
			return;
		}

		auto const& intersectionData = _model->intersectionData(index);
		auto talkerNodeType = intersectionData.talker->type();
		auto listenerNodeType = intersectionData.listener->type();

		if ((talkerNodeType == Node::Type::OutputStream && listenerNodeType == Node::Type::InputStream) || (talkerNodeType == Node::Type::RedundantOutputStream && listenerNodeType == Node::Type::RedundantInputStream))
		{
#pragma message("TODO: Call haveCompatibleFormats(talker, listener)")
			if (intersectionData.flags.test(Model::IntersectionData::Flag::WrongFormat))
			{
				QMenu menu;

				auto* matchTalkerAction = menu.addAction("Match formats using Talker");
				auto* matchListenerAction = menu.addAction("Match formats using Listener");
				menu.addSeparator();
				menu.addAction("Cancel");

#pragma message("TODO: setEnabled() based on format compatibility -> If talker can be set from listener, and vice versa.")
				matchTalkerAction->setEnabled(true);
				matchListenerAction->setEnabled(true);

				auto const talkerID = intersectionData.talker->entityID();
				auto const listenerID = intersectionData.listener->entityID();

				auto const* const talkerStreamNode = static_cast<StreamNode*>(intersectionData.talker);
				auto const* const listenerStreamNode = static_cast<StreamNode*>(intersectionData.listener);

				if (auto* action = menu.exec(viewport()->mapToGlobal(pos)))
				{
					auto const talkerStreamIndex = talkerStreamNode->streamIndex();
					auto const listenerStreamIndex = listenerStreamNode->streamIndex();

					if (action == matchTalkerAction)
					{
						auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
						manager.setStreamInputFormat(listenerID, listenerStreamIndex, talkerStreamNode->streamFormat());
					}
					else if (action == matchListenerAction)
					{
						auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
						manager.setStreamOutputFormat(talkerID, talkerStreamIndex, listenerStreamNode->streamFormat());
					}
				}
			}
		}
		else if (talkerNodeType == Node::Type::RedundantOutput && listenerNodeType == Node::Type::RedundantInput)
		{
			// TODO
		}
	}
	catch (...)
	{
	}
}

void View::onFilterChanged(QString const& filter)
{
	applyFilterPattern(QRegExp{ filter });
}

void View::applyFilterPattern(QRegExp const& pattern)
{
	_verticalHeaderView->setFilterPattern(pattern);
	_horizontalHeaderView->setFilterPattern(pattern);
}

void View::forceFilter()
{
	applyFilterPattern(QRegExp{ _cornerWidget->filterText() });
}

void View::mouseMoveEvent(QMouseEvent* event)
{
	auto const index = indexAt(event->pos());
	selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Columns);
	QTableView::mouseMoveEvent(event);
}

void View::onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept
{
	if (name == settings::ConnectionMatrix_AlwaysShowArrowTip.name)
	{
		auto const alwaysShow = value.toBool();
		_verticalHeaderView->setAlwaysShowArrowTip(alwaysShow);
		_horizontalHeaderView->setAlwaysShowArrowTip(alwaysShow);

		// Manually force a model refresh of the headers
		_model->forceRefreshHeaders();
	}
	else if (name == settings::ConnectionMatrix_AlwaysShowArrowEnd.name)
	{
		auto const alwaysShow = value.toBool();
		_verticalHeaderView->setAlwaysShowArrowEnd(alwaysShow);
		_horizontalHeaderView->setAlwaysShowArrowEnd(alwaysShow);

		// Manually force a model refresh of the headers
		_model->forceRefreshHeaders();
	}
	else if (name == settings::ConnectionMatrix_Transpose.name)
	{
		auto const transposed = value.toBool();

		auto const verticalSectionState = _verticalHeaderView->saveSectionState();
		auto const horizontalSectionState = _horizontalHeaderView->saveSectionState();

		_model->setTransposed(transposed);
		_cornerWidget->setTransposed(transposed);
		_verticalHeaderView->setTransposed(transposed);
		_horizontalHeaderView->setTransposed(transposed);

		_verticalHeaderView->restoreSectionState(horizontalSectionState);
		_horizontalHeaderView->restoreSectionState(verticalSectionState);

		forceFilter();
	}
	else if (name == settings::ConnectionMatrix_ChannelMode.name)
	{
		auto const channelMode = value.toBool();
		auto const mode = channelMode ? Model::Mode::Channel : Model::Mode::Stream;

		_model->setMode(mode);

		forceFilter();
	}
	else if (name == settings::General_ThemeColorIndex.name)
	{
		auto const colorName = qtMate::material::color::Palette::name(value.toInt());

		_cornerWidget->setColor(colorName);
		_verticalHeaderView->setColor(colorName);
		_horizontalHeaderView->setColor(colorName);

		// Manually force a model refresh of the headers
		_model->forceRefreshHeaders();
	}
}

void View::handleCreateChannelConnectionsFinished(avdecc::CreateConnectionsInfo const& info)
{
	std::unordered_set<la::avdecc::UniqueIdentifier, la::avdecc::UniqueIdentifier::hash> iteratedEntityIds;
	for (auto it = info.connectionCreationErrors.begin(), end = info.connectionCreationErrors.end(); it != end; it++) // upper_bound not supported on mac (to iterate over unique keys)
	{
		if (iteratedEntityIds.find(it->first) == iteratedEntityIds.end())
		{
			iteratedEntityIds.insert(it->first);
		}
		else
		{
			continue; // entity already displayed.
		}
		auto entityName = hive::modelsLibrary::helper::toHexQString(it->first.getValue()); // by default show the id if the entity is offline
		{
			auto const controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(it->first);
			if (controlledEntity)
			{
				entityName = hive::modelsLibrary::helper::smartEntityName(*controlledEntity);
			}
		}
		auto errorsForEntity = info.connectionCreationErrors.equal_range(it->first);
		QString errors;

		// if a stream couldn't be stopped we won't show the error. Also the start stream error won't be shown in this case.
		auto stopStreamFailed{ false };
		for (auto i = errorsForEntity.first; i != errorsForEntity.second; ++i)
		{
			if (i->second.commandTypeAcmp)
			{
				switch (*i->second.commandTypeAcmp)
				{
					case hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream:
						errors += "Connecting stream failed. ";
						break;
					case hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream:
						errors += "Disconnecting stream failed. ";
						break;
					case hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectTalkerStream:
						errors += "Disconnecting talker stream failed. ";
						break;
					default:
						break;
				}
			}
			else if (i->second.commandTypeAecp)
			{
				switch (*i->second.commandTypeAecp)
				{
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetStreamFormat:
						errors += "Setting the stream format failed. ";
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::AddStreamPortAudioMappings:
						errors += "Adding of dynamic mappings failed. ";
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::RemoveStreamPortAudioMappings:
						errors += "Removal of dynamic mappings failed. ";
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::StartStream:
						// only show the error if the stop didn't fail
						if (!stopStreamFailed)
						{
							errors += "Starting the stream failed. ";
						}
						else
						{
							continue; // continue the loop
						}
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::StopStream:
						stopStreamFailed = true;
						continue; // never show stop stream failures -> continue the loop
					default:
						break;
				}
			}
			switch (i->second.errorType)
			{
				case avdecc::commandChain::CommandExecutionError::LockedByOther:
					errors += "Entity is locked.";
					break;
				case avdecc::commandChain::CommandExecutionError::AcquiredByOther:
					errors += "Entity is aquired by an other controller.";
					break;
				case avdecc::commandChain::CommandExecutionError::Timeout:
					errors += "Command timed out. Entity might be offline.";
					break;
				case avdecc::commandChain::CommandExecutionError::EntityError:
					errors += "Entity error. Operation might not be supported.";
					break;
				case avdecc::commandChain::CommandExecutionError::NetworkIssue:
					errors += "Network error.";
					break;
				case avdecc::commandChain::CommandExecutionError::CommandFailure:
					errors += "Command failure.";
					break;
				case avdecc::commandChain::CommandExecutionError::NoMediaClockInputAvailable:
					errors += "Device does not have any compatible media clock inputs.";
					break;
				case avdecc::commandChain::CommandExecutionError::NoMediaClockOutputAvailable:
					errors += "Device does not have any compatible media clock outputs.";
					break;
				case avdecc::commandChain::CommandExecutionError::NotSupported:
					errors += "The command is not supported by this device.";
					break;
				default:
					errors += "Unknwon error.";
					break;
			}
			errors += "\n";
		}
		if (!errors.isEmpty())
		{
			QMessageBox::information(qobject_cast<QWidget*>(this), "Error while applying", QString("Error(s) occured on %1 while applying the configuration:\n\n%2").arg(entityName).arg(errors));
		}
	}
}

} // namespace connectionMatrix
