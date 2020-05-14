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

#include "avdecc/helper.hpp"
#include "avdecc/hiveLogItems.hpp"
#include "avdecc/channelConnectionManager.hpp"
#include "avdecc/controllerModel.hpp"
#include "avdecc/mcDomainManager.hpp"
#include "mediaClock/mediaClockManagementDialog.hpp"
#include "internals/config.hpp"
#include "profiles/profiles.hpp"
#include "settingsManager/settings.hpp"
#include "settingsManager/settingsSignaler.hpp"
#include "sparkleHelper/sparkleHelper.hpp"
#include "activeNetworkInterfacesModel.hpp"
#include "aboutDialog.hpp"
#include "deviceDetailsDialog.hpp"
#include "nodeVisitor.hpp"
#include "settingsDialog.hpp"
#include "multiFirmwareUpdateDialog.hpp"
#include "defaults.hpp"
#include "windowsNpfHelper.hpp"

#include <QtMate/widgets/comboBox.hpp>
#include <QtMate/widgets/flatIconButton.hpp>
#include <QtMate/widgets/dynamicHeaderView.hpp>
#include <QtMate/material/color.hpp>
#include <QtMate/material/colorPalette.hpp>
#include <la/avdecc/networkInterfaceHelper.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/widgetModelsLibrary/entityLogoCache.hpp>
#include <hive/widgetModelsLibrary/errorItemDelegate.hpp>
#include <hive/widgetModelsLibrary/imageItemDelegate.hpp>

#include <mutex>
#include <memory>

extern "C"
{
#include <mkdio.h>
}

#define PROG_ID 0x0003
#define VENDOR_ID 0x001B92
#define DEVICE_ID 0x80
#define MODEL_ID 0x00000001

Q_DECLARE_METATYPE(la::avdecc::protocol::ProtocolInterface::Type)

class MainWindowImpl final : public QObject, public Ui::MainWindow, public settings::SettingsManager::Observer
{
public:
	MainWindowImpl(::MainWindow* parent)
		: _parent(parent)
		, _controllerModel(new avdecc::ControllerModel(parent)) // parent takes ownership of the object -> 'new' required
	{
		// Setup common UI
		setupUi(parent);

		// Register all Qt metatypes
		registerMetaTypes();

		// Setup the current profile
		setupProfile();
	}

	// Deleted compiler auto-generated methods
	MainWindowImpl(MainWindowImpl const&) = delete;
	MainWindowImpl(MainWindowImpl&&) = delete;
	MainWindowImpl& operator=(MainWindowImpl const&) = delete;
	MainWindowImpl& operator=(MainWindowImpl&&) = delete;

	// Private Structs
	struct Defaults
	{
		// MainWindow widgets
		bool mainWindow_Toolbar_Visible{ true };
		bool mainWindow_Inspector_Visible{ true };
		bool mainWindow_Logger_Visible{ true };

		// Controller Table View
		bool controllerTableView_EntityLogo_Visible{ true };
		bool controllerTableView_Compatibility_Visible{ true };
		bool controllerTableView_Name_Visible{ true };
		bool controllerTableView_Group_Visible{ true };
		bool controllerTableView_AcquireState_Visible{ true };
		bool controllerTableView_LockState_Visible{ true };
		bool controllerTableView_GrandmasterID_Visible{ true };
		bool controllerTableView_GptpDomain_Visible{ true };
		bool controllerTableView_InterfaceIndex_Visible{ true };
		bool controllerTableView_AssociationID_Visible{ true };
		bool controllerTableView_MediaClockMasterID_Visible{ true };
		bool controllerTableView_MediaClockMasterName_Visible{ true };
	};

	// Private Slots
	Q_SLOT void currentControllerChanged();
	Q_SLOT void currentControlledEntityChanged(QModelIndex const& index);

	// Private methods
	void setupAdvancedView(Defaults const& defaults);
	void setupStandardProfile();
	void setupDeveloperProfile();
	void setupProfile();
	void registerMetaTypes();
	void createViewMenu();
	void createToolbars();
	void createControllerView();
	void checkNpfStatus();
	void loadSettings();
	void connectSignals();
	void showChangeLog(QString const title, QString const versionString);
	void updateStyleSheet(qtMate::material::color::Name const colorName, QString const& filename);
	static QString generateDumpSourceString() noexcept;

	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

	// Private members
	::MainWindow* _parent{ nullptr };
	qtMate::widgets::ComboBox _interfaceComboBox{ _parent };
	ActiveNetworkInterfacesModel _activeNetworkInterfacesModel{ _parent };
	QSortFilterProxyModel _networkInterfacesModelProxy{ _parent };
	qtMate::widgets::FlatIconButton _refreshControllerButton{ "Material Icons", "refresh", _parent };
	qtMate::widgets::FlatIconButton _discoverButton{ "Material Icons", "cast", _parent };
	qtMate::widgets::FlatIconButton _openMcmdDialogButton{ "Material Icons", "schedule", _parent };
	qtMate::widgets::FlatIconButton _openMultiFirmwareUpdateDialogButton{ "Hive", "firmware_upload", _parent };
	qtMate::widgets::FlatIconButton _openSettingsButton{ "Hive", "settings", _parent };
	QLabel _controllerEntityIDLabel{ _parent };
	qtMate::widgets::DynamicHeaderView _controllerDynamicHeaderView{ Qt::Horizontal, _parent };
	avdecc::ControllerModel* _controllerModel{ nullptr };
	bool _shown{ false };
	SettingsSignaler _settingsSignaler{};
};

