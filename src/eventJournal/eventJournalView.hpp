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

#include "eventJournalModel.hpp"
#include "eventJournalTimeline.hpp"

#include <QtMate/widgets/tickableMenu.hpp>

#include <QCheckBox>
#include <QDateTimeEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QWidget>

#include <optional>

/**
* @brief Widget displaying the events of a journal session, with filtering.
* @details Default constructed (eg. when embedded in the MainWindow dock), the view follows the live recording
*          session and offers an export button. Constructed from a loaded session, it displays that session as a
*          standalone viewer window (used to analyze a journal file after a show).
*/
class EventJournalView : public QWidget
{
	Q_OBJECT
public:
	/** Creates a view following the live recording session. */
	EventJournalView(QWidget* parent = nullptr);

	/** Creates a standalone viewer window for a session loaded from the given file. */
	EventJournalView(hive::modelsLibrary::EventJournal::Session&& session, QString const& filePath);

	/** Shows or hides the diagnostic level events (hidden by default, the analyser shows them by default). */
	void setDiagnosticEventsVisible(bool const visible);

	/** Asks the user for the text of a marker and writes it in the live journal (live mode only, the application binds a menu action with a shortcut to it so it works whatever the visibility of the journal dock). */
	void promptMarker();

	/** Texts of the quick markers offered by the Mark button (and usable by any caller of EventJournal::addMarker). */
	static QStringList quickMarkers() noexcept;

	/** Sets the time cursor drawn on the timeline (the instant the analyser's twin of the network shows), pass std::nullopt to hide it. */
	void setCursorTimestamp(std::optional<qint64> const& timestamp);

	/** Returns the index in the session of the current event (the selected row), or std::nullopt if none. */
	std::optional<int> currentEventIndex() const;

	/* Signals */
	/** Emitted when the user requests to select an entity (eg. by double-clicking one of its events). */
	Q_SIGNAL void selectEntityRequested(la::avdecc::UniqueIdentifier const entityID);
	/** Emitted when the current event changes (eventIndex is the index of the event in the session, -1 when no event is current). */
	Q_SIGNAL void currentEventChanged(int const eventIndex);
	/** Emitted when the user activates an event (double-click), before selectEntityRequested (eventIndex is the index of the event in the session). */
	Q_SIGNAL void eventActivated(int const eventIndex);

private:
	void buildUi(bool const isLiveMode);
	void createSeverityFilterMenu();
	void createCategoryFilterMenu();
	void refreshEntityFilterMenu();
	void exportAsCsv();
	void updateTimeRangeFilter();
	void updateStatusLabel();
	void updateSnapshotButton();
	void updateMarkButton();
	void writeMarker(QString const& text);
	void handleRowsInserted(int const firstRow, int const lastRow);
	void handleModelReset();
	void handleSelectionChanged();
	void handleEventDoubleClicked(QModelIndex const& index);

	EventJournalModel _model{ this };
	EventJournalFilterProxyModel _filterProxyModel{ this };

	QPushButton _severityFilterButton{ "Severity", this };
	QPushButton _categoryFilterButton{ "Category", this };
	QPushButton _entityFilterButton{ "Entity", this };
	QLineEdit _searchLineEdit{ this };
	QPushButton _showDiagnosticButton{ "Show Diagnostic", this };
	QPushButton _exportButton{ "Export...", this };
	QPushButton _clearButton{ "Clear", this };
	QPushButton _recordDiagnosticButton{ "Record Diagnostic", this };
	QPushButton _snapshotButton{ "Snapshot Now", this };
	QPushButton _markButton{ "Mark", this };
	QMenu _markMenu{ this };
	QLabel _schemaBannerLabel{ this };
	QMenu _exportMenu{ this };
	QAction* _exportJournalAction{ nullptr };
	QCheckBox _fromCheckBox{ "From:", this };
	QDateTimeEdit _fromDateTimeEdit{ this };
	QCheckBox _toCheckBox{ "To:", this };
	QDateTimeEdit _toDateTimeEdit{ this };
	QLabel _statusLabel{ this };
	QSplitter _splitter{ Qt::Vertical, this };
	EventJournalTimeline _timeline{ this };
	QTableView _tableView{ this };
	QPlainTextEdit _detailsTextEdit{ this };

	qtMate::widgets::TickableMenu _severityFilterMenu{ this };
	qtMate::widgets::TickableMenu _categoryFilterMenu{ this };
	qtMate::widgets::TickableMenu _entityFilterMenu{ this };

	QSet<QString> _knownEntityNames{};
	QSet<QString> _hiddenEntityNames{};
	quint64 _errorCount{ 0u };
	quint64 _warningCount{ 0u };
	bool _isLiveMode{ false };
};
