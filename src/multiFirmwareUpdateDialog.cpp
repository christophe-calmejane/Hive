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
#include "defaults.hpp"

Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)

class ModelPrivate;
class Model : public QAbstractTableModel
{
	Q_OBJECT
public:
	enum class Column
	{
		EntityID,
		Name,
		FirmwareVersion,

		Count
	};

	Model(QObject* parent = nullptr);
	virtual ~Model();

	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual int columnCount(QModelIndex const& parent = {}) const override;
	virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	la::avdecc::UniqueIdentifier controlledEntityID(QModelIndex const& index) const;

private:
	QScopedPointer<ModelPrivate> const d_ptr;
	Q_DECLARE_PRIVATE(Model)
};

/***********************************************/

class ModelPrivate : public QObject
{
	Q_OBJECT
public:
	ModelPrivate(Model* model)
		: q_ptr{ model }
	{
		// Connect avdecc::ControllerManager signals
		auto& manager = avdecc::ControllerManager::getInstance();
		connect(&manager, &avdecc::ControllerManager::controllerOffline, this, &ModelPrivate::handleControllerOffline);
		connect(&manager, &avdecc::ControllerManager::entityOnline, this, &ModelPrivate::handleEntityOnline);
		connect(&manager, &avdecc::ControllerManager::entityOffline, this, &ModelPrivate::handleEntityOffline);
		connect(&manager, &avdecc::ControllerManager::entityNameChanged, this, &ModelPrivate::handleEntityNameChanged);
	}

	int rowCount() const
	{
		return static_cast<int>(_entities.size());
	}

	int columnCount() const
	{
		return la::avdecc::utils::to_integral(Model::Column::Count);
	}

	QVariant data(QModelIndex const& index, int role) const
	{
		auto const row = index.row();
		if (row < 0 || row >= rowCount())
		{
			return {};
		}

		auto const& data = _entities[row];
		auto const column = static_cast<Model::Column>(index.column());

		if (role == Qt::DisplayRole)
		{
			switch (column)
			{
				case Model::Column::EntityID:
					return avdecc::helper::uniqueIdentifierToString(data.entityID);
				case Model::Column::Name:
					return data.name;
				case Model::Column::FirmwareVersion:
					return data.firmwareVersion;
				default:
					break;
			}
		}

		return {};
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role) const
	{
		if (orientation == Qt::Horizontal)
		{
			if (role == Qt::DisplayRole)
			{
				switch (static_cast<Model::Column>(section))
				{
					case Model::Column::EntityID:
						return "Entity ID";
					case Model::Column::Name:
						return "Name";
					case Model::Column::FirmwareVersion:
						return "Firmware Version";
					default:
						break;
				}
			}
		}

		return {};
	}

private:
	// avdecc::ControllerManager

	void handleControllerOffline()
	{
		Q_Q(Model);

		q->beginResetModel();
		_entities.clear();
		q->endResetModel();
	}

