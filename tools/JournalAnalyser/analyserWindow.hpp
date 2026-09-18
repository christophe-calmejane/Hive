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

#include <la/avdecc/internals/uniqueIdentifier.hpp>

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMainWindow>
#include <QString>
#include <QStringList>

#include <unordered_map>
#include <vector>

class EntityInspector;
class EventJournalView;
class NetworkGraphView;

/**
* @brief Main window of the Journal Analyser: one Event Journal file, standalone Entity Inspectors and the Network Graph.
* @details The window analyzes a single journal file (the application runs one instance per journal).
*          The entities of the journal are inspected through a virtual controller populated from the entity snapshots stored in the journal (or from a network state file, File > Load Network State, which replaces them), which also feeds the Network Graph.
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
	void createNetworkGraphDock();
	void createVirtualController();
	void applyAppearance(Qt::ColorScheme const appearance);
	void updateStyleSheet();
	bool loadJournal(QString const& filePath);
	void loadSnapshotEntities();
	void unloadSnapshotEntities();
	void loadNetworkState(QString const& filePath);
	void loadVirtualEntity(QString const& filePath);
	void showNetworkGraph();
	void openInspector(la::avdecc::UniqueIdentifier const entityID);
	void spawnNewInstance(QString const& filePath);
	void updatePlaceholder();

	QString _journalFilePath{};
	EventJournalView* _journalView{ nullptr };
	QLabel* _placeholderLabel{ nullptr };
	QLabel* _journalSummaryLabel{ nullptr };
	QDockWidget* _networkGraphDock{ nullptr };
	NetworkGraphView* _networkGraphView{ nullptr };
	QAction* _schemeSystemAction{ nullptr };
	QAction* _schemeLightAction{ nullptr };
	QAction* _schemeDarkAction{ nullptr };
	std::unordered_map<la::avdecc::UniqueIdentifier, EntityInspector*, la::avdecc::UniqueIdentifier::hash> _inspectors{};
	std::vector<la::avdecc::UniqueIdentifier> _snapshotEntityIDs{}; // Entities loaded from the journal snapshots (unloaded when a network state file replaces them)
	bool _hasLoadedNetworkState{ false };
};
