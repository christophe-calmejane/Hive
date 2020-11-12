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

#include "ui_mainWindow.h"
#include "mainWindow.hpp"
#include <QtWidgets>
#include <QMessageBox>
#include <QFile>
#include <QTextBrowser>
#include <QDateTime>
#include <QAbstractListModel>
#include <QVector>
#include <QSettings>
#include <QLabel>

#ifdef DEBUG
#	include <QFileInfo>
#	include <QDir>
#endif

#include "config.hpp"
//#include "entityLogoCache.hpp"
//#include "errorItemDelegate.hpp"
//#include "imageItemDelegate.hpp"

#include <QtMate/widgets/dynamicHeaderView.hpp>
#include <QtMate/widgets/comboBox.hpp>
//#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
//#include <hive/modelsLibrary/discoveredEntitiesModel.hpp>
#include <hive/widgetModelsLibrary/networkInterfacesListModel.hpp>
#include <hive/widgetModelsLibrary/discoveredEntitiesTableModel.hpp>
#include <hive/widgetModelsLibrary/imageItemDelegate.hpp>

#include <mutex>
#include <memory>
#include <utility>

#include <QVector>

#define PROG_ID 0x0005
#define VENDOR_ID 0x001B92
#define DEVICE_ID 0x80
#define MODEL_ID 0x00000001

//Q_DECLARE_METATYPE(la::avdecc::protocol::ProtocolInterface::Type)
#if 0
class DiscoveredEntitiesTableModel : public hive::modelsLibrary::DiscoveredEntitiesAbstractTableModel
{
public:
	enum class EntityDataFlag : std::uint32_t
	{
		EntityLogo = 1u << 0,
		Compatibility = 1u << 1,
		EntityID = 1u << 2,
		Name = 1u << 3,
		Group = 1u << 4,
		FirmwareVersion = 1u << 5,
	};
	using EntityDataFlags = la::avdecc::utils::EnumBitfield<EntityDataFlag>;

	DiscoveredEntitiesTableModel(EntityDataFlags const entityDataFlags)
		: _entityDataFlags{ entityDataFlags }
		, _count{ static_cast<decltype(_count)>(_entityDataFlags.count()) }
	{
	}

private:
	std::optional<std::pair<EntityDataFlag, QVector<int>>> dataChangedInfoForFlag(ChangedInfoFlag const flag) const noexcept
	{
		switch (flag)
		{
			case ChangedInfoFlag::Name:
				return std::make_pair(EntityDataFlag::Name, QVector<int>{ Qt::DisplayRole });
			case ChangedInfoFlag::GroupName:
				return std::make_pair(EntityDataFlag::Group, QVector<int>{ Qt::DisplayRole });
			case ChangedInfoFlag::Compatibility:
				return std::make_pair(EntityDataFlag::Compatibility, QVector<int>{ Qt::DisplayRole });
			default:
				break;
		}
		return {};
	}

	// hive::modelsLibrary::DiscoveredEntitiesAbstractTableModel overrides
	virtual void entityInfoChanged(std::size_t const index, hive::modelsLibrary::DiscoveredEntitiesModel::Entity const& entity, ChangedInfoFlags const changedInfoFlags) noexcept override
	{
		for (auto const flag : changedInfoFlags)
		{
			if (auto const dataInfoOpt = dataChangedInfoForFlag(flag))
			{
				auto const& [entityDataFlag, roles] = *dataInfoOpt;

				// Is this EntityData active for this model
				if (_entityDataFlags.test(entityDataFlag))
				{
					try
					{
						auto const modelIndex = createIndex(index, static_cast<int>(_entityDataFlags.getPosition(entityDataFlag)));
						emit dataChanged(modelIndex, modelIndex, roles);
					}
					catch (...)
					{
					}
				}
			}
		}
	}

	// QAbstractTableModel overrides
	virtual int rowCount(QModelIndex const& parent = {}) const override
	{
		return static_cast<int>(_model.entitiesCount());
	}

