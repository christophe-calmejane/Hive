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

#include "mainWindow.hpp"
#include <QtWidgets>
#include <QMessageBox>
#include <QFile>
#include <QTextBrowser>

#include "avdecc/helper.hpp"
#include "avdecc/hiveLogItems.hpp"
#include "internals/config.hpp"

#include "nodeVisitor.hpp"
#include "toolkit/dynamicHeaderView.hpp"
#include "avdecc/controllerManager.hpp"
#include "avdecc/mcDomainManager.hpp"
#include "aboutDialog.hpp"
#include "settingsDialog.hpp"
#include "imageItemDelegate.hpp"
#include "settingsManager/settings.hpp"
#include "entityLogoCache.hpp"

#include "updater/updater.hpp"

#include <mutex>
#include <memory>

extern "C"
{
#include <mkdio.h>
}

#define VENDOR_ID 0x001B92
#define DEVICE_ID 0x80
#define MODEL_ID 0x00000001

Q_DECLARE_METATYPE(la::avdecc::protocol::ProtocolInterface::Type)

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
	, _controllerModel(new avdecc::ControllerModel(this))
{
	setupUi(this);

	// Set title
	setWindowTitle(hive::internals::applicationLongName + " - Version " + QCoreApplication::applicationVersion());

	registerMetaTypes();

	createViewMenu();
	createMainToolBar();

	createControllerView();

	populateProtocolComboBox();
	populateInterfaceComboBox();

	loadSettings();

	connectSignals();
}

void MainWindow::currentControllerChanged()
{
	auto const protocolType = _protocolComboBox.currentData().value<la::avdecc::protocol::ProtocolInterface::Type>();
	auto const interfaceName = _interfaceComboBox.currentData().toString();

	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::ProtocolType, _protocolComboBox.currentText());
	settings.setValue(settings::InterfaceName, _interfaceComboBox.currentText());

	try
	{
		// Create a new Controller
		auto& manager = avdecc::ControllerManager::getInstance();
		manager.createController(protocolType, interfaceName, 0x0003, la::avdecc::entity::model::makeEntityModelID(VENDOR_ID, DEVICE_ID, MODEL_ID), "en");
		_controllerEntityIDLabel.setText(avdecc::helper::uniqueIdentifierToString(manager.getControllerEID()));
	}
	catch (la::avdecc::controller::Controller::Exception const& e)
	{
		statusbar->showMessage(e.what(), 2000);
	}
}

void MainWindow::currentControlledEntityChanged(QModelIndex const& index)
{
	if (!index.isValid())
	{
		entityInspector->setControlledEntityID(la::avdecc::UniqueIdentifier{});
		return;
	}

	auto& manager = avdecc::ControllerManager::getInstance();
	auto const entityID = _controllerModel->controlledEntityID(index);
	auto controlledEntity = manager.getControlledEntity(entityID);

	if (controlledEntity)
	{
		entityInspector->setControlledEntityID(entityID);
	}
}

void MainWindow::registerMetaTypes()
{
	//
	qRegisterMetaType<la::avdecc::entity::model::StreamIndex>("la::avdecc::entity::model::StreamIndex");
	qRegisterMetaType<la::avdecc::controller::model::AcquireState>("la::avdecc::controller::model::AcquireState");
	qRegisterMetaType<la::avdecc::UniqueIdentifier>("la::avdecc::UniqueIdentifier");
	qRegisterMetaType<la::avdecc::entity::model::StreamFormat>("la::avdecc::entity::model::StreamFormat");
	qRegisterMetaType<la::avdecc::entity::model::StreamPortIndex>("la::avdecc::entity::model::StreamPortIndex");
	qRegisterMetaType<la::avdecc::entity::model::AvdeccFixedString>("la::avdecc::entity::model::AvdeccFixedString");
	qRegisterMetaType<la::avdecc::entity::model::ConfigurationIndex>("la::avdecc::entity::model::ConfigurationIndex");

	//
	qRegisterMetaType<la::avdecc::logger::Layer>("la::avdecc::logger::Layer");
	qRegisterMetaType<la::avdecc::logger::Level>("la::avdecc::logger::Level");
	qRegisterMetaType<std::string>("std::string");

	qRegisterMetaType<EntityLogoCache::Type>("EntityLogoCache::Type");
}

void MainWindow::createViewMenu()
{
	menuView->addAction(mainToolBar->toggleViewAction());
	menuView->addSeparator();
	menuView->addAction(entityInspectorDockWidget->toggleViewAction());
	menuView->addSeparator();
	menuView->addAction(loggerDockWidget->toggleViewAction());
}

