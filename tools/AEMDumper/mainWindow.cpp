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

#include "ui_mainWindow.h"
#include "mainWindow.hpp"
#include "avdecc/helper.hpp"

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

#include <QtMate/widgets/dynamicHeaderView.hpp>
#include <QtMate/widgets/comboBox.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/widgetModelsLibrary/networkInterfacesListModel.hpp>
#include <hive/widgetModelsLibrary/discoveredEntitiesTableModel.hpp>
#include <hive/widgetModelsLibrary/discoveredEntitiesTableItemDelegate.hpp>

#include <mutex>
#include <memory>
#include <utility>

#include <QVector>

#define PROG_ID 0x0005
#define VENDOR_ID 0x001B92
#define DEVICE_ID 0x80
#define MODEL_ID 0x00000001

class MainWindowImpl final : public QObject, public Ui::MainWindow
{
	static constexpr auto ControllerModelEntityDataFlags = hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlags{ hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityLogo, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Compatibility, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityID, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Name, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Group, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::FirmwareVersion, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityModelID };
	static constexpr auto ControllerModelEntityColumn_EntityLogo = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityLogo);
	static constexpr auto ControllerModelEntityColumn_Compatibility = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Compatibility);
	static constexpr auto ControllerModelEntityColumn_EntityID = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityID);
	static constexpr auto ControllerModelEntityColumn_Name = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Name);
	static constexpr auto ControllerModelEntityColumn_Group = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Group);
	static constexpr auto ControllerModelEntityColumn_EntityModelID = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityModelID);
	static constexpr auto ControllerModelEntityColumn_FirmwareVersion = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::FirmwareVersion);

public:
	MainWindowImpl(::MainWindow* parent, QStringList ansFilesToLoad)
		: _parent(parent)
		, _ansFilesToLoad{ ansFilesToLoad }
	{
		// Setup entity model
		setupEntityModel();

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
	void setupEntityModel();
	void setupView();
	void registerMetaTypes();
	void createToolbars();
	void createControllerView();
	void connectSignals();

	// Private members
	::MainWindow* _parent{ nullptr };
	QStringList _ansFilesToLoad{};
	qtMate::widgets::ComboBox _interfaceComboBox{ _parent };
	hive::widgetModelsLibrary::NetworkInterfacesListModel _networkInterfacesModel{ false };
	QSortFilterProxyModel _networkInterfacesModelProxy{ _parent };
	qtMate::widgets::DynamicHeaderView _controllerDynamicHeaderView{ Qt::Horizontal, _parent };
	hive::widgetModelsLibrary::DiscoveredEntitiesTableModel _controllerModel{ ControllerModelEntityDataFlags };
	hive::widgetModelsLibrary::DiscoveredEntitiesTableItemDelegate _controllerModelItemDelegate{ _parent };
	bool _shown{ false };
	la::avdecc::entity::model::EntityTree _entityModel{};
};

