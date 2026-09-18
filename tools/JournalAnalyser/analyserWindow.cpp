/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "analyserWindow.hpp"
#include "config.hpp"

#include "entityInspector.hpp"
#include "eventJournal/eventJournalView.hpp"
#include "networkGraph/networkGraphView.hpp"
#include "settingsManager/settings.hpp"
#include "settingsManager/settingsManager.hpp"

#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/eventJournal.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <QtMate/material/color.hpp>
#include <QtMate/material/colorPalette.hpp>

#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QStatusBar>
#include <QStyleHints>
#include <QUrl>

#include <tuple>

namespace
{
constexpr auto JournalFileExtension = hive::modelsLibrary::EventJournal::JournalFileExtension;
constexpr auto NetworkStateFileExtension = "ans";
constexpr auto VirtualEntityFileExtension = "ave";

/** Flags used to load network state and virtual entity files, same as Hive. */
la::avdecc::entity::model::jsonSerializer::Flags loadFlags() noexcept
{
	auto flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDiagnostics };
	flags.set(la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat);
	return flags;
}

/** Builds a human readable message for a deserialization error. */
QString deserializationErrorString(la::avdecc::jsonSerializer::DeserializationError const error, std::string const& message) noexcept
{
	switch (error)
	{
		case la::avdecc::jsonSerializer::DeserializationError::NoError:
			return {};
		case la::avdecc::jsonSerializer::DeserializationError::AccessDenied:
			return "Access Denied";
		case la::avdecc::jsonSerializer::DeserializationError::FileReadError:
			return "Error Reading File";
		case la::avdecc::jsonSerializer::DeserializationError::IncompatibleDumpVersion:
			return "Incompatible Dump Version";
		case la::avdecc::jsonSerializer::DeserializationError::ParseError:
			return QString{ "Parse Error: %1" }.arg(message.c_str());
		case la::avdecc::jsonSerializer::DeserializationError::MissingKey:
			return QString{ "Missing Key: %1" }.arg(message.c_str());
		case la::avdecc::jsonSerializer::DeserializationError::InvalidKey:
			return QString{ "Invalid Key: %1" }.arg(message.c_str());
		case la::avdecc::jsonSerializer::DeserializationError::InvalidValue:
			return QString{ "Invalid Value: %1" }.arg(message.c_str());
		case la::avdecc::jsonSerializer::DeserializationError::DuplicateEntityID:
			return QString{ "An Entity already exists with the same EntityID: %1" }.arg(message.c_str());
		case la::avdecc::jsonSerializer::DeserializationError::IncompatibleEntityModelVersion:
			return "Incompatible Entity Model Version";
		case la::avdecc::jsonSerializer::DeserializationError::NotSupported:
			return "Virtual Entity Loading not supported by this version of the AVDECC library";
		case la::avdecc::jsonSerializer::DeserializationError::InternalError:
			return QString{ "Internal Error: %1" }.arg(message.c_str());
		default:
			return message.empty() ? QString{ "Unknown Error" } : QString::fromStdString(message);
	}
}

/** Formats a journal metadata UTC timestamp (ISO 8601) as local time, or returns the raw value if it cannot be parsed. */
QString localTimeFromMetadata(QString const& value) noexcept
{
	auto const dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
	if (!dateTime.isValid())
	{
		return value;
	}
	return dateTime.toLocalTime().toString("yyyy-MM-dd HH:mm:ss");
}
} // namespace

AnalyserWindow::AnalyserWindow(QStringList const& filesToLoad)
	: QMainWindow{ nullptr }
{
	setWindowTitle(journalAnalyser::internals::applicationLongName);
	resize(1400, 900);
	setAcceptDrops(true);

	// Placeholder until a journal is loaded
	_placeholderLabel = new QLabel{ this };
	_placeholderLabel->setAlignment(Qt::AlignCenter);
	_placeholderLabel->setTextFormat(Qt::RichText);
	setCentralWidget(_placeholderLabel);
	updatePlaceholder();

	_journalSummaryLabel = new QLabel{ this };
	statusBar()->addPermanentWidget(_journalSummaryLabel);

	createMenus();
	createNetworkGraphDock();
	createVirtualController();

	// Appearance (the stylesheet depends on the theme color and on the resulting color scheme)
	auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	applyAppearance(settings->getValue<Qt::ColorScheme>(settings::Appearance.name));
	connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
		[this](Qt::ColorScheme)
		{
			updateStyleSheet();
		});

	// Files opened through the OS (macOS FileOpen events) after the window is created
	qApp->installEventFilter(this);

	loadFiles(filesToLoad);
}

