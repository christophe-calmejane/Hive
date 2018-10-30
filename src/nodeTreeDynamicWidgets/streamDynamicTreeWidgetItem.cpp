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

#include "streamDynamicTreeWidgetItem.hpp"
#include "streamFormatComboBox.hpp"
#include "streamConnectionWidget.hpp"

#include <QMenu>

StreamDynamicTreeWidgetItem::StreamDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamNodeStaticModel const* const staticModel, la::avdecc::controller::model::StreamInputNodeDynamicModel const* const inputDynamicModel, la::avdecc::controller::model::StreamOutputNodeDynamicModel const* const outputDynamicModel, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _streamType(streamType)
	, _streamIndex(streamIndex)
{
	auto* currentFormatItem = new QTreeWidgetItem(this);
	currentFormatItem->setText(0, "Stream Format");

	la::avdecc::controller::model::StreamNodeDynamicModel const* const dynamicModel = inputDynamicModel ? static_cast<decltype(dynamicModel)>(inputDynamicModel) : static_cast<decltype(dynamicModel)>(outputDynamicModel);

	auto* formatComboBox = new StreamFormatComboBox{ _entityID };
	formatComboBox->setStreamFormats(staticModel->formats);
	formatComboBox->setCurrentStreamFormat(dynamicModel->currentFormat);

	parent->setItemWidget(currentFormatItem, 1, formatComboBox);

	// Send changes
	connect(formatComboBox, &StreamFormatComboBox::currentFormatChanged, this,
		[this, formatComboBox](la::avdecc::entity::model::StreamFormat const& streamFormat)
		{
			if (_streamType == la::avdecc::entity::model::DescriptorType::StreamInput)
			{
				avdecc::ControllerManager::getInstance().setStreamInputFormat(_entityID, _streamIndex, streamFormat);
			}
			else if (_streamType == la::avdecc::entity::model::DescriptorType::StreamOutput)
			{
				avdecc::ControllerManager::getInstance().setStreamOutputFormat(_entityID, _streamIndex, streamFormat);
			}
		});

	// Listen for changes
	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamFormatChanged, formatComboBox,
		[this, formatComboBox](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
		{
			if (entityID == _entityID && descriptorType == _streamType && streamIndex == _streamIndex)
				formatComboBox->setCurrentStreamFormat(streamFormat);
		});

	//

	{
		// Create fields
		_streamFormat = new QTreeWidgetItem(this);
		_streamFormat->setText(0, "Current Stream Format");

		_streamFlags = new QTreeWidgetItem(this);
		_streamFlags->setText(0, "Stream Flags");

		_streamDestMac = new QTreeWidgetItem(this);
		_streamDestMac->setText(0, "Stream Dest Address");

		_streamID = new QTreeWidgetItem(this);
		_streamID->setText(0, "Stream ID");

		_streamVlanID = new QTreeWidgetItem(this);
		_streamVlanID->setText(0, "Stream Vlan ID");

		_msrpAccumulatedLatency = new QTreeWidgetItem(this);
		_msrpAccumulatedLatency->setText(0, "MSRP Accumulated Latency");

		_msrpFailureCode = new QTreeWidgetItem(this);
		_msrpFailureCode->setText(0, "MSRP Failure Code");

		_msrpFailureBridgeID = new QTreeWidgetItem(this);
		_msrpFailureBridgeID->setText(0, "MSRP Failure Bridge ID");

		// Update info right now
		updateStreamInfo(dynamicModel->streamInfo);

		// Listen for StreamInfoChanged
		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamInfoChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& info)
			{
				if (entityID == _entityID && descriptorType == _streamType && streamIndex == _streamIndex)
				{
					updateStreamInfo(info);
				}
			});
	}

	// StreamInput dynamic info
	if (inputDynamicModel)
	{
		// Create fields
		_connectionState = new QTreeWidgetItem(this);
		_connectionState->setText(0, "Connection State");

		// Update info right now
		updateConnectionState(inputDynamicModel->connectionState);

		// Listen for Connection changed signals
		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamConnectionChanged, this,
			[this](la::avdecc::controller::model::StreamConnectionState const& state)
			{
				auto const listenerID = state.listenerStream.entityID;
				auto const listenerIndex = state.listenerStream.streamIndex;

				if (listenerID == _entityID && listenerIndex == _streamIndex)
				{
					updateConnectionState(state);
				}
			});
	}

	// StreamOutput dynamic info
	if (outputDynamicModel)
	{
		// Create fields
		auto* item = new QTreeWidgetItem(this);
		item->setText(0, "Connections");
		_connections = new QListWidget;
		_connections->setStyleSheet(".QListWidget{margin-top:4px;margin-bottom:4px}");
		parent->setItemWidget(item, 1, _connections);

		// Update info right now
		updateConnections(outputDynamicModel->connections);

		// Listen for Connections changed signal
		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamConnectionsChanged, this,
			[this](la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::controller::model::StreamConnections const& connections)
			{
				if (stream.entityID == _entityID && stream.streamIndex == _streamIndex)
				{
					updateConnections(connections);
				}
			});
