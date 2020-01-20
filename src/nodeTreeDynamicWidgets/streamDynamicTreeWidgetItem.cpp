/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

static inline void setNoValue(QTreeWidgetItem* const widget)
{
	widget->setText(1, "No Value");
	widget->setForeground(0, QColor{ Qt::gray });
	widget->setForeground(1, QColor{ Qt::gray });
}

StreamDynamicTreeWidgetItem::StreamDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamNodeStaticModel const* const staticModel, la::avdecc::entity::model::StreamInputNodeDynamicModel const* const inputDynamicModel, la::avdecc::entity::model::StreamOutputNodeDynamicModel const* const outputDynamicModel, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _streamType(streamType)
	, _streamIndex(streamIndex)
{
	setText(0, "Dynamic Info");

	auto& manager = avdecc::ControllerManager::getInstance();
	auto listenerEntity = manager.getControlledEntity(entityID);

	auto* currentFormatItem = new QTreeWidgetItem(this);
	currentFormatItem->setText(0, "Stream Format");

	la::avdecc::entity::model::StreamNodeDynamicModel const* const dynamicModel = inputDynamicModel ? static_cast<decltype(dynamicModel)>(inputDynamicModel) : static_cast<decltype(dynamicModel)>(outputDynamicModel);

	auto* formatComboBox = new StreamFormatComboBox{ _entityID };
	formatComboBox->setStreamFormats(staticModel->formats);
	formatComboBox->setCurrentStreamFormat(dynamicModel->streamFormat);

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
		auto const createField = [this](auto const& label)
		{
			auto* const widget = new QTreeWidgetItem(this);
			widget->setText(0, label);
			widget->setText(1, "No Value");
			widget->setForeground(0, QColor{ Qt::gray });
			widget->setForeground(1, QColor{ Qt::gray });
			return widget;
		};

		_streamFormat = createField("Current Stream Format");
		_streamWait = createField("Streaming Wait");
		_isClassB = createField("Class B");
		_hasSavedState = createField("Saved State");
		_doesSupportEncrypted = createField("Supports Encrypted");
		_arePdusEncrypted = createField("Encrypted PDUs");
		_hasTalkerFailed = createField("Talker Failed");
		_streamFlags = createField("Last Flags Received");
		_streamDestMac = createField("Stream Dest Address");
		_streamID = createField("Stream ID");
		_streamVlanID = createField("Stream Vlan ID");
		_msrpAccumulatedLatency = createField("MSRP Accumulated Latency");
		_msrpFailureCode = createField("MSRP Failure Code");
		_msrpFailureBridgeID = createField("MSRP Failure Bridge ID");
		_streamFlagsEx = createField("Stream Flags Ex");
		_probingStatus = createField("Probing Status");
		_acmpStatus = createField("Acmp Status");

		// Update info right now
		updateStreamFormat(dynamicModel->streamFormat);
		if (dynamicModel->isStreamRunning)
		{
			updateStreamIsRunning(*dynamicModel->isStreamRunning);
		}
		if (dynamicModel->streamDynamicInfo)
		{
			updateStreamDynamicInfo(*dynamicModel->streamDynamicInfo);
		}

		// Listen for events
		connect(&manager, &avdecc::ControllerManager::streamFormatChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
			{
				if (entityID == _entityID && descriptorType == _streamType && streamIndex == _streamIndex)
				{
					updateStreamFormat(streamFormat);
				}
			});
		connect(&manager, &avdecc::ControllerManager::streamRunningChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning)
			{
				if (entityID == _entityID && descriptorType == _streamType && streamIndex == _streamIndex)
				{
					updateStreamIsRunning(isRunning);
				}
			});
		connect(&manager, &avdecc::ControllerManager::streamDynamicInfoChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamDynamicInfo const& info)
			{
				if (entityID == _entityID && descriptorType == _streamType && streamIndex == _streamIndex)
				{
					updateStreamDynamicInfo(info);
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
		// Create fields
		auto* item = new QTreeWidgetItem(this);
		item->setText(0, "Connections");
		_connections = new QListWidget;
		_connections->setSelectionMode(QAbstractItemView::NoSelection);
		parent->setItemWidget(item, 1, _connections);

		// Update info right now
		updateConnections(outputDynamicModel->connections);

		// Listen for Connections changed signal
		connect(&manager, &avdecc::ControllerManager::streamConnectionsChanged, this,
			[this](la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamConnections const& connections)
			{
				if (stream.entityID == _entityID && stream.streamIndex == _streamIndex)
				{
					updateConnections(connections);
				}
			});
	}
}

void StreamDynamicTreeWidgetItem::updateStreamFormat(la::avdecc::entity::model::StreamFormat const& streamFormat)
{
	_streamFormat->setForeground(0, QColor{ Qt::black });
	_streamFormat->setForeground(1, QColor{ Qt::black });
	_streamFormat->setText(1, avdecc::helper::toHexQString(streamFormat.getValue(), true, true));
}

