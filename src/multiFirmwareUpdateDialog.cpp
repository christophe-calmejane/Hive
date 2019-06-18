#include "multiFirmwareUpdateDialog.hpp"
#include "ui_multiFirmwareUpdateDialog.h"

#include <QLabel>
#include <QProgressBar>
#include <QFileInfo>
#include <QVariant>
#include <QFile>
#include <QByteArray>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSortFilterProxyModel>
#include <QMessageBox>
#include <QFile>
#include <QFileDialog>

#include <la/avdecc/utils.hpp>
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"
#include "firmwareUploadDialog.hpp"


Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)

class FirmwareUpdateSelectionModel : public QSortFilterProxyModel
{
public:
	FirmwareUpdateSelectionModel(QObject* parent = nullptr)
		: QSortFilterProxyModel(parent)
	{
	}

	// in here we only have entities that are
	// live updates for entities going online/ offline should be implemented as well

	// or just use a proxy model with the existing controller model
protected:
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
	{
		// only allow entities with a firmware node.
		auto const& entityIdIndex = sourceModel()->index(sourceRow, 0, sourceParent);

		auto const* controllerModel = static_cast<avdecc::ControllerModel*>(sourceModel());
		auto& controllerManager = avdecc::ControllerManager::getInstance();

		auto const entity = controllerManager.getControlledEntity(controllerModel->controlledEntityID(entityIdIndex));
		if (!entity)
		{
			return false;
		}
		auto containsFirmwareMemoryObject = false;
		for (auto const& memoryObject : entity->getCurrentConfigurationNode().memoryObjects)
		{
			if (memoryObject.second.staticModel->memoryObjectType == la::avdecc::entity::model::MemoryObjectType::FirmwareImage)
			{
				containsFirmwareMemoryObject = true;
				break;
			}
		}
		return containsFirmwareMemoryObject;
	}
};

// @param controllerModel: the controller model to create the firmware update selection model from it.
MultiFirmwareUpdateDialog::MultiFirmwareUpdateDialog(avdecc::ControllerModel* controllerModel, QWidget* parent)
	: QDialog(parent)
	, _ui(new Ui::MultiFirmwareUpdateDialog)
{
	_ui->setupUi(this);

	// Initial configuration
	_ui->buttonContinue->setEnabled(false);
	setWindowTitle("Firmware Update Selection");

	_controllerModel = controllerModel;

	//controllerModel
	FirmwareUpdateSelectionModel* proxyModel = new FirmwareUpdateSelectionModel(this);
	proxyModel->setSourceModel(_controllerModel);

	// use the controller model to display the entities
	_ui->controllerTableView->setModel(proxyModel);

	// configure multiselection
	_ui->controllerTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	_ui->controllerTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	_ui->controllerTableView->setFocusPolicy(Qt::ClickFocus);
	_ui->controllerTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

	_controllerDynamicHeaderView.setHighlightSections(false);
	_controllerDynamicHeaderView.setMandatorySection(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityId));

	_ui->controllerTableView->setHorizontalHeader(&_controllerDynamicHeaderView);

	// only show name and entity id:
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AcquireState), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AssociationId), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Compatibility), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Count), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityLogo), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GptpDomain), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GrandmasterId), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Group), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::InterfaceIndex), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::LockState), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterId), true);
	_controllerDynamicHeaderView.setSectionHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterName), true);

	// stretch the headers
	_ui->controllerTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

	connect(_ui->controllerTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MultiFirmwareUpdateDialog::onItemSelectionChanged);
	connect(_ui->buttonContinue, &QPushButton::clicked, this, &MultiFirmwareUpdateDialog::handleContinueButtonClicked);
}

MultiFirmwareUpdateDialog::~MultiFirmwareUpdateDialog()
{
	delete _ui;
}

void MultiFirmwareUpdateDialog::handleContinueButtonClicked()
{
	startFirmwareUpdate();
}

void MultiFirmwareUpdateDialog::onItemSelectionChanged(QItemSelection const& selected, QItemSelection const& deselected)
{
	// update the next button state
	auto& manager = avdecc::ControllerManager::getInstance();
	auto const& selectedRows = _ui->controllerTableView->selectionModel()->selectedRows();
	if (selectedRows.empty())
	{
		_ui->buttonContinue->setEnabled(false);
		return;
	}
	QString modelName;
	for (auto const& rowIndex : selectedRows)
	{
		auto const selectedEntityId = _controllerModel->controlledEntityID(rowIndex);
		auto const selectedEntity = manager.getControlledEntity(selectedEntityId);
		auto const rowModelName = selectedEntity->getLocalizedString(selectedEntity->getEntityNode().staticModel->modelNameString).data();
		if (modelName.isEmpty())
		{
			modelName = rowModelName;
		}
		else if (modelName != rowModelName)
		{
			_ui->buttonContinue->setEnabled(false);
			return;
		}
	}

	_ui->buttonContinue->setEnabled(true);
}

void MultiFirmwareUpdateDialog::startFirmwareUpdate()
{
	// collect the entity ids
	std::list<la::avdecc::UniqueIdentifier> selectedEntities;
	auto const& selectedRows = _ui->controllerTableView->selectionModel()->selectedRows();
	for (auto const& rowIndex : selectedRows)
	{
		auto const selectedEntityId = _controllerModel->controlledEntityID(rowIndex);
		selectedEntities.push_back(selectedEntityId);
	}

	// show file select dialog
	QString fileName = QFileDialog::getOpenFileName(this, "Choose Firmware File", "", "");
	if (!fileName.isEmpty())
	{
		// Open the file
		QFile file{ fileName };
		if (!file.open(QIODevice::ReadOnly))
		{
			QMessageBox::critical(this, "", "Failed to load firmware file.");
			return;
		}


		auto& manager = avdecc::ControllerManager::getInstance();
		auto maximumLength = 0;

		std::vector<FirmwareUploadDialog::EntityInfo> firmwareUpdateEntityInfos;
		for (auto const& selectedEntityId : selectedEntities)
		{
			auto const selectedEntity = manager.getControlledEntity(selectedEntityId);
			for (auto const& [index, memoryObjectNode] : selectedEntity->getCurrentConfigurationNode().memoryObjects)
			{
				if (memoryObjectNode.staticModel->memoryObjectType == la::avdecc::entity::model::MemoryObjectType::FirmwareImage)
				{
					if (maximumLength == 0)
					{
						// pick first entity's max length, all following entities should have the same.
						maximumLength = memoryObjectNode.staticModel->maximumLength;
					}
					auto const* const model = memoryObjectNode.staticModel;
					firmwareUpdateEntityInfos.push_back({ selectedEntityId, memoryObjectNode.descriptorIndex, model->startAddress });
					break;
				}
			}
		}

		// Read all data
		auto data = file.readAll();

		// Check length
		if (maximumLength != 0 && data.size() > maximumLength)
		{
			QMessageBox::critical(this, "", "The firmware file is not compatible with selected devices.");
			return;
		}

		// close this dialog once a compatible file has been selected
		close();

		// Start firmware upload dialog
		FirmwareUploadDialog dialog{ { data.constData(), static_cast<size_t>(data.count()) }, QFileInfo(fileName).fileName(), firmwareUpdateEntityInfos, this };
		dialog.exec();
	}
}
