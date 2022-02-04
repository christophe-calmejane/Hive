/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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
#include <hive/modelsLibrary/helper.hpp>

#include "ui_deviceDetailsDialog.h"
#include "deviceDetailsChannelTableModel.hpp"
#include "deviceDetailsStreamFormatTableModel.hpp"
#include "internals/config.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/channelConnectionManager.hpp"
#include "avdecc/hiveLogItems.hpp"

// **************************************************************
// class DeviceDetailsDialogImpl
// **************************************************************
/**
* @brief    Internal implemenation of the ui.
* [@author  Marius Erlen]
* [@date    2018-09-21]
*
* Implements the EntityModelVisitor to get informed about every node in the given entity.
* Holds the state of the changes until the apply button is pressed, then uses the hive::modelsLibrary::ControllerManager class to
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
	DeviceDetailsStreamFormatTableModel _deviceDetailsInputStreamFormatTableModel;
	DeviceDetailsStreamFormatTableModel _deviceDetailsOutputStreamFormatTableModel;

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
		tableViewReceive->setItemDelegateForColumn(static_cast<int>(DeviceDetailsChannelTableModelColumn::Connection), new ConnectionInfoItemDelegate());
		tableViewReceive->setStyleSheet("QTableView::item {border: 0px; padding: 6px;} ");
		tableViewReceive->setModel(&_deviceDetailsChannelTableModelReceive);

		tableViewTransmit->setItemDelegateForColumn(static_cast<int>(DeviceDetailsChannelTableModelColumn::ConnectionStatus), new ConnectionStateItemDelegate());
		tableViewTransmit->setItemDelegateForColumn(static_cast<int>(DeviceDetailsChannelTableModelColumn::Connection), new ConnectionInfoItemDelegate());
		tableViewTransmit->setStyleSheet("QTableView::item {border: 0px; padding: 6px;} ");
		tableViewTransmit->setModel(&_deviceDetailsChannelTableModelTransmit);

		tableViewInputStreamFormat->setItemDelegateForColumn(static_cast<int>(DeviceDetailsStreamFormatTableModelColumn::StreamFormat), new StreamFormatItemDelegate(tableViewInputStreamFormat));
		tableViewInputStreamFormat->setStyleSheet("QTableView::item {border: 0px; padding: 6px;} ");
		tableViewInputStreamFormat->setModel(&_deviceDetailsInputStreamFormatTableModel);

		tableViewOutputStreamFormat->setItemDelegateForColumn(static_cast<int>(DeviceDetailsStreamFormatTableModelColumn::StreamFormat), new StreamFormatItemDelegate(tableViewOutputStreamFormat));
		tableViewOutputStreamFormat->setStyleSheet("QTableView::item {border: 0px; padding: 6px;} ");
		tableViewOutputStreamFormat->setModel(&_deviceDetailsOutputStreamFormatTableModel);

		// disable row resize
		QHeaderView* verticalHeaderReceive = tableViewReceive->verticalHeader();
		verticalHeaderReceive->setSectionResizeMode(QHeaderView::Fixed);
		QHeaderView* verticalHeaderTransmit = tableViewTransmit->verticalHeader();
		verticalHeaderTransmit->setSectionResizeMode(QHeaderView::Fixed);
		QHeaderView* verticalHeaderInputStreamFormat = tableViewInputStreamFormat->verticalHeader();
		verticalHeaderInputStreamFormat->setSectionResizeMode(QHeaderView::Fixed);
		QHeaderView* verticalHeaderOutputStreamFormat = tableViewOutputStreamFormat->verticalHeader();
		verticalHeaderOutputStreamFormat->setSectionResizeMode(QHeaderView::Fixed);

		connect(lineEditDeviceName, &QLineEdit::textChanged, this, &DeviceDetailsDialogImpl::lineEditDeviceNameChanged);
		connect(lineEditGroupName, &QLineEdit::textChanged, this, &DeviceDetailsDialogImpl::lineEditGroupNameChanged);
		connect(comboBoxConfiguration, &QComboBox::currentTextChanged, this, &DeviceDetailsDialogImpl::comboBoxConfigurationChanged);
		connect(comboBox_PredefinedPT, &QComboBox::currentTextChanged, this, &DeviceDetailsDialogImpl::comboBoxPredefinedPTChanged);
		connect(radioButton_PredefinedPT, &QRadioButton::clicked, this, &DeviceDetailsDialogImpl::radioButtonPredefinedPTClicked);

		connect(&_deviceDetailsChannelTableModelReceive, &DeviceDetailsChannelTableModel::dataEdited, this, &DeviceDetailsDialogImpl::tableDataChanged);
		connect(&_deviceDetailsChannelTableModelTransmit, &DeviceDetailsChannelTableModel::dataEdited, this, &DeviceDetailsDialogImpl::tableDataChanged);
		connect(&_deviceDetailsInputStreamFormatTableModel, &DeviceDetailsStreamFormatTableModel::dataEdited, this, &DeviceDetailsDialogImpl::tableDataChanged);
		connect(&_deviceDetailsOutputStreamFormatTableModel, &DeviceDetailsStreamFormatTableModel::dataEdited, this, &DeviceDetailsDialogImpl::tableDataChanged);

		connect(&_deviceDetailsChannelTableModelReceive, &DeviceDetailsChannelTableModel::dataChanged, this, [this](const QModelIndex& /*topLeft*/, const QModelIndex& /*bottomRight*/, const QVector<int>& /*roles*/)
			{
				if (tableViewReceive)
					tableViewReceive->viewport()->update();
			});
		connect(&_deviceDetailsChannelTableModelTransmit, &DeviceDetailsChannelTableModel::dataChanged, this, [this](const QModelIndex& /*topLeft*/, const QModelIndex& /*bottomRight*/, const QVector<int>& /*roles*/)
			{
				if (tableViewTransmit)
					tableViewTransmit->viewport()->update();
			});

		connect(pushButtonApplyChanges, &QPushButton::clicked, this, &DeviceDetailsDialogImpl::applyChanges);
		connect(pushButtonRevertChanges, &QPushButton::clicked, this, &DeviceDetailsDialogImpl::revertChanges);

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();

		connect(&manager, &hive::modelsLibrary::ControllerManager::entityOnline, this, &DeviceDetailsDialogImpl::entityOnline);
		connect(&manager, &hive::modelsLibrary::ControllerManager::entityOffline, this, &DeviceDetailsDialogImpl::entityOffline);
		connect(&manager, &hive::modelsLibrary::ControllerManager::endAecpCommand, this, &DeviceDetailsDialogImpl::onEndAecpCommand);
		connect(&manager, &hive::modelsLibrary::ControllerManager::gptpChanged, this, &DeviceDetailsDialogImpl::gptpChanged);
		connect(&manager, &hive::modelsLibrary::ControllerManager::streamRunningChanged, this, &DeviceDetailsDialogImpl::streamRunningChanged);
		connect(&manager, &hive::modelsLibrary::ControllerManager::streamOutputConnectionsChanged, this, &DeviceDetailsDialogImpl::streamOutputConnectionsChanged);
		connect(&manager, &hive::modelsLibrary::ControllerManager::streamPortAudioMappingsChanged, this, &DeviceDetailsDialogImpl::streamPortAudioMappingsChanged);
		connect(&manager, &hive::modelsLibrary::ControllerManager::streamDynamicInfoChanged, this, &DeviceDetailsDialogImpl::streamDynamicInfoChanged);
		connect(&channelConnectionManager, &avdecc::ChannelConnectionManager::listenerChannelConnectionsUpdate, this, &DeviceDetailsDialogImpl::listenerChannelConnectionsUpdate);

		// register for changes, to update the data live in the dialog, except the user edited it already:
		connect(&manager, &hive::modelsLibrary::ControllerManager::entityNameChanged, this, &DeviceDetailsDialogImpl::entityNameChanged);
		connect(&manager, &hive::modelsLibrary::ControllerManager::entityGroupNameChanged, this, &DeviceDetailsDialogImpl::entityGroupNameChanged);
		connect(&manager, &hive::modelsLibrary::ControllerManager::audioClusterNameChanged, this, &DeviceDetailsDialogImpl::audioClusterNameChanged);
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

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
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
			_dialog->setWindowTitle(QCoreApplication::applicationName() + " - Device View - " + hive::modelsLibrary::helper::smartEntityName(*controlledEntity));

			_deviceDetailsInputStreamFormatTableModel.setControlledEntityID(entityID);
			_deviceDetailsOutputStreamFormatTableModel.setControlledEntityID(entityID);

			if (!leaveOutGeneralData)
			{
				// get the device name into the line edit
				const QSignalBlocker blockerLineEditDeviceName(lineEditDeviceName);
				lineEditDeviceName->setText(hive::modelsLibrary::helper::entityName(*controlledEntity));
				setModifiedStyleOnWidget(lineEditDeviceName, false);

				// get the group name into the line edit
				const QSignalBlocker blockerLineEditGroupName(lineEditGroupName);
				lineEditGroupName->setText(hive::modelsLibrary::helper::groupName(*controlledEntity));
				setModifiedStyleOnWidget(lineEditGroupName, false);

				auto const& entityNode = controlledEntity->getEntityNode();

				auto const* const staticModel = entityNode.staticModel;
				auto const* const dynamicModel = entityNode.dynamicModel;

				labelEntityIdValue->setText(hive::modelsLibrary::helper::toHexQString(entityID.getValue(), true, true));
				if (staticModel)
				{
					labelVendorNameValue->setText(hive::modelsLibrary::helper::localizedString(*controlledEntity, staticModel->vendorNameString));
					labelModelNameValue->setText(hive::modelsLibrary::helper::localizedString(*controlledEntity, staticModel->modelNameString));
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
			tableViewInputStreamFormat->resizeColumnsToContents();
			tableViewInputStreamFormat->resizeRowsToContents();
			tableViewOutputStreamFormat->resizeColumnsToContents();
			tableViewOutputStreamFormat->resizeRowsToContents();
		}
	}

	/**
	* Invoked when the apply button is clicked.
	* Writes all data via the hive::modelsLibrary::ControllerManager. The hive::modelsLibrary::ControllerManager::endAecpCommand signal is used
	* to determine the state of the requested commands.
	*/
	void applyChanges()
	{
		_hasChangesByUser = false;
		updateButtonStates();
		_applyRequested = true;
		_expectedChanges = 0;
		_gottenChanges = 0;

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();

		try
		{
			// iterate over the stream format table changes, collect them into a list and write them via avdecc
			auto changedStreamFormats = QList<StreamFormatTableRowEntry>();
			// collect all STREAM_INPUT format changes
			for (auto& descriptorIdx : _deviceDetailsInputStreamFormatTableModel.getChanges().keys())
			{
				auto sfChanges = _deviceDetailsInputStreamFormatTableModel.getChanges().value(descriptorIdx);
				for (auto col : sfChanges->keys())
					if (col == DeviceDetailsStreamFormatTableModelColumn::StreamFormat && sfChanges->value(col).canConvert<StreamFormatTableRowEntry>())
						changedStreamFormats.append(sfChanges->value(col).value<StreamFormatTableRowEntry>());
			}
			// collect all STREAM_OUTPUT format changes
			for (auto& descriptorIdx : _deviceDetailsOutputStreamFormatTableModel.getChanges().keys())
			{
				auto sfChanges = _deviceDetailsOutputStreamFormatTableModel.getChanges().value(descriptorIdx);
				for (auto col : sfChanges->keys())
					if (col == DeviceDetailsStreamFormatTableModelColumn::StreamFormat && sfChanges->value(col).canConvert<StreamFormatTableRowEntry>())
						changedStreamFormats.append(sfChanges->value(col).value<StreamFormatTableRowEntry>());
			}
			// determin if any of the format changes is in conflict with currently set 'media clock domain' sampling rate and abort on user request
			for (auto const& streamFormatData : changedStreamFormats)
			{
				auto hasSampleRateConflict = false;

				// get the 'media clock master' entity to be able to access its sampling rate
				auto& clockConnectionManager = avdecc::mediaClock::MCDomainManager::getInstance();
				auto const& mediaClockMaster = clockConnectionManager.getMediaClockMaster(_entityID);
				auto controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(mediaClockMaster.first);

				if (controlledEntity)
				{
					try
					{
						// create a streamformatinfo to then derive the samplingrate for comparison purposes
						auto const& streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormatData.streamFormat);
						auto* audioUnitDynModel = controlledEntity->getAudioUnitNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex, 0).dynamicModel;
						if (streamFormatInfo && audioUnitDynModel)
						{
							// get the streamformat's corresponding sampling rate by first deriving the pull and base freq vals from streamformatinfo
							auto const& streamFormatSampleRate = streamFormatInfo->getSamplingRate();

							// get the 'media clock domain' sampling rate from the 'media clock master's entities audioUnit
							auto const& mediaClockMasterSampleRate = audioUnitDynModel->currentSamplingRate;

							// if the 'media clock master' audioUnit SR and the streamFormat SR do not match, we have found a conflict that the user needs to be warned about
							hasSampleRateConflict = (mediaClockMasterSampleRate != streamFormatSampleRate);
						}
					}
					catch (la::avdecc::controller::ControlledEntity::Exception const&)
					{
						// Ignore exception
					}
					catch (...)
					{
						// Uncaught exception
						AVDECC_ASSERT(false, "Uncaught exception");
					}
				}

				if (hasSampleRateConflict)
				{
					auto result = QMessageBox::warning(_dialog, "", "The selected stream formats are conflicting with the Media Clock Domain sample rate the device belongs to.\nContinue?", QMessageBox::StandardButton::Abort | QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Abort);
					if (result == QMessageBox::StandardButton::Abort)
					{
						revertChanges();
						return;
					}
				}
			}
			// apply the new stream formats
			for (auto const& streamFormatData : changedStreamFormats)
			{
				if (streamFormatData.streamType == la::avdecc::entity::model::DescriptorType::StreamInput)
				{
					manager.setStreamInputFormat(_entityID, streamFormatData.streamIndex, streamFormatData.streamFormat);
					_expectedChanges++;
				}
				else if (streamFormatData.streamType == la::avdecc::entity::model::DescriptorType::StreamOutput)
				{
					manager.setStreamOutputFormat(_entityID, streamFormatData.streamIndex, streamFormatData.streamFormat);
					_expectedChanges++;
				}
			}

			// apply new device name
			if (_hasChangesMap.contains(lineEditDeviceName) && _hasChangesMap[lineEditDeviceName])
			{
				manager.setEntityName(_entityID, lineEditDeviceName->text());
				_expectedChanges++;
			}

			// apply new group name
			if (_hasChangesMap.contains(lineEditGroupName) && _hasChangesMap[lineEditGroupName])
			{
				manager.setEntityGroupName(_entityID, lineEditGroupName->text());
				_expectedChanges++;
			}

			// iterate over the rx/tx channels table changes and write them via avdecc
			auto changesReceive = _deviceDetailsChannelTableModelReceive.getChanges();
			for (auto descriptorIdx : changesReceive.keys())
			{
				auto rxChanges = changesReceive.value(descriptorIdx);
				for (auto col : rxChanges->keys())
				{
					if (col == DeviceDetailsChannelTableModelColumn::ChannelName)
					{
						manager.setAudioClusterName(_entityID, *_activeConfigurationIndex, descriptorIdx, rxChanges->value(col).toString());
						_expectedChanges++;
					}
				}
			}

			auto changesTransmit = _deviceDetailsChannelTableModelTransmit.getChanges();
			for (auto descriptorIdx : changesTransmit.keys())
			{
				auto txChanges = changesTransmit.value(descriptorIdx);
				for (auto col : txChanges->keys())
				{
					if (col == DeviceDetailsChannelTableModelColumn::ChannelName)
					{
						manager.setAudioClusterName(_entityID, *_activeConfigurationIndex, descriptorIdx, txChanges->value(col).toString());
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
		catch (la::avdecc::controller::ControlledEntity::Exception const& e)
		{
			LOG_HIVE_ERROR(QString("%1 throws an exception (%2). Please dump Full Network State from File menu and send the file for support.").arg(__FUNCTION__).arg(e.what()));
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

		_deviceDetailsInputStreamFormatTableModel.resetChangedData();
		_deviceDetailsInputStreamFormatTableModel.removeAllNodes();
		_deviceDetailsOutputStreamFormatTableModel.resetChangedData();
		_deviceDetailsOutputStreamFormatTableModel.removeAllNodes();

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
		comboBoxConfiguration->addItem(hive::modelsLibrary::helper::configurationName(controlledEntity, node), node.descriptorIndex);

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

		auto configurationNode = controlledEntity->getCurrentConfigurationNode();
		for (auto const& streamInput : configurationNode.streamInputs)
		{
			_deviceDetailsInputStreamFormatTableModel.addNode(streamInput.first, streamInput.second.descriptorType, streamInput.second.dynamicModel->streamFormat);
		}
		for (auto i = 0; i < _deviceDetailsInputStreamFormatTableModel.rowCount(); i++)
		{
			auto modIdx = _deviceDetailsInputStreamFormatTableModel.index(i, static_cast<int>(DeviceDetailsStreamFormatTableModelColumn::StreamFormat), QModelIndex());
			tableViewInputStreamFormat->openPersistentEditor(modIdx);
		}
		for (auto const& streamOutput : configurationNode.streamOutputs)
		{
			_deviceDetailsOutputStreamFormatTableModel.addNode(streamOutput.first, streamOutput.second.descriptorType, streamOutput.second.dynamicModel->streamFormat);
		}
		for (auto i = 0; i < _deviceDetailsOutputStreamFormatTableModel.rowCount(); i++)
		{
			auto modIdx = _deviceDetailsOutputStreamFormatTableModel.index(i, static_cast<int>(DeviceDetailsStreamFormatTableModelColumn::StreamFormat), QModelIndex());
			tableViewOutputStreamFormat->openPersistentEditor(modIdx);
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
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::ControlNode const& /*node*/) noexcept override {}

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
	void entityOnline(la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const /*enumerationTime*/)
	{
		if (_entityID == entityID)
		{
			// when this dialog exists, the entity it refers to cannot come online unless something is bugged (dialog shall only exist for online entities)
		}
		else
		{
			_deviceDetailsChannelTableModelReceive.channelConnectionsUpdate(entityID);
			_deviceDetailsChannelTableModelTransmit.channelConnectionsUpdate(entityID);
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
		else
		{
			_deviceDetailsChannelTableModelReceive.channelConnectionsUpdate(entityID);
			_deviceDetailsChannelTableModelTransmit.channelConnectionsUpdate(entityID);
		}
	}

	/**
	* Invoked after a command has been exectued. We use it to detect if all data that was changed has been written.
	* @param entityID		The id of the entity the command was executed on.
	* @param cmdType		The executed command type.
	* @param commandStatus  The status of the command.
	*/
	void onEndAecpCommand(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType cmdType, la::avdecc::entity::model::DescriptorIndex /*descriptorIndex*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const /*commandStatus*/)
	{
		// TODO propably show message when a command failed.
		if (entityID == _entityID)
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);

			switch (cmdType)
			{
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetEntityName:
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetEntityGroupName:
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetAudioClusterName:
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetStreamFormat:
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

		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
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
	void streamOutputConnectionsChanged(la::avdecc::entity::model::StreamIdentification const& streamIdentification, la::avdecc::entity::model::StreamConnections const&)
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
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
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

	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(_controlledEntityID);

	_pImpl->loadCurrentControlledEntity(_controlledEntityID, false);
}
//#include "deviceDetailsDialog.moc"
