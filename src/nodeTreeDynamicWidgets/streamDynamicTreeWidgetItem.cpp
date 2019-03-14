/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "streamDynamicTreeWidgetItem.hpp"
#include "streamFormatComboBox.hpp"
#include "talkerStreamConnectionWidget.hpp"
#include "nodeTreeWidget.hpp"

#include <QMenu>

StreamDynamicTreeWidgetItem::StreamDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamNodeStaticModel const* const staticModel, la::avdecc::controller::model::StreamInputNodeDynamicModel const* const inputDynamicModel, la::avdecc::controller::model::StreamOutputNodeDynamicModel const* const outputDynamicModel, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _streamType(streamType)
	, _streamIndex(streamIndex)
{
	auto& manager = avdecc::ControllerManager::getInstance();
	auto listenerEntity = manager.getControlledEntity(entityID);

	auto* currentFormatItem = new QTreeWidgetItem(this);
	currentFormatItem->setText(0, "Stream Format");

	la::avdecc::controller::model::StreamNodeDynamicModel const* const dynamicModel = inputDynamicModel ? static_cast<decltype(dynamicModel)>(inputDynamicModel) : static_cast<decltype(dynamicModel)>(outputDynamicModel);

	auto* formatComboBox = new StreamFormatComboBox{ _entityID };
	formatComboBox->setStreamFormats(staticModel->formats);
	formatComboBox->setCurrentStreamFormat(dynamicModel->streamInfo.streamFormat);

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
	connect(&manager, &avdecc::ControllerManager::streamFormatChanged, formatComboBox,
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

		if (dynamicModel->streamInfo.streamInfoFlagsEx.has_value())
		{
			_streamFlagsEx = new QTreeWidgetItem(this);
			_streamFlagsEx->setText(0, "Stream Flags Ex");
		}

		if (dynamicModel->streamInfo.probingStatus.has_value())
		{
			_probingStatus = new QTreeWidgetItem(this);
			_probingStatus->setText(0, "Probing Status");
		}

		if (dynamicModel->streamInfo.acmpStatus.has_value())
		{
			_acmpStatus = new QTreeWidgetItem(this);
			_acmpStatus->setText(0, "Acmp Status");
		}

		// Update info right now
		updateStreamInfo(dynamicModel->streamInfo);

		// Listen for StreamInfoChanged
		connect(&manager, &avdecc::ControllerManager::streamInfoChanged, this,
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
		if (listenerEntity && listenerEntity->getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
		{
			_connectionState->setText(0, "Binding State");
		}
		else
		{
			_connectionState->setText(0, "Connection State");
		}
		_connectionStateWidget = new ListenerStreamConnectionWidget(inputDynamicModel->connectionState, parent);
		parent->setItemWidget(_connectionState, 1, _connectionStateWidget);
	}

	// StreamOutput dynamic info
	if (outputDynamicModel)
	{
		if (!listenerEntity || !listenerEntity->getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
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
			connect(&manager, &avdecc::ControllerManager::streamConnectionsChanged, this,
				[this](la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::controller::model::StreamConnections const& connections)
				{
					if (stream.entityID == _entityID && stream.streamIndex == _streamIndex)
					{
						updateConnections(connections);
					}
				});
		}
	}
}

void StreamDynamicTreeWidgetItem::updateStreamInfo(la::avdecc::entity::model::StreamInfo const& streamInfo)
{
	_streamFormat->setText(1, avdecc::helper::toHexQString(streamInfo.streamFormat.getValue(), true, true));
	setFlagsItemText(_streamFlags, la::avdecc::utils::forceNumeric(streamInfo.streamInfoFlags.value()), avdecc::helper::flagsToString(streamInfo.streamInfoFlags));
	_streamDestMac->setText(1, QString::fromStdString(la::avdecc::networkInterface::macAddressToString(streamInfo.streamDestMac, true)));
	_streamID->setText(1, avdecc::helper::toHexQString(streamInfo.streamID, true, true));
	_streamVlanID->setText(1, QString::number(streamInfo.streamVlanID));
	_msrpAccumulatedLatency->setText(1, QString::number(streamInfo.msrpAccumulatedLatency));
	_msrpFailureCode->setText(1, QString::number(streamInfo.msrpFailureCode));
	_msrpFailureBridgeID->setText(1, avdecc::helper::toHexQString(streamInfo.msrpFailureBridgeID, true, true));
	// Milan extension information
	if (_streamFlagsEx)
	{
		if (streamInfo.streamInfoFlagsEx.has_value())
		{
			auto const flagsEx = *streamInfo.streamInfoFlagsEx;
			setFlagsItemText(_streamFlagsEx, la::avdecc::utils::forceNumeric(flagsEx.value()), avdecc::helper::flagsToString(flagsEx));
		}
		else
		{
			_streamFlagsEx->setText(1, "No Value");
		}
	}
	if (_probingStatus)
	{
		if (streamInfo.probingStatus.has_value())
		{
			auto const probingStatus = *streamInfo.probingStatus;
			_probingStatus->setText(1, QString("%1 (%2)").arg(avdecc::helper::toHexQString(la::avdecc::utils::to_integral(probingStatus), true, true)).arg(avdecc::helper::probingStatusToString(probingStatus)));
		}
		else
		{
			_probingStatus->setText(1, "No Value");
		}
	}
	if (_acmpStatus)
	{
		if (streamInfo.acmpStatus.has_value())
		{
			auto const acmpStatus = *streamInfo.acmpStatus;
			_acmpStatus->setText(1, QString("%1 (%2)").arg(avdecc::helper::toHexQString(acmpStatus.getValue(), true, true)).arg(avdecc::helper::toUpperCamelCase(static_cast<std::string>(acmpStatus))));
		}
		else
		{
			_acmpStatus->setText(1, "No Value");
		}
	}
}

void StreamDynamicTreeWidgetItem::updateConnections(la::avdecc::controller::model::StreamConnections const& connections)
{
	_connections->clear();

	auto const streamConnection{ la::avdecc::entity::model::StreamIdentification{ _entityID, _streamIndex } };

	for (auto const& connection : connections)
	{
		auto const entityID = connection.entityID;
		auto const streamIndex = connection.streamIndex;

		auto* widget = new TalkerStreamConnectionWidget{ streamConnection, la::avdecc::entity::model::StreamIdentification{ entityID, streamIndex }, _connections };
		auto* item = new QListWidgetItem(_connections);
		item->setSizeHint(widget->sizeHint());

		_connections->setItemWidget(item, widget);
	}

	_connections->sortItems();
}
