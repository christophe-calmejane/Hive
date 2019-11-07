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

#include "deviceDetailsDialog.hpp"

#include <QCoreApplication>
#include <QLayout>
#include <QStringListModel>
#include <QMessageBox>

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

#include "ui_deviceDetailsDialog.h"
#include "deviceDetailsChannelTableModel.hpp"
#include "internals/config.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/channelConnectionManager.hpp"

// **************************************************************
// class DeviceDetailsDialogImpl
// **************************************************************
/**
* @brief    Internal implemenation of the ui.
* [@author  Marius Erlen]
* [@date    2018-09-21]
*
* Implements the EntityModelVisitor to get informed about every node in the given entity.
* Holds the state of the changes until the apply button is pressed, then uses the avdecc::ControllerManager class to
* write the changes to the device.
*/
class DeviceDetailsDialogImpl final : private Ui::DeviceDetailsDialog, public QObject, public la::avdecc::controller::model::EntityModelVisitor
{
private:
	::DeviceDetailsDialog* _dialog;
	la::avdecc::UniqueIdentifier _entityID;
	std::optional<la::avdecc::entity::model::DescriptorIndex> _activeConfigurationIndex = std::nullopt, _previousConfigurationIndex = std::nullopt;
	std::optional<uint32_t> _userSelectedLatency = std::nullopt;
	QMap<QWidget*, bool> _hasChangesMap;
	bool _applyRequested = false;
	int _expectedChanges = 0;
	int _gottenChanges = 0;
	bool _hasChangesByUser = false;