	virtual int columnCount(QModelIndex const& parent = {}) const override
	{
		return static_cast<int>(_count);
	}

	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
	{
		if (orientation == Qt::Horizontal)
		{
			try
			{
				auto const entityDataFlag = _entityDataFlags.at(section);

				if (role == Qt::DisplayRole)
				{
					switch (entityDataFlag)
					{
						case EntityDataFlag::EntityLogo:
							return "Logo";
						case EntityDataFlag::Compatibility:
							return "Compat";
						case EntityDataFlag::EntityID:
							return "Entity ID";
						case EntityDataFlag::Name:
							return "Name";
						case EntityDataFlag::Group:
							return "Group";
						case EntityDataFlag::FirmwareVersion:
							return "Firmware Version";
						default:
							break;
					}
				}
			}
			catch (...)
			{
			}
		}

		return {};
	}
	virtual QVariant data(QModelIndex const& index, int role) const override
	{
		auto const row = static_cast<std::size_t>(index.row());
		auto const section = static_cast<decltype(_count)>(index.column());
		if (row < static_cast<std::size_t>(rowCount()) && section < _count)
		{
			if (auto const entityOpt = _model.entity(row))
			{
				auto const& entity = (*entityOpt).get();

				try
				{
					auto const entityDataFlag = _entityDataFlags.at(section);

					switch (role)
					{
						case Qt::DisplayRole:
						{
							switch (entityDataFlag)
							{
								case EntityDataFlag::EntityID:
									return hive::modelsLibrary::helper::uniqueIdentifierToString(entity.entityID);
								case EntityDataFlag::Name:
									return entity.name;
								case EntityDataFlag::Group:
									return entity.groupName;
								case EntityDataFlag::FirmwareVersion:
									return entity.firmwareVersion ? *entity.firmwareVersion : "N/A";
								default:
									break;
							}
							break;
						}
						case ImageItemDelegate::ImageRole:
						{
							switch (entityDataFlag)
							{
								case EntityDataFlag::EntityLogo:
									if (entity.aemSupported)
									{
										auto& logoCache = EntityLogoCache::getInstance();
										return logoCache.getImage(entity.entityID, EntityLogoCache::Type::Entity, true);
									}
									break;
								default:
									break;
							}
							break;
						}
						default:
							break;
					}
				}
				catch (...)
				{
				}
			}
		}

		return {};
	}

	// Private members
	hive::modelsLibrary::DiscoveredEntitiesModel _model{ this };
	EntityDataFlags _entityDataFlags{};
	std::underlying_type_t<EntityDataFlag> _count{ 0u };
};
#endif

class MainWindowImpl final : public QObject, public Ui::MainWindow
{
	static constexpr auto ControllerModelEntityDataFlags = hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlags{ hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityLogo, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Compatibility, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityID, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Name, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Group, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::FirmwareVersion };
	static constexpr auto ControllerModelEntityColumn_EntityLogo = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityLogo);
	static constexpr auto ControllerModelEntityColumn_Compatibility = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Compatibility);
	static constexpr auto ControllerModelEntityColumn_EntityID = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityID);
	static constexpr auto ControllerModelEntityColumn_Name = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Name);
	static constexpr auto ControllerModelEntityColumn_Group = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Group);
	static constexpr auto ControllerModelEntityColumn_FirmwareVersion = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::FirmwareVersion);

public:
	MainWindowImpl(::MainWindow* parent)
		: _parent(parent)
	{
		// Setup common UI
		setupUi(parent);

		// Register all Qt metatypes
		registerMetaTypes();

		// Setup view
		setupView();
	}

	// Deleted compiler auto-generated methods
	MainWindowImpl(MainWindowImpl const&) = delete;
	MainWindowImpl(MainWindowImpl&&) = delete;
	MainWindowImpl& operator=(MainWindowImpl const&) = delete;
	MainWindowImpl& operator=(MainWindowImpl&&) = delete;

	// Private Slots
	Q_SLOT void currentControllerChanged();

	// Private methods
	void setupView();
	void registerMetaTypes();
	void createToolbars();
	void createControllerView();
	void connectSignals();
	static QString generateDumpSourceString() noexcept;

	// Private members
	::MainWindow* _parent{ nullptr };
	qtMate::widgets::ComboBox _interfaceComboBox{ _parent };
	hive::widgetModelsLibrary::NetworkInterfacesListModel _networkInterfacesModel{};
	QSortFilterProxyModel _networkInterfacesModelProxy{ _parent };
	qtMate::widgets::DynamicHeaderView _controllerDynamicHeaderView{ Qt::Horizontal, _parent };
	hive::widgetModelsLibrary::DiscoveredEntitiesTableModel _controllerModel{ ControllerModelEntityDataFlags };
	bool _shown{ false };
};

void MainWindowImpl::setupView()
{
	// Create toolbars
	createToolbars();

	// Create the ControllerView widget
	createControllerView();

	// Initialize UI defaults
	controllerTableView->setColumnWidth(ControllerModelEntityColumn_EntityLogo, 60);
	controllerTableView->setColumnWidth(ControllerModelEntityColumn_Compatibility, 50);
	controllerTableView->setColumnWidth(ControllerModelEntityColumn_EntityID, 160);
	controllerTableView->setColumnWidth(ControllerModelEntityColumn_Name, 180);
	controllerTableView->setColumnWidth(ControllerModelEntityColumn_Group, 180);
	controllerTableView->setColumnWidth(ControllerModelEntityColumn_FirmwareVersion, 120);

	// Connect all signals
	connectSignals();
}