void MainWindowImpl::setupAdvancedView(Defaults const& defaults)
{
	// Create "view" sub-menu
	createViewMenu();

	// Create toolbars
	createToolbars();

	// Create the ControllerView widget
	createControllerView();

	// Initialize UI defaults
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityLogo), !defaults.controllerTableView_EntityLogo_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Compatibility), !defaults.controllerTableView_Compatibility_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Name), !defaults.controllerTableView_Name_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Group), !defaults.controllerTableView_Group_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AcquireState), !defaults.controllerTableView_AcquireState_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::LockState), !defaults.controllerTableView_LockState_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GrandmasterID), !defaults.controllerTableView_GrandmasterID_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GptpDomain), !defaults.controllerTableView_GptpDomain_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::InterfaceIndex), !defaults.controllerTableView_InterfaceIndex_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AssociationID), !defaults.controllerTableView_AssociationID_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterID), !defaults.controllerTableView_MediaClockMasterID_Visible);
	controllerTableView->setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterName), !defaults.controllerTableView_MediaClockMasterName_Visible);

	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityLogo), defaults::ui::AdvancedView::ColumnWidth_Logo);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Compatibility), defaults::ui::AdvancedView::ColumnWidth_Compatibility);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityID), defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Name), defaults::ui::AdvancedView::ColumnWidth_Name);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Group), defaults::ui::AdvancedView::ColumnWidth_Group);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AcquireState), defaults::ui::AdvancedView::ColumnWidth_ExclusiveAccessState);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::LockState), defaults::ui::AdvancedView::ColumnWidth_ExclusiveAccessState);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GrandmasterID), defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GptpDomain), defaults::ui::AdvancedView::ColumnWidth_GPTPDomain);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::InterfaceIndex), defaults::ui::AdvancedView::ColumnWidth_InterfaceIndex);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AssociationID), defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterID), defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	controllerTableView->setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterName), defaults::ui::AdvancedView::ColumnWidth_Name);

	controllerToolBar->setVisible(defaults.mainWindow_Toolbar_Visible);
	entityInspectorDockWidget->setVisible(defaults.mainWindow_Inspector_Visible);
	loggerDockWidget->setVisible(defaults.mainWindow_Logger_Visible);

	// Load settings, overriding defaults
	loadSettings();

	// Configure Widget parameters
	splitter->setStretchFactor(0, 0); // Entities List has less weight than
	splitter->setStretchFactor(1, 1); // the Matrix, as far as expand is concerned

	// Connect all signals
	connectSignals();

	// Create channel connection manager instance
	avdecc::ChannelConnectionManager::getInstance();
}

void MainWindowImpl::setupStandardProfile()
{
	setupAdvancedView(Defaults{ true, false, false, true, true, true, true, false, false, false, false, false, false, true, true });
}

void MainWindowImpl::setupDeveloperProfile()
{
	setupAdvancedView(Defaults{});
}

void MainWindowImpl::setupProfile()
{
	// Update the UI and other stuff, based on the current Profile
	auto& settings = settings::SettingsManager::getInstance();

	auto const userProfile = settings.getValue(settings::UserProfile.name).value<profiles::ProfileType>();

	switch (userProfile)
	{
		case profiles::ProfileType::None:
		default:
			AVDECC_ASSERT(false, "No profile selected");
			[[fallthrough]];
		case profiles::ProfileType::Standard:
			setupStandardProfile();
			break;
		case profiles::ProfileType::Developer:
			setupDeveloperProfile();
			break;
	}
}

void MainWindowImpl::currentControllerChanged()
{
	auto& settings = settings::SettingsManager::getInstance();

	auto const protocolType = settings.getValue(settings::Network_ProtocolType.name).value<la::avdecc::protocol::ProtocolInterface::Type>();
	auto const interfaceID = _interfaceComboBox.currentData().toString();

	// Check for WinPcap driver
	if (protocolType == la::avdecc::protocol::ProtocolInterface::Type::PCap && _shown)
	{
		checkNpfStatus();
	}

	// Clear the current controller
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	manager.destroyController();
	_controllerEntityIDLabel.clear();

	if (interfaceID.isEmpty())
	{
		LOG_HIVE_WARN("No Network Interface selected. Please choose one.");
		return;
	}

	settings.setValue(settings::InterfaceID, interfaceID);

	try
	{
		// Create a new Controller
#ifdef DEBUG
		auto const progID = std::uint16_t{ PROG_ID + 1 }; // Use next PROG_ID in debug (so we can launch 2 Hive instances at the same time, one in Release and one in Debug)
#else // !DEBUG
		auto const progID = std::uint16_t{ PROG_ID };
#endif // DEBUG
		manager.createController(protocolType, interfaceID, progID, la::avdecc::entity::model::makeEntityModelID(VENDOR_ID, DEVICE_ID, MODEL_ID), "en");
		_controllerEntityIDLabel.setText(hive::modelsLibrary::helper::uniqueIdentifierToString(manager.getControllerEID()));
	}
	catch (la::avdecc::controller::Controller::Exception const& e)
	{
		LOG_HIVE_WARN(e.what());
#ifdef __linux__
		if (e.getError() == la::avdecc::controller::Controller::Error::InterfaceOpenError)
		{
			LOG_HIVE_INFO("Make sure Hive is allowed to use RAW SOCKETS. Close Hive and run the following command to give it access (you can select the log line and use CTRL+C to copy the command)");
			LOG_HIVE_INFO("sudo setcap cap_net_raw+ep Hive");
		}
#endif // __linux__
	}
}