AnalyserWindow::~AnalyserWindow() noexcept = default;

void AnalyserWindow::createMenus()
{
	// File
	{
		auto* const fileMenu = menuBar()->addMenu("&File");

		auto* const openJournalAction = fileMenu->addAction("&Open Journal...");
		openJournalAction->setShortcut(QKeySequence::Open);
		connect(openJournalAction, &QAction::triggered, this,
			[this]()
			{
				auto const filePath = QFileDialog::getOpenFileName(this, "Open Event Journal", {}, QString{ "Hive Event Journal Files (*.%1)" }.arg(JournalFileExtension));
				if (!filePath.isEmpty())
				{
					loadFiles({ filePath });
				}
			});

		auto* const loadNetworkStateAction = fileMenu->addAction("&Load Network State...");
		loadNetworkStateAction->setShortcut(QKeySequence{ "Ctrl+L" });
		loadNetworkStateAction->setToolTip("Loads the entities of a network state file, so they can be inspected and displayed in the Network Graph");
		connect(loadNetworkStateAction, &QAction::triggered, this,
			[this]()
			{
				auto const filePath = QFileDialog::getOpenFileName(this, "Load Network State", {}, QString{ "ATDECC Network State Files (*.%1);;ATDECC Virtual Entity Files (*.%2)" }.arg(NetworkStateFileExtension).arg(VirtualEntityFileExtension));
				if (!filePath.isEmpty())
				{
					loadFiles({ filePath });
				}
			});

		fileMenu->addSeparator();

		auto* const closeAction = fileMenu->addAction("&Close");
		closeAction->setShortcut(QKeySequence::Close);
		connect(closeAction, &QAction::triggered, this, &AnalyserWindow::close);
	}

	// View
	{
		auto* const viewMenu = menuBar()->addMenu("&View");

		// The Network Graph dock action is added once the dock exists (createNetworkGraphDock)
		viewMenu->setObjectName("viewMenu");

		auto* const appearanceMenu = new QMenu{ "Appearance", viewMenu };
		_schemeSystemAction = appearanceMenu->addAction("System");
		_schemeLightAction = appearanceMenu->addAction("Light");
		_schemeDarkAction = appearanceMenu->addAction("Dark");
		auto* const actionGroup = new QActionGroup{ this };
		for (auto* const action : { _schemeSystemAction, _schemeLightAction, _schemeDarkAction })
		{
			action->setCheckable(true);
			actionGroup->addAction(action);
		}
		connect(_schemeSystemAction, &QAction::triggered, this,
			[this]()
			{
				applyAppearance(Qt::ColorScheme::Unknown);
			});
		connect(_schemeLightAction, &QAction::triggered, this,
			[this]()
			{
				applyAppearance(Qt::ColorScheme::Light);
			});
		connect(_schemeDarkAction, &QAction::triggered, this,
			[this]()
			{
				applyAppearance(Qt::ColorScheme::Dark);
			});
		viewMenu->addMenu(appearanceMenu);
	}

	// Help
	{
		auto* const helpMenu = menuBar()->addMenu("&Help");
		auto* const aboutAction = helpMenu->addAction(QString{ "&About %1" }.arg(journalAnalyser::internals::applicationShortName));
		aboutAction->setMenuRole(QAction::AboutRole);
		connect(aboutAction, &QAction::triggered, this,
			[this]()
			{
				QMessageBox::about(this, QString{ "About %1" }.arg(journalAnalyser::internals::applicationShortName), QString{ "<b>%1</b> %2<br/><br/>Offline analysis of Hive Event Journal files.<br/><br/>%3<br/><a href=\"%4\">%4</a>" }.arg(journalAnalyser::internals::applicationLongName).arg(journalAnalyser::internals::versionString).arg(journalAnalyser::internals::readableCopyright).arg(journalAnalyser::internals::projectURL));
			});
	}
}

