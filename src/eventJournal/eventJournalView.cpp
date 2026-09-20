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

#include "eventJournalView.hpp"
#include "autoScrollBar.hpp"
#include "settingsManager/settings.hpp"

#include <hive/modelsLibrary/helper.hpp>

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>

using EventJournal = hive::modelsLibrary::EventJournal;

namespace
{
/** Handles the non-checkable All/None convenience actions of a filter menu. */
void applyAllNoneAction(qtMate::widgets::TickableMenu& menu, QAction* const action)
{
	if (!action->isCheckable())
	{
		auto const checked = action->text() == "All";
		auto const lock = QSignalBlocker{ &menu };
		for (auto* a : menu.actions())
		{
			if (a->isCheckable())
			{
				a->setChecked(checked);
			}
		}
	}
}

/** Returns the data() values of all unchecked checkable actions of a filter menu. */
QSet<int> uncheckedActionsData(qtMate::widgets::TickableMenu const& menu)
{
	auto hidden = QSet<int>{};
	for (auto const* a : menu.actions())
	{
		if (a->isCheckable() && !a->isChecked())
		{
			hidden.insert(a->data().toInt());
		}
	}
	return hidden;
}

/** Returns the texts of all unchecked checkable actions of a filter menu. */
QSet<QString> uncheckedActionsTexts(qtMate::widgets::TickableMenu const& menu)
{
	auto hidden = QSet<QString>{};
	for (auto const* a : menu.actions())
	{
		if (a->isCheckable() && !a->isChecked())
		{
			hidden.insert(a->text());
		}
	}
	return hidden;
}
} // namespace

