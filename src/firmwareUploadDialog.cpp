#include "firmwareUploadDialog.hpp"
#include "ui_firmwareUploadDialog.h"
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"
#include <la/avdecc/utils.hpp>

#include <QLabel>
#include <QProgressBar>
#include <QFileInfo>
#include <QVariant>
#include <QFile>
#include <QByteArray>
#include <QMessageBox>
#include <QCloseEvent>

enum class UpdateState
{
	Waiting = 0,
	StartUpload,
	Uploading,
	StartStore,
	Storing,
	Failed,
	Complete,
};

Q_DECLARE_METATYPE(UpdateState)

class UploadWidget : public QWidget
{
public:
	UploadWidget(QWidget* parent = nullptr)
	: QWidget{parent}
	{
		setProgress(0);
		_layout.addWidget(&_label);
		_layout.addWidget(&_progressBar);
	}
	
	void setText(QString const& text)
	{
		_label.setText(text);
	}
	
	void setProgress(int const progress)
	{
		// A negative percentComplete value means the progress is unknown but still continuing
		if (progress < 0)
		{
			_progressBar.setRange(0, 0);
			_progressBar.setValue(0);
		}
		else
		{
			auto const clampedProgress = std::max(0, std::min(100, progress));
			_progressBar.setRange(0, 100);
			_progressBar.setValue(clampedProgress);
		}
	}
	
private:
	QVBoxLayout _layout{this};
	QLabel _label;
	QProgressBar _progressBar;
};

FirmwareUploadDialog::FirmwareUploadDialog(la::avdecc::controller::Controller::DeviceMemoryBuffer&& firmwareData, QString const& name, std::vector<EntityInfo> entitiesToUpdate, QWidget *parent)
	: QDialog(parent), _ui(new Ui::FirmwareUploadDialog), _firmwareData(firmwareData)
{
	_ui->setupUi(this);
	
	// Initial configuration
	_ui->listWidget->setSelectionMode(QAbstractItemView::NoSelection);
	_ui->startPushButton->setEnabled(true);
	_ui->abortPushButton->setEnabled(false);
	setWindowTitle(QString("Firmware Update: %1").arg(name));

	// Create 
	for (auto const& entityInfos : entitiesToUpdate)
	{
		scheduleUpload(entityInfos);
	}

	// Connect signals
	auto& manager = avdecc::ControllerManager::getInstance();
	connect(&manager, &avdecc::ControllerManager::operationProgress, this, [this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::OperationID const operationID, float const percentComplete)
	{
		for (auto row = 0; row < _ui->listWidget->count(); ++row)
		{
			auto* item = _ui->listWidget->item(row);
			auto* widget = static_cast<UploadWidget*>(_ui->listWidget->itemWidget(item));

			auto const eID = item->data(la::avdecc::to_integral(ItemRole::EntityID)).value<la::avdecc::UniqueIdentifier>();
			auto const dIndex = item->data(la::avdecc::to_integral(ItemRole::DescriptorIndex)).value<la::avdecc::entity::model::DescriptorIndex>();
			auto const oID = item->data(la::avdecc::to_integral(ItemRole::OperationID)).value<la::avdecc::entity::model::OperationID>();
			auto const state = item->data(la::avdecc::to_integral(ItemRole::UpdateState)).value<UpdateState>();
			if (state == UpdateState::Storing && entityID == eID && descriptorType == la::avdecc::entity::model::DescriptorType::MemoryObject && descriptorIndex == dIndex && operationID == oID)
			{
				widget->setProgress(static_cast<int>(percentComplete));
			}
		}
	});
	connect(&manager, &avdecc::ControllerManager::operationCompleted, this, [this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::OperationID const operationID, bool const failed)
	{
		for (auto row = 0; row < _ui->listWidget->count(); ++row)
		{
			auto* item = _ui->listWidget->item(row);
			auto* widget = static_cast<UploadWidget*>(_ui->listWidget->itemWidget(item));

			auto const eID = item->data(la::avdecc::to_integral(ItemRole::EntityID)).value<la::avdecc::UniqueIdentifier>();
			auto const dIndex = item->data(la::avdecc::to_integral(ItemRole::DescriptorIndex)).value<la::avdecc::entity::model::DescriptorIndex>();
			auto const oID = item->data(la::avdecc::to_integral(ItemRole::OperationID)).value<la::avdecc::entity::model::OperationID>();
			auto const state = item->data(la::avdecc::to_integral(ItemRole::UpdateState)).value<UpdateState>();
			if (state == UpdateState::Storing && entityID == eID && descriptorType == la::avdecc::entity::model::DescriptorType::MemoryObject && descriptorIndex == dIndex && operationID == oID)
			{
				auto const entityName = item->data(la::avdecc::to_integral(ItemRole::EntityName)).toString();
				// Failed
				if (failed)
				{
					widget->setText(QString("%1: Failed").arg(entityName));
					item->setData(la::avdecc::to_integral(ItemRole::UpdateState), QVariant::fromValue(UpdateState::Failed));
				}
				// Succeeded
				else
				{
					widget->setText(QString("%1: Complete").arg(entityName));
					item->setData(la::avdecc::to_integral(ItemRole::UpdateState), QVariant::fromValue(UpdateState::Complete));
				}
				checkAllDone();
			}
		}
	});
}

