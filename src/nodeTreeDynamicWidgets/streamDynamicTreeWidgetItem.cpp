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

#include "streamDynamicTreeWidgetItem.hpp"
#include "streamFormatComboBox.hpp"
#include "talkerStreamConnectionWidget.hpp"
#include "nodeTreeWidget.hpp"

#include <hive/modelsLibrary/helper.hpp>
#include <QtMate/material/color.hpp>

#include <QMenu>
#include <QMessageBox>

static inline void setNoValue(QTreeWidgetItem* const widget)
{
	widget->setText(1, "No Value");
	widget->setForeground(0, qtMate::material::color::disabledForegroundColor());
	widget->setForeground(1, qtMate::material::color::disabledForegroundColor());
}

StreamDynamicTreeWidgetItem::StreamDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamNodeStaticModel const* const staticModel, la::avdecc::entity::model::StreamInputNodeDynamicModel const* const inputDynamicModel, la::avdecc::entity::model::StreamOutputNodeDynamicModel const* const outputDynamicModel, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _streamType(streamType)
	, _streamIndex(streamIndex)
{
	setText(0, "Dynamic Info");

	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto listenerEntity = manager.getControlledEntity(entityID);

	auto* currentFormatItem = new QTreeWidgetItem(this);
	currentFormatItem->setText(0, "Stream Format");

	la::avdecc::entity::model::StreamNodeDynamicModel const* const dynamicModel = inputDynamicModel ? static_cast<decltype(dynamicModel)>(inputDynamicModel) : static_cast<decltype(dynamicModel)>(outputDynamicModel);

	auto* formatComboBox = new StreamFormatComboBox{};
	formatComboBox->setStreamFormats(staticModel->formats);
	parent->setItemWidget(currentFormatItem, 1, formatComboBox);

	// Send changes
	formatComboBox->setDataChangedHandler(
		[this, parent, formatComboBox](auto const& previousStreamFormat, auto const& newStreamFormat)
		{
			if (_streamType == la::avdecc::entity::model::DescriptorType::StreamInput)
			{
				avdecc::helper::smartChangeInputStreamFormat(parent, false, _entityID, _streamIndex, newStreamFormat, formatComboBox,
					[this, parent, formatComboBox, previousStreamFormat](hive::modelsLibrary::CommandsExecutor::ExecutorResult const result)
					{
						if (result.getResult() != hive::modelsLibrary::CommandsExecutor::ExecutorResult::Result::Success)
						{
							formatComboBox->setCurrentStreamFormat(previousStreamFormat);
						}
					});
			}
			else if (_streamType == la::avdecc::entity::model::DescriptorType::StreamOutput)
			{
				hive::modelsLibrary::ControllerManager::getInstance().setStreamOutputFormat(_entityID, _streamIndex, newStreamFormat, formatComboBox->getBeginCommandHandler(hive::modelsLibrary::ControllerManager::AecpCommandType::SetStreamFormat), formatComboBox->getResultHandler(hive::modelsLibrary::ControllerManager::AecpCommandType::SetStreamFormat, previousStreamFormat));
			}
		});

	// Listen for changes
	connect(&manager, &hive::modelsLibrary::ControllerManager::streamFormatChanged, formatComboBox,
		[this, formatComboBox](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
		{
			if (entityID == _entityID && descriptorType == _streamType && streamIndex == _streamIndex)
			{
				formatComboBox->setCurrentStreamFormat(streamFormat);
			}
		});

	// Update now
	formatComboBox->setCurrentStreamFormat(dynamicModel->streamFormat);

	//

	{
		// Create fields
		auto const createField = [this](auto const& label)
		{
			auto* const widget = new QTreeWidgetItem(this);
			widget->setText(0, label);
			widget->setText(1, "No Value");
			widget->setForeground(0, QColor{ qtMate::material::color::disabledForegroundColor() });
			widget->setForeground(1, QColor{ qtMate::material::color::disabledForegroundColor() });
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
		connect(&manager, &hive::modelsLibrary::ControllerManager::streamFormatChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
			{
				if (entityID == _entityID && descriptorType == _streamType && streamIndex == _streamIndex)
				{
					updateStreamFormat(streamFormat);
				}
			});
		connect(&manager, &hive::modelsLibrary::ControllerManager::streamRunningChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning)
			{
				if (entityID == _entityID && descriptorType == _streamType && streamIndex == _streamIndex)
				{
					updateStreamIsRunning(isRunning);
				}
			});
		connect(&manager, &hive::modelsLibrary::ControllerManager::streamDynamicInfoChanged, this,
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
		_connectionStateWidget = new ListenerStreamConnectionWidget({ _entityID, _streamIndex }, inputDynamicModel->connectionInfo, parent);
		parent->setItemWidget(_connectionState, 1, _connectionStateWidget);
		connect(parent, &QTreeWidget::itemSelectionChanged, this,
			[this, parent]()
			{
				// Update the selection state of the widget
				_connectionStateWidget->selectionChanged(_connectionState->isSelected());
			});
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
		connect(&manager, &hive::modelsLibrary::ControllerManager::streamOutputConnectionsChanged, this,
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
	auto const color = qtMate::material::color::foregroundColor();
	_streamFormat->setForeground(0, color);
	_streamFormat->setForeground(1, color);
	_streamFormat->setText(1, hive::modelsLibrary::helper::toHexQString(streamFormat.getValue(), true, true));
}

void StreamDynamicTreeWidgetItem::updateStreamIsRunning(bool const isRunning)
{
	auto const color = qtMate::material::color::foregroundColor();
	_streamWait->setForeground(0, color);
	_streamWait->setForeground(1, color);
	_streamWait->setText(1, isRunning ? "No" : "Yes");
}

void StreamDynamicTreeWidgetItem::updateStreamDynamicInfo(la::avdecc::entity::model::StreamDynamicInfo const& streamDynamicInfo)
{
	auto const setBoolValue = [](auto* const widget, bool const value)
	{
		auto const color = qtMate::material::color::foregroundColor();
		widget->setForeground(0, color);
		widget->setForeground(1, color);
		widget->setText(1, value ? "Yes" : "No");
	};

	auto const setStringValue = [](auto* const widget, QString const value)
	{
		auto const color = qtMate::material::color::foregroundColor();
		widget->setForeground(0, color);
		widget->setForeground(1, color);
		widget->setText(1, value);
	};

	setBoolValue(_isClassB, streamDynamicInfo.isClassB);
	setBoolValue(_hasSavedState, streamDynamicInfo.hasSavedState);
	setBoolValue(_doesSupportEncrypted, streamDynamicInfo.doesSupportEncrypted);
	setBoolValue(_arePdusEncrypted, streamDynamicInfo.arePdusEncrypted);
	setBoolValue(_hasTalkerFailed, streamDynamicInfo.hasTalkerFailed);
	auto const color = qtMate::material::color::foregroundColor();
	_streamFlags->setForeground(0, color);
	_streamFlags->setForeground(1, color);
	setFlagsItemText(_streamFlags, la::avdecc::utils::forceNumeric(streamDynamicInfo._streamInfoFlags.value()), avdecc::helper::flagsToString(streamDynamicInfo._streamInfoFlags));

	if (streamDynamicInfo.streamID)
	{
		setStringValue(_streamID, hive::modelsLibrary::helper::uniqueIdentifierToString(*streamDynamicInfo.streamID));
	}
	else
	{
		setNoValue(_streamID);
	}
	if (streamDynamicInfo.streamDestMac)
	{
		setStringValue(_streamDestMac, QString::fromStdString(la::networkInterface::NetworkInterfaceHelper::macAddressToString(*streamDynamicInfo.streamDestMac, true)));
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
		setStringValue(_msrpFailureCode, QString("%1 (%2)").arg(hive::modelsLibrary::helper::toHexQString(la::avdecc::utils::to_integral(*streamDynamicInfo.msrpFailureCode), true, true)).arg(avdecc::helper::msrpFailureCodeToString(*streamDynamicInfo.msrpFailureCode)));
		setStringValue(_msrpFailureBridgeID, hive::modelsLibrary::helper::toHexQString(*streamDynamicInfo.msrpFailureBridgeID, true, true));
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
		_streamFlagsEx->setForeground(0, qtMate::material::color::foregroundColor());
		_streamFlagsEx->setForeground(1, qtMate::material::color::foregroundColor());
		setFlagsItemText(_streamFlagsEx, la::avdecc::utils::forceNumeric(flagsEx.value()), avdecc::helper::flagsToString(flagsEx));
	}
	else
	{
		setNoValue(_streamFlagsEx);
	}
	if (streamDynamicInfo.probingStatus)
	{
		auto const probingStatus = *streamDynamicInfo.probingStatus;
		setStringValue(_probingStatus, QString("%1 (%2)").arg(hive::modelsLibrary::helper::toHexQString(la::avdecc::utils::to_integral(probingStatus), true, true)).arg(avdecc::helper::probingStatusToString(probingStatus)));
	}
	else
	{
		setNoValue(_probingStatus);
	}
	if (streamDynamicInfo.acmpStatus)
	{
		auto const acmpStatus = *streamDynamicInfo.acmpStatus;
		setStringValue(_acmpStatus, QString("%1 (%2)").arg(hive::modelsLibrary::helper::toHexQString(acmpStatus.getValue(), true, true)).arg(hive::modelsLibrary::helper::toUpperCamelCase(static_cast<std::string>(acmpStatus))));
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