EventJournalView::EventJournalView(QWidget* parent)
	: QWidget{ parent }
{
	buildUi(true);

	auto& journal = EventJournal::getInstance();

	// The recording level is restored from the settings before the controller starts (this view is created with the main window), so the first session starts in the mode of the previous one
	auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	journal.setRecordingLevel(settings->getValue(settings::EventJournal_DiagnosticLevel.name).toBool() ? EventJournal::Level::Diagnostic : EventJournal::Level::User);
	_recordDiagnosticButton.setChecked(journal.recordingLevel() == EventJournal::Level::Diagnostic);
	connect(&_recordDiagnosticButton, &QPushButton::toggled, this,
		[this](bool const checked)
		{
			auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
			settings->setValue(settings::EventJournal_DiagnosticLevel.name, checked);
			EventJournal::getInstance().setRecordingLevel(checked ? EventJournal::Level::Diagnostic : EventJournal::Level::User);
		});
	connect(&journal, &EventJournal::recordingLevelChanged, this,
		[this](EventJournal::Level const level)
		{
			auto const lock = QSignalBlocker{ &_recordDiagnosticButton };
			_recordDiagnosticButton.setChecked(level == EventJournal::Level::Diagnostic);
			updateStatusLabel();
			updateSnapshotButton();
		});
	connect(&_snapshotButton, &QPushButton::clicked, this,
		[this]()
		{
			auto const count = EventJournal::getInstance().snapshotAllEntities();
			_statusLabel.setText(QString{ "Snapshot of %1 entities taken" }.arg(count));
		});

	// Markers: what the user heard or did, written in the journal to line up with the events (a custom text, or one of the quick markers)
	connect(_markMenu.addAction("Custom Marker..."), &QAction::triggered, this, &EventJournalView::promptMarker);
	_markMenu.addSeparator();
	for (auto const& text : quickMarkers())
	{
		connect(_markMenu.addAction(text), &QAction::triggered, this,
			[this, text]()
			{
				writeMarker(text);
			});
	}
	_markButton.setMenu(&_markMenu);
	_markButton.setToolTip("Write a marker in the journal: what you heard or did at this moment, so it lines up with the events");

	_exportJournalAction->setEnabled(journal.isRecording());
	_clearButton.setEnabled(journal.isRecording());
	updateSnapshotButton();
	updateMarkButton();
	connect(&journal, &EventJournal::recordingStarted, this,
		[this](QString const&)
		{
			_exportJournalAction->setEnabled(true);
			_clearButton.setEnabled(true);
			updateSnapshotButton();
			updateMarkButton();
		});
	connect(&journal, &EventJournal::recordingStopped, this,
		[this]()
		{
			_exportJournalAction->setEnabled(false);
			_clearButton.setEnabled(false);
			updateSnapshotButton();
			updateMarkButton();
		});
	connect(_exportJournalAction, &QAction::triggered, this,
		[this]()
		{
			auto const filename = QFileDialog::getSaveFileName(this, "Export Event Journal As...", QString{ "%1/EventJournal_%2.%3" }.arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")).arg(EventJournal::JournalFileExtension), QString{ "Event Journal Files (*.%1)" }.arg(EventJournal::JournalFileExtension));
			if (!filename.isEmpty())
			{
				if (!EventJournal::getInstance().exportCurrentSession(filename))
				{
					QMessageBox::critical(this, {}, "Failed to export the event journal.");
				}
			}
		});

	_model.followLiveSession();
}

EventJournalView::EventJournalView(EventJournal::Session&& session, QString const& filePath)
	: QWidget{ nullptr }
{
	buildUi(false);

	setWindowTitle(QString{ "Event Journal - %1" }.arg(QFileInfo{ filePath }.fileName()));
	resize(1024, 600);

	// A file written by a newer schema is opened in degraded mode: say so, without blocking
	if (session.isNewerThanReader())
	{
		_schemaBannerLabel.setText(QString{ "This journal was recorded with a newer version (schema version %1, this viewer understands version %2): the events it does not know are displayed as Session events, and some information may be missing." }.arg(session.schemaVersion()).arg(EventJournal::CurrentSchemaVersion));
		_schemaBannerLabel.show();
	}

	_model.setSession(std::move(session));
}

void EventJournalView::setDiagnosticEventsVisible(bool const visible)
{
	_showDiagnosticButton.setChecked(visible);
}

QStringList EventJournalView::quickMarkers() noexcept
{
	return { "Show start", "Show end", "Glitch heard", "Network work start", "Network work end" };
}

void EventJournalView::promptMarker()
{
	if (!_isLiveMode)
	{
		return;
	}
	auto ok = false;
	auto const text = QInputDialog::getText(this, "Journal Marker", "What happened (written in the journal at this moment):", QLineEdit::Normal, {}, &ok);
	if (ok)
	{
		writeMarker(text);
	}
}

void EventJournalView::writeMarker(QString const& text)
{
	if (EventJournal::getInstance().addMarker(text))
	{
		_statusLabel.setText(QString{ "Marker written: %1" }.arg(text.trimmed()));
	}
	else
	{
		_statusLabel.setText("Marker not written (the journal is not recording, or the text is empty)");
	}
}

void EventJournalView::buildUi(bool const isLiveMode)
{
	_isLiveMode = isLiveMode;
	_filterProxyModel.setSourceModel(&_model);

	// Filters row
	auto* filterLayout = new QHBoxLayout{};
	filterLayout->addWidget(&_severityFilterButton);
	filterLayout->addWidget(&_categoryFilterButton);
	filterLayout->addWidget(&_entityFilterButton);
	filterLayout->addWidget(&_searchLineEdit, 1);
	filterLayout->addWidget(&_showDiagnosticButton);
	filterLayout->addWidget(&_exportButton);

	// Diagnostic level events are hidden by default, so a diagnostic journal stays readable by a sound engineer
	_showDiagnosticButton.setCheckable(true);
	_showDiagnosticButton.setToolTip("Display the diagnostic level events (recorded when diagnostic recording is enabled)");
	connect(&_showDiagnosticButton, &QPushButton::toggled, this,
		[this](bool const checked)
		{
			_filterProxyModel.setDiagnosticEventsVisible(checked);
			updateStatusLabel();
		});

	// Clearing and the recording level only make sense while recording the live session (the standalone viewer displays a read-only loaded file)
	if (isLiveMode)
	{
		filterLayout->addWidget(&_clearButton);
		connect(&_clearButton, &QPushButton::clicked, this,
			[this]()
			{
				if (QMessageBox::question(this, {}, "Clear all events from the current journal? This cannot be undone (export the journal first if you need to keep it).", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
				{
					EventJournal::getInstance().clearCurrentSession();
				}
			});
		filterLayout->addWidget(&_recordDiagnosticButton);
		_recordDiagnosticButton.setCheckable(true);
		_recordDiagnosticButton.setToolTip("Also record the diagnostic level events (everything needed for deep analysis, much larger journal files). The choice is remembered for the next sessions.");
		filterLayout->addWidget(&_snapshotButton);
		_snapshotButton.setToolTip("Store a snapshot of every online entity in the journal now (diagnostic recording only), as a fresh reference point for the analysis");
		filterLayout->addWidget(&_markButton);
	}
	else
	{
		_clearButton.hide();
		_recordDiagnosticButton.hide();
		_snapshotButton.hide();
		_markButton.hide();
	}

	// Degraded mode banner (file mode, journal written by a newer schema)
	_schemaBannerLabel.setWordWrap(true);
	_schemaBannerLabel.setStyleSheet("QLabel { background-color: #f0ad4e; color: #212121; padding: 4px; }");
	_schemaBannerLabel.hide();

	if (isLiveMode)
	{
		_exportJournalAction = _exportMenu.addAction("Journal File...");
	}
	auto* exportCsvAction = _exportMenu.addAction("CSV File...");
	connect(exportCsvAction, &QAction::triggered, this,
		[this]()
		{
			exportAsCsv();
		});
	_exportButton.setMenu(&_exportMenu);

	createSeverityFilterMenu();
	createCategoryFilterMenu();

	_entityFilterButton.setMenu(&_entityFilterMenu);
	connect(&_entityFilterMenu, &QMenu::triggered, this,
		[this](QAction* action)
		{
			applyAllNoneAction(_entityFilterMenu, action);
			_hiddenEntityNames = uncheckedActionsTexts(_entityFilterMenu);
			_filterProxyModel.setHiddenEntities(_hiddenEntityNames);
			updateStatusLabel();
		});
	refreshEntityFilterMenu();

	_searchLineEdit.setPlaceholderText("Search (regular expression)");
	_searchLineEdit.setClearButtonEnabled(true);
	connect(&_searchLineEdit, &QLineEdit::textChanged, this,
		[this](QString const& text)
		{
			_filterProxyModel.setSearchPattern(text);
			updateStatusLabel();
		});

	// Time range row
	auto* timeLayout = new QHBoxLayout{};
	timeLayout->addWidget(&_fromCheckBox);
	timeLayout->addWidget(&_fromDateTimeEdit);
	timeLayout->addSpacing(10);
	timeLayout->addWidget(&_toCheckBox);
	timeLayout->addWidget(&_toDateTimeEdit);
	timeLayout->addStretch();
	timeLayout->addWidget(&_statusLabel);

	for (auto* dateTimeEdit : { &_fromDateTimeEdit, &_toDateTimeEdit })
	{
		dateTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
		dateTimeEdit->setEnabled(false);
		connect(dateTimeEdit, &QDateTimeEdit::dateTimeChanged, this,
			[this](QDateTime const&)
			{
				updateTimeRangeFilter();
			});
	}
	auto const setupTimeCheckBox = [this](QCheckBox& checkBox, QDateTimeEdit& dateTimeEdit, bool const isFrom)
	{
		connect(&checkBox, &QCheckBox::toggled, this,
			[this, &dateTimeEdit, isFrom](bool const checked)
			{
				dateTimeEdit.setEnabled(checked);
				if (checked && _model.rowCount() > 0)
				{
					// Initialize the bound to the session time range
					auto const row = isFrom ? 0 : (_model.rowCount() - 1);
					auto const timestamp = _model.index(row, 0).data(EventJournalModel::TimestampRole).toLongLong();
					auto const lock = QSignalBlocker{ &dateTimeEdit };
					dateTimeEdit.setDateTime(QDateTime::fromMSecsSinceEpoch(timestamp));
				}
				updateTimeRangeFilter();
			});
	};
	setupTimeCheckBox(_fromCheckBox, _fromDateTimeEdit, true);
	setupTimeCheckBox(_toCheckBox, _toDateTimeEdit, false);

	// Events table
	_tableView.setModel(&_filterProxyModel);
	_tableView.setSelectionBehavior(QAbstractItemView::SelectRows);
	_tableView.setSelectionMode(QAbstractItemView::SingleSelection);
	_tableView.setWordWrap(false);
	_tableView.verticalHeader()->hide();
	_tableView.horizontalHeader()->setStretchLastSection(true);
	_tableView.setColumnWidth(static_cast<int>(EventJournalModel::Column::Timestamp), 160);
	_tableView.setColumnWidth(static_cast<int>(EventJournalModel::Column::Severity), 80);
	_tableView.setColumnWidth(static_cast<int>(EventJournalModel::Column::Category), 90);
	_tableView.setColumnWidth(static_cast<int>(EventJournalModel::Column::Entity), 180);
	_tableView.setColumnWidth(static_cast<int>(EventJournalModel::Column::Subject), 180);
	if (isLiveMode)
	{
		_tableView.setVerticalScrollBar(new AutoScrollBar{ Qt::Vertical, &_tableView });
	}
	connect(_tableView.selectionModel(), &QItemSelectionModel::currentRowChanged, this,
		[this](QModelIndex const&, QModelIndex const&)
		{
			handleSelectionChanged();
		});
	connect(&_tableView, &QTableView::doubleClicked, this,
		[this](QModelIndex const& index)
		{
			handleEventDoubleClicked(index);
		});

	// Details pane
	_detailsTextEdit.setReadOnly(true);
	_detailsTextEdit.setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	_detailsTextEdit.setPlaceholderText("Select an event to display its details");

	// Timeline (visualizes the filtered events, synchronized with the table selection)
	_timeline.setModel(&_filterProxyModel);
	connect(&_timeline, &EventJournalTimeline::eventClicked, this,
		[this](int const row)
		{
			auto const index = _filterProxyModel.index(row, 0);
			_tableView.setCurrentIndex(index);
			_tableView.scrollTo(index);
		});

	_splitter.addWidget(&_timeline);
	_splitter.addWidget(&_tableView);
	_splitter.addWidget(&_detailsTextEdit);
	_splitter.setStretchFactor(0, 1);
	_splitter.setStretchFactor(1, 4);
	_splitter.setStretchFactor(2, 1);
	_splitter.setCollapsible(1, false);

	auto* layout = new QVBoxLayout{ this };
	layout->setContentsMargins(2, 2, 2, 2);
	layout->addWidget(&_schemaBannerLabel);
	layout->addLayout(filterLayout);
	layout->addLayout(timeLayout);
	layout->addWidget(&_splitter, 1);

	// Model change tracking (entity filter menu, severity counters, status label)
	connect(&_model, &QAbstractItemModel::rowsInserted, this,
		[this](QModelIndex const&, int const first, int const last)
		{
			handleRowsInserted(first, last);
		});
	connect(&_model, &QAbstractItemModel::modelReset, this,
		[this]()
		{
			handleModelReset();
		});
	handleModelReset();
}

void EventJournalView::createSeverityFilterMenu()
{
	for (auto const severity : { EventJournal::Severity::Info, EventJournal::Severity::Warning, EventJournal::Severity::Error, EventJournal::Severity::Recovered })
	{
		auto* action = _severityFilterMenu.addAction(EventJournal::severityToString(severity));
		action->setCheckable(true);
		action->setChecked(true);
		action->setData(static_cast<int>(severity));
	}
	_severityFilterMenu.addSeparator();
	_severityFilterMenu.addAction("All");
	_severityFilterMenu.addAction("None");
	_severityFilterButton.setMenu(&_severityFilterMenu);

	connect(&_severityFilterMenu, &QMenu::triggered, this,
		[this](QAction* action)
		{
			applyAllNoneAction(_severityFilterMenu, action);
			_filterProxyModel.setHiddenSeverities(uncheckedActionsData(_severityFilterMenu));
			updateStatusLabel();
		});
}

void EventJournalView::createCategoryFilterMenu()
{
	for (auto const category : { EventJournal::Category::Session, EventJournal::Category::Entity, EventJournal::Category::Connection, EventJournal::Category::Counters, EventJournal::Category::MediaClock, EventJournal::Category::Gptp, EventJournal::Category::Link, EventJournal::Category::Latency, EventJournal::Category::Redundancy, EventJournal::Category::Streaming, EventJournal::Category::Configuration, EventJournal::Category::Control, EventJournal::Category::Marker })
	{
		auto* action = _categoryFilterMenu.addAction(EventJournal::categoryToString(category));
		action->setCheckable(true);
		action->setChecked(true);
		action->setData(static_cast<int>(category));
	}
	_categoryFilterMenu.addSeparator();
	_categoryFilterMenu.addAction("All");
	_categoryFilterMenu.addAction("None");
	_categoryFilterButton.setMenu(&_categoryFilterMenu);

	connect(&_categoryFilterMenu, &QMenu::triggered, this,
		[this](QAction* action)
		{
			applyAllNoneAction(_categoryFilterMenu, action);
			_filterProxyModel.setHiddenCategories(uncheckedActionsData(_categoryFilterMenu));
			updateStatusLabel();
		});
}

void EventJournalView::refreshEntityFilterMenu()
{
	_entityFilterMenu.clear();
	_entityFilterMenu.addAction("All");
	_entityFilterMenu.addAction("None");
	_entityFilterMenu.addSeparator();

	auto sortedNames = QStringList{ _knownEntityNames.cbegin(), _knownEntityNames.cend() };
	std::sort(sortedNames.begin(), sortedNames.end(),
		[](QString const& lhs, QString const& rhs)
		{
			return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
		});
	for (auto const& name : sortedNames)
	{
		auto* action = _entityFilterMenu.addAction(name);
		action->setCheckable(true);
		action->setChecked(!_hiddenEntityNames.contains(name));
	}
}

void EventJournalView::exportAsCsv()
{
	// If filters are currently reducing the displayed events, ask whether they should apply to the export
	auto applyFilters = false;
	if (_filterProxyModel.rowCount() != _model.rowCount())
	{
		applyFilters = QMessageBox::question(this, {}, "Apply the current filters to the exported CSV?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes;
	}

	auto const filename = QFileDialog::getSaveFileName(this, "Export CSV As...", QString{ "%1/EventJournal_%2.csv" }.arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")), "CSV Files (*.csv)");
	if (filename.isEmpty())
	{
		return;
	}

	auto file = QFile{ filename };
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QMessageBox::critical(this, {}, "Failed to write the CSV file.");
		return;
	}
	auto stream = QTextStream{ &file };
	stream.setGenerateByteOrderMark(true); // UTF-8 BOM, helps spreadsheet applications detect the encoding

	auto const escape = [](QString field)
	{
		if (field.contains(',') || field.contains('"') || field.contains('\n'))
		{
			field.replace("\"", "\"\"");
			field = '"' + field + '"';
		}
		return field;
	};

	stream << "Time,Severity,Category,Level,Entity ID,Entity Name,Subject,Summary,Details\n";
	auto const rowCount = applyFilters ? _filterProxyModel.rowCount() : _model.rowCount();
	for (auto row = 0; row < rowCount; ++row)
	{
		auto const sourceRow = applyFilters ? _filterProxyModel.mapToSource(_filterProxyModel.index(row, 0)).row() : row;
		auto const& event = _model.eventAtRow(sourceRow);
		auto const entityID = event.entityID ? hive::modelsLibrary::helper::uniqueIdentifierToString(event.entityID) : QString{};
		stream << QDateTime::fromMSecsSinceEpoch(event.timestamp).toString("yyyy-MM-dd HH:mm:ss.zzz") << ',' << EventJournal::severityToString(event.severity) << ',' << EventJournal::categoryToString(event.category) << ',' << EventJournal::levelToString(event.level) << ',' << entityID << ',' << escape(event.entityName) << ',' << escape(event.subject) << ',' << escape(event.summary) << ',' << escape(event.details) << '\n';
	}
}

void EventJournalView::updateTimeRangeFilter()
{
	auto from = std::optional<qint64>{};
	auto to = std::optional<qint64>{};
	if (_fromCheckBox.isChecked())
	{
		from = _fromDateTimeEdit.dateTime().toMSecsSinceEpoch();
	}
	if (_toCheckBox.isChecked())
	{
		// The edit has a one second resolution, make the bound inclusive of that whole second
		to = _toDateTimeEdit.dateTime().toMSecsSinceEpoch() + 999;
	}
	_filterProxyModel.setTimeRange(from, to);
	updateStatusLabel();
}

void EventJournalView::updateStatusLabel()
{
	auto text = QString{ "%1 / %2 events - %3 errors, %4 warnings" }.arg(_filterProxyModel.rowCount()).arg(_model.rowCount()).arg(_errorCount).arg(_warningCount);
	if (_isLiveMode && EventJournal::getInstance().recordingLevel() == EventJournal::Level::Diagnostic)
	{
		text += " - Diagnostic recording";
	}
	_statusLabel.setText(text);
}

void EventJournalView::updateSnapshotButton()
{
	auto const& journal = EventJournal::getInstance();
	_snapshotButton.setEnabled(journal.isRecording() && journal.recordingLevel() == EventJournal::Level::Diagnostic);
}

void EventJournalView::updateMarkButton()
{
	_markButton.setEnabled(EventJournal::getInstance().isRecording());
}

void EventJournalView::handleRowsInserted(int const firstRow, int const lastRow)
{
	auto entityListChanged = false;
	for (auto row = firstRow; row <= lastRow; ++row)
	{
		auto const& event = _model.eventAtRow(row);
		if (event.severity == EventJournal::Severity::Error)
		{
			++_errorCount;
		}
		else if (event.severity == EventJournal::Severity::Warning)
		{
			++_warningCount;
		}
		auto const entityName = EventJournalModel::entityDisplayName(event);
		if (!entityName.isEmpty() && !_knownEntityNames.contains(entityName))
		{
			_knownEntityNames.insert(entityName);
			entityListChanged = true;
		}
	}
	if (entityListChanged)
	{
		refreshEntityFilterMenu();
	}
	updateStatusLabel();
}

void EventJournalView::handleModelReset()
{
	_errorCount = 0u;
	_warningCount = 0u;
	_knownEntityNames.clear();
	_hiddenEntityNames.clear();
	_filterProxyModel.setHiddenEntities({});
	_detailsTextEdit.clear();
	if (_model.rowCount() > 0)
	{
		handleRowsInserted(0, _model.rowCount() - 1);
	}
	else
	{
		refreshEntityFilterMenu();
		updateStatusLabel();
	}
}

void EventJournalView::setCursorTimestamp(std::optional<qint64> const& timestamp)
{
	_timeline.setCursorTimestamp(timestamp);
}

std::optional<int> EventJournalView::currentEventIndex() const
{
	auto const currentIndex = _tableView.selectionModel()->currentIndex();
	if (!currentIndex.isValid())
	{
		return std::nullopt;
	}
	return _filterProxyModel.mapToSource(currentIndex).row();
}

bool EventJournalView::selectEvent(int const eventIndex)
{
	if (eventIndex < 0 || eventIndex >= _model.rowCount())
	{
		return false;
	}
	auto const proxyIndex = _filterProxyModel.mapFromSource(_model.index(eventIndex, 0));
	if (!proxyIndex.isValid())
	{
		return false;
	}
	_tableView.selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	_tableView.scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
	return true;
}

void EventJournalView::handleSelectionChanged()
{
	auto const currentIndex = _tableView.selectionModel()->currentIndex();
	if (!currentIndex.isValid())
	{
		_detailsTextEdit.clear();
		_timeline.setSelectedRow(std::nullopt);
		emit currentEventChanged(-1);
		return;
	}
	_timeline.setSelectedRow(currentIndex.row());
	// Pan the timeline (if needed) so the selected event's marker is visible
	_timeline.ensureRowVisible(currentIndex.row());
	auto const sourceIndex = _filterProxyModel.mapToSource(currentIndex);
	auto const& event = _model.eventAtRow(sourceIndex.row());
	emit currentEventChanged(sourceIndex.row());

	auto text = QString{};
	text += QString{ "Time:      %1\n" }.arg(QDateTime::fromMSecsSinceEpoch(event.timestamp).toString("yyyy-MM-dd HH:mm:ss.zzz"));
	text += QString{ "Severity:  %1\n" }.arg(EventJournal::severityToString(event.severity));
	text += QString{ "Category:  %1\n" }.arg(EventJournal::categoryToString(event.category));
	if (event.level != EventJournal::Level::User)
	{
		text += QString{ "Level:     %1\n" }.arg(EventJournal::levelToString(event.level));
	}
	if (event.entityID)
	{
		text += QString{ "Entity:    %1 (%2)\n" }.arg(EventJournalModel::entityDisplayName(event), hive::modelsLibrary::helper::uniqueIdentifierToString(event.entityID));
	}
	if (!event.subject.isEmpty())
	{
		text += QString{ "Subject:   %1\n" }.arg(event.subject);
	}
	text += QString{ "Summary:   %1\n" }.arg(event.summary);
	if (!event.details.isEmpty())
	{
		auto const document = QJsonDocument::fromJson(event.details.toUtf8());
		text += QString{ "Details:   %1" }.arg(QString::fromUtf8(document.toJson(QJsonDocument::Indented)));
	}
	_detailsTextEdit.setPlainText(text);
}

void EventJournalView::handleEventDoubleClicked(QModelIndex const& index)
{
	if (!index.isValid())
	{
		return;
	}
	auto const sourceIndex = _filterProxyModel.mapToSource(index);
	auto const& event = _model.eventAtRow(sourceIndex.row());
	emit eventActivated(sourceIndex.row());
	// Requesting the selection of the entity activates it in the entities list, which in turn activates it in the inspector
	if (event.entityID)
	{
		emit selectEntityRequested(event.entityID);
	}
}
