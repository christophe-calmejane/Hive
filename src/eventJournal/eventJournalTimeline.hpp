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

#include <QAbstractItemModel>
#include <QWidget>

#include <optional>
#include <vector>

/**
* @brief Timeline visualization of journal events.
* @details Displays the events of a journal model (usually the filter proxy, so filters apply) on a horizontal time
*          axis, with one lane per event category. Recurring events of the same category line up horizontally, making
*          the time between occurrences (eg. repeated gPTP grandmaster changes) directly visible.
*          Interactions: mouse wheel zooms around the cursor, left drag pans, double-click fits the whole session,
*          hovering a marker shows the event details (including the time since the previous event of the same lane),
*          clicking a marker selects the event (eventClicked signal, used to synchronize the events table).
*/
class EventJournalTimeline : public QWidget
{
	Q_OBJECT
public:
	EventJournalTimeline(QWidget* parent = nullptr);

	/** Sets the model to visualize. Rows are expected to provide EventJournalModel roles. */
	void setModel(QAbstractItemModel* model);

	/** Highlights the marker of the given model row (pass std::nullopt to clear the highlight). */
	void setSelectedRow(std::optional<int> const& row);

	/** Pans the view (if needed) so the marker of the given model row becomes visible on the timeline. */
	void ensureRowVisible(int const row);

	/* Signals */
	Q_SIGNAL void eventClicked(int row);

	// QWidget overrides
	virtual QSize sizeHint() const override;
	virtual QSize minimumSizeHint() const override;

protected:
	virtual void paintEvent(QPaintEvent* event) override;
	virtual void wheelEvent(QWheelEvent* event) override;
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void mouseReleaseEvent(QMouseEvent* event) override;
	virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
	virtual void leaveEvent(QEvent* event) override;

private:
	struct Marker
	{
		qint64 timestamp{ 0 };
		hive::modelsLibrary::EventJournal::Severity severity{ hive::modelsLibrary::EventJournal::Severity::Info };
		int laneIndex{ 0 };
		int row{ 0 }; /**< Row in the visualized model */
		qint64 previousInLane{ -1 }; /**< Timestamp of the previous marker in the same lane (-1 if none) */
	};

	void rebuild();
	void fitToData();
	void zoom(double const factor, qint64 const anchorTime);
	void clampView();
	QRect plotRect() const;
	double timeToX(qint64 const timestamp) const;
	qint64 xToTime(double const x) const;
	std::optional<std::size_t> markerAt(QPoint const& position) const;
	QColor severityColor(hive::modelsLibrary::EventJournal::Severity const severity) const;

	QAbstractItemModel* _model{ nullptr };
	std::vector<Marker> _markers{};
	std::vector<hive::modelsLibrary::EventJournal::Category> _lanes{};
	qint64 _dataStart{ 0 };
	qint64 _dataEnd{ 0 };
	qint64 _viewStart{ 0 };
	qint64 _viewEnd{ 0 };
	bool _autoFit{ true };
	std::optional<int> _selectedRow{};
	std::optional<QPoint> _panLastPosition{};
	bool _panned{ false };
};