void MainWindowImpl::currentControlledEntityChanged(QModelIndex const& index)
{
	if (!index.isValid())
	{
		entityInspector->setControlledEntityID(la::avdecc::UniqueIdentifier{});
		return;
	}

	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto const entityID = _controllerModel->controlledEntityID(index);
	auto controlledEntity = manager.getControlledEntity(entityID);

	if (controlledEntity)
	{
		entityInspector->setControlledEntityID(entityID);
	}
}

// Private methods
void MainWindowImpl::registerMetaTypes()
{
	//
	qRegisterMetaType<la::avdecc::logger::Layer>("la::avdecc::logger::Layer");
	qRegisterMetaType<la::avdecc::logger::Level>("la::avdecc::logger::Level");
}

void MainWindowImpl::createViewMenu()
{
	// Exclusive connection matrix modes
	auto* actionGroup = new QActionGroup{ this };
	actionGroup->addAction(actionStreamModeRouting);
	actionGroup->addAction(actionChannelModeRouting);
	menuView->addSeparator();

	// Toolbars visibility toggle
	menuView->addAction(controllerToolBar->toggleViewAction());
	menuView->addAction(utilitiesToolBar->toggleViewAction());
	menuView->addSeparator();

	// Entity Inspector visibility toggle
	menuView->addAction(entityInspectorDockWidget->toggleViewAction());
	menuView->addSeparator();

	// Logger visibility toggle
	menuView->addAction(loggerDockWidget->toggleViewAction());
}

void MainWindowImpl::createToolbars()
{
	// Controller Toolbar
	{
		auto* interfaceLabel = new QLabel("Interface");
		interfaceLabel->setMinimumWidth(50);
		_interfaceComboBox.setMinimumWidth(100);
		_interfaceComboBox.setModel(&_activeNetworkInterfacesModel);

		// The combobox takes ownership of the item delegate
		auto* interfaceComboBoxItemDelegate = new hive::widgetModelsLibrary::ErrorItemDelegate{ qtMate::material::color::Palette::name(settings::SettingsManager::getInstance().getValue(settings::General_ThemeColorIndex.name).toInt()) };
		_interfaceComboBox.setItemDelegate(interfaceComboBoxItemDelegate);
		connect(&_settingsSignaler, &SettingsSignaler::themeColorNameChanged, interfaceComboBoxItemDelegate, &hive::widgetModelsLibrary::ErrorItemDelegate::setThemeColorName);

		auto* controllerEntityIDLabel = new QLabel("Controller ID: ");
		controllerEntityIDLabel->setMinimumWidth(50);
		_controllerEntityIDLabel.setMinimumWidth(100);

		controllerToolBar->setMinimumHeight(30);
		controllerToolBar->addWidget(interfaceLabel);
		controllerToolBar->addWidget(&_interfaceComboBox);
		controllerToolBar->addSeparator();
		controllerToolBar->addWidget(controllerEntityIDLabel);
		controllerToolBar->addWidget(&_controllerEntityIDLabel);
	}

	// Utilities Toolbar
	{
		_refreshControllerButton.setToolTip("Reload Controller");
		_discoverButton.setToolTip("Force Entities Discovery");
		_openMcmdDialogButton.setToolTip("Media Clock Management");
		_openSettingsButton.setToolTip("Settings");
		_openMultiFirmwareUpdateDialogButton.setToolTip("Device Firmware Update");

		// Controller
		utilitiesToolBar->setMinimumHeight(30);
		utilitiesToolBar->addWidget(&_refreshControllerButton);
		utilitiesToolBar->addWidget(&_discoverButton);

		// Tools
		utilitiesToolBar->addSeparator();
		utilitiesToolBar->addWidget(&_openMcmdDialogButton);
		utilitiesToolBar->addWidget(&_openMultiFirmwareUpdateDialogButton);

		// Settings
		utilitiesToolBar->addSeparator();
		utilitiesToolBar->addWidget(&_openSettingsButton);
	}

#ifdef Q_OS_MAC
	// See https://bugreports.qt.io/browse/QTBUG-13635
	controllerToolBar->setStyleSheet("QToolBar QLabel { padding-bottom: 5; }");
	utilitiesToolBar->setStyleSheet("QToolBar QLabel { padding-bottom: 5; }");
#endif
}

void MainWindowImpl::createControllerView()
{
	controllerTableView->setModel(_controllerModel);
	controllerTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	controllerTableView->setSelectionMode(QAbstractItemView::SingleSelection);
	controllerTableView->setContextMenuPolicy(Qt::CustomContextMenu);
	controllerTableView->setFocusPolicy(Qt::ClickFocus);

	// Disable row resizing
	controllerTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

	// The table view does not take ownership on the item delegate
	auto* imageItemDelegate{ new hive::widgetModelsLibrary::ImageItemDelegate{ _parent } };
	controllerTableView->setItemDelegateForColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityLogo), imageItemDelegate);
	controllerTableView->setItemDelegateForColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Compatibility), imageItemDelegate);
	controllerTableView->setItemDelegateForColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AcquireState), imageItemDelegate);
	controllerTableView->setItemDelegateForColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::LockState), imageItemDelegate);

	// The table view does not take ownership on the item delegate
	auto* errorItemDelegate{ new hive::widgetModelsLibrary::ErrorItemDelegate{ qtMate::material::color::Palette::name(settings::SettingsManager::getInstance().getValue(settings::General_ThemeColorIndex.name).toInt()), _parent } };
	controllerTableView->setItemDelegateForColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityID), errorItemDelegate);
	connect(&_settingsSignaler, &SettingsSignaler::themeColorNameChanged, errorItemDelegate, &hive::widgetModelsLibrary::ErrorItemDelegate::setThemeColorName);

	_controllerDynamicHeaderView.setHighlightSections(false);
	_controllerDynamicHeaderView.setMandatorySection(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityID));
	controllerTableView->setHorizontalHeader(&_controllerDynamicHeaderView);
}

