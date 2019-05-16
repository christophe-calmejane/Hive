/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QApplication>
#include <QFontDatabase>

#include <QSharedMemory>
#include <QMessageBox>
#include <QFile>
#include <QSplashScreen>

#include <iostream>
#include <chrono>

#include "mainWindow.hpp"
#include "avdecc/controllerManager.hpp"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "profiles/profileSelectionDialog.hpp"

#ifdef DEBUG
#	define SPLASH_DELAY 0
#else // !DEBUG
#	define SPLASH_DELAY 1250
#endif // DEBUG

// Setup BugTrap on windows (win32 only right now)
#if defined(Q_OS_WIN32) && defined(HAVE_BUGTRAP)
#	define BUGREPORTER_CATCH_EXCEPTIONS
#	include <Windows.h>
#	include "BugTrap.h"
void setupBugReporter()
{
	BT_InstallSehFilter();

	BT_SetTerminate();
	BT_SetSupportEMail("christophe.calmejane@l-acoustics.com");
	BT_SetFlags(BTF_DETAILEDMODE | BTF_ATTACHREPORT | BTF_SHOWADVANCEDUI | BTF_DESCRIBEERROR);
	BT_SetSupportServer("localhost", 9999);
}

#else // Nothing on other OS right now
void setupBugReporter() {}
#endif

int main(int argc, char* argv[])
{
	// Setup Bug Reporter
	setupBugReporter();

	// Configure QT Application
#if defined(Q_OS_WIN)
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

	QCoreApplication::setOrganizationDomain(hive::internals::organizationDomain);
	QCoreApplication::setOrganizationName(hive::internals::organizationName);
	QCoreApplication::setApplicationName(hive::internals::applicationShortName);
	QCoreApplication::setApplicationVersion(hive::internals::versionString);

	// We want to propagate style sheet styles to all widgets
	QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles, true);

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
	settings.registerSetting(settings::AutomaticPNGDownloadEnabled);
	settings.registerSetting(settings::TransposeConnectionMatrix);
	settings.registerSetting(settings::AutomaticCheckForUpdates);
	settings.registerSetting(settings::CheckForBetaVersions);
	settings.registerSetting(settings::ThemeColorIndex);

	// Network
	settings.registerSetting(settings::ProtocolType);
	settings.registerSetting(settings::InterfaceTypeEthernet);
	settings.registerSetting(settings::InterfaceTypeWiFi);

	// Controller
	settings.registerSetting(settings::AemCacheEnabled);

	// Load fonts
	// https://material.io/icons/
	QFontDatabase::addApplicationFont(":/MaterialIcons-Regular.ttf");

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

	QPixmap logo(":/Logo.png");
	QSplashScreen splash(logo, Qt::WindowStaysOnTopHint);
	splash.show();
	app.processEvents();

	/* Load everything we need */
	std::chrono::time_point<std::chrono::system_clock> start{ std::chrono::system_clock::now() };

	// Load main window
	auto window = MainWindow{};
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
	avdecc::ControllerManager::getInstance().destroyController();

	// Return from main
	return retValue;
}
