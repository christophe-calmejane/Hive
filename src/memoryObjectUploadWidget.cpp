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

#include "MemoryObjectUploadWidget.hpp"
#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include "avdecc/controllerManager.hpp"
#include <QLabel>
#include <QDebug>
#include <QFileDialog>


static void startUploadOperationHandler(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::entity::model::OperationID operationId)
{
	if (entity != nullptr)
	{
		qDebug() << __FUNCTION__ << "(): " << la::avdecc::toHexString(entity->getEntity().getEntityID()).c_str() << " finished with " << la::avdecc::entity::ControllerEntity::statusToString(status).c_str();
	}
	else
	{
		qDebug() << __FUNCTION__ << "(): unknown entity finished with " << la::avdecc::entity::ControllerEntity::statusToString(status).c_str();
	}
	
}

static void startStoreAndEraseOperationHandler(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::entity::model::OperationID /* operationId */)
{
	if (entity != nullptr)
	{
		qDebug() << __FUNCTION__ << "(): " << la::avdecc::toHexString(entity->getEntity().getEntityID()).c_str() << " finished with " << la::avdecc::entity::ControllerEntity::statusToString(status).c_str();
	}
	else
	{
		qDebug() << __FUNCTION__ << "(): unknown entity finished with " << la::avdecc::entity::ControllerEntity::statusToString(status).c_str();
	}

}

static void writeMemoryHandler(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AaCommandStatus const status)
{
	qDebug() << __FUNCTION__ << "(): " << la::avdecc::toHexString(entity->getEntity().getEntityID()).c_str() << " finished with " << la::avdecc::entity::ControllerEntity::statusToString(status).c_str();

	if (status == la::avdecc::entity::ControllerEntity::AaCommandStatus::Success)
	{
		auto& manager = avdecc::ControllerManager::getInstance();

		manager.startStoreAndRebootMemoryObjectOperation(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::MemoryObject, 0, startStoreAndEraseOperationHandler);
	}
}

MemoryObjectUploadWidget::MemoryObjectUploadWidget(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, std::uint64_t const address, QWidget *parent) :
	_targetEntityID(targetEntityID), _descriptorIndex(descriptorIndex), _address(address), QWidget(parent)
{
	_uploadMainLayout = new QHBoxLayout(parent);
	_stateLayout = new QVBoxLayout(parent);
	_progressLayout = new QVBoxLayout(parent);
	_fileLayout = new QVBoxLayout(parent);
	_actionLayout = new QVBoxLayout(parent);

	_stateLabel = new QLabel("State", parent);
	_currentUploadState = new QLabel("Inactive", parent);

	_stateLayout->addWidget(_stateLabel);
	_stateLayout->addWidget(_currentUploadState);

	_progressLabel = new QLabel("Progress", parent);
	_progressBar = new QProgressBar(parent);
	_progressBar->setMinimum(0);
	_progressBar->setMaximum(1000);
	_progressLayout->addWidget(_progressLabel);
	_progressLayout->addWidget(_progressBar);

	_fileLabel = new QLabel("File", parent);
	_fileHLayout = new QHBoxLayout();
	_filePathLabel = new QLabel("-");
	_filePathLabel->setMinimumWidth(100);
	
	_fileHLayout->addWidget(_filePathLabel);
	_fileHLayout->addWidget(&_fileSelect);

	_fileLayout->addWidget(_fileLabel);
	_fileLayout->addLayout(_fileHLayout);

	_actionStartButton.setDisabled(true);
	_actionAbortButton.setDisabled(true);

	_actionLayout->addWidget(&_actionStartButton);
	_actionLayout->addWidget(&_actionAbortButton);

	_uploadMainLayout->addLayout(_stateLayout);
	_uploadMainLayout->addLayout(_progressLayout);
	_uploadMainLayout->addLayout(_fileLayout);
	_uploadMainLayout->addLayout(_actionLayout);

	connect(&_fileSelect, &QPushButton::clicked, this, &MemoryObjectUploadWidget::fileSelectClicked);
	connect(&_actionStartButton, &QPushButton::clicked, this, &MemoryObjectUploadWidget::uploadClicked);
	connect(&_actionAbortButton, &QPushButton::clicked, this, &MemoryObjectUploadWidget::abortClicked);

	auto& controllerManager = avdecc::ControllerManager::getInstance();

	qRegisterMetaType<std::uint16_t>("std::uint16_t");

	connect(&controllerManager, &avdecc::ControllerManager::operationStatus, this, &MemoryObjectUploadWidget::progressUpdate);

	setLayout(_uploadMainLayout);
}

MemoryObjectUploadWidget::~MemoryObjectUploadWidget()
{

}

void MemoryObjectUploadWidget::fileSelectClicked()
{
	QString selectedUploadFile;
	QFileInfo fileInfo;
	
	selectedUploadFile = QFileDialog::getOpenFileName(this, "Open Upload File", _selectedFile, "All types (*.*)");

	if (selectedUploadFile.length() > 0)
	{
		fileInfo.setFile(selectedUploadFile);

		_selectedFile = selectedUploadFile;
		_filePathLabel->setText(fileInfo.fileName());

		_actionStartButton.setDisabled(false);
	}
}

void MemoryObjectUploadWidget::abortClicked()
{
	// TODO: necessary?
}

void MemoryObjectUploadWidget::uploadClicked()
{
	auto& manager = avdecc::ControllerManager::getInstance();
	QFile uploadFile(_selectedFile);

	if (uploadFile.open(QIODevice::ReadOnly))
	{
		QByteArray fileData = uploadFile.readAll();

		_progressBar->reset();
		_currentUploadState->setText("Upload");

		la::avdecc::controller::Controller::DeviceMemoryBuffer memoryBuffer(fileData.constData(), fileData.count());

		// TODO: should we store the operation id which will be returned by START_OPERATION
		manager.startUploadMemoryObjectOperation(_targetEntityID, la::avdecc::entity::model::DescriptorType::MemoryObject, _descriptorIndex, fileData.count(), startUploadOperationHandler);

		manager.writeDeviceMemory(_targetEntityID, _address, memoryBuffer, writeMemoryHandler);
	}
}

void MemoryObjectUploadWidget::progressUpdate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType descriptorType, la::avdecc::entity::model::DescriptorIndex descriptorIndex, std::uint16_t operationId, std::uint16_t percentComplete)
{
	if (_targetEntityID == targetEntityID
		&& la::avdecc::entity::model::DescriptorType::MemoryObject == descriptorType
		&& _descriptorIndex == descriptorIndex)
		// TODO /* check the operation id? */
	{
		if (_progressBar->value() > percentComplete)
		{
			_currentUploadState->setText("Storing");
		}

		_progressBar->setValue(percentComplete);
	}
}