	DeviceDetailsChannelTableModel _deviceDetailsChannelTableModelReceive;
	DeviceDetailsChannelTableModel _deviceDetailsChannelTableModelTransmit;

public:
	/**
	* Constructor.
	* @param parent The parent class.
	*/
	DeviceDetailsDialogImpl(::DeviceDetailsDialog* parent)
	{
		// Link UI
		setupUi(parent);
		_dialog = parent;

		updateButtonStates();

		comboBox_PredefinedPT->addItem("0.25 ms", 250000);
		comboBox_PredefinedPT->addItem("0.5 ms", 500000);
		comboBox_PredefinedPT->addItem("1 ms", 1000000);
		comboBox_PredefinedPT->addItem("2 ms", 2000000);

		// setup the table view data models.
		tableViewReceive->setItemDelegateForColumn(static_cast<int>(DeviceDetailsChannelTableModelColumn::ConnectionStatus), new ConnectionStateItemDelegate());
		tableViewTransmit->setItemDelegateForColumn(static_cast<int>(DeviceDetailsChannelTableModelColumn::ConnectionStatus), new ConnectionStateItemDelegate());
		tableViewReceive->setItemDelegateForColumn(static_cast<int>(DeviceDetailsChannelTableModelColumn::Connection), new ConnectionInfoItemDelegate());
		tableViewTransmit->setItemDelegateForColumn(static_cast<int>(DeviceDetailsChannelTableModelColumn::Connection), new ConnectionInfoItemDelegate());
		tableViewReceive->setStyleSheet("QTableView::item {border: 0px; padding: 6px;} ");
		tableViewTransmit->setStyleSheet("QTableView::item {border: 0px; padding: 6px;} ");

		tableViewReceive->setModel(&_deviceDetailsChannelTableModelReceive);
		tableViewTransmit->setModel(&_deviceDetailsChannelTableModelTransmit);

		// disable row resize
		QHeaderView* verticalHeaderReceive = tableViewReceive->verticalHeader();
		verticalHeaderReceive->setSectionResizeMode(QHeaderView::Fixed);
		QHeaderView* verticalHeaderTransmit = tableViewTransmit->verticalHeader();
		verticalHeaderTransmit->setSectionResizeMode(QHeaderView::Fixed);

		connect(lineEditDeviceName, &QLineEdit::textChanged, this, &DeviceDetailsDialogImpl::lineEditDeviceNameChanged);
		connect(lineEditGroupName, &QLineEdit::textChanged, this, &DeviceDetailsDialogImpl::lineEditGroupNameChanged);
		connect(comboBoxConfiguration, &QComboBox::currentTextChanged, this, &DeviceDetailsDialogImpl::comboBoxConfigurationChanged);
		connect(comboBox_PredefinedPT, &QComboBox::currentTextChanged, this, &DeviceDetailsDialogImpl::comboBoxPredefinedPTChanged);
		connect(radioButton_PredefinedPT, &QRadioButton::clicked, this, &DeviceDetailsDialogImpl::radioButtonPredefinedPTClicked);

		connect(&_deviceDetailsChannelTableModelReceive, &DeviceDetailsChannelTableModel::dataEdited, this, &DeviceDetailsDialogImpl::tableDataChanged);
		connect(&_deviceDetailsChannelTableModelTransmit, &DeviceDetailsChannelTableModel::dataEdited, this, &DeviceDetailsDialogImpl::tableDataChanged);

		connect(pushButtonApplyChanges, &QPushButton::clicked, this, &DeviceDetailsDialogImpl::applyChanges);
		connect(pushButtonRevertChanges, &QPushButton::clicked, this, &DeviceDetailsDialogImpl::revertChanges);

		auto& manager = avdecc::ControllerManager::getInstance();
		auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();

		connect(&manager, &avdecc::ControllerManager::entityOffline, this, &DeviceDetailsDialogImpl::entityOffline);
		connect(&manager, &avdecc::ControllerManager::endAecpCommand, this, &DeviceDetailsDialogImpl::onEndAecpCommand);
		connect(&manager, &avdecc::ControllerManager::gptpChanged, this, &DeviceDetailsDialogImpl::gptpChanged);
		connect(&manager, &avdecc::ControllerManager::streamRunningChanged, this, &DeviceDetailsDialogImpl::streamRunningChanged);
		connect(&manager, &avdecc::ControllerManager::streamConnectionsChanged, this, &DeviceDetailsDialogImpl::streamConnectionsChanged);
		connect(&manager, &avdecc::ControllerManager::streamPortAudioMappingsChanged, this, &DeviceDetailsDialogImpl::streamPortAudioMappingsChanged);
		connect(&manager, &avdecc::ControllerManager::streamDynamicInfoChanged, this, &DeviceDetailsDialogImpl::streamDynamicInfoChanged);
		connect(&channelConnectionManager, &avdecc::ChannelConnectionManager::listenerChannelConnectionsUpdate, this, &DeviceDetailsDialogImpl::listenerChannelConnectionsUpdate);

		// register for changes, to update the data live in the dialog, except the user edited it already:
		connect(&manager, &avdecc::ControllerManager::entityNameChanged, this, &DeviceDetailsDialogImpl::entityNameChanged);
		connect(&manager, &avdecc::ControllerManager::entityGroupNameChanged, this, &DeviceDetailsDialogImpl::entityGroupNameChanged);
		connect(&manager, &avdecc::ControllerManager::audioClusterNameChanged, this, &DeviceDetailsDialogImpl::audioClusterNameChanged);
	}

