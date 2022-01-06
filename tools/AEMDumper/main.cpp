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

#include <la/avdecc/utils.hpp>

#include <QApplication>
#include <QFontDatabase>

#include <QSharedMemory>
#include <QMessageBox>
#include <QFile>
#include <QSplashScreen>
#include <QtGlobal>

#include <iostream>
#include <chrono>

#include "mainWindow.hpp"
#include "config.hpp"

#include <hive/modelsLibrary/controllerManager.hpp>

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
#	include "BugTrap.h"

void setupBugReporter()
{
	BT_InstallSehFilter();

	BT_SetTerminate();
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
#if QT_VERSION < 0x060000
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif // QT < 6

	QCoreApplication::setOrganizationDomain(aemDumper::internals::companyDomain);
	QCoreApplication::setOrganizationName(aemDumper::internals::companyName);
	QCoreApplication::setApplicationName(aemDumper::internals::applicationShortName);
	QCoreApplication::setApplicationVersion(aemDumper::internals::versionString);

	// Create the Qt Application
	QApplication app(argc, argv);

	// Runtime sanity check on Avdecc Library compilation options
	{
		auto const options = la::avdecc::getCompileOptions();
		if (!options.test(la::avdecc::CompileOption::EnableRedundancy))
		{
			QMessageBox::warning(nullptr, "", "Avdecc Library was not compiled with Redundancy feature, which is required by " + aemDumper::internals::applicationShortName);
			return 0;
		}
	}

	// Runtime sanity check on Avdecc Controller Library compilation options
	auto const options = la::avdecc::controller::getCompileOptions();
	if (!options.test(la::avdecc::controller::CompileOption::EnableRedundancy))
	{
		QMessageBox::warning(nullptr, "", "Avdecc Controller Library was not compiled with Redundancy feature, which is required by " + aemDumper::internals::applicationShortName);
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

	// Load main window
	auto window = MainWindow{};
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