FirmwareUploadDialog::~FirmwareUploadDialog()
{
	delete _ui;
}

bool FirmwareUploadDialog::areAllDone() const noexcept
{
	auto const[total, failed, succeed] = getCounts();

	AVDECC_ASSERT(total >= (failed + succeed), "Counts mismatch");
	return total == (failed + succeed);
}

void FirmwareUploadDialog::checkAllDone() noexcept
{
	auto const[total, failed, succeed] = getCounts();

	auto const done{ failed + succeed };

	if (total == done)
	{
		_ui->startPushButton->setEnabled(false);
		_ui->abortPushButton->setEnabled(false);

		if (failed == 0)
		{
			// Single entity
			if (total == 1)
			{
				QMessageBox::information(this, "", "Firmware successfully updated");
			}
			else
			{
				QMessageBox::information(this, "", "Firmware successfully updated on all entities");
			}
		}
		else
		{
			// Single entity
			if (total == 1)
			{
				QMessageBox::warning(this, "", "Failed to update firmware");
			}
			// No succeeded
			else if (succeed == 0)
			{
				QMessageBox::warning(this, "", QString("Failed to update firmware on %1 entities").arg(failed));
			}
			else
			{
				QMessageBox::warning(this, "", QString("Failed to update firmware on %1 entities, but succeeded on %2").arg(failed).arg(succeed));
			}
		}
	}
}

std::tuple<size_t, size_t, size_t> FirmwareUploadDialog::getCounts() const noexcept
{
	auto const total = _ui->listWidget->count();
	auto failed{ 0u };
	auto succeed{ 0u };

	for (auto row = 0; row < total; ++row)
	{
		auto* item = _ui->listWidget->item(row);
		auto* widget = static_cast<UploadWidget*>(_ui->listWidget->itemWidget(item));

		auto const state = item->data(la::avdecc::to_integral(ItemRole::UpdateState)).value<UpdateState>();
		switch (state)
		{
			case UpdateState::Waiting:
			case UpdateState::Complete:
				++succeed;
				break;
			case UpdateState::Failed:
				++failed;
				break;
			default:
				break;
		}
	}

	return std::make_tuple(total, failed, succeed);
}