void StreamDynamicTreeWidgetItem::updateStreamIsRunning(bool const isRunning)
{
	_streamWait->setForeground(0, QColor{ Qt::black });
	_streamWait->setForeground(1, QColor{ Qt::black });
	_streamWait->setText(1, isRunning ? "No" : "Yes");
}

void StreamDynamicTreeWidgetItem::updateStreamDynamicInfo(la::avdecc::entity::model::StreamDynamicInfo const& streamDynamicInfo)
{
	auto const setBoolValue = [](auto* const widget, bool const value)
	{
		widget->setForeground(0, QColor{ Qt::black });
		widget->setForeground(1, QColor{ Qt::black });
		widget->setText(1, value ? "Yes" : "No");
	};

	auto const setStringValue = [](auto* const widget, QString const value)
	{
		widget->setForeground(0, QColor{ Qt::black });
		widget->setForeground(1, QColor{ Qt::black });
		widget->setText(1, value);
	};

	setBoolValue(_isClassB, streamDynamicInfo.isClassB);
	setBoolValue(_hasSavedState, streamDynamicInfo.hasSavedState);
	setBoolValue(_doesSupportEncrypted, streamDynamicInfo.doesSupportEncrypted);
	setBoolValue(_arePdusEncrypted, streamDynamicInfo.arePdusEncrypted);
	setBoolValue(_hasTalkerFailed, streamDynamicInfo.hasTalkerFailed);
	_streamFlags->setForeground(0, QColor{ Qt::black });
	_streamFlags->setForeground(1, QColor{ Qt::black });
	setFlagsItemText(_streamFlags, la::avdecc::utils::forceNumeric(streamDynamicInfo._streamInfoFlags.value()), avdecc::helper::flagsToString(streamDynamicInfo._streamInfoFlags));

	if (streamDynamicInfo.streamID)
	{
		setStringValue(_streamID, avdecc::helper::uniqueIdentifierToString(*streamDynamicInfo.streamID));
	}
	else
	{
		setNoValue(_streamID);
	}
	if (streamDynamicInfo.streamDestMac)
	{
		setStringValue(_streamDestMac, QString::fromStdString(la::avdecc::networkInterface::macAddressToString(*streamDynamicInfo.streamDestMac, true)));
	}
	else
	{
		setNoValue(_streamDestMac);
	}
	if (streamDynamicInfo.streamVlanID)
	{
		setStringValue(_streamVlanID, QString::number(*streamDynamicInfo.streamVlanID));
	}
	else
	{
		setNoValue(_streamVlanID);
	}
	if (streamDynamicInfo.msrpAccumulatedLatency)
	{
		setStringValue(_msrpAccumulatedLatency, QString::number(*streamDynamicInfo.msrpAccumulatedLatency));
	}
	else
	{
		setNoValue(_msrpAccumulatedLatency);
	}
	if (streamDynamicInfo.msrpFailureCode && streamDynamicInfo.msrpFailureBridgeID)
	{
		setStringValue(_msrpFailureCode, QString::number(*streamDynamicInfo.msrpFailureCode));
		setStringValue(_msrpFailureBridgeID, avdecc::helper::toHexQString(*streamDynamicInfo.msrpFailureBridgeID, true, true));
	}
	else
	{
		setNoValue(_msrpFailureCode);
		setNoValue(_msrpFailureBridgeID);
	}
	// Milan extension information
	if (streamDynamicInfo.streamInfoFlagsEx)
	{
		auto const flagsEx = *streamDynamicInfo.streamInfoFlagsEx;
		_streamFlagsEx->setForeground(0, QColor{ Qt::black });
		_streamFlagsEx->setForeground(1, QColor{ Qt::black });
		setFlagsItemText(_streamFlagsEx, la::avdecc::utils::forceNumeric(flagsEx.value()), avdecc::helper::flagsToString(flagsEx));
	}
	else
	{
		setNoValue(_streamFlagsEx);
	}
	if (streamDynamicInfo.probingStatus)
	{
		auto const probingStatus = *streamDynamicInfo.probingStatus;
		setStringValue(_probingStatus, QString("%1 (%2)").arg(avdecc::helper::toHexQString(la::avdecc::utils::to_integral(probingStatus), true, true)).arg(avdecc::helper::probingStatusToString(probingStatus)));
	}
	else
	{
		setNoValue(_probingStatus);
	}
	if (streamDynamicInfo.acmpStatus)
	{
		auto const acmpStatus = *streamDynamicInfo.acmpStatus;
		setStringValue(_acmpStatus, QString("%1 (%2)").arg(avdecc::helper::toHexQString(acmpStatus.getValue(), true, true)).arg(avdecc::helper::toUpperCamelCase(static_cast<std::string>(acmpStatus))));
	}
	else
	{
		setNoValue(_acmpStatus);
	}
}

void StreamDynamicTreeWidgetItem::updateConnections(la::avdecc::entity::model::StreamConnections const& connections)
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