	void handleEntityOnline(la::avdecc::UniqueIdentifier const& entityID)
	{
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity && controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
			{
				auto const& entityNode = controlledEntity->getEntityNode();
				if (entityNode.dynamicModel)
				{
					auto const configurationIndex = entityNode.dynamicModel->currentConfiguration;
					auto const& configurationNode = controlledEntity->getConfigurationNode(configurationIndex);
					for (auto const& [memoryObjectIndex, memoryObjectNode] : configurationNode.memoryObjects)
					{
						if (memoryObjectNode.staticModel->memoryObjectType == la::avdecc::entity::model::MemoryObjectType::FirmwareImage)
						{
							Q_Q(Model);

							// Insert at the end
							auto const row = rowCount();
							emit q->beginInsertRows({}, row, row);

							_entities.push_back(EntityData{ entityID, avdecc::helper::smartEntityName(*controlledEntity), entityNode.dynamicModel->firmwareVersion.data() });

							// Update the cache
							rebuildEntityRowMap();

							emit q->endInsertRows();

							return;
						}
					}
				}
			}
		}
		catch (...)
		{
			// Ignore exceptions
		}
	}

	void handleEntityOffline(la::avdecc::UniqueIdentifier const& entityID)
	{
		if (auto const row = entityRow(entityID))
		{
			Q_Q(Model);
			emit q->beginRemoveRows({}, *row, *row);

			// Remove the entity from the model
			auto const it = std::next(std::begin(_entities), *row);
			_entities.erase(it);

			// Update the cache
			rebuildEntityRowMap();

			emit q->endRemoveRows();
		}
	}

	void handleEntityNameChanged(la::avdecc::UniqueIdentifier const& entityID, QString const& entityName)
	{
		if (auto const row = entityRow(entityID))
		{
			auto& data = _entities[*row];
			data.name = entityName;

			dataChanged(*row, Model::Column::Name);
		}
	}

	// Build the entityID to row map
	void rebuildEntityRowMap()
	{
		_entityRowMap.clear();

		for (auto row = 0; row < rowCount(); ++row)
		{
			auto const& data = _entities[row];
			_entityRowMap.emplace(std::make_pair(data.entityID, row));
		}
	}

	// Returns the entity row if found in the model
	std::optional<int> entityRow(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto const it = _entityRowMap.find(entityID);
		if (AVDECC_ASSERT_WITH_RET(it != std::end(_entityRowMap), "Invalid entityID"))
		{
			return it->second;
		}
		return {};
	}

	la::avdecc::UniqueIdentifier controlledEntityID(QModelIndex const& index) const
	{
		auto const row = index.row();
		if (row >= 0 && row < rowCount())
		{
			return _entities[index.row()].entityID;
		}
		return la::avdecc::UniqueIdentifier{};
	}


private:
	Model* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(Model);

	struct EntityData
	{
		la::avdecc::UniqueIdentifier entityID{};
		QString name{};
		QString firmwareVersion{};
	};

	using Entities = std::vector<EntityData>;
	using EntityRowMap = std::unordered_map<la::avdecc::UniqueIdentifier, int, la::avdecc::UniqueIdentifier::hash>;

	QModelIndex createIndex(int const row, Model::Column const column) const
	{
		Q_Q(const Model);
		return q->createIndex(row, la::avdecc::utils::to_integral(column));
	}

	void dataChanged(int const row, Model::Column const column, QVector<int> const& roles = { Qt::DisplayRole })
	{
		auto const index = createIndex(row, column);
		if (index.isValid())
		{
			Q_Q(Model);
			emit q->dataChanged(index, index, roles);
		}
	}

	Entities _entities{};
	EntityRowMap _entityRowMap{};
};

Model::Model(QObject* parent)
	: QAbstractTableModel{ parent }
	, d_ptr{ new ModelPrivate{ this } }
{
	// Initialize the model for each existing entity
	auto& manager = avdecc::ControllerManager::getInstance();
	manager.foreachEntity(
		[this](la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::ControlledEntity const&)
		{
			Q_D(Model);
			d->handleEntityOnline(entityID);
		});
}

Model::~Model() = default;

int Model::rowCount(QModelIndex const&) const
{
	Q_D(const Model);
	return d->rowCount();
}

int Model::columnCount(QModelIndex const&) const
{
	Q_D(const Model);
	return d->columnCount();
}

QVariant Model::data(QModelIndex const& index, int role) const
{
	Q_D(const Model);
	return d->data(index, role);
}

QVariant Model::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_D(const Model);
	return d->headerData(section, orientation, role);
}

la::avdecc::UniqueIdentifier Model::controlledEntityID(QModelIndex const& index) const
{
	Q_D(const Model);
	return d->controlledEntityID(index);
}

/***********************************************/

MultiFirmwareUpdateDialog::MultiFirmwareUpdateDialog(QWidget* parent)
	: QDialog{ parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint }
	, _ui{ new Ui::MultiFirmwareUpdateDialog }
	, _model{ new Model{ this } }
{
	_ui->setupUi(this);

	// Initial configuration
	_ui->buttonContinue->setEnabled(false);
	setWindowTitle("Firmware Update Selection");

	_ui->controllerTableView->setModel(_model);

	// configure multiselection
	_ui->controllerTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	_ui->controllerTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	_ui->controllerTableView->setFocusPolicy(Qt::ClickFocus);
	_ui->controllerTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	_ui->controllerTableView->horizontalHeader()->setStretchLastSection(true);

	_ui->controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(Model::Column::EntityID), defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	_ui->controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(Model::Column::Name), defaults::ui::AdvancedView::ColumnWidth_Name);
	_ui->controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(Model::Column::FirmwareVersion), defaults::ui::AdvancedView::ColumnWidth_Firmware);

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