void FirmwareUploadDialog::scheduleUpload(EntityInfo const& entityInfo) noexcept
{
	auto const[entityID, descriptorIndex, memoryObjectAddress] = entityInfo;
	auto* item = new QListWidgetItem{_ui->listWidget};
	auto* widget = new UploadWidget{this};
	auto& manager = avdecc::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(entityID);

	if (controlledEntity)
	{
		auto const name = avdecc::helper::smartEntityName(*controlledEntity);
		item->setData(la::avdecc::to_integral(ItemRole::EntityID), QVariant::fromValue(entityID));
		item->setData(la::avdecc::to_integral(ItemRole::DescriptorIndex), QVariant::fromValue(descriptorIndex));
		item->setData(la::avdecc::to_integral(ItemRole::MemoryObjectAddress), QVariant::fromValue(memoryObjectAddress));
		item->setData(la::avdecc::to_integral(ItemRole::EntityName), QVariant::fromValue(name));
		item->setData(la::avdecc::to_integral(ItemRole::OperationID), QVariant::fromValue(la::avdecc::entity::model::OperationID{ 0u }));
		item->setData(la::avdecc::to_integral(ItemRole::UpdateState), QVariant::fromValue(UpdateState::Waiting));

		widget->setText(QString("%1: Waiting to start").arg(name));
		widget->setProgress(0);

		item->setSizeHint(widget->sizeHint());

		_ui->listWidget->setItemWidget(item, widget);
	}
}

void FirmwareUploadDialog::closeEvent(QCloseEvent* event)
{
	// If we have pending operations, display a message and prevent closing
	if (!areAllDone())
	{
		QMessageBox::warning(this, "", "One or multiple operation(s) are still ongoing.\nClick Abort or wait for them to complete.");
		event->ignore();
	}
}

void FirmwareUploadDialog::reject()
{
	// Only accept the reject (ESC key pressed) if we don't have any pending operation
	if (areAllDone())
	{
		QDialog::reject();
	}
}

void FirmwareUploadDialog::on_startPushButton_clicked()
{
	_ui->startPushButton->setEnabled(false);
	_ui->abortPushButton->setEnabled(true);
	
	auto& manager = avdecc::ControllerManager::getInstance();

	for (auto row = 0; row < _ui->listWidget->count(); ++row)
	{
		auto* item = _ui->listWidget->item(row);
		auto* widget = static_cast<UploadWidget*>(_ui->listWidget->itemWidget(item));

		auto const entityID = item->data(la::avdecc::to_integral(ItemRole::EntityID)).value<la::avdecc::UniqueIdentifier>();
		auto const descriptorIndex = item->data(la::avdecc::to_integral(ItemRole::DescriptorIndex)).value<la::avdecc::entity::model::DescriptorIndex>();
		auto const entityName = item->data(la::avdecc::to_integral(ItemRole::EntityName)).toString();

		widget->setText(QString("%1: Uploading").arg(entityName));
		widget->setProgress(0);
		item->setData(la::avdecc::to_integral(ItemRole::UpdateState), QVariant::fromValue(UpdateState::StartUpload));

		// Query an OperationID to start the upload
		manager.startUploadMemoryObjectOperation(entityID, descriptorIndex, _firmwareData.size(), [this, item, widget, entityName](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::entity::model::OperationID const operationID)
		{
			// Handle the result of startUploadMemoryObjectOperation
			QMetaObject::invokeMethod(widget, [this, item, widget, status, operationID, entityID, descriptorIndex, entityName]()
			{
				// Failed to startUploadMemoryObjectOperation
				if (!status)
				{
					widget->setText(QString("%1: Upload failed: %2").arg(entityName).arg(QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status))));
					item->setData(la::avdecc::to_integral(ItemRole::UpdateState), QVariant::fromValue(UpdateState::Failed));
					checkAllDone();
				}
				// Succeeded
				else
				{
					auto const memoryObjectAddress = item->data(la::avdecc::to_integral(ItemRole::MemoryObjectAddress)).value<std::uint64_t>();
					item->setData(la::avdecc::to_integral(ItemRole::OperationID), QVariant::fromValue(operationID));
					item->setData(la::avdecc::to_integral(ItemRole::UpdateState), QVariant::fromValue(UpdateState::Uploading));

					// Write the firmware to the MemoryObject
					auto& manager = avdecc::ControllerManager::getInstance();
					manager.writeDeviceMemory(entityID, memoryObjectAddress, _firmwareData, [widget, this](la::avdecc::controller::ControlledEntity const* const /*entity*/, float const percentComplete)
					{
						// Upload progress
						QMetaObject::invokeMethod(widget, [this, widget, percentComplete]()
						{
							widget->setProgress(static_cast<int>(percentComplete));
						}, Qt::QueuedConnection);
					}, [widget, this, entityName, item, entityID, descriptorIndex](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AaCommandStatus const status)
					{
						// Upload complete
						QMetaObject::invokeMethod(widget, [this, item, widget, status, entityID, descriptorIndex, entityName]()
						{
							// Failed to upload
							if (!status)
							{
								widget->setText(QString("%1: Upload Failed: %2").arg(entityName).arg(QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status))));
								item->setData(la::avdecc::to_integral(ItemRole::UpdateState), QVariant::fromValue(UpdateState::Failed));
								checkAllDone();
							}
							// Succeeded
							else
							{
								widget->setText(QString("%1: Storing").arg(entityName));
								widget->setProgress(0);
								item->setData(la::avdecc::to_integral(ItemRole::UpdateState), QVariant::fromValue(UpdateState::StartStore));

								// Query an OperationID to store the firmware and reboot
								auto& manager = avdecc::ControllerManager::getInstance();
								manager.startStoreAndRebootMemoryObjectOperation(entityID, descriptorIndex, [this, item, widget, entityName](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::entity::model::OperationID const operationID)
								{
									// Handle the result of startStoreAndRebootMemoryObjectOperation
									QMetaObject::invokeMethod(widget, [this, item, widget, status, entityID, descriptorIndex, operationID, entityName]()
									{
										// Failed to startStoreAndRebootMemoryObjectOperation
										if (!status)
										{
											widget->setText(QString("%1: Upload failed: %2").arg(entityName).arg(QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status))));
											item->setData(la::avdecc::to_integral(ItemRole::UpdateState), QVariant::fromValue(UpdateState::Failed));
											checkAllDone();
										}
										// Succeeded
										else
										{
											// Store the OperationID, and wait for operationProgress and operationCompleted QT signals
											item->setData(la::avdecc::to_integral(ItemRole::OperationID), QVariant::fromValue(operationID));
											item->setData(la::avdecc::to_integral(ItemRole::UpdateState), QVariant::fromValue(UpdateState::Storing));
										}
									}, Qt::QueuedConnection);
								});
							}
						}, Qt::QueuedConnection);
					});
				}
			}, Qt::QueuedConnection);
		});
	}
}

