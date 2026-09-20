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

#pragma once

#include <hive/modelsLibrary/eventJournal.hpp>
#include <hive/modelsLibrary/journalAnalysis.hpp>
#include <hive/modelsLibrary/journalReplay.hpp>

#include <la/avdecc/internals/uniqueIdentifier.hpp>

#include <nlohmann/json.hpp>

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QTextBrowser>
#include <QToolBar>
#include <QUrl>

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class EntityInspector;
class EventJournalView;
class NetworkGraphView;

/**
* @brief Main window of the Journal Analyser: one Event Journal file, standalone Entity Inspectors and the Network Graph.
* @details The window analyzes a single journal file (the application runs one instance per journal).
*          The entities of the journal are inspected through a virtual controller hosting the twin of the network: the entities as they were at the selected event, rebuilt by the JournalReplay from the snapshots and the change tracking events of the journal (a network state file, File > Load Network State, replaces the twin), which also feeds the Network Graph.
*          The Findings dock shows the report of the JournalAnalysis (the computations of the after show reports), with links to the events; File > Export Report writes it as a Markdown file.
*/
class AnalyserWindow : public QMainWindow
{
	Q_OBJECT
public:
	/**
	* @brief Creates the main window.
	* @param[in] filesToLoad Files to load at startup: a journal file (.hej) and optionally network state (.ans) or virtual entity (.ave) files.
	*/
	AnalyserWindow(QStringList const& filesToLoad);
	virtual ~AnalyserWindow() noexcept override;

	// Deleted compiler auto-generated methods
	AnalyserWindow(AnalyserWindow const&) = delete;
	AnalyserWindow(AnalyserWindow&&) = delete;
	AnalyserWindow& operator=(AnalyserWindow const&) = delete;
	AnalyserWindow& operator=(AnalyserWindow&&) = delete;

	/**
	* @brief Loads the given files, dispatching on their extension.
	* @details A journal file is loaded in this window if none is loaded yet, otherwise it is opened in a new instance of the application.
	*          Network state and virtual entity files are loaded in the virtual controller.
	*/
	void loadFiles(QStringList const& files);

protected:
	virtual bool eventFilter(QObject* watched, QEvent* event) override;
	virtual void dragEnterEvent(QDragEnterEvent* event) override;
	virtual void dropEvent(QDropEvent* event) override;
	virtual void closeEvent(QCloseEvent* event) override;

private:
	void createMenus();
	void createReplayToolBar();
	void createNetworkGraphDock();
	void createFindingsDock();
	void createVirtualController();
	void applyAppearance(Qt::ColorScheme const appearance);
	void updateStyleSheet();
	bool loadJournal(QString const& filePath);
	void loadTwin(std::size_t const eventIndex);
	bool loadTwinEntity(la::avdecc::UniqueIdentifier const entityID, nlohmann::json const& document);
	void unloadTwin(QString const& reason);
	void updateTwinStatus(qint64 const elapsedMs);
	void loadNetworkState(QString const& filePath);
	void loadVirtualEntity(QString const& filePath);
	void addTopologyFromFile(QString const& filePath, bool const isNetworkState);
	void runAnalysis();
	void exportReport();
	void handleFindingsLink(QUrl const& url);
	void showNetworkGraph();
	void openInspector(la::avdecc::UniqueIdentifier const entityID);
	void spawnNewInstance(QString const& filePath);
	void updatePlaceholder();

	QString _journalFilePath{};
	hive::modelsLibrary::EventJournal::Session _session{}; // The loaded session (the events are implicitly shared with the view and the replay), for the analysis
	EventJournalView* _journalView{ nullptr };
	QLabel* _placeholderLabel{ nullptr };
	QLabel* _journalSummaryLabel{ nullptr };
	QDockWidget* _networkGraphDock{ nullptr };
	NetworkGraphView* _networkGraphView{ nullptr };
	QDockWidget* _findingsDock{ nullptr };
	QTextBrowser* _findingsBrowser{ nullptr };
	QAction* _exportReportAction{ nullptr };
	hive::modelsLibrary::JournalAnalysis::Topology _topology{}; // From the journal, completed by the network state and virtual entity files loaded
	std::unique_ptr<hive::modelsLibrary::JournalAnalysis> _analysis{};
	QAction* _schemeSystemAction{ nullptr };
	QAction* _schemeLightAction{ nullptr };
	QAction* _schemeDarkAction{ nullptr };
	std::unordered_map<la::avdecc::UniqueIdentifier, EntityInspector*, la::avdecc::UniqueIdentifier::hash> _inspectors{};
	std::unique_ptr<hive::modelsLibrary::JournalReplay> _replay{};
	QToolBar* _replayToolBar{ nullptr };
	QAction* _followSelectionAction{ nullptr };
	QAction* _latestAction{ nullptr };
	QLabel* _twinStatusLabel{ nullptr };
	std::unordered_set<la::avdecc::UniqueIdentifier, la::avdecc::UniqueIdentifier::hash> _twinEntityIDs{}; // Entities of the twin currently loaded in the virtual controller (unloaded when a network state file replaces the twin)
	bool _twinEnabled{ false }; // The twin follows the journal (false before a journal with snapshots is loaded, and once a network state file replaced it)
	bool _hasLoadedNetworkState{ false }; // Entities are available in the virtual controller (from the twin or from a network state file)
};
