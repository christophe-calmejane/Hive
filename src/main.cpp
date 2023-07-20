/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "mainWindow.hpp"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "profiles/profileSelectionDialog.hpp"
#include "processHelper/processHelper.hpp"

#include <la/avdecc/utils.hpp>
#ifdef USE_SPARKLE
#	include <sparkleHelper/sparkleHelper.hpp>
#endif // USE_SPARKLE
#include <hive/modelsLibrary/controllerManager.hpp>

#include <QApplication>
#include <QFontDatabase>
#include <QSharedMemory>
#include <QMessageBox>
#include <QFile>
#include <QSplashScreen>
#include <QCommandLineParser>
#include <QScreen>
#include <QStringList>
#include <QtGlobal>
#if QT_VERSION < 0x050F00
#	include <QDesktopWidget>
#endif // Qt < 5.15.0

#include <iostream>
#include <chrono>

#ifdef DEBUG
#	define SPLASH_DELAY 0
#else // !DEBUG
#	define SPLASH_DELAY 1250
#endif // DEBUG

static QtMessageHandler previousHandler = nullptr;
static void qtMessageHandler(QtMsgType msgType, QMessageLogContext const& logContext, QString const& message)
{
	if (msgType == QtMsgType::QtFatalMsg)
	{
		AVDECC_ASSERT_WITH_RET(false, message.toStdString());
	}
	previousHandler(msgType, logContext, message);
}

// Setup BugTrap on windows (win32 only right now)
#if defined(Q_OS_WIN32) && defined(HAVE_BUGTRAP)
#	define BUGREPORTER_CATCH_EXCEPTIONS
#	include <Windows.h>
#	include <DbgHelp.h>
#	include "BugTrap.h"

void setupBugReporter()
{
	BT_InstallSehFilter();

	BT_SetTerminate();
#	ifndef HIVE_IS_RELEASE_VERSION
	BT_SetDumpType(MiniDumpWithDataSegs | MiniDumpWithFullMemory);
#	endif
	BT_SetSupportEMail("christophe.calmejane@l-acoustics.com");
	BT_SetFlags(BTF_DETAILEDMODE | BTF_ATTACHREPORT | BTF_SHOWADVANCEDUI | BTF_DESCRIBEERROR);
	BT_SetSupportServer("hive-crash-reports.changeip.org", 9999);
}

#else // Nothing on other OS right now
void setupBugReporter() {}
#endif

struct InstanceInfo
{
	QSharedMemory shm{ "d2794ee0-ab5e-48a5-9189-78a9e2c40635" };
	bool isAlreadyRunning{ false };
	utils::ProcessHelper::ProcessID pid{ 0u };
};

int main(int argc, char* argv[])
{
	// Setup Bug Reporter
	setupBugReporter();

	// Replace Qt Message Handler
	previousHandler = qInstallMessageHandler(&qtMessageHandler);

	// Configure QT Application
	QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles, true);
#if QT_VERSION < 0x060000
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif // QT < 6

	QCoreApplication::setOrganizationDomain(hive::internals::companyDomain);
	QCoreApplication::setOrganizationName(hive::internals::companyName);
	QCoreApplication::setApplicationName(hive::internals::applicationShortName);
	QCoreApplication::setApplicationVersion(hive::internals::versionString);

	// Create the Qt Application
	QApplication app(argc, argv);

	// Runtime sanity check on Avdecc Library compilation options
	{
		auto const options = la::avdecc::getCompileOptions();
		if (!options.test(la::avdecc::CompileOption::EnableRedundancy))
		{
			QMessageBox::warning(nullptr, "", "Avdecc Library was not compiled with Redundancy feature, which is required by " + hive::internals::applicationShortName);
			return 0;
		}
	}

	// Runtime sanity check on Avdecc Controller Library compilation options
	auto const options = la::avdecc::controller::getCompileOptions();
	if (!options.test(la::avdecc::controller::CompileOption::EnableRedundancy))
	{
		QMessageBox::warning(nullptr, "", "Avdecc Controller Library was not compiled with Redundancy feature, which is required by " + hive::internals::applicationShortName);
		return 0;
	}

	// Check if we are already running
	auto instanceInfo = InstanceInfo{};
	if (!instanceInfo.shm.create(8, QSharedMemory::ReadWrite))
	{
		// If we failed to create the shared memory, check if it's because it already exists
		if (instanceInfo.shm.error() == QSharedMemory::AlreadyExists)
		{
			// Attach to the shared memory
			if (instanceInfo.shm.attach(QSharedMemory::ReadWrite))
			{
				// Make sure the other instance is still running by checking its process ID
				{
					// Read the process ID from the shared memory
					instanceInfo.shm.lock();
					instanceInfo.pid = *static_cast<utils::ProcessHelper::ProcessID const*>(instanceInfo.shm.constData());
					instanceInfo.shm.unlock();
				}
				// Check if the process is still running
				instanceInfo.isAlreadyRunning = utils::ProcessHelper::isProcessRunning(instanceInfo.pid);
			}
		}
	}
	// If not already running, write our process ID to the shared memory
	if (!instanceInfo.isAlreadyRunning && instanceInfo.shm.isAttached())
	{
		instanceInfo.shm.lock();
		*static_cast<utils::ProcessHelper::ProcessID*>(instanceInfo.shm.data()) = utils::ProcessHelper::getCurrentProcessID();
		instanceInfo.shm.unlock();
	}

	// Parse command line
	auto parser = QCommandLineParser{};
	auto const singleOption = QCommandLineOption{ { "s", "single" }, "Exist if another instance is already running" };
	auto const settingsFileOption = QCommandLineOption{ "settings", "Use the specified Settings file (.ini)", "Hive Settings" };
	auto const ansFilesOption = QCommandLineOption{ "ans", "Load the specified ATDECC Network State (.ans)", "Network State" };
	auto const aveFilesOption = QCommandLineOption{ "ave", "Load the specified ATDECC Virtual Entity (.ave)", "Virtual Entity" };
	parser.addOption(singleOption);
	parser.addOption(settingsFileOption);
	parser.addOption(ansFilesOption);
	parser.addOption(aveFilesOption);
	parser.addPositionalArgument("files", "Files to load (.ave, .ans, .json)", "[files...]");
	parser.addHelpOption();
	parser.addVersionOption();

	parser.process(app);

	// Get files to load
	auto filesToLoad = QStringList{};
	for (auto const& value : parser.values(ansFilesOption))
	{
		filesToLoad.append(value);
	}
	for (auto const& value : parser.values(aveFilesOption))
	{
		filesToLoad.append(value);
	}
	for (auto const& value : parser.positionalArguments())
	{
		filesToLoad.append(value);
	}