void FirmwareUploadDialog::on_abortPushButton_clicked()
{
	_ui->startPushButton->setEnabled(true);
	_ui->abortPushButton->setEnabled(false);
	
	auto& manager = avdecc::ControllerManager::getInstance();

	for (auto row = 0; row < _ui->listWidget->count(); ++row)
	{
		auto* item = _ui->listWidget->item(row);
		auto* widget = static_cast<UploadWidget*>(_ui->listWidget->itemWidget(item));

		auto const entityID = item->data(la::avdecc::to_integral(ItemRole::EntityID)).value<la::avdecc::UniqueIdentifier>();
		auto const descriptorIndex = item->data(la::avdecc::to_integral(ItemRole::DescriptorIndex)).value<la::avdecc::entity::model::DescriptorIndex>();
		auto const entityName = item->data(la::avdecc::to_integral(ItemRole::EntityName)).toString();
		auto const operationID = item->data(la::avdecc::to_integral(ItemRole::OperationID)).value<la::avdecc::entity::model::OperationID>();

		widget->setText(QString("%1: Aborted").arg(entityName));
		manager.abortOperation(entityID, la::avdecc::entity::model::DescriptorType::MemoryObject, descriptorIndex, operationID, [](la::avdecc::UniqueIdentifier const /*entityID*/, la::avdecc::entity::model::DescriptorType const /*descriptorType*/, la::avdecc::entity::model::DescriptorIndex const /*descriptorIndex*/, la::avdecc::entity::model::OperationID const /*operationID*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const /*status*/)
		{
		});

	}
}