void AnalyserWindow::createNetworkGraphDock()
{
	_networkGraphView = new NetworkGraphView{ this };
	_networkGraphDock = new QDockWidget{ "Network Graph", this };
	_networkGraphDock->setObjectName("networkGraphDock");
	_networkGraphDock->setWidget(_networkGraphView);
	addDockWidget(Qt::RightDockWidgetArea, _networkGraphDock);
	// Hidden until entities are loaded (the graph of an empty virtual controller is empty)
	_networkGraphDock->hide();

	if (auto* const viewMenu = menuBar()->findChild<QMenu*>("viewMenu"))
	{
		auto* const toggleAction = _networkGraphDock->toggleViewAction();
		viewMenu->insertAction(viewMenu->actions().isEmpty() ? nullptr : viewMenu->actions().first(), toggleAction);
		viewMenu->insertSeparator(viewMenu->actions().size() > 1 ? viewMenu->actions().at(1) : nullptr);
	}

	// Selecting an entity in the graph brings its inspector to the front, when one is open
	connect(_networkGraphView, &NetworkGraphView::entitySelectionChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID)
		{
			if (auto const it = _inspectors.find(entityID); it != _inspectors.end())
			{
				it->second->raise();
				it->second->activateWindow();
			}
		});
}

void AnalyserWindow::createVirtualController()
{
	// A virtual controller never touches the network: it only hosts the entities loaded from files
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	try
	{
		manager.createController(la::avdecc::protocol::ProtocolInterface::Type::Virtual, journalAnalyser::internals::applicationShortName, 0x0001, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), "en", nullptr);
	}
	catch (la::avdecc::controller::Controller::Exception const& e)
	{
		QMessageBox::critical(this, "Failed to create the virtual controller", QString{ "Entities cannot be inspected: %1" }.arg(e.what()));
	}
}

void AnalyserWindow::applyAppearance(Qt::ColorScheme const appearance)
{
	switch (appearance)
	{
		case Qt::ColorScheme::Light:
			_schemeLightAction->setChecked(true);
			break;
		case Qt::ColorScheme::Dark:
			_schemeDarkAction->setChecked(true);
			break;
		default:
			_schemeSystemAction->setChecked(true);
			break;
	}
	QApplication::styleHints()->setColorScheme(appearance);

	auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	settings->setValue(settings::Appearance.name, appearance);

	updateStyleSheet();
}

void AnalyserWindow::updateStyleSheet()
{
	// Same stylesheet as Hive, so the reused widgets look the same
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	auto const colorName = qtMate::material::color::Palette::name(settings->getValue(settings::General_ThemeColorIndex.name).toInt());
	auto const themeBackgroundColor = qtMate::material::color::value(colorName);
	auto const themeForegroundTextColor = qtMate::material::color::foregroundValue(colorName);
	auto const connectionMatrixHighlightColor = qtMate::material::color::value(colorName, qtMate::material::color::isDarkColorScheme() ? qtMate::material::color::Shade::Shade900 : qtMate::material::color::Shade::Shade100);
	auto const flatButtonColor = qtMate::material::color::value(qtMate::material::color::Name::Gray, qtMate::material::color::isDarkColorScheme() ? qtMate::material::color::Shade::Shade300 : qtMate::material::color::Shade::Shade800);
	auto const complementaryColor = qtMate::material::color::complementaryValue(colorName);

	auto styleFile = QFile{ ":/style.qss" };
	if (styleFile.open(QFile::ReadOnly))
	{
		auto const styleSheet = QString{ styleFile.readAll() }.arg(themeBackgroundColor.name()).arg(themeForegroundTextColor.name()).arg(connectionMatrixHighlightColor.name()).arg(flatButtonColor.name()).arg(complementaryColor.name());
		qApp->setStyleSheet(styleSheet);
	}
}

void AnalyserWindow::loadFiles(QStringList const& files)
{
	for (auto const& filePath : files)
	{
		auto const extension = QFileInfo{ filePath }.suffix().toLower();
		if (extension == JournalFileExtension)
		{
			loadJournal(filePath);
		}
		else if (extension == NetworkStateFileExtension)
		{
			loadNetworkState(filePath);
		}
		else if (extension == VirtualEntityFileExtension)
		{
			loadVirtualEntity(filePath);
		}
		else
		{
			statusBar()->showMessage(QString{ "Ignored '%1': not a journal, network state or virtual entity file" }.arg(QFileInfo{ filePath }.fileName()), 5000);
		}
	}
}