#ifdef _WIN32
	// If we have a single file to load, we want to forward it to any already running instance
	if (filesToLoad.size() == 1 && instanceInfo.isAlreadyRunning)
	{
		auto const data = filesToLoad[0].toUtf8();
		auto const dataLength = data.size();

		// On windows, we send a custom message to the main window of the other instance
		// First we need to get the main window handle of the other instance (using its process ID)
		static auto mainWindowHandle = HWND{ 0u };
		auto const enumWindowsProc = [](HWND hwnd, LPARAM lParam)
		{
			auto windowPid = DWORD{ 0u };
			auto const otherPID = static_cast<decltype(windowPid)>(lParam);
			GetWindowThreadProcessId(hwnd, &windowPid);
			if (windowPid == otherPID)
			{
				mainWindowHandle = hwnd;
				return FALSE;
			}
			return TRUE;
		};
		EnumWindows(enumWindowsProc, static_cast<LPARAM>(instanceInfo.pid));

		if (mainWindowHandle)
		{
			// Send a WM_COPYDATA message to the main window of the other instance
			auto cds = COPYDATASTRUCT{};
			cds.dwData = static_cast<decltype(cds.dwData)>(MainWindow::MessageType::LoadFileMessage);
			cds.cbData = dataLength;
			cds.lpData = const_cast<char*>(data.constData());
			SendMessageA(mainWindowHandle, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
			return 0;
		}
	}
#endif // _WIN32

	// Check if another instance is already running and the single instance option was specified
	if (instanceInfo.isAlreadyRunning && parser.isSet(singleOption))
	{
		QMessageBox::critical(nullptr, {}, "Another instance of Hive is already running.", QMessageBox::Ok);
		return 0;
	}

	// Register settings (creating default value if none was saved before)
	auto const settingsFileParsed = parser.value(settingsFileOption);
	auto settingsFile = std::optional<QString>{};
	if (!settingsFileParsed.isEmpty())
	{
		settingsFile = settingsFileParsed;
	}
	auto settingsManager = settings::SettingsManager::create(settingsFile);
	auto& settings = *settingsManager;
	app.setProperty(settings::SettingsManager::PropertyName, QVariant::fromValue(settingsManager.get()));

	// General
	settings.registerSetting(settings::LastLaunchedVersion);
	settings.registerSetting(settings::General_AutomaticPNGDownloadEnabled);
	settings.registerSetting(settings::General_AutomaticCheckForUpdates);
	settings.registerSetting(settings::General_CheckForBetaVersions);
	settings.registerSetting(settings::General_ThemeColorIndex);

	// Connection matrix
	settings.registerSetting(settings::ConnectionMatrix_Transpose);
	settings.registerSetting(settings::ConnectionMatrix_ChannelMode);
	settings.registerSetting(settings::ConnectionMatrix_AlwaysShowArrowTip);
	settings.registerSetting(settings::ConnectionMatrix_AlwaysShowArrowEnd);
	settings.registerSetting(settings::ConnectionMatrix_ShowMediaLockedDot);
	settings.registerSetting(settings::ConnectionMatrix_AllowCRFAudioConnection);
	settings.registerSetting(settings::ConnectionMatrix_CollapsedByDefault);
	settings.registerSetting(settings::ConnectionMatrix_ShowEntitySummary);

	// Network
	settings.registerSetting(settings::Network_ProtocolType);
	settings.registerSetting(settings::Network_InterfaceTypeEthernet);
	settings.registerSetting(settings::Network_InterfaceTypeWiFi);

	// Controller
	settings.registerSetting(settings::Controller_DiscoveryDelay);
	settings.registerSetting(settings::Controller_AemCacheEnabled);
	settings.registerSetting(settings::Controller_FullStaticModelEnabled);
	settings.registerSetting(settings::Controller_AdvertisingEnabled);
	settings.registerSetting(settings::Controller_ControllerSubID);

	// Check settings version
	auto mustResetViewSettings = false;
	auto const settingsVersion = settings.getValue(settings::ViewSettingsVersion).toInt();
	if (settingsVersion != settings::ViewSettingsCurrentVersion)
	{
		mustResetViewSettings = true;
	}
	settings.setValue(settings::ViewSettingsVersion, settings::ViewSettingsCurrentVersion);

	// Load fonts
	if (QFontDatabase::addApplicationFont(":/MaterialIcons-Regular.ttf") == -1) // From https://material.io/icons/
	{
		QMessageBox::critical(nullptr, "", "Failed to load font resource.\n\nCannot continue!");
		return 1;
	}
	if (QFontDatabase::addApplicationFont(":/Hive.ttf") == -1) // Our own made font
	{
		QMessageBox::critical(nullptr, "", "Failed to load font resource.\n\nCannot continue!");
		return 1;
	}

	// Read saved profile
	auto const userProfile = settings.getValue<profiles::ProfileType>(settings::UserProfile.name);

	// First time launch, ask the user to choose a profile
	if (userProfile == profiles::ProfileType::None)
	{
		auto profileSelectionDialog = profiles::ProfileSelectionDialog{};
		profileSelectionDialog.exec();
		auto const profile = profileSelectionDialog.selectedProfile();
		settings.setValue(settings::UserProfile.name, profile);
	}

	auto const logo = QPixmap{ ":/Logo.png" };
	auto splash = QSplashScreen{ logo, Qt::WindowStaysOnTopHint };

	// Use MainWindow geometry on a dummy widget to get the target screen (i.e, where the main window will appear)
	QWidget dummy;
	auto const mainWindowGeometry = settings.getValue(settings::MainWindowGeometry).toByteArray();
	dummy.restoreGeometry(mainWindowGeometry);
#if QT_VERSION < 0x050F00
	auto const availableScreenGeometry = QApplication::desktop()->screenGeometry(&dummy);
#else // Qt >= 5.15.0
	auto const availableScreenGeometry = dummy.screen()->availableGeometry();
#endif // Qt < 5.15.0

	// Center our splash screen on this target screen
	splash.move(availableScreenGeometry.center() - logo.rect().center());

	splash.show();
	app.processEvents();

	/* Load everything we need */
	std::chrono::time_point<std::chrono::system_clock> start{ std::chrono::system_clock::now() };

#ifdef USE_SPARKLE
	// Initialize Sparkle
	{
		QFile signatureFile(":/dsa_pub.pem");
		if (signatureFile.open(QIODevice::ReadOnly))
		{
			auto content = QString(signatureFile.readAll());
			Sparkle::getInstance().init(hive::internals::buildNumber.toStdString(), content.toStdString());
		}
	}
#endif // USE_SPARKLE

	// Load main window (and all associated resources) while the splashscreen is displayed
	auto window = MainWindow{ mustResetViewSettings, std::move(filesToLoad) };

#if defined(Q_OS_MACOS)
	// The native window has to be created before the first processEvents() for the initial position and size to be correctly set.
	// On macOS show() is required because obj-c lazy init is being used to create the view (move/resize will be ignored until actually created).
	// We don't want to do the same on Windows/Linux as the window would actually be shown then hidden immediately causing a small blink.
	window.show();
	window.hide();
#endif

	/* Loading done - Keep the splashscreen displayed until specified delay */
	do
	{
		app.processEvents();
		// Wait a little bit so we don't burn the CPU
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	} while (splash.isVisible() && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() <= SPLASH_DELAY);

	/* Ok, kill the splashscreen and show the main window */
	splash.close();
	window.setReady();
	window.show();

	auto retValue = int{ 0u };
#ifndef BUGREPORTER_CATCH_EXCEPTIONS
	try
#endif // !BUGREPORTER_CATCH_EXCEPTIONS
	{
		retValue = app.exec();
	}
#ifndef BUGREPORTER_CATCH_EXCEPTIONS
	catch (std::exception const& e)
	{
		QMessageBox::warning(nullptr, "", QString("Uncaught exception: %1").arg(e.what()));
	}
	catch (...)
	{
		QMessageBox::warning(nullptr, "", "Uncaught exception");
	}
#endif // !BUGREPORTER_CATCH_EXCEPTIONS

	// Destroy the controller before leaving main (so it's properly cleaned before all static variables are destroyed in a random order)
	hive::modelsLibrary::ControllerManager::getInstance().destroyController();

	// Return from main
	return retValue;
}