void MainWindowImpl::checkNpfStatus()
{
#ifdef _WIN32
	static std::once_flag once;
	std::call_once(once,
		[this]()
		{
			auto const npfStatus = npf::getStatus();
			switch (npfStatus)
			{
				case npf::Status::NotInstalled:
					QMessageBox::warning(_parent, "", "The WinPcap library is required for Hive to communicate with AVB Entities on the network.\nIt looks like you uninstalled it, or didn't choose to install it when running Hive installation.\n\nYou need to rerun the installer and follow the instructions to install WinPcap.");
					break;
				case npf::Status::NotStarted:
				{
					auto choice = QMessageBox::warning(_parent, "", "The WinPcap library must be started for Hive to communicate with AVB Entities on the network.\n\nDo you want to start WinPcap now?", QMessageBox::StandardButton::Yes, QMessageBox::StandardButton::No);
					if (choice == QMessageBox::StandardButton::Yes)
					{
						npf::startService();
						choice = QMessageBox::question(_parent, "", "Do you want to configure the library to automatically start when Windows boots (recommended)?", QMessageBox::StandardButton::Yes, QMessageBox::StandardButton::No);
						if (choice == QMessageBox::StandardButton::Yes)
						{
							npf::setServiceAutoStart();
						}
						// Postpone Controller Refresh
						QTimer::singleShot(0,
							[this]()
							{
								currentControllerChanged();
							});
					}
					break;
				}
				default:
					break;
			}
		});
#endif // _WIN32
}

