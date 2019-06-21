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
			if (controlledEntity)
			{
				auto const& entityNode = controlledEntity->getEntityNode();
				if (entityNode.dynamicModel)
				{
					auto const configurationIndex = entityNode.dynamicModel->currentConfiguration;
					auto const& configurationNode = controlledEntity->getConfigurationNode(configurationIndex);
					for (auto const& [memoryObjectIndex, memoryObjectNode] : configurationNode.memoryObjects)
					{
						//if (memoryObjectNode.staticModel->memoryObjectType == la::avdecc::entity::model::MemoryObjectType::FirmwareImage)
						{
							Q_Q(Model);

							// Insert at the end
							auto const row = rowCount();
							emit q->beginInsertRows({}, row, row);

							_entities.push_back(EntityData{ entityID, avdecc::helper::entityName(*controlledEntity), entityNode.dynamicModel->firmwareVersion.data() });

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
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
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
			Q_Q(Model);

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

	Entities _entities;
	EntityRowMap _entityRowMap{};
};

Model::Model(QObject* parent)
	: QAbstractTableModel{ parent }
	, d_ptr{ new ModelPrivate{ this } }
{
	// Initialize the model for each existing entity
	auto& manager = avdecc::ControllerManager::getInstance();
	manager.foreachEntity(
			[this](la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::ControlledEntity const& controlledEntity)
			{
				Q_D(Model);
				d->handleEntityOnline(entityID);
			});
}

Model::~Model() = default;

int Model::rowCount(QModelIndex const& parent) const
{
	Q_D(const Model);
	return d->rowCount();
}

int Model::columnCount(QModelIndex const& parent) const
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

// @param Model: the controller model to create the firmware update selection model from it.
MultiFirmwareUpdateDialog::MultiFirmwareUpdateDialog(QWidget* parent)
	: QDialog(parent)
	, _ui(new Ui::MultiFirmwareUpdateDialog)
	, _model{ new Model{ this } }
	{
	_ui->setupUi(this);

	// Initial configuration
	_ui->buttonContinue->setEnabled(false);
	setWindowTitle("Firmware Update Selection");


	// use the controller model to display the entities
	_ui->controllerTableView->setModel(_model);

	// configure multiselection
	_ui->controllerTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	_ui->controllerTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	_ui->controllerTableView->setFocusPolicy(Qt::ClickFocus);
	_ui->controllerTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

	_controllerDynamicHeaderView.setHighlightSections(false);
	_controllerDynamicHeaderView.setMandatorySection(la::avdecc::utils::to_integral(Model::Column::EntityID));

	_ui->controllerTableView->setHorizontalHeader(&_controllerDynamicHeaderView);

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
		auto const selectedEntityId = _model->controlledEntityID(rowIndex);
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
		auto const selectedEntityId = _model->controlledEntityID(rowIndex);
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


#include "multiFirmwareUpdateDialog.moc"
