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
#include <QtWidgets>
#include <QMessageBox>
#include <QFile>
#include <QTextBrowser>
#include <QDateTime>
#include <QAbstractListModel>
#include <QVector>
#include <QSettings>
#include <QLabel>
#include <QStringView>

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
#include "newsFeed/newsFeed.hpp"
#include "internals/config.hpp"
#include "profiles/profiles.hpp"
#include "settingsManager/settings.hpp"
#include "settingsManager/settingsSignaler.hpp"
#include "activeNetworkInterfacesModel.hpp"
#include "aboutDialog.hpp"
#include "deviceDetailsDialog.hpp"
#include "nodeVisitor.hpp"
#include "settingsDialog.hpp"
#include "multiFirmwareUpdateDialog.hpp"
#include "defaults.hpp"
#include "windowsNpfHelper.hpp"
#include "visibilitySettings.hpp"
#include "listViewSelectionToMatrixModelController.hpp"

#include <QtMate/widgets/comboBox.hpp>
#include <QtMate/widgets/flatIconButton.hpp>
#include <QtMate/widgets/dynamicHeaderView.hpp>
#include <QtMate/material/color.hpp>
#include <QtMate/material/colorPalette.hpp>
#include <la/networkInterfaceHelper/networkInterfaceHelper.hpp>
#ifdef USE_SPARKLE
#	include <sparkleHelper/sparkleHelper.hpp>
#endif // USE_SPARKLE
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/networkInterfacesModel.hpp>
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
	Q_OBJECT
public:
	MainWindowImpl(bool const mustResetViewSettings, ::MainWindow* parent)
		: _parent(parent)
		, _mustResetViewSettings{ mustResetViewSettings }
	{
		// Setup entity model
		setupEntityModel();

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

	// Private Slots
	Q_SLOT void currentControllerChanged();

	// Private methods
	void setupAdvancedView(hive::VisibilityDefaults const& defaults);
	void setupMatrixProfile();
	void setupStandardProfile();
	void setupDeveloperProfile();
	void setupProfile();
	void setupEntityModel();
	void registerMetaTypes();
	void createViewMenu();
	void createToolbars();
	void checkNpfStatus();
	void loadSettings();
	void connectSignals();
	void showChangeLog(QString const title, QString const versionString);
	void showNewsFeed(QString const& news);
	void updateStyleSheet(qtMate::material::color::Name const colorName, QString const& filename);

	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

	// Private members
	::MainWindow* _parent{ nullptr };
	qtMate::widgets::ComboBox _interfaceComboBox{ _parent };
	ActiveNetworkInterfacesModel _activeNetworkInterfacesModel{ _parent };
	QSortFilterProxyModel _networkInterfacesModelProxy{ _parent };
	qtMate::widgets::FlatIconButton _refreshControllerButton{ "Material Icons", "refresh", _parent };
	qtMate::widgets::FlatIconButton _discoverButton{ "Hive", "radar", _parent };
	qtMate::widgets::FlatIconButton _openMcmdDialogButton{ "Material Icons", "schedule", _parent };
	qtMate::widgets::FlatIconButton _openMultiFirmwareUpdateDialogButton{ "Hive", "firmware_upload", _parent };
	qtMate::widgets::FlatIconButton _openSettingsButton{ "Hive", "settings", _parent };
	QLabel _controllerEntityIDLabel{ _parent };
	bool _shown{ false };
	la::avdecc::entity::model::EntityTree _entityModel{};
	SettingsSignaler _settingsSignaler{};
	bool _mustResetViewSettings{ false };
	bool _usingBetaAppcast{ false };
	bool _usingBackupAppcast{ false };
	std::unique_ptr<ListViewSelectionToMatrixModelController> _listViewSelectionToMatrixModelController{ nullptr }; // Remove smartpointer once in use in the upcoming layout classes
};