void MainWindowImpl::setupEntityModel()
{
	auto& configTree = _entityModel.configurationTrees[la::avdecc::entity::model::ConfigurationIndex{ 0u }] = la::avdecc::entity::model::ConfigurationTree{};
	configTree.dynamicModel.isActiveConfiguration = true;

	_entityModel.dynamicModel.entityName = aemDumper::internals::applicationShortName.toStdString();
	_entityModel.dynamicModel.firmwareVersion = aemDumper::internals::versionString.toStdString();
}

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
	controllerTableView->setColumnWidth(ControllerModelEntityColumn_EntityModelID, 160);
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
		manager.createController(la::avdecc::protocol::ProtocolInterface::Type::PCap, interfaceID, progID, la::avdecc::entity::model::makeEntityModelID(VENDOR_ID, DEVICE_ID, MODEL_ID), "en", &_entityModel);
		manager.enableEntityAdvertising(10);

		// Try to load ANS files
		auto const flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics, la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat };
		for (auto const& file : _ansFilesToLoad)
		{
			auto const [error, message] = manager.loadVirtualEntitiesFromJsonNetworkState(file, flags);
			if (!!error)
			{
				QMessageBox::warning(_parent, "Failed to load Network State", QString("Error loading JSON file '%1':\n%2").arg(file).arg(message.c_str()));
			}
		}
		_ansFilesToLoad.clear();
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

	// Set delegate for the entire table
	controllerTableView->setItemDelegate(&_controllerModelItemDelegate);

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
	connect(controllerTableView, &QTableView::customContextMenuRequested, this,
		[this](QPoint const& pos)
		{
			auto const index = controllerTableView->indexAt(pos);

			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto const entityOpt = _controllerModel.entity(index.row());

			if (entityOpt)
			{
				QMenu menu;
				auto const& entity = (*entityOpt).get();
				auto const entityID = entity.entityID;
				auto const entityModelID = entity.entityModelID;

				// Dump Entity
				auto* const dumpFullEntity = menu.addAction("Export Full Entity...");
				auto* const dumpEntityModel = menu.addAction("Export Entity Model...");
				dumpEntityModel->setEnabled(entity.isAemSupported && entityModelID && entity.hasAnyConfigurationTree);

				menu.addSeparator();

				// Cancel
				menu.addAction("Cancel");

				if (auto* action = menu.exec(controllerTableView->viewport()->mapToGlobal(pos)))
				{
					if (action == dumpFullEntity || action == dumpEntityModel)
					{
						auto baseFileName = QString{};
						auto binaryFilterName = QString{};
						if (action == dumpFullEntity)
						{
							binaryFilterName = "AVDECC Virtual Entity Files (*.ave)";
							baseFileName = QString("%1/Entity_%2").arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).arg(hive::modelsLibrary::helper::uniqueIdentifierToString(entityID));
						}
						else
						{
							// Do some validation
							if (!isValidEntityModelID(entityModelID))
							{
								QMessageBox::warning(_parent, "", "EntityModelID is not valid (invalid Vendor OUI-24), cannot same the Model of this Entity.");
								return;
							}
							if (!isEntityModelComplete(entityID))
							{
								QMessageBox::warning(_parent, "", "'Full AEM Enumeration' option must be Enabled in order to export Model of a multi-configuration Entity.");
								return;
							}
							binaryFilterName = "AVDECC Entity Model Files (*.aem)";
							baseFileName = QString("%1/EntityModel_%2").arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).arg(hive::modelsLibrary::helper::uniqueIdentifierToString(entityModelID));
						}
						auto const dumpFile = [this, &entityID, &manager, isFullEntity = (action == dumpFullEntity)](auto const& baseFileName, auto const& binaryFilterName, auto const isBinary)
						{
							auto const filename = QFileDialog::getSaveFileName(_parent, "Save As...", baseFileName, binaryFilterName);
							if (!filename.isEmpty())
							{
								auto flags = la::avdecc::entity::model::jsonSerializer::Flags{};
								if (isFullEntity)
								{
									flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDiagnostics };
								}
								else
								{
									flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel };
								}
								if (isBinary)
								{
									flags.set(la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat);
								}
								auto [error, message] = manager.serializeControlledEntityAsJson(entityID, filename, flags, avdecc::helper::generateDumpSourceString(aemDumper::internals::applicationShortName, aemDumper::internals::versionString));
								if (!error)
								{
									QMessageBox::information(_parent, "", "Export successfully completed:\n" + filename);
								}
								else
								{
									if (error == la::avdecc::jsonSerializer::SerializationError::InvalidDescriptorIndex && isFullEntity)
									{
										auto const choice = QMessageBox::question(_parent, "", QString("EntityID %1 model is not fully IEEE1722.1 compliant.\n%2\n\nDo you want to export anyway?").arg(hive::modelsLibrary::helper::uniqueIdentifierToString(entityID)).arg(message.c_str()), QMessageBox::StandardButton::Yes, QMessageBox::StandardButton::No);
										if (choice == QMessageBox::StandardButton::Yes)
										{
											flags.set(la::avdecc::entity::model::jsonSerializer::Flag::IgnoreAEMSanityChecks);
											auto const result = manager.serializeControlledEntityAsJson(entityID, filename, flags, avdecc::helper::generateDumpSourceString(aemDumper::internals::applicationShortName, aemDumper::internals::versionString));
											error = std::get<0>(result);
											message = std::get<1>(result);
											if (!error)
											{
												QMessageBox::information(_parent, "", "Export completed but with warnings:\n" + filename);
											}
											// Fallthrough to warning message
										}
									}
									if (!!error)
									{
										QMessageBox::warning(_parent, "", QString("Export of EntityID %1 failed:\n%2").arg(hive::modelsLibrary::helper::uniqueIdentifierToString(entityID)).arg(message.c_str()));
									}
								}
							}
						};
						if (QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier))
						{
							dumpFile(baseFileName, "JSON Files (*.json)", false);
						}
						else
						{
							dumpFile(baseFileName, binaryFilterName, true);
						}
					}
				}
			}
		});

	auto* refreshController = new QShortcut{ QKeySequence{ "Ctrl+R" }, _parent };
	connect(refreshController, &QShortcut::activated, this, &MainWindowImpl::currentControllerChanged);
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
				else
				{
					// Postpone Controller start
					QTimer::singleShot(0,
						[this]()
						{
							_pImpl->currentControllerChanged();
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

void MainWindow::dragEnterEvent([[maybe_unused]] QDragEnterEvent* event)
{
#ifdef DEBUG
	for (auto const& u : event->mimeData()->urls())
	{
		auto const f = QFileInfo{ u.fileName() };
		auto const ext = f.suffix();
		if (ext == "ave" || ext == "ans" || ext == "json")
		{
			event->acceptProposedAction();
			return;
		}
	}
#endif // DEBUG
}

void MainWindow::dropEvent([[maybe_unused]] QDropEvent* event)
{
#ifdef DEBUG
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();

	auto const getErrorString = [](auto const error, auto const& message)
	{
		auto msg = QString{};
		if (!!error)
		{
			switch (error)
			{
				case la::avdecc::jsonSerializer::DeserializationError::AccessDenied:
					msg = "Access Denied";
					break;
				case la::avdecc::jsonSerializer::DeserializationError::FileReadError:
					msg = "Error Reading File";
					break;
				case la::avdecc::jsonSerializer::DeserializationError::UnsupportedDumpVersion:
					msg = "Unsupported Dump Version";
					break;
				case la::avdecc::jsonSerializer::DeserializationError::ParseError:
					msg = QString("Parse Error: %1").arg(message.c_str());
					break;
				case la::avdecc::jsonSerializer::DeserializationError::MissingKey:
					msg = QString("Missing Key: %1").arg(message.c_str());
					break;
				case la::avdecc::jsonSerializer::DeserializationError::InvalidKey:
					msg = QString("Invalid Key: %1").arg(message.c_str());
					break;
				case la::avdecc::jsonSerializer::DeserializationError::InvalidValue:
					msg = QString("Invalid Value: %1").arg(message.c_str());
					break;
				case la::avdecc::jsonSerializer::DeserializationError::OtherError:
					msg = message.c_str();
					break;
				case la::avdecc::jsonSerializer::DeserializationError::DuplicateEntityID:
					msg = QString("An Entity already exists with the same EntityID: %1").arg(message.c_str());
					break;
				case la::avdecc::jsonSerializer::DeserializationError::NotCompliant:
					msg = message.c_str();
					break;
				case la::avdecc::jsonSerializer::DeserializationError::Incomplete:
					msg = message.c_str();
					break;
				case la::avdecc::jsonSerializer::DeserializationError::NotSupported:
					msg = "Virtual Entity Loading not supported by this version of the AVDECC library";
					break;
				case la::avdecc::jsonSerializer::DeserializationError::InternalError:
					msg = QString("Internal Error: %1").arg(message.c_str());
					break;
				default:
					AVDECC_ASSERT(false, "Unknown Error");
					msg = "Unknown Error";
					break;
			}
		}
		return msg;
	};

	auto const loadEntity = [&manager, &getErrorString](auto const& filePath, auto const flags)
	{
		auto const [error, message] = manager.loadVirtualEntityFromJson(filePath, flags);
		return std::make_tuple(error, getErrorString(error, message));
	};

	auto const loadNetworkState = [&manager, &getErrorString](auto const& filePath, auto const flags)
	{
		auto const [error, message] = manager.loadVirtualEntitiesFromJsonNetworkState(filePath, flags);
		return std::make_tuple(error, getErrorString(error, message));
	};

	for (auto const& u : event->mimeData()->urls())
	{
		auto const f = u.toLocalFile();
		auto const fi = QFileInfo{ f };
		auto const ext = fi.suffix();

		// AVDECC Virtual Entity
		if (ext == "ave")
		{
			auto flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDiagnostics };
			flags.set(la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat);
			auto [error, message] = loadEntity(f, flags);
			if (!!error)
			{
				if (error == la::avdecc::jsonSerializer::DeserializationError::NotCompliant)
				{
					auto const choice = QMessageBox::question(this, "", "Entity model is not fully IEEE1722.1 compliant.\n\nDo you want to import anyway?", QMessageBox::StandardButton::Yes, QMessageBox::StandardButton::No);
					if (choice == QMessageBox::StandardButton::Yes)
					{
						flags.set(la::avdecc::entity::model::jsonSerializer::Flag::IgnoreAEMSanityChecks);
						auto const result = loadEntity(f, flags);
						error = std::get<0>(result);
						message = std::get<1>(result);
						// Fallthrough to warning message
					}
				}
				if (!!error)
				{
					QMessageBox::warning(this, "Failed to load Entity", QString("Error loading JSON file '%1':\n%2").arg(f).arg(message));
				}
			}
		}

		// AVDECC Network State
		else if (ext == "ans")
		{
			auto flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDiagnostics };
			flags.set(la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat);
			auto [error, message] = loadNetworkState(f, flags);
			if (!!error)
			{
				QMessageBox::warning(this, "Failed to load Network State", QString("Error loading JSON file '%1':\n%2").arg(f).arg(message));
			}
		}

		// Any kind of file, we have to autodetect
		else if (ext == "json")
		{
			auto flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDiagnostics };
			flags.set(la::avdecc::entity::model::jsonSerializer::Flag::IgnoreAEMSanityChecks);
			// Start with AVE file type
			auto [error, message] = loadEntity(f, flags);
			if (!!error)
			{
				// Then try ANS file type
				loadNetworkState(f, flags);
			}
		}
	}
#endif // DEBUG
}


MainWindow::MainWindow(QStringList ansFilesToLoad, QWidget* parent)
	: QMainWindow(parent)
	, _pImpl(new MainWindowImpl(this, ansFilesToLoad))
{
	// Set title
	setWindowTitle(QCoreApplication::applicationName() + " - Version " + QCoreApplication::applicationVersion());

#ifdef DEBUG
	// Register AcceptDrops so we can drop VirtualEntities as JSON
	setAcceptDrops(true);
#endif
}

MainWindow::~MainWindow() noexcept
{
	delete _pImpl;
}