#pragma message("TODO: When the notification is available")
	}
}

void StreamDynamicTreeWidgetItem::updateStreamInfo(la::avdecc::entity::model::StreamInfo const& streamInfo)
{
	_streamFormat->setText(1, avdecc::helper::toHexQString(streamInfo.streamFormat, true, true));
	_streamFlags->setText(1, avdecc::helper::flagsToString(streamInfo.streamInfoFlags));
	_streamDestMac->setText(1, QString::fromStdString(la::avdecc::networkInterface::macAddressToString(streamInfo.streamDestMac, true)));
	_streamID->setText(1, avdecc::helper::toHexQString(streamInfo.streamID, true, true));
	_streamVlanID->setText(1, QString::number(streamInfo.streamVlanID));
	_msrpAccumulatedLatency->setText(1, QString::number(streamInfo.msrpAccumulatedLatency));
	_msrpFailureCode->setText(1, QString::number(streamInfo.msrpFailureCode));
	_msrpFailureBridgeID->setText(1, avdecc::helper::toHexQString(streamInfo.msrpFailureBridgeID, true, true));
}

void StreamDynamicTreeWidgetItem::updateConnections(la::avdecc::controller::model::StreamConnections const& connections)
{
	_connections->clear();

	auto const streamConnection{ la::avdecc::entity::model::StreamIdentification{ _entityID, _streamIndex } };

	for (auto const& connection : connections)
	{
		auto const entityID = connection.entityID;
		auto const streamIndex = connection.streamIndex;

		auto* widget = new StreamConnectionWidget{ streamConnection, la::avdecc::entity::model::StreamIdentification{ entityID, streamIndex }, _connections };
		auto* item = new QListWidgetItem(_connections);
		item->setSizeHint(widget->sizeHint());

		_connections->setItemWidget(item, widget);
	}

	_connections->sortItems();
}

void StreamDynamicTreeWidgetItem::updateConnectionState(la::avdecc::controller::model::StreamConnectionState const& connectionState)
{
	QString stateText{ "Unknown" };

	auto const getTalkerStreamNameText = [&connectionState]()
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto const controlledEntity = manager.getControlledEntity(connectionState.talkerStream.entityID);
		if (controlledEntity)
		{
			return QString("%1:%2").arg(avdecc::helper::smartEntityName(*controlledEntity)).arg(connectionState.talkerStream.streamIndex);
		}
		return QString("%1:%2").arg(avdecc::helper::uniqueIdentifierToString(connectionState.talkerStream.entityID)).arg(connectionState.talkerStream.streamIndex);
	};

	switch (connectionState.state)
	{
		case la::avdecc::controller::model::StreamConnectionState::State::NotConnected:
			stateText = "Not Connected";
			break;
		case la::avdecc::controller::model::StreamConnectionState::State::FastConnecting:
			stateText = "Fast Connecting to " + getTalkerStreamNameText();
			break;
		case la::avdecc::controller::model::StreamConnectionState::State::Connected:
			stateText = "Connected to " + getTalkerStreamNameText();
			break;
		default:
			AVDECC_ASSERT(false, "Unhandled case");
			break;
	}
	_connectionState->setText(1, stateText);
}