void MainWindowImpl::setupAdvancedView(hive::VisibilityDefaults const& defaults)
{
	// Create "view" sub-menu
	createViewMenu();

	// Create toolbars
	createToolbars();

	// Setup the ControllerView widget
	discoveredEntitiesView->setupView(defaults);

	// Create ListViewSelectionToMatrixModelController
	// Initialization to be moved to NSDM when layout classes are introduced
	_listViewSelectionToMatrixModelController = std::make_unique<ListViewSelectionToMatrixModelController>(discoveredEntitiesView->entitiesTableView(), static_cast<connectionMatrix::Model*>(routingTableView->model()), this);

	controllerToolBar->setVisible(defaults.mainWindow_ControllerToolbar_Visible);
	utilitiesToolBar->setVisible(defaults.mainWindow_UtilitiesToolbar_Visible);
	entitiesDockWidget->setVisible(defaults.mainWindow_EntitiesList_Visible);
	entityInspectorDockWidget->setVisible(defaults.mainWindow_Inspector_Visible);
	loggerDockWidget->setVisible(defaults.mainWindow_Logger_Visible);

	// Load settings, overriding defaults
	loadSettings();

	// Configure Widget parameters
	splitter->setStretchFactor(0, 0); // Entities List has less weight than
	splitter->setStretchFactor(1, 1); // the Matrix, as far as expand is concerned
	_parent->setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea); // Give priority to vertical dock widget
	_parent->setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea); // Give priority to vertical dock widget

	// Connect all signals
	connectSignals();

	// Create channel connection manager instance
	avdecc::ChannelConnectionManager::getInstance();
}

void MainWindowImpl::setupMatrixProfile()
{
	setupAdvancedView(hive::VisibilityDefaults{ true, false, false, false, false, true, true, true, true, false, false, false, false, false, false, true, true });
}

void MainWindowImpl::setupStandardProfile()
{
	setupAdvancedView(hive::VisibilityDefaults{ true, true, true, false, false, true, true, true, true, false, false, false, false, false, false, true, true });
}

void MainWindowImpl::setupDeveloperProfile()
{
	setupAdvancedView(hive::VisibilityDefaults{});
}

void MainWindowImpl::setupProfile()
{
	// Update the UI and other stuff, based on the current Profile
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();

	auto const userProfile = settings->getValue<profiles::ProfileType>(settings::UserProfile.name);

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
		case profiles::ProfileType::Matrix:
			setupMatrixProfile();
			break;
	}
}