void MainWindowImpl::loadSettings()
{
	auto& settings = settings::SettingsManager::getInstance();

	LOG_HIVE_DEBUG("Settings location: " + settings.getFilePath());

	auto const networkInterfaceId = settings.getValue(settings::InterfaceID).toString();
	auto const networkInterfaceIndex = _interfaceComboBox.findData(networkInterfaceId);

	// Select the interface from the settings, if present and active
	if (networkInterfaceIndex >= 0 && _activeNetworkInterfacesModel.isEnabled(networkInterfaceId))
	{
		_interfaceComboBox.setCurrentIndex(networkInterfaceIndex);
	}
	else
	{
		_interfaceComboBox.setCurrentIndex(-1);
	}

	// Check if currently saved ProtocolInterface is supported
	auto protocolType = settings.getValue(settings::Network_ProtocolType.name).value<la::avdecc::protocol::ProtocolInterface::Type>();
	auto supportedTypes = la::avdecc::protocol::ProtocolInterface::getSupportedProtocolInterfaceTypes();
	if (!supportedTypes.test(protocolType))
	{
		auto const wasConfigured = protocolType != la::avdecc::protocol::ProtocolInterface::Type::None;
		// Previous ProtocolInterface Type is no longer supported
		if (wasConfigured)
		{
			LOG_HIVE_WARN(QString("Previously configured Network Protocol is no longer supported: %1").arg(QString::fromStdString(la::avdecc::protocol::ProtocolInterface::typeToString(protocolType))));
		}
		if (!supportedTypes.empty())
		{
			// Force the first supported ProtocolInterface, and save it to the settings, before we call registerSettingObserver
			protocolType = *supportedTypes.begin();
			if (wasConfigured)
			{
				LOG_HIVE_WARN(QString("Forcing new Network Protocol: %1").arg(QString::fromStdString(la::avdecc::protocol::ProtocolInterface::typeToString(protocolType))));
			}
			settings.setValue(settings::Network_ProtocolType.name, la::avdecc::utils::to_integral(protocolType));
		}
	}

	// Configure connectionMatrix mode
	auto const channelMode = settings.getValue(settings::ConnectionMatrix_ChannelMode.name).toBool();
	auto* action = channelMode ? actionChannelModeRouting : actionStreamModeRouting;
	action->setChecked(true);

	_controllerDynamicHeaderView.restoreState(settings.getValue(settings::ControllerDynamicHeaderViewState).toByteArray());
	loggerView->header()->restoreState(settings.getValue(settings::LoggerDynamicHeaderViewState).toByteArray());
	entityInspector->restoreState(settings.getValue(settings::EntityInspectorState).toByteArray());
	splitter->restoreState(settings.getValue(settings::SplitterState).toByteArray());

	// Configure settings observers
	settings.registerSettingObserver(settings::Network_ProtocolType.name, this);
	settings.registerSettingObserver(settings::Controller_DiscoveryDelay.name, this);
	settings.registerSettingObserver(settings::Controller_AemCacheEnabled.name, this);
	settings.registerSettingObserver(settings::Controller_FullStaticModelEnabled.name, this);
	settings.registerSettingObserver(settings::ConnectionMatrix_ChannelMode.name, this);
	settings.registerSettingObserver(settings::General_ThemeColorIndex.name, this);
	settings.registerSettingObserver(settings::General_AutomaticCheckForUpdates.name, this);
	settings.registerSettingObserver(settings::General_CheckForBetaVersions.name, this);

	// Start the SettingsSignaler
	_settingsSignaler.start();
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
	connect(&_refreshControllerButton, &QPushButton::clicked, this, &MainWindowImpl::currentControllerChanged);
	connect(&_discoverButton, &QPushButton::clicked, this,
		[]()
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			manager.discoverRemoteEntities();
		});

	connect(&_openMcmdDialogButton, &QPushButton::clicked, actionMediaClockManagement, &QAction::trigger);
	connect(&_openMultiFirmwareUpdateDialogButton, &QPushButton::clicked, actionDeviceFirmwareUpdate, &QAction::trigger);

	connect(&_openSettingsButton, &QPushButton::clicked, actionSettings, &QAction::trigger);

	connect(actionChannelModeRouting, &QAction::toggled, this,
		[this](bool checked)
		{
			// Update settings
			auto& settings = settings::SettingsManager::getInstance();
			settings.setValue(settings::ConnectionMatrix_ChannelMode.name, checked);
			// Toggle
			auto* action = checked ? actionChannelModeRouting : actionStreamModeRouting;
			action->setChecked(true);
		});

	connect(controllerTableView->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindowImpl::currentControlledEntityChanged);
	connect(&_controllerDynamicHeaderView, &qtMate::widgets::DynamicHeaderView::sectionChanged, this,
		[this]()
		{
			auto& settings = settings::SettingsManager::getInstance();
			settings.setValue(settings::ControllerDynamicHeaderViewState, _controllerDynamicHeaderView.saveState());
		});

	connect(controllerTableView, &QTableView::doubleClicked, this,
		[this](QModelIndex const& index)
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto const entityID = _controllerModel->controlledEntityID(index);
			auto controlledEntity = manager.getControlledEntity(entityID);

			if (controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
			{
				DeviceDetailsDialog* dialog = new DeviceDetailsDialog(_parent);
				dialog->setAttribute(Qt::WA_DeleteOnClose);
				dialog->setControlledEntityID(entityID);
				dialog->show();
			}
		});

	connect(controllerTableView, &QTableView::customContextMenuRequested, this,
		[this](QPoint const& pos)
		{
			auto const index = controllerTableView->indexAt(pos);

			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto const entityID = _controllerModel->controlledEntityID(index);
			auto controlledEntity = manager.getControlledEntity(entityID);

			if (controlledEntity)
			{
				QMenu menu;
				auto const& entity = controlledEntity->getEntity();
				auto const entityModelID = entity.getEntityModelID();

				auto* acquireAction{ static_cast<QAction*>(nullptr) };
				auto* releaseAction{ static_cast<QAction*>(nullptr) };
				auto* lockAction{ static_cast<QAction*>(nullptr) };
				auto* unlockAction{ static_cast<QAction*>(nullptr) };
				auto* deviceView{ static_cast<QAction*>(nullptr) };
				auto* inspect{ static_cast<QAction*>(nullptr) };
				auto* getLogo{ static_cast<QAction*>(nullptr) };
				auto* clearErrorFlags{ static_cast<QAction*>(nullptr) };
				auto* dumpFullEntity{ static_cast<QAction*>(nullptr) };
				auto* dumpEntityModel{ static_cast<QAction*>(nullptr) };

				if (entity.getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
				{
					// Do not propose Acquire if the device is Milan (not supported)
					if (!controlledEntity->getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
					{
						QString acquireText;
						auto const isAcquired = controlledEntity->isAcquired();
						auto const isAcquiredByOther = controlledEntity->isAcquiredByOther();

						{
							if (isAcquiredByOther)
							{
								acquireText = "Try to acquire";
							}
							else
							{
								acquireText = "Acquire";
							}
							acquireAction = menu.addAction(acquireText);
							acquireAction->setEnabled(!isAcquired);
						}
						{
							releaseAction = menu.addAction("Release");
							releaseAction->setEnabled(isAcquired);
						}
					}
					// Lock
					{
						QString lockText;
						auto const isLocked = controlledEntity->isLocked();
						auto const isLockedByOther = controlledEntity->isLockedByOther();

						{
							if (isLockedByOther)
							{
								lockText = "Try to lock";
							}
							else
							{
								lockText = "Lock";
							}
							lockAction = menu.addAction(lockText);
							lockAction->setEnabled(!isLocked);
						}
						{
							unlockAction = menu.addAction("Unlock");
							unlockAction->setEnabled(isLocked);
						}
					}

					menu.addSeparator();

					// Device Details, Inspect, Logo, ...
					{
						deviceView = menu.addAction("Device Details...");
					}
					{
						inspect = menu.addAction("Inspect Entity Model...");
					}
					{
						getLogo = menu.addAction("Retrieve Entity Logo");
						getLogo->setEnabled(!hive::widgetModelsLibrary::EntityLogoCache::getInstance().isImageInCache(entityID, hive::widgetModelsLibrary::EntityLogoCache::Type::Entity));
					}
					{
						clearErrorFlags = menu.addAction("Acknowledge Counters Errors");
					}
				}

				menu.addSeparator();

				// Dump Entity
				{
					dumpFullEntity = menu.addAction("Export Full Entity...");
					dumpEntityModel = menu.addAction("Export Entity Model...");
					dumpEntityModel->setEnabled(entity.getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) && entityModelID);
				}

				menu.addSeparator();

				// Cancel
				menu.addAction("Cancel");

				// Release the controlled entity before starting a long operation (menu.exec)
				controlledEntity.reset();

				if (auto* action = menu.exec(controllerTableView->viewport()->mapToGlobal(pos)))
				{
					if (action == acquireAction)
					{
						manager.acquireEntity(entityID, false);
					}
					else if (action == releaseAction)
					{
						manager.releaseEntity(entityID);
					}
					else if (action == lockAction)
					{
						manager.lockEntity(entityID);
					}
					else if (action == unlockAction)
					{
						manager.unlockEntity(entityID);
					}
					else if (action == deviceView)
					{
						DeviceDetailsDialog* dialog = new DeviceDetailsDialog(_parent);
						dialog->setAttribute(Qt::WA_DeleteOnClose);
						dialog->setControlledEntityID(entityID);
						dialog->show();
					}
					else if (action == inspect)
					{
						auto* inspector = new EntityInspector;
						inspector->setAttribute(Qt::WA_DeleteOnClose);
						inspector->setControlledEntityID(entityID);
						inspector->restoreGeometry(entityInspector->saveGeometry());
						inspector->show();
					}
					else if (action == getLogo)
					{
						hive::widgetModelsLibrary::EntityLogoCache::getInstance().getImage(entityID, hive::widgetModelsLibrary::EntityLogoCache::Type::Entity, true);
					}
					else if (action == clearErrorFlags)
					{
						manager.clearAllStreamInputCounterValidFlags(entityID);
						manager.clearAllStatisticsCounterValidFlags(entityID);
					}
					else if (action == dumpFullEntity || action == dumpEntityModel)
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

	connect(entityInspector, &EntityInspector::stateChanged, this,
		[this]()
		{
			auto& settings = settings::SettingsManager::getInstance();
			settings.setValue(settings::EntityInspectorState, entityInspector->saveState());
		});

	connect(loggerView->header(), &qtMate::widgets::DynamicHeaderView::sectionChanged, this,
		[this]()
		{
			auto& settings = settings::SettingsManager::getInstance();
			settings.setValue(settings::LoggerDynamicHeaderViewState, loggerView->header()->saveState());
		});

	connect(splitter, &QSplitter::splitterMoved, this,
		[this]()
		{
			auto& settings = settings::SettingsManager::getInstance();
			settings.setValue(settings::SplitterState, splitter->saveState());
		});

	// Connect ControllerManager events
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	connect(&manager, &hive::modelsLibrary::ControllerManager::transportError, this,
		[this]()
		{
			LOG_HIVE_ERROR("Error reading from the active Network Interface");
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::endAecpCommand, this,
		[this](la::avdecc::UniqueIdentifier const /*entityID*/, hive::modelsLibrary::ControllerManager::AecpCommandType commandType, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
		{
			if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::Success)
			{
				QMessageBox::warning(_parent, "", "<i>" + hive::modelsLibrary::ControllerManager::typeToString(commandType) + "</i> failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::endAcmpCommand, this,
		[this](la::avdecc::UniqueIdentifier const /*talkerEntityID*/, la::avdecc::entity::model::StreamIndex const /*talkerStreamIndex*/, la::avdecc::UniqueIdentifier const /*listenerEntityID*/, la::avdecc::entity::model::StreamIndex const /*listenerStreamIndex*/, hive::modelsLibrary::ControllerManager::AcmpCommandType commandType, la::avdecc::entity::ControllerEntity::ControlStatus const status)
		{
			if (status != la::avdecc::entity::ControllerEntity::ControlStatus::Success)
			{
				QMessageBox::warning(_parent, "", "<i>" + hive::modelsLibrary::ControllerManager::typeToString(commandType) + "</i> failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
			}
		});

	//

	connect(actionExportFullNetworkState, &QAction::triggered, this,
		[this]()
		{
			auto filterName = QString{};
			auto flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics };
			if (QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier))
			{
				filterName = "JSON Files (*.json)";
			}
			else
			{
				filterName = "AVDECC Network State Files (*.ans)";
				flags.set(la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat);
			}
			auto const filename = QFileDialog::getSaveFileName(_parent, "Save As...", QString("%1/FullDump_%2").arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")), filterName);
			if (!filename.isEmpty())
			{
				auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
				auto [error, message] = manager.serializeAllControlledEntitiesAsJson(filename, flags, generateDumpSourceString());
				if (!error)
				{
					QMessageBox::information(_parent, "", "Export successfully completed:\n" + filename);
				}
				else
				{
					QMessageBox::warning(_parent, "", QString("Export failed:\n%1").arg(message.c_str()));
				}
			}
		});

	//

	connect(actionSettings, &QAction::triggered, this,
		[this]()
		{
			SettingsDialog dialog{ _parent };
			dialog.exec();
		});

	connect(actionDeviceFirmwareUpdate, &QAction::triggered, this,
		[this]()
		{
			MultiFirmwareUpdateDialog dialog{ _parent };
			dialog.exec();
		});

	connect(actionMediaClockManagement, &QAction::triggered, this,
		[this]()
		{
			MediaClockManagementDialog dialog{ _parent };
			dialog.exec();
		});

	//

	connect(actionAbout, &QAction::triggered, this,
		[this]()
		{
			AboutDialog dialog{ _parent };
			dialog.exec();
		});

	//

	connect(actionChangeLog, &QAction::triggered, this,
		[this]()
		{
			showChangeLog("Change Log", "");
		});

	//

	connect(actionOpenProjectWebPage, &QAction::triggered, this,
		[]()
		{
			QDesktopServices::openUrl(hive::internals::projectURL);
		});

	//

	connect(actionCheckForUpdates, &QAction::triggered, this,
		[]()
		{
			Sparkle::getInstance().manualCheckForUpdate();
		});


	// Connect keyboard shortcuts

	auto* refreshController = new QShortcut{ QKeySequence{ "Ctrl+R" }, _parent };
	connect(refreshController, &QShortcut::activated, this, &MainWindowImpl::currentControllerChanged);

	auto* toggleMatrixMode = new QShortcut{ QKeySequence{ "Ctrl+M" }, _parent };
	connect(toggleMatrixMode, &QShortcut::activated, this,
		[this]()
		{
			// Toggle mode (stream based vs. channel based routing matrix) in settings
			auto& settings = settings::SettingsManager::getInstance();
			auto const channelModeActiveInverted = !settings.getValue(settings::ConnectionMatrix_ChannelMode.name).toBool();
			auto* action = channelModeActiveInverted ? actionChannelModeRouting : actionStreamModeRouting;
			action->setChecked(true);
		});

#ifdef DEBUG
	auto* reloadStyleSheet = new QShortcut{ QKeySequence{ "F5" }, _parent };
	connect(reloadStyleSheet, &QShortcut::activated, _parent,
		[this]()
		{
			auto& settings = settings::SettingsManager::getInstance();
			auto const themeColorIndex = settings.getValue(settings::General_ThemeColorIndex.name).toInt();
			auto const colorName = qtMate::material::color::Palette::name(themeColorIndex);
			updateStyleSheet(colorName, QString{ RESOURCES_ROOT_DIR } + "/style.qss");
			LOG_HIVE_DEBUG("StyleSheet reloaded");
		});
#endif

	// Connect Sparkle events
	auto& sparkle = Sparkle::getInstance();
	sparkle.setIsShutdownAllowedHandler(
		[]()
		{
			return true;
		});
	sparkle.setLogHandler(
		[this](auto const& message, auto const level)
		{
			auto const qMessage = QString::fromStdString(message);
			switch (level)
			{
				case Sparkle::LogLevel::Info:
					LOG_HIVE_INFO(qMessage);
					break;
				case Sparkle::LogLevel::Warn:
					LOG_HIVE_WARN(qMessage);
					break;
				case Sparkle::LogLevel::Error:
					LOG_HIVE_ERROR(qMessage);
					break;
				default:
					LOG_HIVE_DEBUG(qMessage);
					break;
			}
		});
}

void MainWindowImpl::showChangeLog(QString const title, QString const versionString)
{
	// Create dialog popup
	QDialog dialog{ _parent };
	QVBoxLayout layout{ &dialog };
	QTextBrowser view;
	layout.addWidget(&view);
	dialog.setWindowTitle(hive::internals::applicationShortName + " - " + title);
	dialog.resize(800, 600);
	QPushButton closeButton{ "Close" };
	connect(&closeButton, &QPushButton::clicked, &dialog,
		[&dialog]()
		{
			dialog.accept();
		});
	layout.addWidget(&closeButton);

	view.setContextMenuPolicy(Qt::NoContextMenu);
	view.setOpenExternalLinks(true);
	QFile changelogFile(":/CHANGELOG.md");
	if (changelogFile.open(QIODevice::ReadOnly))
	{
		auto content = QString(changelogFile.readAll());

		auto const startPos = content.indexOf("## [");
		auto endPos = versionString.isEmpty() ? -1 : content.indexOf("## [" + versionString + "]");
		if (endPos == -1)
			endPos = content.size();
		auto const changelog = QStringRef(&content, startPos, endPos - startPos);

		auto buffer = changelog.toUtf8();
		auto* mmiot = mkd_string(buffer.data(), buffer.size(), 0);
		if (mmiot == nullptr)
			return;
		std::unique_ptr<MMIOT, std::function<void(MMIOT*)>> scopedMmiot{ mmiot, [](MMIOT* ptr)
			{
				if (ptr != nullptr)
					mkd_cleanup(ptr);
			} };

		if (mkd_compile(mmiot, 0) == 0)
			return;

		char* docPointer{ nullptr };
		auto const docLength = mkd_document(mmiot, &docPointer);
		if (docLength == 0)
			return;

		view.setHtml(QString::fromUtf8(docPointer, docLength));

		// Run dialog
		dialog.exec();
	}
}

void MainWindow::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);

	static std::once_flag once;
	std::call_once(once,
		[this]()
		{
			_pImpl->_shown = true;
			auto& settings = settings::SettingsManager::getInstance();

			// Start Sparkle
			{
				Sparkle::getInstance().start();
			}
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
#ifdef _WIN32
			// Check for WinPcap
			{
				auto const protocolType = settings.getValue(settings::Network_ProtocolType.name).value<la::avdecc::protocol::ProtocolInterface::Type>();
				if (protocolType == la::avdecc::protocol::ProtocolInterface::Type::PCap)
				{
					// Postpone check (because of possible dialog creation)
					QTimer::singleShot(0,
						[this]()
						{
							_pImpl->checkNpfStatus();
						});
				}
			}
#endif // _WIN32
			// Check if this is the first time we launch a new Hive version
			{
				auto lastVersion = settings.getValue(settings::LastLaunchedVersion.name).toString();
				settings.setValue(settings::LastLaunchedVersion.name, hive::internals::cmakeVersionString);

				// Do not show the ChangeLog during first ever launch, or if the last launched version is the same than current one
				if (lastVersion.isEmpty() || lastVersion == hive::internals::cmakeVersionString)
					return;

				// Postpone the dialog creation
				QTimer::singleShot(0,
					[this, versionString = std::move(lastVersion)]()
					{
						_pImpl->showChangeLog("What's New", versionString);
					});
			}
		});
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	auto& settings = settings::SettingsManager::getInstance();

	// Save window geometry
	settings.setValue(settings::MainWindowGeometry, saveGeometry());
	settings.setValue(settings::MainWindowState, saveState());

	// Remove settings observers
	settings.unregisterSettingObserver(settings::Network_ProtocolType.name, _pImpl);
	settings.unregisterSettingObserver(settings::Controller_DiscoveryDelay.name, _pImpl);
	settings.unregisterSettingObserver(settings::Controller_AemCacheEnabled.name, _pImpl);
	settings.unregisterSettingObserver(settings::Controller_FullStaticModelEnabled.name, _pImpl);
	settings.unregisterSettingObserver(settings::ConnectionMatrix_ChannelMode.name, _pImpl);
	settings.unregisterSettingObserver(settings::General_ThemeColorIndex.name, _pImpl);
	settings.unregisterSettingObserver(settings::General_AutomaticCheckForUpdates.name, _pImpl);
	settings.unregisterSettingObserver(settings::General_CheckForBetaVersions.name, _pImpl);

	qApp->closeAllWindows();

	QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
	for (auto const& u : event->mimeData()->urls())
	{
		auto const f = QFileInfo{ u.fileName() };
		auto const ext = f.suffix();
		if (ext == "ave" /* || ext == "ans"*/)
		{
			event->acceptProposedAction();
			return;
		}
	}
}

void MainWindow::dropEvent(QDropEvent* event)
{
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();

	auto const loadEntity = [&manager](auto const& filePath, auto const flags)
	{
		auto const [error, message] = manager.loadVirtualEntityFromJson(filePath, flags);
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
		return std::make_tuple(error, msg);
	};

	for (auto const& u : event->mimeData()->urls())
	{
		auto const f = u.toLocalFile();
		auto const fi = QFileInfo{ f };
		auto const ext = fi.suffix();

		// AVDECC Virtual Entity
		if (ext == "ave")
		{
			auto flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics };
			flags.set(la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat);
			auto [error, message] = loadEntity(u.toLocalFile(), flags);
			if (!!error)
			{
				if (error == la::avdecc::jsonSerializer::DeserializationError::NotCompliant)
				{
					auto const choice = QMessageBox::question(this, "", "Entity model is not fully IEEE1722.1 compliant.\n\nDo you want to import anyway?", QMessageBox::StandardButton::Yes, QMessageBox::StandardButton::No);
					if (choice == QMessageBox::StandardButton::Yes)
					{
						flags.set(la::avdecc::entity::model::jsonSerializer::Flag::IgnoreAEMSanityChecks);
						auto const result = loadEntity(u.toLocalFile(), flags);
						error = std::get<0>(result);
						message = std::get<1>(result);
						// Fallthrough to warning message
					}
				}
				if (!!error)
				{
					QMessageBox::warning(this, "Failed to load JSON entity", QString("Error loading JSON file '%1':\n%2").arg(f).arg(message));
				}
			}
		}

		// AVDECC Network State
		//else if (ext == "ans")
		//{
		//}
	}
}

void MainWindowImpl::updateStyleSheet(qtMate::material::color::Name const colorName, QString const& filename)
{
	auto const baseBackgroundColor = qtMate::material::color::value(colorName);
	auto const baseForegroundColor = QColor{ qtMate::material::color::luminance(colorName) == qtMate::material::color::Luminance::Dark ? Qt::white : Qt::black };
	auto const connectionMatrixBackgroundColor = qtMate::material::color::value(colorName, qtMate::material::color::Shade::Shade100);

	// Load and apply the stylesheet
	auto styleFile = QFile{ filename };
	if (styleFile.open(QFile::ReadOnly))
	{
		auto const styleSheet = QString{ styleFile.readAll() }.arg(baseBackgroundColor.name()).arg(baseForegroundColor.name()).arg(connectionMatrixBackgroundColor.name());

		qApp->setStyleSheet(styleSheet);
	}
}

QString MainWindowImpl::generateDumpSourceString() noexcept
{
	static auto s_DumpSource = QString{ "%1 v%2 using L-Acoustics AVDECC Controller v%3" }.arg(hive::internals::applicationShortName).arg(hive::internals::versionString).arg(la::avdecc::controller::getVersion().c_str());

	return s_DumpSource;
}

void MainWindowImpl::onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept
{
	if (name == settings::Network_ProtocolType.name)
	{
		currentControllerChanged();
	}
	else if (name == settings::Controller_DiscoveryDelay.name)
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const delay = std::chrono::seconds{ value.toInt() };
		manager.setAutomaticDiscoveryDelay(delay);
	}
	else if (name == settings::Controller_AemCacheEnabled.name || name == settings::Controller_FullStaticModelEnabled.name)
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		if (name == settings::Controller_AemCacheEnabled.name)
		{
			auto const enabled = value.toBool();
			manager.setEnableAemCache(enabled);
		}
		else if (name == settings::Controller_FullStaticModelEnabled.name)
		{
			auto const enabled = value.toBool();
			manager.setEnableFullAemEnumeration(enabled);
		}
		if (_shown)
		{
			currentControllerChanged();
		}
	}
	else if (name == settings::General_ThemeColorIndex.name)
	{
		auto const colorName = qtMate::material::color::Palette::name(value.toInt());
		updateStyleSheet(colorName, ":/style.qss");
	}
	else if (name == settings::General_AutomaticCheckForUpdates.name)
	{
		Sparkle::getInstance().setAutomaticCheckForUpdates(value.toBool());
	}
	else if (name == settings::General_CheckForBetaVersions.name)
	{
		if (value.toBool())
		{
			Sparkle::getInstance().setAppcastUrl(hive::internals::appcastBetasUrl);
		}
		else
		{
			Sparkle::getInstance().setAppcastUrl(hive::internals::appcastReleasesUrl);
		}
	}
}

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
	, _pImpl(new MainWindowImpl(this))
{
	// Set title
	setWindowTitle(hive::internals::applicationLongName + " - Version " + QCoreApplication::applicationVersion());

	// Register AcceptDrops so we can drop VirtualEntities as JSON
	setAcceptDrops(true);

	// Restore geometry
	auto& settings = settings::SettingsManager::getInstance();
	restoreGeometry(settings.getValue(settings::MainWindowGeometry).toByteArray());
	restoreState(settings.getValue(settings::MainWindowState).toByteArray());
}

MainWindow::~MainWindow() noexcept
{
	delete _pImpl;
}
