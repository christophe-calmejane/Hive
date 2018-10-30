/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "deviceDetailsDialog.hpp"

#include <QCoreApplication>
#include <QLayout>

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

#include "ui_deviceDetailsDialog.h"
#include "deviceDetailsChannelTableModel.hpp"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/channelConnectionManager.hpp"
#include "entityLogoCache.hpp"

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
	la::avdecc::UniqueIdentifier _entityID;
	la::avdecc::entity::model::DescriptorIndex _activeConfigurationIndex, _previousConfigurationIndex;
	QMap<QWidget*, bool> _hasChangesMap;
	bool _applyRequested;
	int _expectedChanges;
	int _gottenChanges;

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

		_applyRequested = false;
		_activeConfigurationIndex = std::numeric_limits<la::avdecc::entity::model::DescriptorIndex>::max();
		_previousConfigurationIndex = std::numeric_limits<la::avdecc::entity::model::DescriptorIndex>::max();

		// setup the table view data models.
		_deviceDetailsChannelTableModelReceive.registerUiWidget(tableViewReceive);
		_deviceDetailsChannelTableModelTransmit.registerUiWidget(tableViewTransmit);
		tableViewReceive->setModel(&_deviceDetailsChannelTableModelReceive);
		tableViewTransmit->setModel(&_deviceDetailsChannelTableModelTransmit);

		connect(lineEditDeviceName, &QLineEdit::textChanged, this, &DeviceDetailsDialogImpl::lineEditDeviceNameChanged);

		connect(lineEditGroupName, &QLineEdit::textChanged, this, &DeviceDetailsDialogImpl::lineEditGroupNameChanged);

		connect(comboBoxConfiguration, &QComboBox::currentTextChanged, this, &DeviceDetailsDialogImpl::comboBoxConfigurationChanged);

		connect(pushButtonApplyChanges, &QPushButton::clicked, this, &DeviceDetailsDialogImpl::applyChanges);

		connect(pushButtonRevertChanges, &QPushButton::clicked, this, &DeviceDetailsDialogImpl::revertChanges);

		auto& manager = avdecc::ControllerManager::getInstance();

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::endAecpCommand, this, &DeviceDetailsDialogImpl::onEndAecpCommand);

		// register for changes, to update the data live in the dialog, except the user edited it already:
		//connect(&manager, &avdecc::ControllerManager::entityNameChanged, this, &DeviceDetailsDialogImpl::entityNameChanged);
		//connect(&manager, &avdecc::ControllerManager::entityGroupNameChanged, this, &DeviceDetailsDialogImpl::entityGroupNameChanged);
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

		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);

		if (!leaveOutGeneralData)
		{
			// get the device name into the line edit
			lineEditDeviceName->setText(avdecc::helper::entityName(*controlledEntity));
			setModifiedStyleOnWidget(lineEditDeviceName, false);

			// get the group name into the line edit
			lineEditGroupName->setText(avdecc::helper::groupName(*controlledEntity));
			setModifiedStyleOnWidget(lineEditGroupName, false);

			_previousConfigurationIndex = controlledEntity->getCurrentConfigurationNode().descriptorIndex;
		}

		{
			const QSignalBlocker blocker(comboBoxConfiguration);
			comboBoxConfiguration->clear();
		}

		if (controlledEntity)
		{
			// invokes various visit methods.
			controlledEntity->accept(this);
			comboBoxConfiguration->setCurrentIndex(_activeConfigurationIndex);
		}

		tableViewReceive->resizeColumnsToContents();
		tableViewReceive->resizeRowsToContents();
		tableViewTransmit->resizeColumnsToContents();
		tableViewTransmit->resizeRowsToContents();
	}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::EntityNode const& node) noexcept override {}

	/**
	* Get every configuration. Set the active configuration if it wasn't set before already.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::ConfigurationNode const& node) noexcept override
	{
		{
			const QSignalBlocker blocker(comboBoxConfiguration);
			comboBoxConfiguration->addItem(node.dynamicModel->objectName.data(), node.descriptorIndex);

			// relevant Node:
			if (node.dynamicModel->isActiveConfiguration && _activeConfigurationIndex == std::numeric_limits<la::avdecc::entity::model::DescriptorIndex>::max())
			{
				_activeConfigurationIndex = node.descriptorIndex;
			}
		}
	}

	/**
	* Add every transmit and receive node into the table.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::AudioUnitNode const& node) noexcept override
	{
		auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
		for (auto const& inputPair : node.streamPortInputs)
		{
			for (auto const& inputAudioCluster : inputPair.second.audioClusters)
			{
				for (std::uint16_t channelIndex = 0u; channelIndex < inputAudioCluster.second.staticModel->channelCount; channelIndex++)
				{
					const avdecc::ConnectionInformation connectionInformation = channelConnectionManager.getChannelConnectionsReverse(_entityID, _activeConfigurationIndex, ((la::avdecc::controller::model::AudioUnitNode const* const)parent)->descriptorIndex, inputPair.first, inputAudioCluster.first, inputPair.second.staticModel->baseCluster, channelIndex);

					_deviceDetailsChannelTableModelReceive.addNode(inputAudioCluster.second, channelIndex, connectionInformation);
				}
			}
		}

		for (auto const& outputPair : node.streamPortOutputs)
		{
			for (auto const& outputAudioCluster : outputPair.second.audioClusters)
			{
				for (std::uint16_t channelIndex = 0u; channelIndex < outputAudioCluster.second.staticModel->channelCount; channelIndex++)
				{
					const avdecc::ConnectionInformation connectionInformation = channelConnectionManager.getChannelConnections(_entityID, _activeConfigurationIndex, ((la::avdecc::controller::model::AudioUnitNode const* const)parent)->descriptorIndex, outputPair.first, outputAudioCluster.first, outputPair.second.staticModel->baseCluster, channelIndex);

					_deviceDetailsChannelTableModelTransmit.addNode(outputAudioCluster.second, channelIndex, connectionInformation);
				}
			}
		}
	}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::StreamInputNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::StreamOutputNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::AvbInterfaceNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::ClockSourceNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::LocaleNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::StringsNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::StreamPortNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::AudioClusterNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::AudioMapNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::ClockDomainNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::RedundantStreamNode const& node) noexcept override {}

	/**
	* Ignored.
	*/
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::MemoryObjectNode const& node) noexcept override {}

	/**
	* Invoked whenever the entity name gets changed in the model.
	* @param entityID   The id of the entity that got updated.
	* @param entityName The new name.
	*/
	Q_SLOT void DeviceDetailsDialogImpl::entityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityName)
	{
		if (_entityID == entityID && (!_hasChangesMap.contains(lineEditDeviceName) || !_hasChangesMap[lineEditDeviceName]))
		{
			lineEditDeviceName->setText(entityName);
		}
	}

	/**
	* Invoked whenever the entity group name gets changed in the model.
	* @param entityID		 The id of the entity that got updated.
	* @param entityGroupName The new group name.
	*/
	Q_SLOT void DeviceDetailsDialogImpl::entityGroupNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityGroupName)
	{
		if (_entityID == entityID && (!_hasChangesMap.contains(lineEditGroupName) || !_hasChangesMap[lineEditGroupName]))
		{
			lineEditGroupName->setText(entityGroupName);
		}
	}

	/**
	* Invoked whenever a new item is selected in the configuration combo box.
	* @param text The selected text.
	*/
	Q_SLOT void DeviceDetailsDialogImpl::comboBoxConfigurationChanged(QString text)
	{
		if (_activeConfigurationIndex != comboBoxConfiguration->currentData().toInt())
		{
			_activeConfigurationIndex = comboBoxConfiguration->currentData().toInt();

			_deviceDetailsChannelTableModelTransmit.resetChangedData();
			_deviceDetailsChannelTableModelTransmit.removeAllNodes();
			_deviceDetailsChannelTableModelReceive.resetChangedData();
			_deviceDetailsChannelTableModelReceive.removeAllNodes();

			// read out actual data again
			loadCurrentControlledEntity(_entityID, true);
		}
	}

	/**
	* Invoked after a command has been exectued. We use it to detect if all data that was changed has been written.
	* @param entityID		The id of the entity the command was executed on.
	* @param cmdType		The executed command type.
	* @param commandStatus  The status of the command. 
	*/
	Q_SLOT void DeviceDetailsDialogImpl::onEndAecpCommand(la::avdecc::UniqueIdentifier const entityID, avdecc::ControllerManager::AecpCommandType cmdType, la::avdecc::entity::ControllerEntity::AemCommandStatus const commandStatus)
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
	* Invoked whenever the entity name gets changed in the view.
	* @param entityName The new group name.
	*/
	Q_SLOT void DeviceDetailsDialogImpl::lineEditDeviceNameChanged(QString const& entityName)
	{
		this->setModifiedStyleOnWidget(lineEditDeviceName, true);
	}

	/**
	* Invoked whenever the entity group name gets changed in the view.
	* @param entityGroupName The new group name.
	*/
	Q_SLOT void DeviceDetailsDialogImpl::lineEditGroupNameChanged(QString const& entityGroupName)
	{
		this->setModifiedStyleOnWidget(lineEditGroupName, true);
	}

	/**
	* Invoked when the apply button is clicked.
	* Writes all data via the avdecc::ControllerManager. The avdecc::ControllerManager::endAecpCommand signal is used
	* to determine the state of the requested commands.
	*/
	Q_SLOT void DeviceDetailsDialogImpl::applyChanges()
	{
		_applyRequested = true;
		_expectedChanges = 0;
		_gottenChanges = 0;

		// set all data
		if (_hasChangesMap.contains(lineEditDeviceName) && _hasChangesMap[lineEditDeviceName])
		{
			avdecc::ControllerManager::getInstance().setEntityName(_entityID, lineEditDeviceName->text());
			_expectedChanges++;
		}
		if (_hasChangesMap.contains(lineEditGroupName) && _hasChangesMap[lineEditGroupName])
		{
			avdecc::ControllerManager::getInstance().setEntityGroupName(_entityID, lineEditGroupName->text());
			_expectedChanges++;
		}

		if (_previousConfigurationIndex != _activeConfigurationIndex)
		{
			avdecc::ControllerManager::getInstance().setConfiguration(_entityID, _activeConfigurationIndex);
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
					avdecc::ControllerManager::getInstance().setAudioClusterName(_entityID, _activeConfigurationIndex, e, rxChanges->value(f).toString());
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
					avdecc::ControllerManager::getInstance().setAudioClusterName(_entityID, _activeConfigurationIndex, e, txChanges->value(f).toString());
					_expectedChanges++;
				}
			}
		}
	}

	/**
	* Invoked when the cancel button is clicked. Reverts all changes in the dialog.
	*/
	Q_SLOT void DeviceDetailsDialogImpl::revertChanges()
	{
		_activeConfigurationIndex = -1;

		_deviceDetailsChannelTableModelTransmit.resetChangedData();
		_deviceDetailsChannelTableModelTransmit.removeAllNodes();
		_deviceDetailsChannelTableModelReceive.resetChangedData();
		_deviceDetailsChannelTableModelReceive.removeAllNodes();

		// read out actual data again
		loadCurrentControlledEntity(_entityID, false);
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
		if (modified)
		{
			widget->setStyleSheet("font-style: italic");
		}
		else
		{
			widget->setStyleSheet("");
		}
	}
};

/**
* Constructor.
*/
DeviceDetailsDialog::DeviceDetailsDialog(QWidget* parent)
	: QDialog(parent)
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
	_pImpl->loadCurrentControlledEntity(_controlledEntityID, false);
	auto& manager = avdecc::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(entityID);
	setWindowTitle(QCoreApplication::applicationName() + " - Device View - " + avdecc::helper::entityName(*controlledEntity));
}
//#include "deviceDetailsDialog.moc"