	/**
	* Loads all data needed from an entity to display in this dialog.
	* @param entityID Id of the entity to load the data from.
	*/
	void loadCurrentControlledEntity(la::avdecc::UniqueIdentifier const entityID, bool leaveOutGeneralData)
	{
		if (!entityID)
			return;

		_entityID = entityID;
		_hasChangesByUser = false;
		_activeConfigurationIndex = std::nullopt;
		updateButtonStates();

		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);
		if (controlledEntity)
		{
			la::avdecc::controller::model::ConfigurationNode configurationNode;
			try
			{
				configurationNode = controlledEntity->getCurrentConfigurationNode();
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
				return;
			}
			_dialog->setWindowTitle(QCoreApplication::applicationName() + " - Device View - " + avdecc::helper::smartEntityName(*controlledEntity));

			if (!leaveOutGeneralData)
			{
				// get the device name into the line edit
				const QSignalBlocker blockerLineEditDeviceName(lineEditDeviceName);
				lineEditDeviceName->setText(avdecc::helper::entityName(*controlledEntity));
				setModifiedStyleOnWidget(lineEditDeviceName, false);

				// get the group name into the line edit
				const QSignalBlocker blockerLineEditGroupName(lineEditGroupName);
				lineEditGroupName->setText(avdecc::helper::groupName(*controlledEntity));
				setModifiedStyleOnWidget(lineEditGroupName, false);

				auto const& entityNode = controlledEntity->getEntityNode();

				auto const* const staticModel = entityNode.staticModel;
				auto const* const dynamicModel = entityNode.dynamicModel;

				labelEntityIdValue->setText(avdecc::helper::toHexQString(entityID.getValue(), true, true));
				if (staticModel)
				{
					labelVendorNameValue->setText(controlledEntity->getLocalizedString(staticModel->vendorNameString).data());
					labelModelNameValue->setText(controlledEntity->getLocalizedString(staticModel->modelNameString).data());
				}
				if (dynamicModel)
				{
					labelFirmwareVersionValue->setText(dynamicModel->firmwareVersion.data());
					labelSerialNumberValue->setText(dynamicModel->serialNumber.data());
				}

				_previousConfigurationIndex = configurationNode.descriptorIndex;
			}

			{
				const QSignalBlocker blocker(comboBoxConfiguration);
				comboBoxConfiguration->clear();
			}

			if (controlledEntity)
			{
				// invokes various visit methods.
				controlledEntity->accept(this, false);
				if (_activeConfigurationIndex)
				{
					comboBoxConfiguration->setCurrentIndex(*_activeConfigurationIndex);
				}

				auto pureListener = (!configurationNode.streamInputs.empty() && configurationNode.streamOutputs.empty());
				auto pureTalker = (configurationNode.streamInputs.empty() && !configurationNode.streamOutputs.empty());

				auto tabCnt = tabWidget->count();
				for (auto i = 0; i < tabCnt; ++i)
				{
					QWidget* w = tabWidget->widget(i);
					if (!w)
						continue;

					if (pureListener && (w->objectName() == "tabLatency" || w->objectName() == "tabTransmit"))
					{
						tabWidget->removeTab(i);
						i--;
					}
					else if (pureTalker && w->objectName() == "tabReceive")
					{
						tabWidget->removeTab(i);
						i--;
					}
				}

				if (!pureListener)
				{
					loadLatencyData();
				}
			}

			tableViewReceive->resizeColumnsToContents();
			tableViewReceive->resizeRowsToContents();
			tableViewTransmit->resizeColumnsToContents();
			tableViewTransmit->resizeRowsToContents();
		}
	}

	/**
	* Invoked when the apply button is clicked.
	* Writes all data via the avdecc::ControllerManager. The avdecc::ControllerManager::endAecpCommand signal is used
	* to determine the state of the requested commands.
	*/
	void applyChanges()
	{
		_hasChangesByUser = false;
		updateButtonStates();
		_applyRequested = true;
		_expectedChanges = 0;
		_gottenChanges = 0;

		auto& manager = avdecc::ControllerManager::getInstance();

		// set all data
		if (_hasChangesMap.contains(lineEditDeviceName) && _hasChangesMap[lineEditDeviceName])
		{
			manager.setEntityName(_entityID, lineEditDeviceName->text());
			_expectedChanges++;
		}
		if (_hasChangesMap.contains(lineEditGroupName) && _hasChangesMap[lineEditGroupName])
		{
			manager.setEntityGroupName(_entityID, lineEditGroupName->text());
			_expectedChanges++;
		}

		//iterate over the changes and write them via avdecc
		QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsChannelTableModelColumn, QVariant>*> changesReceive = _deviceDetailsChannelTableModelReceive.getChanges();
		for (auto e : changesReceive.keys())
		{
			auto rxChanges = changesReceive.value(e);
			for (auto f : rxChanges->keys())
			{
				if (f == DeviceDetailsChannelTableModelColumn::ChannelName)
				{
					manager.setAudioClusterName(_entityID, *_activeConfigurationIndex, e, rxChanges->value(f).toString());
					_expectedChanges++;
				}
			}
		}

		QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsChannelTableModelColumn, QVariant>*> changesTransmit = _deviceDetailsChannelTableModelTransmit.getChanges();
		for (auto e : changesTransmit.keys())
		{
			auto txChanges = changesTransmit.value(e);
			for (auto f : txChanges->keys())
			{
				if (f == DeviceDetailsChannelTableModelColumn::ChannelName)
				{
					manager.setAudioClusterName(_entityID, *_activeConfigurationIndex, e, txChanges->value(f).toString());
					_expectedChanges++;
				}
			}
		}

		// apply the new stream info (latency)
		if (_userSelectedLatency)
		{
			auto const controlledEntity = manager.getControlledEntity(_entityID);
			if (controlledEntity)
			{
				auto configurationNode = controlledEntity->getCurrentConfigurationNode();
				for (auto const& streamOutput : configurationNode.streamOutputs)
				{
					auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamOutput.second.dynamicModel->streamFormat);
					auto const streamType = streamFormatInfo->getType();
					if (streamType == la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference)
					{
						// skip clock stream
						continue;
					}
					auto const streamLatency = streamOutput.second.dynamicModel->streamDynamicInfo ? (*streamOutput.second.dynamicModel->streamDynamicInfo).msrpAccumulatedLatency : decltype(_userSelectedLatency){ std::nullopt };
					if (streamLatency != *_userSelectedLatency)
					{
						auto streamInfo = la::avdecc::entity::model::StreamInfo{};
						streamInfo.streamInfoFlags.set(la::avdecc::entity::StreamInfoFlag::MsrpAccLatValid);
						streamInfo.msrpAccumulatedLatency = *_userSelectedLatency;

						// TODO: All streams have to be stopped for this to work. So this needs a state machine / task sequence.
						// TODO: needs update of library:
						manager.setStreamOutputInfo(_entityID, streamOutput.first, streamInfo);
					}
				}
			}
		}

		// applying the new configuration shall be done as the last step, as it may change everything displayed.
		// TODO: All streams have to be stopped for this to function. So this needs a state machine / task sequence.
		if (_previousConfigurationIndex != _activeConfigurationIndex)
		{
			manager.setConfiguration(_entityID, *_activeConfigurationIndex); // this needs a handler to make it sequential.
			_expectedChanges++;
		}
	}

	/**
	* Invoked when the cancel button is clicked. Reverts all changes in the dialog.
	*/
	void revertChanges()
	{
		_hasChangesByUser = false;
		updateButtonStates();
		_activeConfigurationIndex = std::nullopt;
		_userSelectedLatency = std::nullopt;

		_deviceDetailsChannelTableModelTransmit.resetChangedData();
		_deviceDetailsChannelTableModelTransmit.removeAllNodes();
		_deviceDetailsChannelTableModelReceive.resetChangedData();
		_deviceDetailsChannelTableModelReceive.removeAllNodes();

		// read out actual data again
		loadCurrentControlledEntity(_entityID, false);
	}


	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::EntityNode const& /*node*/) noexcept override {}

	/**
	* Get every configuration. Set the active configuration if it wasn't set before already.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::EntityNode const* const /*parent*/, la::avdecc::controller::model::ConfigurationNode const& node) noexcept override
	{
		QSignalBlocker const blocker(comboBoxConfiguration);
		comboBoxConfiguration->addItem(avdecc::helper::configurationName(controlledEntity, node), node.descriptorIndex);

		// relevant Node:
		if (node.dynamicModel->isActiveConfiguration && !_activeConfigurationIndex)
		{
			_activeConfigurationIndex = node.descriptorIndex;
		}
	}

	/**
	* Add every transmit and receive node into the table.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::AudioUnitNode const& node) noexcept override
	{
		if (!controlledEntity)
		{
			return;
		}
		auto audioUnitIndex = node.descriptorIndex;

		auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
		for (auto const& streamPortInputKV : node.streamPortInputs)
		{
			for (auto const& inputAudioClusterKV : streamPortInputKV.second.audioClusters)
			{
				for (std::uint16_t channelIndex = 0u; channelIndex < inputAudioClusterKV.second.staticModel->channelCount; channelIndex++)
				{
					auto sourceChannelIdentification = avdecc::ChannelIdentification{ *_previousConfigurationIndex, inputAudioClusterKV.first, channelIndex, avdecc::ChannelConnectionDirection::InputToOutput, audioUnitIndex, streamPortInputKV.first, streamPortInputKV.second.staticModel->baseCluster };
					auto connectionInformation = channelConnectionManager.getChannelConnectionsReverse(_entityID, sourceChannelIdentification);

					_deviceDetailsChannelTableModelReceive.addNode(connectionInformation);
				}
			}
		}

		for (auto const& streamPortOutputKV : node.streamPortOutputs)
		{
			for (auto const& outputAudioClusterKV : streamPortOutputKV.second.audioClusters)
			{
				for (std::uint16_t channelIndex = 0u; channelIndex < outputAudioClusterKV.second.staticModel->channelCount; channelIndex++)
				{
					auto sourceChannelIdentification = avdecc::ChannelIdentification{ *_previousConfigurationIndex, outputAudioClusterKV.first, channelIndex, avdecc::ChannelConnectionDirection::OutputToInput, audioUnitIndex, streamPortOutputKV.first, streamPortOutputKV.second.staticModel->baseCluster };
					auto connectionInformation = channelConnectionManager.getChannelConnections(_entityID, sourceChannelIdentification);

					_deviceDetailsChannelTableModelTransmit.addNode(connectionInformation);
				}
			}
		}
	}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::StreamInputNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::StreamOutputNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::AvbInterfaceNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::ClockSourceNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::LocaleNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*grandParent*/, la::avdecc::controller::model::LocaleNode const* const /*parent*/, la::avdecc::controller::model::StringsNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*grandParent*/, la::avdecc::controller::model::AudioUnitNode const* const /*parent*/, la::avdecc::controller::model::StreamPortNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*grandGrandParent*/, la::avdecc::controller::model::AudioUnitNode const* const /*grandParent*/, la::avdecc::controller::model::StreamPortNode const* const /*parent*/, la::avdecc::controller::model::AudioClusterNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*grandGrandParent*/, la::avdecc::controller::model::AudioUnitNode const* const /*grandParent*/, la::avdecc::controller::model::StreamPortNode const* const /*parent*/, la::avdecc::controller::model::AudioMapNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::ClockDomainNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::RedundantStreamNode const& /*node*/) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::MemoryObjectNode const& /*node*/) noexcept override {}


	// Slots

	/**
	* Invoked whenever the entity name gets changed in the model.
	* @param entityID   The id of the entity that got updated.
	* @param entityName The new name.
	*/
	void entityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityName)
	{
		if (_entityID == entityID && (!_hasChangesMap.contains(lineEditDeviceName) || !_hasChangesMap[lineEditDeviceName]))
		{
			QSignalBlocker blocker(*lineEditDeviceName); // block the changed signal for the below call.
			lineEditDeviceName->setText(entityName);
		}

		// update the window title
		_dialog->setWindowTitle(QCoreApplication::applicationName() + " - Device View - " + entityName);
	}

	/**
	* Invoked whenever the entity group name gets changed in the model.
	* @param entityID		 The id of the entity that got updated.
	* @param entityGroupName The new group name.
	*/
	void entityGroupNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityGroupName)
	{
		if (_entityID == entityID && (!_hasChangesMap.contains(lineEditGroupName) || !_hasChangesMap[lineEditGroupName]))
		{
			QSignalBlocker blocker(*lineEditGroupName); // block the changed signal for the below call.
			lineEditGroupName->setText(entityGroupName);
		}
	}

	/**
	* Invoked whenever the entity group name gets changed in the model.
	* @param entityID		 The id of the entity that got updated.
	* @param entityGroupName The new group name.
	*/
	void audioClusterNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& audioClusterName)
	{
		_deviceDetailsChannelTableModelReceive.updateAudioClusterName(entityID, configurationIndex, audioClusterIndex, audioClusterName);
		_deviceDetailsChannelTableModelTransmit.updateAudioClusterName(entityID, configurationIndex, audioClusterIndex, audioClusterName);
	}

	/**
	* Invoked whenever a new item is selected in the configuration combo box.
	* @param text The selected text.
	*/
	void comboBoxConfigurationChanged(QString const& /*text*/)
	{
		if (_activeConfigurationIndex != comboBoxConfiguration->currentData().toInt())
		{
			_activeConfigurationIndex = comboBoxConfiguration->currentData().toInt();

			_hasChangesByUser = true;
			updateButtonStates();
		}
	}

	/**
	* If the displayed entity goes offline, this dialog is closed automatically.
	*/
	void entityOffline(la::avdecc::UniqueIdentifier const entityID)
	{
		if (_entityID == entityID)
		{
			// close this dialog
			_dialog->close();
		}
	}

	/**
	* Invoked after a command has been exectued. We use it to detect if all data that was changed has been written.
	* @param entityID		The id of the entity the command was executed on.
	* @param cmdType		The executed command type.
	* @param commandStatus  The status of the command.
	*/
	void onEndAecpCommand(la::avdecc::UniqueIdentifier const entityID, avdecc::ControllerManager::AecpCommandType cmdType, la::avdecc::entity::ControllerEntity::AemCommandStatus const /*commandStatus*/)
	{
		// TODO propably show message when a command failed.
		if (entityID == _entityID)
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);

			switch (cmdType)
			{
				case avdecc::ControllerManager::AecpCommandType::SetEntityName:
				case avdecc::ControllerManager::AecpCommandType::SetEntityGroupName:
				case avdecc::ControllerManager::AecpCommandType::SetAudioClusterName:
					_gottenChanges++;
					break;
				default:
					break;
			}
		}
		if (_applyRequested && _gottenChanges >= _expectedChanges)
		{
			_applyRequested = false;
			revertChanges(); // read out current state after apply.
		}
	}

	/**
	* Updates the receive table model on changes.
	* @param channels  All channels of the devices that have changed (listener side only)
	*/
	void listenerChannelConnectionsUpdate(std::set<std::pair<la::avdecc::UniqueIdentifier, avdecc::ChannelIdentification>> const& channels)
	{
		_deviceDetailsChannelTableModelReceive.channelConnectionsUpdate(channels);

		tableViewReceive->resizeColumnsToContents();
		tableViewReceive->resizeRowsToContents();
		tableViewTransmit->resizeColumnsToContents();
		tableViewTransmit->resizeRowsToContents();
	}

	/**
	* Updates the table models on changes.
	*/
	void gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const /*avbInterfaceIndex*/, la::avdecc::UniqueIdentifier const /*grandMasterID*/, std::uint8_t const /*grandMasterDomain*/)
	{
		_deviceDetailsChannelTableModelReceive.channelConnectionsUpdate(entityID);
		_deviceDetailsChannelTableModelTransmit.channelConnectionsUpdate(entityID);

		tableViewReceive->resizeColumnsToContents();
		tableViewReceive->resizeRowsToContents();
		tableViewTransmit->resizeColumnsToContents();
		tableViewTransmit->resizeRowsToContents();
	}

	/**
	* Updates the table models on stream connection changes.
	*/
	void streamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const /*descriptorType*/, la::avdecc::entity::model::StreamIndex const /*streamIndex*/, bool const /*isRunning*/)
	{
		_deviceDetailsChannelTableModelReceive.channelConnectionsUpdate(entityID);
		_deviceDetailsChannelTableModelTransmit.channelConnectionsUpdate(entityID);

		tableViewReceive->resizeColumnsToContents();
		tableViewReceive->resizeRowsToContents();
		tableViewTransmit->resizeColumnsToContents();
		tableViewTransmit->resizeRowsToContents();
	}

	/**
	* Updates the latency tab data.
	*/
	void streamDynamicInfoChanged(la::avdecc::UniqueIdentifier const /*entityID*/, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const /*streamIndex*/, la::avdecc::entity::model::StreamDynamicInfo const /*streamDynamicInfo*/)
	{
		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
		{
			// update latency tab.
			loadLatencyData();
		}
	}

	/**
	* Updates the transmit table model on audio mapping changes.
	*/
	void streamPortAudioMappingsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamPortIndex const /*streamPortIndex*/)
	{
		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
		{
			_deviceDetailsChannelTableModelTransmit.channelConnectionsUpdate(entityID);

			tableViewReceive->resizeColumnsToContents();
			tableViewReceive->resizeRowsToContents();
			tableViewTransmit->resizeColumnsToContents();
			tableViewTransmit->resizeRowsToContents();
		}
	}

	/**
	* Updates the transmit table models on stream connection changes.
	*/
	void streamConnectionsChanged(la::avdecc::entity::model::StreamIdentification const& streamIdentification, la::avdecc::entity::model::StreamConnections const&)
	{
		_deviceDetailsChannelTableModelTransmit.channelConnectionsUpdate(streamIdentification.entityID);

		tableViewReceive->resizeColumnsToContents();
		tableViewReceive->resizeRowsToContents();
		tableViewTransmit->resizeColumnsToContents();
		tableViewTransmit->resizeRowsToContents();
	}

	/**
	* Invoked whenever the entity name gets changed in the view.
	*/
	void lineEditDeviceNameChanged(QString const& /*entityName*/)
	{
		setModifiedStyleOnWidget(lineEditDeviceName, true);
		_hasChangesByUser = true;
		updateButtonStates();
	}

	/**
	* Invoked whenever the entity group name gets changed in the view.
	* @param entityGroupName The new group name.
	*/
	void lineEditGroupNameChanged(QString const& /*entityGroupName*/)
	{
		setModifiedStyleOnWidget(lineEditGroupName, true);
		_hasChangesByUser = true;
		updateButtonStates();
	}

	/**
	* Invoked whenever the entity group name gets changed in the view.
	* @param entityGroupName The new group name.
	*/
	void comboBoxPredefinedPTChanged(QString const& /*text*/)
	{
		if (radioButton_PredefinedPT->isChecked() && (_userSelectedLatency != std::nullopt || *_userSelectedLatency != comboBox_PredefinedPT->currentData().toUInt()))
		{
			_userSelectedLatency = comboBox_PredefinedPT->currentData().toUInt();
			_hasChangesByUser = true;
			updateButtonStates();
		}
	}

	/**
	* Invoked whenever the entity group name gets changed in the view.
	* @param entityGroupName The new group name.
	*/
	void radioButtonPredefinedPTClicked(bool state)
	{
		if (state && (_userSelectedLatency != std::nullopt || *_userSelectedLatency != comboBox_PredefinedPT->currentData().toUInt()))
		{
			_userSelectedLatency = comboBox_PredefinedPT->currentData().toUInt();
			_hasChangesByUser = true;
			updateButtonStates();
		}
	}

	/**
	* Invoked whenever one of tables on the receive and transmit tabs is edited by the user.
	* @param entityGroupName The new group name.
	*/
	void tableDataChanged()
	{
		_hasChangesByUser = true;
		updateButtonStates();
	}