bool AnalyserWindow::loadJournal(QString const& filePath)
{
	// One journal per instance: another journal opens in a new instance of the application
	if (_journalView)
	{
		spawnNewInstance(filePath);
		return true;
	}

	auto session = hive::modelsLibrary::EventJournal::loadSession(filePath);
	if (!session)
	{
		QMessageBox::warning(this, "Failed to load Event Journal", QString{ "Error loading event journal file '%1'" }.arg(filePath));
		return false;
	}

	// Session summary for the status bar, gathered before the session is handed over to the view
	auto const eventCount = session->events.size();
	auto const schemaVersion = session->metadata.value("schema_version", "1");
	auto const hiveVersion = session->metadata.value("hive_version");
	auto const computerName = session->metadata.value("computer_name");
	auto const startedAt = session->metadata.value("started_utc");
	auto const firstTimestamp = eventCount > 0u ? QDateTime::fromMSecsSinceEpoch(session->events.front().timestamp).toString("yyyy-MM-dd HH:mm:ss") : QString{};
	auto const lastTimestamp = eventCount > 0u ? QDateTime::fromMSecsSinceEpoch(session->events.back().timestamp).toString("yyyy-MM-dd HH:mm:ss") : QString{};

	_journalFilePath = filePath;
	_journalView = new EventJournalView{ std::move(*session), filePath };
	connect(_journalView, &EventJournalView::selectEntityRequested, this, &AnalyserWindow::openInspector);
	setCentralWidget(_journalView); // Deletes the placeholder
	_placeholderLabel = nullptr;

	setWindowTitle(QString{ "%1 - %2" }.arg(QFileInfo{ filePath }.fileName()).arg(journalAnalyser::internals::applicationLongName));
	setWindowFilePath(filePath);

	auto summary = QString{ "%1 events" }.arg(eventCount);
	if (!firstTimestamp.isEmpty())
	{
		summary += QString{ ", %1 to %2 (local time)" }.arg(firstTimestamp).arg(lastTimestamp);
	}
	else if (!startedAt.isEmpty())
	{
		summary += QString{ ", started %1 (local time)" }.arg(localTimeFromMetadata(startedAt));
	}
	if (!computerName.isEmpty())
	{
		summary += QString{ ", recorded on %1" }.arg(computerName);
	}
	if (!hiveVersion.isEmpty())
	{
		summary += QString{ " by Hive %1" }.arg(hiveVersion);
	}
	summary += QString{ ", schema version %1" }.arg(schemaVersion);
	_journalSummaryLabel->setText(summary);

	if (!_hasLoadedNetworkState)
	{
		statusBar()->showMessage("Load a network state file (File > Load Network State) to inspect the entities and display the Network Graph", 10000);
	}
	return true;
}

void AnalyserWindow::loadNetworkState(QString const& filePath)
{
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto const [error, message] = manager.loadVirtualEntitiesFromJsonNetworkState(filePath, loadFlags());
	if (!!error)
	{
		QMessageBox::warning(this, "Failed to load Network State", QString{ "Error loading file '%1':\n%2" }.arg(filePath).arg(deserializationErrorString(error, message)));
		return;
	}
	_hasLoadedNetworkState = true;
	showNetworkGraph();
	statusBar()->showMessage(QString{ "Loaded network state '%1'" }.arg(QFileInfo{ filePath }.fileName()), 5000);
	updatePlaceholder();
}

void AnalyserWindow::loadVirtualEntity(QString const& filePath)
{
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto flags = loadFlags();
	auto [error, message] = manager.loadVirtualEntityFromJson(filePath, flags);
	if (error == la::avdecc::jsonSerializer::DeserializationError::NotCompliant)
	{
		if (QMessageBox::question(this, {}, "Entity model is not fully IEEE1722.1 compliant.\n\nDo you want to import anyway?", QMessageBox::StandardButton::Yes, QMessageBox::StandardButton::No) == QMessageBox::StandardButton::Yes)
		{
			flags.set(la::avdecc::entity::model::jsonSerializer::Flag::IgnoreAEMSanityChecks);
			std::tie(error, message) = manager.loadVirtualEntityFromJson(filePath, flags);
		}
	}
	if (!!error)
	{
		QMessageBox::warning(this, "Failed to load Entity", QString{ "Error loading file '%1':\n%2" }.arg(filePath).arg(deserializationErrorString(error, message)));
		return;
	}
	_hasLoadedNetworkState = true;
	showNetworkGraph();
	statusBar()->showMessage(QString{ "Loaded virtual entity '%1'" }.arg(QFileInfo{ filePath }.fileName()), 5000);
	updatePlaceholder();
}

