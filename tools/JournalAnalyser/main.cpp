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

#include "application.hpp"
#include "settingsManager/settings.hpp"
#include "settingsManager/settingsManager.hpp"

#include <hive/modelsLibrary/controllerManager.hpp>
#include <la/avdecc/utils.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QMessageBox>
#include <QSettings>
#include <QStringList>
#include <QtGlobal>

static QtMessageHandler previousHandler = nullptr;
static void qtMessageHandler(QtMsgType msgType, QMessageLogContext const& logContext, QString const& message)
{
	if (msgType == QtMsgType::QtFatalMsg)
	{
		AVDECC_ASSERT_WITH_RET(false, message.toStdString());
	}
	previousHandler(msgType, logContext, message);
}

// Fonts required by the reused Hive widgets
static auto const s_fonts = QStringList{
	":/MaterialIcons-Regular.ttf", // From https://material.io/icons/
	":/Hive.ttf", // Our own made font
	":/FuturaLT.ttf", // Futura LT Book
	":/FuturaLT-ExtraBold.ttf", // Futura LT ExtraBold
};

static bool loadFonts()
{
	for (auto const& fontPath : s_fonts)
	{
		if (QFontDatabase::addApplicationFont(fontPath) == -1)
		{
			QMessageBox::critical(nullptr, "", QString{ "Failed to load font resource %1.\n\nCannot continue!" }.arg(fontPath));
			return false;
		}
	}
	return true;
}

/** Registers the settings read by the reused Hive widgets (with their default values), in the analyser's own settings file. */
static void registerSettings(settings::SettingsManager& settings)
{
	settings.registerSetting(settings::General_ThemeColorIndex);
	settings.registerSetting(settings::Appearance);
	settings.registerSetting(settings::NetworkGraph_AggregatedLayout);
	settings.registerSetting(settings::Controller_AemCacheEnabled);
	settings.registerSetting(settings::Controller_FastEnumerationEnabled);
	settings.registerSetting(settings::Controller_FullStaticModelEnabled);
}

int main(int argc, char* argv[])
{
	// Replace Qt Message Handler
	previousHandler = qInstallMessageHandler(&qtMessageHandler);

#if defined(Q_OS_WIN32)
	// Enable dark mode on windows
	qputenv("QT_QPA_PLATFORM", "windows:darkmode=2"); // Not actually needed, since Qt 6.5 dark mode is enabled by default
#endif // Q_OS_WIN32

	// Set the "fusion" style (same as Hive: dark mode was removed from the windowsvista default style in Qt 6.5)
	QApplication::setStyle("fusion");

	// Configure QT Application
	QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles, true);

	QCoreApplication::setOrganizationDomain(journalAnalyser::internals::companyDomain);
	QCoreApplication::setOrganizationName(journalAnalyser::internals::companyName);
	QCoreApplication::setApplicationName(journalAnalyser::internals::applicationShortName);
	QCoreApplication::setApplicationVersion(journalAnalyser::internals::versionString);

	// Create the Qt Application (HiveApplication tracks the color scheme for the reused widgets, and buffers the macOS FileOpen events received before the main window exists)
	auto app = HiveApplication{ argc, argv };

	// Parse command line
	auto parser = QCommandLineParser{};
	parser.setApplicationDescription("Offline analysis of Hive Event Journal files");
	parser.addPositionalArgument("files", "Journal file (.hej) to analyze, and optionally network state (.ans) or virtual entity (.ave) files to load", "[files...]");
	parser.addHelpOption();
	parser.addVersionOption();
	parser.process(app);

	// Runtime sanity check on Avdecc Library compilation options
	{
		auto const options = la::avdecc::getCompileOptions();
		if (!options.test(la::avdecc::CompileOption::EnableRedundancy))
		{
			QMessageBox::warning(nullptr, "", "Avdecc Library was not compiled with Redundancy feature, which is required by " + journalAnalyser::internals::applicationShortName);
			return 0;
		}
	}

	// Runtime sanity check on Avdecc Controller Library compilation options
	{
		auto const options = la::avdecc::controller::getCompileOptions();
		if (!options.test(la::avdecc::controller::CompileOption::EnableRedundancy))
		{
			QMessageBox::warning(nullptr, "", "Avdecc Controller Library was not compiled with Redundancy feature, which is required by " + journalAnalyser::internals::applicationShortName);
			return 0;
		}
	}

	// Settings: the analyser keeps its own file (same location scheme as Hive, but never Hive's file)
	auto const settingsFilePath = QSettings{ QSettings::Format::IniFormat, QSettings::Scope::UserScope, journalAnalyser::internals::companyName, journalAnalyser::internals::applicationShortName }.fileName();
	auto settingsManager = settings::SettingsManager::create(settingsFilePath);
	registerSettings(*settingsManager);
	app.setProperty(settings::SettingsManager::PropertyName, QVariant::fromValue(settingsManager.get()));

	if (!loadFonts())
	{
		return 1;
	}

	// Files to load: command line arguments, plus the files the OS asked to open before the window existed
	auto filesToLoad = parser.positionalArguments();
	filesToLoad.append(app.takeFilesToLoad());

	auto window = AnalyserWindow{ filesToLoad };
	window.show();

	auto retValue = int{ 0u };
	try
	{
		retValue = app.exec();
	}
	catch (std::exception const& e)
	{
		QMessageBox::warning(nullptr, "", QString{ "Uncaught exception: %1" }.arg(e.what()));
	}
	catch (...)
	{
		QMessageBox::warning(nullptr, "", "Uncaught exception");
	}

	// Destroy the controller before leaving main (so it's properly cleaned before all static variables are destroyed in a random order)
	hive::modelsLibrary::ControllerManager::getInstance().destroyController();

	return retValue;
}