void MainWindow::createMainToolBar()
{
	auto* protocolLabel = new QLabel("Protocol");
	protocolLabel->setMinimumWidth(50);
	_protocolComboBox.setMinimumWidth(100);

	auto* interfaceLabel = new QLabel("Interface");
	interfaceLabel->setMinimumWidth(50);
	_interfaceComboBox.setMinimumWidth(100);

	auto* controllerEntityIDLabel = new QLabel("Controller ID: ");
	controllerEntityIDLabel->setMinimumWidth(50);
	_controllerEntityIDLabel.setMinimumWidth(100);

	mainToolBar->setMinimumHeight(30);

	mainToolBar->addWidget(protocolLabel);
	mainToolBar->addWidget(&_protocolComboBox);

	mainToolBar->addSeparator();

	mainToolBar->addWidget(interfaceLabel);
	mainToolBar->addWidget(&_interfaceComboBox);

	mainToolBar->addSeparator();

	mainToolBar->addWidget(controllerEntityIDLabel);
	mainToolBar->addWidget(&_controllerEntityIDLabel);
}

void MainWindow::createControllerView()
{
	controllerTableView->setModel(_controllerModel);
	controllerTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	controllerTableView->setSelectionMode(QAbstractItemView::SingleSelection);
	controllerTableView->setContextMenuPolicy(Qt::CustomContextMenu);

	auto* imageItemDelegate{ new ImageItemDelegate };
	controllerTableView->setItemDelegateForColumn(0, imageItemDelegate);
	controllerTableView->setItemDelegateForColumn(4, imageItemDelegate);

	_controllerDynamicHeaderView.setHighlightSections(false);
	controllerTableView->setHorizontalHeader(&_controllerDynamicHeaderView);

	int column{ 0 };
	controllerTableView->setColumnWidth(column++, 40);
	controllerTableView->setColumnWidth(column++, 160);
	controllerTableView->setColumnWidth(column++, 180);
	controllerTableView->setColumnWidth(column++, 80);
	controllerTableView->setColumnWidth(column++, 80);
	controllerTableView->setColumnWidth(column++, 160);
	controllerTableView->setColumnWidth(column++, 80);
	controllerTableView->setColumnWidth(column++, 90);
	controllerTableView->setColumnWidth(column++, 160);
	controllerTableView->setColumnWidth(column++, 180);
	controllerTableView->setColumnWidth(column++, 185);
}

void MainWindow::populateProtocolComboBox()
{
	const std::map<la::avdecc::protocol::ProtocolInterface::Type, QString> protocolInterfaceName{
		{ la::avdecc::protocol::ProtocolInterface::Type::None, "None" },
		{ la::avdecc::protocol::ProtocolInterface::Type::PCap, "PCap" },
		{ la::avdecc::protocol::ProtocolInterface::Type::MacOSNative, "MacOS Native" },
		{ la::avdecc::protocol::ProtocolInterface::Type::Proxy, "Proxy" },
		{ la::avdecc::protocol::ProtocolInterface::Type::Virtual, "Virtual" },
	};

	for (auto const& type : la::avdecc::protocol::ProtocolInterface::getSupportedProtocolInterfaceTypes())
	{
#ifndef DEBUG
		if (type == la::avdecc::protocol::ProtocolInterface::Type::Virtual)
			continue;
#endif // !DEBUG
		_protocolComboBox.addItem(protocolInterfaceName.at(type), QVariant::fromValue(type));
	}
}

void MainWindow::populateInterfaceComboBox()
{
	la::avdecc::networkInterface::enumerateInterfaces(
		[this](la::avdecc::networkInterface::Interface const& networkInterface)
		{
			if (networkInterface.type != la::avdecc::networkInterface::Interface::Type::Loopback && networkInterface.isActive)
			{
				_interfaceComboBox.addItem(QString::fromStdString(networkInterface.alias), QString::fromStdString(networkInterface.name));
			}
		});
}

void MainWindow::loadSettings()
{
	auto& settings = settings::SettingsManager::getInstance();

	LOG_HIVE_DEBUG("Settings location: " + settings.getFilePath());

	_protocolComboBox.setCurrentText(settings.getValue(settings::ProtocolType).toString());
	_interfaceComboBox.setCurrentText(settings.getValue(settings::InterfaceName).toString());

	currentControllerChanged();

	_controllerDynamicHeaderView.restoreState(settings.getValue(settings::ControllerDynamicHeaderViewState).toByteArray());
	loggerView->header()->restoreState(settings.getValue(settings::LoggerDynamicHeaderViewState).toByteArray());
	entityInspector->restoreState(settings.getValue(settings::EntityInspectorState).toByteArray());
	splitter->restoreState(settings.getValue(settings::SplitterState).toByteArray());

	restoreGeometry(settings.getValue(settings::MainWindowGeometry).toByteArray());
	restoreState(settings.getValue(settings::MainWindowState).toByteArray());
}