void AnalyserWindow::showNetworkGraph()
{
	if (_networkGraphDock->isVisible())
	{
		return;
	}
	_networkGraphDock->show();
	// The topology of a whole installation is wide: give the graph half of the window the first time it is shown
	resizeDocks({ _networkGraphDock }, { width() / 2 }, Qt::Horizontal);
}

void AnalyserWindow::openInspector(la::avdecc::UniqueIdentifier const entityID)
{
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	if (!manager.getControlledEntity(entityID))
	{
		auto const hint = _hasLoadedNetworkState ? QString{ "it is not part of the loaded network state" } : QString{ "load a network state file first (File > Load Network State)" };
		statusBar()->showMessage(QString{ "Cannot inspect entity %1: %2" }.arg(hive::modelsLibrary::helper::uniqueIdentifierToString(entityID)).arg(hint), 5000);
		return;
	}

	auto* inspector = static_cast<EntityInspector*>(nullptr);
	if (auto const it = _inspectors.find(entityID); it != _inspectors.end())
	{
		inspector = it->second;
	}
	else
	{
		// Standalone window (no parent), closed and destroyed by the user or when this window closes
		inspector = new EntityInspector{ nullptr };
		inspector->setAttribute(Qt::WA_DeleteOnClose);
		inspector->resize(900, 700);
		inspector->setControlledEntityID(entityID);
		connect(inspector, &QObject::destroyed, this,
			[this, entityID]()
			{
				_inspectors.erase(entityID);
			});
		_inspectors[entityID] = inspector;
	}
	inspector->show();
	inspector->raise();
	inspector->activateWindow();
}

void AnalyserWindow::spawnNewInstance(QString const& filePath)
{
	if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), { QDir::toNativeSeparators(filePath) }))
	{
		QMessageBox::warning(this, "Failed to open Event Journal", QString{ "Cannot start a new instance of %1 for '%2'" }.arg(journalAnalyser::internals::applicationShortName).arg(filePath));
	}
}

void AnalyserWindow::updatePlaceholder()
{
	if (!_placeholderLabel)
	{
		return;
	}
	_placeholderLabel->setText(QString{ "<h2>%1</h2><p>Open a Hive Event Journal file (File &gt; Open Journal, or drop a .%2 file here).</p><p>Load a network state file (File &gt; Load Network State, or drop a .%3 file here) to inspect the entities and display the Network Graph.</p>" }.arg(journalAnalyser::internals::applicationLongName).arg(JournalFileExtension).arg(NetworkStateFileExtension));
}

bool AnalyserWindow::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::FileOpen)
	{
		auto const* const fileEvent = static_cast<QFileOpenEvent const*>(event);
		loadFiles({ fileEvent->file() });
		return true;
	}
	return QMainWindow::eventFilter(watched, event);
}

void AnalyserWindow::dragEnterEvent(QDragEnterEvent* event)
{
	for (auto const& url : event->mimeData()->urls())
	{
		auto const extension = QFileInfo{ url.toLocalFile() }.suffix().toLower();
		if (extension == JournalFileExtension || extension == NetworkStateFileExtension || extension == VirtualEntityFileExtension)
		{
			event->acceptProposedAction();
			return;
		}
	}
}

void AnalyserWindow::dropEvent(QDropEvent* event)
{
	auto files = QStringList{};
	for (auto const& url : event->mimeData()->urls())
	{
		files.append(url.toLocalFile());
	}
	loadFiles(files);
}

void AnalyserWindow::closeEvent(QCloseEvent* event)
{
	// The inspectors are standalone windows: close them with the main window (the application quits when the last window is closed)
	auto inspectors = std::vector<EntityInspector*>{};
	for (auto const& [entityID, inspector] : _inspectors)
	{
		inspectors.push_back(inspector);
	}
	for (auto* const inspector : inspectors)
	{
		inspector->close();
	}
	QMainWindow::closeEvent(event);
}