void MainWindowImpl::currentControllerChanged()
{
	auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();

	auto protocolType = settings->getValue<la::avdecc::protocol::ProtocolInterface::Type>(settings::Network_ProtocolType.name);
	auto const interfaceID = _interfaceComboBox.currentData().toString();

	// Check for No ProtocolInterface
	if (protocolType == la::avdecc::protocol::ProtocolInterface::Type::None)
	{
		QMessageBox::warning(_parent, "", "No Network Protocol selected.\nPlease choose one from the Settings.");
		return;
	}

	// Check for special Offline Interface
	if (interfaceID.toStdString() == hive::modelsLibrary::NetworkInterfacesModel::OfflineInterfaceName)
	{
		protocolType = la::avdecc::protocol::ProtocolInterface::Type::Virtual;
	}

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

	settings->setValue(settings::InterfaceID, interfaceID);

	try
	{
		// Create a new Controller
#ifdef DEBUG
		auto const progID = std::uint16_t{ PROG_ID + 1 }; // Use next PROG_ID in debug (so we can launch 2 Hive instances at the same time, one in Release and one in Debug)
#else // !DEBUG
		auto const progID = std::uint16_t{ PROG_ID };
#endif // DEBUG
		manager.createController(protocolType, interfaceID, progID, la::avdecc::entity::model::makeEntityModelID(VENDOR_ID, DEVICE_ID, MODEL_ID), "en", &_entityModel);
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

// Private methods
void MainWindowImpl::setupEntityModel()
{
	_entityModel.dynamicModel.entityName = hive::internals::applicationShortName.toStdString();
	_entityModel.dynamicModel.firmwareVersion = hive::internals::versionString.toStdString();
}

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

	// Main widgets visibility toggle
	menuView->addAction(entitiesDockWidget->toggleViewAction());
	menuView->addAction(entityInspectorDockWidget->toggleViewAction());
	menuView->addSeparator();

	// Logger visibility toggle
	menuView->addAction(loggerDockWidget->toggleViewAction());
}

void MainWindowImpl::createToolbars()
{
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();

	// Controller Toolbar
	{
		auto* interfaceLabel = new QLabel("Interface");
		interfaceLabel->setMinimumWidth(50);
		_interfaceComboBox.setMinimumWidth(100);
		_interfaceComboBox.setModel(&_activeNetworkInterfacesModel);

		// The combobox takes ownership of the item delegate
		auto* interfaceComboBoxItemDelegate = new hive::widgetModelsLibrary::ErrorItemDelegate{ true, qtMate::material::color::Palette::name(settings->getValue(settings::General_ThemeColorIndex.name).toInt()) };
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
		_discoverButton.setToolTip("Discover Entities");
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
	auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();

	LOG_HIVE_INFO("Settings location: " + settings->getFilePath());

	auto const networkInterfaceId = settings->getValue(settings::InterfaceID).toString();
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
	auto protocolType = settings->getValue<la::avdecc::protocol::ProtocolInterface::Type>(settings::Network_ProtocolType.name);
	auto supportedTypes = la::avdecc::protocol::ProtocolInterface::getSupportedProtocolInterfaceTypes();
	// Remove Virtual Protocol Interface in Release
#ifndef DEBUG
	supportedTypes.reset(la::avdecc::protocol::ProtocolInterface::Type::Virtual);
#endif
	if (!supportedTypes.test(protocolType))
	{
		auto const wasConfigured = protocolType != la::avdecc::protocol::ProtocolInterface::Type::None;
		// Previous ProtocolInterface Type is no longer supported
		if (wasConfigured)
		{
			LOG_HIVE_WARN(QString("Previously configured Network Protocol is no longer supported: %1").arg(QString::fromStdString(la::avdecc::protocol::ProtocolInterface::typeToString(protocolType))));
		}
		// If at least one type remains, use it
		if (!supportedTypes.empty())
		{
			// Force the first supported ProtocolInterface
			protocolType = *supportedTypes.begin();
			if (wasConfigured)
			{
				LOG_HIVE_WARN(QString("Forcing new Network Protocol: %1").arg(QString::fromStdString(la::avdecc::protocol::ProtocolInterface::typeToString(protocolType))));
			}
		}
		else
		{
			protocolType = la::avdecc::protocol::ProtocolInterface::Type::None;
		}
		// Save the new ProtocolInterface to the settings, before we call registerSettingObserver
		settings->setValue(settings::Network_ProtocolType.name, protocolType);
	}

	// Configure connectionMatrix mode
	auto const channelMode = settings->getValue(settings::ConnectionMatrix_ChannelMode.name).toBool();
	auto* action = channelMode ? actionChannelModeRouting : actionStreamModeRouting;
	action->setChecked(true);

	if (!_mustResetViewSettings)
	{
		discoveredEntitiesView->entitiesTableView()->restoreState();
		loggerView->header()->restoreState(settings->getValue(settings::LoggerDynamicHeaderViewState).toByteArray());
		entityInspector->restoreState(settings->getValue(settings::EntityInspectorState).toByteArray());
		splitter->restoreState(settings->getValue(settings::SplitterState).toByteArray());
	}

	// Configure settings observers
	settings->registerSettingObserver(settings::Network_ProtocolType.name, this);
	settings->registerSettingObserver(settings::Controller_DiscoveryDelay.name, this);
	settings->registerSettingObserver(settings::Controller_AemCacheEnabled.name, this);
	settings->registerSettingObserver(settings::Controller_FullStaticModelEnabled.name, this);
	settings->registerSettingObserver(settings::ConnectionMatrix_ChannelMode.name, this);
	settings->registerSettingObserver(settings::General_ThemeColorIndex.name, this);
	settings->registerSettingObserver(settings::General_AutomaticCheckForUpdates.name, this);
	settings->registerSettingObserver(settings::General_CheckForBetaVersions.name, this);

	// Start the SettingsSignaler
	_settingsSignaler.start();
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
			auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
			settings->setValue(settings::ConnectionMatrix_ChannelMode.name, checked);
			// Toggle
			auto* action = checked ? actionChannelModeRouting : actionStreamModeRouting;
			action->setChecked(true);
		});

	// Connect discoveredEntities::view signals
	connect(discoveredEntitiesView->entitiesTableView(), &discoveredEntities::View::doubleClicked, this,
		[this](la::avdecc::UniqueIdentifier const entityID)
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);

			if (controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) && controlledEntity->hasAnyConfiguration())
			{
				DeviceDetailsDialog* dialog = new DeviceDetailsDialog(_parent);
				dialog->setAttribute(Qt::WA_DeleteOnClose);
				dialog->setControlledEntityID(entityID);
				dialog->show();
			}
		});

	connect(discoveredEntitiesView->entitiesTableView(), &discoveredEntities::View::selectedControlledEntityChanged, entityInspector, &EntityInspector::setControlledEntityID);

	connect(discoveredEntitiesView, &DiscoveredEntitiesView::filterChanged, routingTableView,
		[this](QString const& filter)
		{
			if (discoveredEntitiesView->isFilterLinked())
			{
				routingTableView->talkerFilterLineEdit()->setText(filter);
				routingTableView->listenerFilterLineEdit()->setText(filter);
			}
		});
	connect(discoveredEntitiesView, &DiscoveredEntitiesView::filterLinkStateChanged, routingTableView,
		[this](bool const isLinked, QString const& filter)
		{
			auto* const talkerFilter = routingTableView->talkerFilterLineEdit();
			auto* const listenerFilter = routingTableView->listenerFilterLineEdit();
			talkerFilter->setEnabled(!isLinked);
			listenerFilter->setEnabled(!isLinked);
			talkerFilter->setText(isLinked ? filter : "");
			listenerFilter->setText(isLinked ? filter : "");
		});

	// Connect EntityInspector signals
	connect(entityInspector, &EntityInspector::stateChanged, this,
		[this]()
		{
			auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
			settings->setValue(settings::EntityInspectorState, entityInspector->saveState());
			discoveredEntitiesView->setInspectorGeometry(entityInspector->saveGeometry());
		});

	connect(loggerView->header(), &qtMate::widgets::DynamicHeaderView::sectionChanged, this,
		[this]()
		{
			auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
			settings->setValue(settings::LoggerDynamicHeaderViewState, loggerView->header()->saveState());
		});

	connect(splitter, &QSplitter::splitterMoved, this,
		[this]()
		{
			auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
			settings->setValue(settings::SplitterState, splitter->saveState());
		});

	// Connect ControllerManager events
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	connect(&manager, &hive::modelsLibrary::ControllerManager::transportError, this,
		[this]()
		{
			LOG_HIVE_ERROR("Error reading from the active Network Interface");
			QMessageBox::warning(_parent, "", "Error reading from the active Network Interface.<br>Check connection and click the <i>Reload Controller</i> button.");
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::endAecpCommand, this,
		[this](la::avdecc::UniqueIdentifier const /*entityID*/, hive::modelsLibrary::ControllerManager::AecpCommandType commandType, la::avdecc::entity::model::DescriptorIndex /*descriptorIndex*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
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
			auto flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDiagnostics };
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
				auto [error, message] = manager.serializeAllControlledEntitiesAsJson(filename, flags, avdecc::helper::generateDumpSourceString(hive::internals::applicationShortName, hive::internals::versionString));
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
#ifdef USE_SPARKLE
			Sparkle::getInstance().manualCheckForUpdate();
#else // !USE_SPARKLE
			QMessageBox::information(nullptr, "Updates not supported", "Not compiled with auto-update support.");
#endif // USE_SPARKLE
		});


	// Connect keyboard shortcuts

	auto* refreshController = new QShortcut{ QKeySequence{ "Ctrl+R" }, _parent };
	connect(refreshController, &QShortcut::activated, this, &MainWindowImpl::currentControllerChanged);

	auto* toggleMatrixMode = new QShortcut{ QKeySequence{ "Ctrl+M" }, _parent };
	connect(toggleMatrixMode, &QShortcut::activated, this,
		[this]()
		{
			// Toggle mode (stream based vs. channel based routing matrix) in settings
			auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
			auto const channelModeActiveInverted = !settings->getValue(settings::ConnectionMatrix_ChannelMode.name).toBool();
			auto* action = channelModeActiveInverted ? actionChannelModeRouting : actionStreamModeRouting;
			action->setChecked(true);
		});

#ifdef DEBUG
	auto* reloadStyleSheet = new QShortcut{ QKeySequence{ "F5" }, _parent };
	connect(reloadStyleSheet, &QShortcut::activated, _parent,
		[this]()
		{
			auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
			auto const themeColorIndex = settings->getValue(settings::General_ThemeColorIndex.name).toInt();
			auto const colorName = qtMate::material::color::Palette::name(themeColorIndex);
			updateStyleSheet(colorName, QString{ RESOURCES_ROOT_DIR } + "/style.qss");
			LOG_HIVE_DEBUG("StyleSheet reloaded");
		});
#endif

#ifdef USE_SPARKLE
	// Connect Sparkle events
	auto& sparkle = Sparkle::getInstance();
	sparkle.setIsShutdownAllowedHandler(
		[]()
		{
			return true;
		});
	sparkle.setShutdownRequestHandler(
		[]()
		{
			QCoreApplication::postEvent(qApp, new QEvent{ QEvent::Type::Quit });
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
	sparkle.setUpdateFailedHandler(
		[this]()
		{
			QMetaObject::invokeMethod(this,
				[this]()
				{
					if (!_usingBackupAppcast)
					{
						_usingBackupAppcast = true;
						auto fallbackUrl = std::string{};
						auto setFallback = false;
						// Use backup appcast
						if (_usingBetaAppcast)
						{
							if (hive::internals::appcastBetasFallbackUrl != hive::internals::appcastBetasUrl)
							{
								setFallback = true;
								fallbackUrl = hive::internals::appcastBetasFallbackUrl;
							}
						}
						else
						{
							if (hive::internals::appcastReleasesFallbackUrl != hive::internals::appcastReleasesUrl)
							{
								setFallback = true;
								fallbackUrl = hive::internals::appcastReleasesFallbackUrl;
							}
						}
						if (setFallback)
						{
							Sparkle::getInstance().setAppcastUrl(fallbackUrl);
							LOG_HIVE_WARN("Trying autoupdate fallback URL");
						}
					}
				});
		});
#endif // USE_SPARKLE
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
		auto const changelog = QStringView(content.data() + startPos, static_cast<qsizetype>(endPos - startPos));

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

void MainWindowImpl::showNewsFeed(QString const& news)
{
	// Create dialog popup
	QDialog dialog{ _parent };
	QVBoxLayout layout{ &dialog };
	QTextBrowser view;
	layout.addWidget(&view);
	dialog.setWindowTitle(hive::internals::applicationShortName + " - " + "News");
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

	auto buffer = news.toUtf8();
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

void MainWindow::showEvent(QShowEvent* event)
{
	QMainWindow::showEvent(event);

	if (_isReady)
	{
		static std::once_flag once;
		std::call_once(once,
			[this]()
			{
				_pImpl->_shown = true;
				auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();

#ifdef USE_SPARKLE
				// Start Sparkle
				{
					Sparkle::getInstance().start();
				}
#endif // USE_SPARKLE

				// Check if we have a network interface selected and start controller
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
				// Check if this is the first time we launch a new Hive version
				{
					auto lastVersion = settings->getValue(settings::LastLaunchedVersion.name).toString();
					settings->setValue(settings::LastLaunchedVersion.name, hive::internals::cmakeVersionString);

					// Do not show the ChangeLog during first ever launch, or if the last launched version is the same than current one
					if (!lastVersion.isEmpty() && lastVersion != hive::internals::cmakeVersionString)
					{
						// Postpone the dialog creation
						QTimer::singleShot(0,
							[this, versionString = std::move(lastVersion)]()
							{
								_pImpl->showChangeLog("What's New", versionString);
							});
					}
				}
				// Check if we need to show the News Feed
				{
					auto& newsFeed = NewsFeed::getInstance();
					connect(&newsFeed, &NewsFeed::newsAvailable, this,
						[this](QString const& news, std::uint64_t const serverTimestamp)
						{
							auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
							if (serverTimestamp > settings->getValue(settings::LastCheckTime.name).value<std::uint32_t>())
							{
								settings->setValue(settings::LastCheckTime.name, QVariant::fromValue(serverTimestamp));
							}

							if (!news.isEmpty())
							{
								_pImpl->showNewsFeed(news);
							}
						});

					auto const lastCheckTime = settings->getValue(settings::LastCheckTime.name).value<std::uint32_t>();
					newsFeed.checkForNews(lastCheckTime);
				}
			});
	}
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();

	// Save window geometry
	settings->setValue(settings::MainWindowGeometry, saveGeometry());
	settings->setValue(settings::MainWindowState, saveState());

	// Remove settings observers
	settings->unregisterSettingObserver(settings::Network_ProtocolType.name, _pImpl);
	settings->unregisterSettingObserver(settings::Controller_DiscoveryDelay.name, _pImpl);
	settings->unregisterSettingObserver(settings::Controller_AemCacheEnabled.name, _pImpl);
	settings->unregisterSettingObserver(settings::Controller_FullStaticModelEnabled.name, _pImpl);
	settings->unregisterSettingObserver(settings::ConnectionMatrix_ChannelMode.name, _pImpl);
	settings->unregisterSettingObserver(settings::General_ThemeColorIndex.name, _pImpl);
	settings->unregisterSettingObserver(settings::General_AutomaticCheckForUpdates.name, _pImpl);
	settings->unregisterSettingObserver(settings::General_CheckForBetaVersions.name, _pImpl);

	qApp->closeAllWindows();

	QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
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
}

void MainWindow::dropEvent(QDropEvent* event)
{
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

void MainWindowImpl::onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept
{
	if (name == settings::Network_ProtocolType.name)
	{
		if (_shown)
		{
			currentControllerChanged();
		}
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
#ifdef USE_SPARKLE
	else if (name == settings::General_AutomaticCheckForUpdates.name)
	{
		Sparkle::getInstance().setAutomaticCheckForUpdates(value.toBool());
	}
	else if (name == settings::General_CheckForBetaVersions.name)
	{
		if (value.toBool())
		{
			_usingBetaAppcast = true;
			Sparkle::getInstance().setAppcastUrl(hive::internals::appcastBetasUrl);
		}
		else
		{
			_usingBetaAppcast = false;
			Sparkle::getInstance().setAppcastUrl(hive::internals::appcastReleasesUrl);
		}
	}
#endif // USE_SPARKLE
}

MainWindow::MainWindow(bool const mustResetViewSettings, QWidget* parent)
	: QMainWindow(parent)
	, _pImpl(new MainWindowImpl(mustResetViewSettings, this))
{
	// Set title
	setWindowTitle(hive::internals::applicationLongName + " - Version " + QCoreApplication::applicationVersion());

	// Register AcceptDrops so we can drop VirtualEntities as JSON
	setAcceptDrops(true);

	// Restore geometry
	if (!mustResetViewSettings)
	{
		auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
		restoreGeometry(settings->getValue(settings::MainWindowGeometry).toByteArray());
		restoreState(settings->getValue(settings::MainWindowState).toByteArray());
	}
}

MainWindow::~MainWindow() noexcept
{
	delete _pImpl;
}

void MainWindow::setReady() noexcept
{
	_isReady = true;
}

#include "mainWindow.moc"