void MultiFirmwareUpdateDialog::onItemSelectionChanged(QItemSelection const&, QItemSelection const&)
{
	// Update the next button state
	auto const& selectedRows = _ui->controllerTableView->selectionModel()->selectedRows();
	if (selectedRows.empty())
	{
		_ui->buttonContinue->setEnabled(false);
		return;
	}

	auto& manager = avdecc::ControllerManager::getInstance();

	// Check that all selected entities have the same model name
	QString modelName;
	for (auto const& rowIndex : selectedRows)
	{
		auto const entityID = _model->controlledEntityID(rowIndex);
		auto const controlledEntity = manager.getControlledEntity(entityID);
		auto const& entityNode = controlledEntity->getEntityNode();

		if (entityNode.staticModel)
		{
			auto const rowModelName = controlledEntity->getLocalizedString(entityNode.staticModel->modelNameString).data();

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
	}

	_ui->buttonContinue->setEnabled(true);
}

void MultiFirmwareUpdateDialog::startFirmwareUpdate()
{
	// Show file select dialog
	auto const fileName = QFileDialog::getOpenFileName(this, "Choose Firmware File", "", "");
	if (fileName.isEmpty())
	{
		return;
	}

	// Open the file
	auto file = QFile{ fileName };
	if (!file.open(QIODevice::ReadOnly))
	{
		QMessageBox::critical(this, "", "Failed to load firmware file.");
		return;
	}

	// Determine the maximum length (TODO, cache in the model?)
	auto& manager = avdecc::ControllerManager::getInstance();
	auto maximumLength = 0;

	auto firmwareUpdateEntityInfos = std::vector<FirmwareUploadDialog::EntityInfo>{};

	auto const& selectedRows = _ui->controllerTableView->selectionModel()->selectedRows();
	for (auto const& index : selectedRows)
	{
		auto const entityID = _model->controlledEntityID(index);
		auto const controlledEntity = manager.getControlledEntity(entityID);
		if (controlledEntity)
		{
			auto const& entityNode = controlledEntity->getEntityNode();
			if (entityNode.dynamicModel)
			{
				auto const configurationIndex = entityNode.dynamicModel->currentConfiguration;
				auto const& configurationNode = controlledEntity->getConfigurationNode(configurationIndex);
				for (auto const& [memoryObjectIndex, memoryObjectNode] : configurationNode.memoryObjects)
				{
					if (memoryObjectNode.staticModel->memoryObjectType == la::avdecc::entity::model::MemoryObjectType::FirmwareImage)
					{
						if (maximumLength == 0)
						{
							// Pick first entity's max length, all following entities should have the same.
							maximumLength = memoryObjectNode.staticModel->maximumLength;
						}

						auto const* const model = memoryObjectNode.staticModel;
						firmwareUpdateEntityInfos.emplace_back(entityID, memoryObjectNode.descriptorIndex, model->startAddress);
						break;
					}
				}
			}
		}
	}

	// Read all data
	auto firmwareFileData = file.readAll();

	// Check length
	if (maximumLength != 0 && firmwareFileData.size() > maximumLength)
	{
		QMessageBox::critical(this, "", "The firmware file is not compatible with selected devices.");
		return;
	}

	// Close this dialog once a compatible file has been selected
	close();

	// Start firmware upload dialog
	auto dialog = FirmwareUploadDialog{ { firmwareFileData.constData(), static_cast<size_t>(firmwareFileData.count()) }, QFileInfo(fileName).fileName(), firmwareUpdateEntityInfos, this };
	dialog.exec();
}

#include "multiFirmwareUpdateDialog.moc"
