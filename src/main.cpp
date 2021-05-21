/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <la/avdecc/utils.hpp>

#include <QApplication>
#include <QFontDatabase>

#include <QSharedMemory>
#include <QMessageBox>
#include <QFile>
#include <QSplashScreen>
#include <QDesktopWidget>

#include <iostream>
#include <chrono>

#include "mainWindow.hpp"
#include "sparkleHelper/sparkleHelper.hpp"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "profiles/profileSelectionDialog.hpp"

#include <hive/modelsLibrary/controllerManager.hpp>

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
#ifndef HIVE_IS_RELEASE_VERSION
	BT_SetDumpType(MiniDumpWithDataSegs | MiniDumpWithFullMemory);
#endif
	BT_SetSupportEMail("christophe.calmejane@l-acoustics.com");
	BT_SetFlags(BTF_DETAILEDMODE | BTF_ATTACHREPORT | BTF_SHOWADVANCEDUI | BTF_DESCRIBEERROR);
	BT_SetSupportServer("hive-crash-reports.changeip.org", 9999);
}

#else // Nothing on other OS right now
void setupBugReporter() {}
#endif

int main(int argc, char* argv[])
{
	// Setup Bug Reporter
	setupBugReporter();

	// Replace Qt Message Handler
	previousHandler = qInstallMessageHandler(&qtMessageHandler);

	// Configure QT Application
	QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles, true);
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

	QCoreApplication::setOrganizationDomain(hive::internals::organizationDomain);
	QCoreApplication::setOrganizationName(hive::internals::organizationName);
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

#if 0
	QSharedMemory lock("d2794ee0-ab5e-48a5-9189-78a9e2c40635");
	if (!lock.create(512, QSharedMemory::ReadWrite))
	{
#	pragma message("TODO: Read the SM and cast to a processID. Check if that processID is still active and if not, continue.")
		QMessageBox::critical(nullptr, {}, "An instance of this application is already running.", QMessageBox::Ok);
		return 1;
	}
#endif

	// Register settings (creating default value if none was saved before)
	auto& settings = settings::SettingsManager::getInstance();

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

	// Network
	settings.registerSetting(settings::Network_ProtocolType);
	settings.registerSetting(settings::Network_InterfaceTypeEthernet);
	settings.registerSetting(settings::Network_InterfaceTypeWiFi);

	// Controller
	settings.registerSetting(settings::Controller_DiscoveryDelay);
	settings.registerSetting(settings::Controller_AemCacheEnabled);
	settings.registerSetting(settings::Controller_FullStaticModelEnabled);

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
	auto const userProfile = settings.getValue(settings::UserProfile.name).value<profiles::ProfileType>();

	// First time launch, ask the user to choose a profile
	if (userProfile == profiles::ProfileType::None)
	{
		auto profileSelectionDialog = profiles::ProfileSelectionDialog{};
		profileSelectionDialog.exec();
		auto const profile = profileSelectionDialog.selectedProfile();
		settings.setValue(settings::UserProfile.name, la::avdecc::utils::to_integral(profile));
	}

	auto const logo = QPixmap{ ":/Logo.png" };
	auto splash = QSplashScreen{ logo, Qt::WindowStaysOnTopHint };

	// Use MainWindow geometry on a dummy widget to get the target screen (i.e, where the main window will appear)
	QWidget dummy;
	auto const mainWindowGeometry = settings.getValue(settings::MainWindowGeometry).toByteArray();
	dummy.restoreGeometry(mainWindowGeometry);
	auto const availableScreenGeometry = QApplication::desktop()->screenGeometry(&dummy);

	// Center our splash screen on this target screen
	splash.move(availableScreenGeometry.center() - logo.rect().center());

	splash.show();
	app.processEvents();

	/* Load everything we need */
	std::chrono::time_point<std::chrono::system_clock> start{ std::chrono::system_clock::now() };

	// Initialize Sparkle
	{
		QFile signatureFile(":/dsa_pub.pem");
		if (signatureFile.open(QIODevice::ReadOnly))
		{
			auto content = QString(signatureFile.readAll());
			Sparkle::getInstance().init(hive::internals::buildNumber.toStdString(), content.toStdString());
		}
	}

	// Load main window
	auto window = MainWindow{ mustResetViewSettings };
	//window.show(); // This forces the creation of the window // Don't try to show it, it blinks sometimes (and window.hide() seems to create the window too)
	window.hide(); // Immediately hides it (even though it was not actually shown since processEvents was not called)

	/* Loading done - Keep the splashscreen displayed until specified delay */
	do
	{
		app.processEvents();
		// Wait a little bit so we don't burn the CPU
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	} while (splash.isVisible() && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() <= SPLASH_DELAY);

	/* Ok, kill the splashscreen and show the main window */
	splash.close();
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