void MainWindow::connectSignals()
{
	connect(&_protocolComboBox, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::currentControllerChanged);
	connect(&_interfaceComboBox, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::currentControllerChanged);

	connect(controllerTableView->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::currentControlledEntityChanged);
	connect(&_controllerDynamicHeaderView, &qt::toolkit::DynamicHeaderView::sectionChanged, this,
		[this]()
		{
			auto& settings = settings::SettingsManager::getInstance();
			settings.setValue(settings::ControllerDynamicHeaderViewState, _controllerDynamicHeaderView.saveState());
		});
	connect(controllerTableView, &QTableView::customContextMenuRequested, this,
		[this](QPoint const& pos)
		{
			auto const index = controllerTableView->indexAt(pos);

			auto& manager = avdecc::ControllerManager::getInstance();
			auto const entityID = _controllerModel->controlledEntityID(index);
			auto controlledEntity = manager.getControlledEntity(entityID);

			if (controlledEntity)
			{
				QMenu menu;
				auto const& entity = controlledEntity->getEntity();

				auto* acquireAction{ static_cast<QAction*>(nullptr) };
				auto* releaseAction{ static_cast<QAction*>(nullptr) };
				auto* inspect{ static_cast<QAction*>(nullptr) };
				auto* getLogo{ static_cast<QAction*>(nullptr) };

				if (la::avdecc::hasFlag(entity.getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
				{
					QString acquireText;
					auto const isAcquired = controlledEntity->isAcquired();
					auto const isAcquiredByOther = controlledEntity->isAcquiredByOther();

					{
						if (isAcquiredByOther)
							acquireText = "Try to acquire";
						else
							acquireText = "Acquire";
						acquireAction = menu.addAction(acquireText);
						acquireAction->setEnabled(!isAcquired);
					}
					{
						releaseAction = menu.addAction("Release");
						releaseAction->setEnabled(isAcquired);
					}
					menu.addSeparator();
					{
						inspect = menu.addAction("Inspect");
					}
					{
						getLogo = menu.addAction("Retrieve Entity Logo");
						getLogo->setEnabled(!EntityLogoCache::getInstance().isImageInCache(entityID, EntityLogoCache::Type::Entity));
					}
				}
				menu.addSeparator();
				menu.addAction("Cancel");

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
						EntityLogoCache::getInstance().getImage(entityID, EntityLogoCache::Type::Entity, true);
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

	connect(loggerView->header(), &qt::toolkit::DynamicHeaderView::sectionChanged, this,
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
	auto& manager = avdecc::ControllerManager::getInstance();
	connect(&manager, &avdecc::ControllerManager::endAecpCommand, this,
		[this](la::avdecc::UniqueIdentifier const entityID, avdecc::ControllerManager::AecpCommandType commandType, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
		{
			if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::Success)
			{
				QMessageBox::warning(this, "", "<i>" + avdecc::ControllerManager::typeToString(commandType) + "</i> failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
			}
		});
	connect(&manager, &avdecc::ControllerManager::endAcmpCommand, this,
		[this](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, avdecc::ControllerManager::AcmpCommandType commandType, la::avdecc::entity::ControllerEntity::ControlStatus const status)
		{
			if (status != la::avdecc::entity::ControllerEntity::ControlStatus::Success)
			{
				QMessageBox::warning(this, "", "<i>" + avdecc::ControllerManager::typeToString(commandType) + "</i> failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
			}
		});

	//

	connect(actionSettings, &QAction::triggered, this,
		[this]()
		{
			SettingsDialog dialog{ this };
			dialog.exec();
		});

	//

	connect(actionAbout, &QAction::triggered, this,
		[this]()
		{
			AboutDialog dialog{ this };
			dialog.exec();
		});

	//

	connect(actionChangeLog, &QAction::triggered, this,
		[this]()
		{
			showChangeLog("Change Log", "");
		});

	// Connect updater signals
	auto const& updater = Updater::getInstance();
	connect(&updater, &Updater::newVersionAvailable, this,
		[](QString version, QString downloadURL)
		{
			QString message{ "New version (" + version + ") available here " + downloadURL };

			QMessageBox::information(nullptr, "", message);
			LOG_HIVE_INFO(message);
		});
	connect(&updater, &Updater::checkFailed, this,
		[](QString reason)
		{
			LOG_HIVE_WARN("Failed to check for new version: " + reason);
		});
}

void MainWindow::showChangeLog(QString const title, QString const versionString)
{
	// Create dialog popup
	QDialog dialog{ this };
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
	static std::once_flag once;
	std::call_once(once,
		[this]()
		{
			// Start a new version check
			Updater::getInstance().checkForNewVersion();

			// Check version change
			auto& settings = settings::SettingsManager::getInstance();
			auto lastVersion = settings.getValue(settings::LastLaunchedVersion.name).toString();
			settings.setValue(settings::LastLaunchedVersion.name, hive::internals::versionString);

			if (lastVersion == hive::internals::versionString)
				return;

			// Postpone the dialog creation
			QTimer::singleShot(0,
				[this, versionString = std::move(lastVersion)]()
				{
					showChangeLog("What's New", versionString);
				});
		});
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::MainWindowGeometry, saveGeometry());
	settings.setValue(settings::MainWindowState, saveState());

	qApp->closeAllWindows();

	QMainWindow::closeEvent(event);
}
