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
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
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
	controllerTableView->setItemDelegateForColumn(ControllerModelEntityColumn_Compatibility, imageItemDelegate);

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
				dumpEntityModel->setEnabled(entity.isAemSupported && entityModelID);

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
									flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics };
								}
								else
								{
									flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel };
								}
								if (isBinary)
								{
									flags.set(la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat);
								}
								auto [error, message] = manager.serializeControlledEntityAsJson(entityID, filename, flags, generateDumpSourceString());
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
											auto const result = manager.serializeControlledEntityAsJson(entityID, filename, flags, generateDumpSourceString());
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