private:
	/**
	* Sets a flag in the _hasChangesMap map to indicate that this textbox has been edited.
	* Then sets the text style inside the widget to italic to indicate the change for the user.
	* If the parameter modified is set to false, the style is reset.
	* @param widget   The widget that was edited.
	* @param modified True to mark the given widget as modified.
	*/
	void setModifiedStyleOnWidget(QWidget* widget, bool modified)
	{
		if (!_hasChangesMap.contains(widget))
		{
			_hasChangesMap.insert(widget, false);
		}
		_hasChangesMap[widget] = modified;
	}

	void updateButtonStates()
	{
		pushButtonApplyChanges->setEnabled(_hasChangesByUser);
		pushButtonRevertChanges->setEnabled(_hasChangesByUser);
	}

	void loadLatencyData()
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(_entityID);
		if (controlledEntity)
		{
			la::avdecc::controller::model::ConfigurationNode configurationNode;
			try
			{
				configurationNode = controlledEntity->getCurrentConfigurationNode();
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
				return;
			}
			// latency tab data
			auto latency = decltype(la::avdecc::entity::model::StreamDynamicInfo::msrpAccumulatedLatency){ std::nullopt };
			for (auto const& streamOutput : configurationNode.streamOutputs)
			{
				auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamOutput.second.dynamicModel->streamFormat);
				auto const streamType = streamFormatInfo->getType();
				if (streamType == la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference)
				{
					// skip clock stream
					continue;
				}
				if (streamOutput.second.dynamicModel->streamDynamicInfo)
				{
					auto const streamLatency = (*streamOutput.second.dynamicModel->streamDynamicInfo).msrpAccumulatedLatency;
					if (latency != streamLatency && latency != std::nullopt)
					{
						// unequal values
						latency = std::nullopt;
						break;
					}
					latency = streamLatency;
				}
			}

			const QSignalBlocker blocker(comboBox_PredefinedPT);
			if (latency == std::nullopt)
			{
				comboBox_PredefinedPT->setCurrentIndex(0);
				lineEdit_CustomPT->setText("-");
				radioButton_CustomPT->setChecked(true);
			}
			else
			{
				int index = comboBox_PredefinedPT->findData(*latency);
				if (index != -1)
				{
					comboBox_PredefinedPT->setCurrentIndex(index);

					lineEdit_CustomPT->setText("-");
					radioButton_PredefinedPT->setChecked(true);
				}
				else
				{
					comboBox_PredefinedPT->setCurrentIndex(0);
					lineEdit_CustomPT->setText(QString::number((*latency / 1000000.0f)).append(" ms"));
					radioButton_CustomPT->setChecked(true);
				}
			}
		}
	}
};

/**
* Constructor.
*/
DeviceDetailsDialog::DeviceDetailsDialog(QWidget* parent)
	: QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
	, _pImpl(new DeviceDetailsDialogImpl(this))
{
}

/**
* Destructor. Deletes private implementation
*/
DeviceDetailsDialog::~DeviceDetailsDialog() noexcept
{
	delete _pImpl;
}

/**
* Sets the controlled entity id and loads the corresponding entity data.
* @param entityID The id of the entity to display and edit in the dialog.
*/
void DeviceDetailsDialog::setControlledEntityID(la::avdecc::UniqueIdentifier const entityID)
{
	if (_controlledEntityID == entityID)
	{
		return;
	}
	_controlledEntityID = entityID;

	auto& manager = avdecc::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(_controlledEntityID);

	_pImpl->loadCurrentControlledEntity(_controlledEntityID, false);
}
//#include "deviceDetailsDialog.moc"