void MainWindowImpl::currentControllerChanged()
{
	auto const interfaceID = _interfaceComboBox.currentData().toString();

	// Clear the current controller
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	manager.destroyController();

	if (interfaceID.isEmpty())
	{
		return;
	}

	try
	{
		// Create a new Controller
#ifdef DEBUG
		auto const progID = std::uint16_t{ PROG_ID + 1 }; // Use next PROG_ID in debug (so we can launch 2 Hive instances at the same time, one in Release and one in Debug)
#else // !DEBUG
		auto const progID = std::uint16_t{ PROG_ID };
#endif // DEBUG
		manager.createController(la::avdecc::protocol::ProtocolInterface::Type::PCap, interfaceID, progID, la::avdecc::entity::model::makeEntityModelID(VENDOR_ID, DEVICE_ID, MODEL_ID), "en");
	}
	catch (la::avdecc::controller::Controller::Exception const&)
	{
	}
}

// Private methods
void MainWindowImpl::registerMetaTypes()
{
	//qRegisterMetaType<std::string>("std::string");
}

void MainWindowImpl::createToolbars()
{
	// Controller Toolbar
	{
		auto* interfaceLabel = new QLabel("Interface");
		interfaceLabel->setMinimumWidth(50);
		_interfaceComboBox.setMinimumWidth(100);
		_interfaceComboBox.setModel(&_networkInterfacesModel);

		controllerToolBar->setMinimumHeight(30);
		controllerToolBar->addWidget(interfaceLabel);
		controllerToolBar->addWidget(&_interfaceComboBox);
	}
}

void MainWindowImpl::createControllerView()
{
	controllerTableView->setModel(&_controllerModel);
	controllerTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	controllerTableView->setSelectionMode(QAbstractItemView::SingleSelection);
	controllerTableView->setContextMenuPolicy(Qt::CustomContextMenu);
	controllerTableView->setFocusPolicy(Qt::ClickFocus);

	// Disable row resizing
	controllerTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

	// The table view does not take ownership on the item delegate
	auto* imageItemDelegate{ new hive::widgetModelsLibrary::ImageItemDelegate{ _parent } };
	controllerTableView->setItemDelegateForColumn(ControllerModelEntityColumn_EntityLogo, imageItemDelegate);

	_controllerDynamicHeaderView.setHighlightSections(false);
	_controllerDynamicHeaderView.setMandatorySection(ControllerModelEntityColumn_EntityID);
	controllerTableView->setHorizontalHeader(&_controllerDynamicHeaderView);
}

static inline bool isValidEntityModelID(la::avdecc::UniqueIdentifier const entityModelID) noexcept
{
	if (entityModelID)
	{
		auto const [vendorID, deviceID, modelID] = la::avdecc::entity::model::splitEntityModelID(entityModelID);
		return vendorID != 0x00000000 && vendorID != 0x00FFFFFF;
	}
	return false;
}

static inline bool isEntityModelComplete(la::avdecc::UniqueIdentifier const entityID) noexcept
{
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(entityID);

	if (controlledEntity)
	{
		return controlledEntity->isEntityModelValidForCaching();
	}

	return true;
}

void MainWindowImpl::connectSignals()
{
	connect(&_interfaceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindowImpl::currentControllerChanged);
}

void MainWindow::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);

	static std::once_flag once;
	std::call_once(once,
		[this]()
		{
			_pImpl->_shown = true;

			// Check if we have a network interface selected
			{
				auto const interfaceID = _pImpl->_interfaceComboBox.currentData().toString();
				if (interfaceID.isEmpty())
				{
					// Postpone the dialog creation
					QTimer::singleShot(0,
						[this]()
						{
							QMessageBox::warning(this, "", "No Network Interface selected.\nPlease choose one in the Toolbar.");
						});
				}
			}
		});
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	qApp->closeAllWindows();

	QMainWindow::closeEvent(event);
}

QString MainWindowImpl::generateDumpSourceString() noexcept
{
	static auto s_DumpSource = QString{ "%1 v%2 using L-Acoustics AVDECC Controller v%3" }.arg(aemDumper::internals::applicationShortName).arg(aemDumper::internals::versionString).arg(la::avdecc::controller::getVersion().c_str());

	return s_DumpSource;
}

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
	, _pImpl(new MainWindowImpl(this))
{
	// Set title
	setWindowTitle(aemDumper::internals::applicationLongName + " - Version " + QCoreApplication::applicationVersion());
}

MainWindow::~MainWindow() noexcept
{
	delete _pImpl;
}
