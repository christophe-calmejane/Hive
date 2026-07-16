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

#include "eventJournalTimeline.hpp"
#include "eventJournalModel.hpp"

#include <QtMate/material/color.hpp>

#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>

using EventJournal = hive::modelsLibrary::EventJournal;

namespace
{
constexpr auto LabelsWidth = 92;
constexpr auto AxisHeight = 18;
constexpr auto TopMargin = 4;
constexpr auto MarkerRadius = 3.5;
constexpr auto MarkerHitDistance = 6;
constexpr auto MinimumViewSpan = qint64{ 1000 }; // 1 second

/** All categories, in display order (defines the lanes order) */
constexpr std::array<EventJournal::Category, 9> AllCategories{ EventJournal::Category::Session, EventJournal::Category::Entity, EventJournal::Category::Connection, EventJournal::Category::Counters, EventJournal::Category::MediaClock, EventJournal::Category::Gptp, EventJournal::Category::Link, EventJournal::Category::Latency, EventJournal::Category::Redundancy };

/** Formats a duration in a human readable way (eg. "2 min 13 s") */
QString formatDuration(qint64 const milliseconds)
{
	if (milliseconds < 1000)
	{
		return QString{ "%1 ms" }.arg(milliseconds);
	}
	auto const totalSeconds = milliseconds / 1000;
	auto const hours = totalSeconds / 3600;
	auto const minutes = (totalSeconds % 3600) / 60;
	auto const seconds = totalSeconds % 60;
	auto text = QString{};
	if (hours > 0)
	{
		text += QString{ "%1 h " }.arg(hours);
	}
	if (minutes > 0)
	{
		text += QString{ "%1 min " }.arg(minutes);
	}
	if (seconds > 0 || text.isEmpty())
	{
		text += QString{ "%1 s" }.arg(seconds);
	}
	return text.trimmed();
}
} // namespace

EventJournalTimeline::EventJournalTimeline(QWidget* parent)
	: QWidget{ parent }
{
	setMouseTracking(true);
}

void EventJournalTimeline::setModel(QAbstractItemModel* model)
{
	if (_model)
	{
		disconnect(_model, nullptr, this, nullptr);
	}
	_model = model;
	if (_model)
	{
		auto const triggerRebuild = [this]()
		{
			rebuild();
		};
		connect(_model, &QAbstractItemModel::rowsInserted, this, triggerRebuild);
		connect(_model, &QAbstractItemModel::rowsRemoved, this, triggerRebuild);
		connect(_model, &QAbstractItemModel::modelReset, this, triggerRebuild);
		connect(_model, &QAbstractItemModel::layoutChanged, this, triggerRebuild);
	}
	rebuild();
}

void EventJournalTimeline::setSelectedRow(std::optional<int> const& row)
{
	_selectedRow = row;
	update();
}

void EventJournalTimeline::ensureRowVisible(int const row)
{
	// Find the marker of that model row
	auto const it = std::find_if(_markers.begin(), _markers.end(),
		[row](Marker const& marker)
		{
			return marker.row == row;
		});
	if (it == _markers.end())
	{
		return;
	}

	auto const timestamp = it->timestamp;
	auto const span = _viewEnd - _viewStart;
	auto const margin = span / 10; // Keep the marker slightly away from the edges
	auto shift = qint64{ 0 };
	if (timestamp < _viewStart + margin)
	{
		shift = timestamp - (_viewStart + margin);
	}
	else if (timestamp > _viewEnd - margin)
	{
		shift = timestamp - (_viewEnd - margin);
	}
	if (shift != 0)
	{
		_viewStart += shift;
		_viewEnd += shift;
		_autoFit = false;
		clampView();
		update();
	}
}

QSize EventJournalTimeline::sizeHint() const
{
	return QSize{ 400, 140 };
}

QSize EventJournalTimeline::minimumSizeHint() const
{
	// Enough height for one readable label per lane
	auto const lanesHeight = static_cast<int>(std::max(_lanes.size(), std::size_t{ 3 })) * 13;
	return QSize{ 200, TopMargin + AxisHeight + lanesHeight };
}

void EventJournalTimeline::rebuild()
{
	_markers.clear();
	_lanes.clear();

	auto const rowCount = _model ? _model->rowCount() : 0;
	if (rowCount > 0)
	{
		// Determine which categories are present, keeping the display order
		auto presentCategories = std::array<bool, AllCategories.size()>{};
		for (auto row = 0; row < rowCount; ++row)
		{
			auto const category = _model->index(row, 0).data(EventJournalModel::CategoryRole).toInt();
			if (category >= 0 && category < static_cast<int>(AllCategories.size()))
			{
				presentCategories[static_cast<std::size_t>(category)] = true;
			}
		}
		auto laneForCategory = std::array<int, AllCategories.size()>{};
		laneForCategory.fill(-1);
		for (auto const category : AllCategories)
		{
			auto const categoryIndex = static_cast<std::size_t>(category);
			if (presentCategories[categoryIndex])
			{
				laneForCategory[categoryIndex] = static_cast<int>(_lanes.size());
				_lanes.push_back(category);
			}
		}

		// Build the markers, tracking the previous marker of each lane for the "time since previous occurrence" display
		auto previousInLane = std::vector<qint64>(_lanes.size(), qint64{ -1 });
		_markers.reserve(static_cast<std::size_t>(rowCount));
		for (auto row = 0; row < rowCount; ++row)
		{
			auto const index = _model->index(row, 0);
			auto const category = index.data(EventJournalModel::CategoryRole).toInt();
			if (category < 0 || category >= static_cast<int>(AllCategories.size()))
			{
				continue;
			}
			auto marker = Marker{};
			marker.timestamp = index.data(EventJournalModel::TimestampRole).toLongLong();
			marker.severity = static_cast<EventJournal::Severity>(index.data(EventJournalModel::SeverityRole).toInt());
			marker.laneIndex = laneForCategory[static_cast<std::size_t>(category)];
			marker.row = row;
			marker.previousInLane = previousInLane[static_cast<std::size_t>(marker.laneIndex)];
			previousInLane[static_cast<std::size_t>(marker.laneIndex)] = marker.timestamp;
			_markers.push_back(marker);
		}
	}

	if (!_markers.empty())
	{
		auto const [minIt, maxIt] = std::minmax_element(_markers.begin(), _markers.end(),
			[](Marker const& lhs, Marker const& rhs)
			{
				return lhs.timestamp < rhs.timestamp;
			});
		_dataStart = minIt->timestamp;
		_dataEnd = maxIt->timestamp;
	}
	else
	{
		_dataStart = 0;
		_dataEnd = 0;
	}

	if (_autoFit)
	{
		fitToData();
	}
	else
	{
		clampView();
	}
	updateGeometry(); // The minimum height depends on the number of lanes
	update();
}

void EventJournalTimeline::fitToData()
{
	// Pad the data range by 2% on each side so the first and last markers are not glued to the edges
	auto const span = std::max(_dataEnd - _dataStart, MinimumViewSpan);
	auto const padding = span / 50;
	_viewStart = _dataStart - padding;
	_viewEnd = _dataEnd + padding;
}

void EventJournalTimeline::zoom(double const factor, qint64 const anchorTime)
{
	auto const span = static_cast<double>(_viewEnd - _viewStart);
	auto newSpan = span / factor;
	auto const maxSpan = static_cast<double>(std::max(_dataEnd - _dataStart, MinimumViewSpan)) * 1.1;
	newSpan = std::clamp(newSpan, static_cast<double>(MinimumViewSpan), maxSpan);
	auto const anchorRatio = span > 0 ? static_cast<double>(anchorTime - _viewStart) / span : 0.5;
	_viewStart = anchorTime - static_cast<qint64>(anchorRatio * newSpan);
	_viewEnd = _viewStart + static_cast<qint64>(newSpan);
	_autoFit = false;
	clampView();
	update();
}

void EventJournalTimeline::clampView()
{
	if (_markers.empty())
	{
		return;
	}
	auto const span = _viewEnd - _viewStart;
	auto const padding = std::max(_dataEnd - _dataStart, MinimumViewSpan) / 50;
	if (_viewStart < _dataStart - padding - span / 2)
	{
		_viewStart = _dataStart - padding - span / 2;
		_viewEnd = _viewStart + span;
	}
	if (_viewEnd > _dataEnd + padding + span / 2)
	{
		_viewEnd = _dataEnd + padding + span / 2;
		_viewStart = _viewEnd - span;
	}
}

QRect EventJournalTimeline::plotRect() const
{
	return QRect{ LabelsWidth, TopMargin, std::max(width() - LabelsWidth - 4, 10), std::max(height() - TopMargin - AxisHeight, 10) };
}

double EventJournalTimeline::timeToX(qint64 const timestamp) const
{
	auto const plot = plotRect();
	auto const span = std::max(_viewEnd - _viewStart, qint64{ 1 });
	return plot.left() + static_cast<double>(timestamp - _viewStart) / static_cast<double>(span) * plot.width();
}

qint64 EventJournalTimeline::xToTime(double const x) const
{
	auto const plot = plotRect();
	auto const span = std::max(_viewEnd - _viewStart, qint64{ 1 });
	return _viewStart + static_cast<qint64>((x - plot.left()) / plot.width() * static_cast<double>(span));
}

std::optional<std::size_t> EventJournalTimeline::markerAt(QPoint const& position) const
{
	if (_lanes.empty())
	{
		return std::nullopt;
	}
	auto const plot = plotRect();
	auto const laneHeight = static_cast<double>(plot.height()) / static_cast<double>(_lanes.size());
	auto best = std::optional<std::size_t>{};
	auto bestDistance = static_cast<double>(MarkerHitDistance);
	for (auto markerIndex = std::size_t{ 0 }; markerIndex < _markers.size(); ++markerIndex)
	{
		auto const& marker = _markers[markerIndex];
		auto const markerX = timeToX(marker.timestamp);
		auto const markerY = plot.top() + (marker.laneIndex + 0.5) * laneHeight;
		auto const distance = std::hypot(markerX - position.x(), markerY - position.y());
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = markerIndex;
		}
	}
	return best;
}

QColor EventJournalTimeline::severityColor(EventJournal::Severity const severity) const
{
	auto const colorName = qtMate::material::color::backgroundColorName();
	auto const shade = qtMate::material::color::colorSchemeShade();
	switch (severity)
	{
		case EventJournal::Severity::Error:
			return qtMate::material::color::foregroundErrorColorValue(colorName, shade);
		case EventJournal::Severity::Warning:
			return qtMate::material::color::foregroundWarningColorValue(colorName, shade);
		case EventJournal::Severity::Recovered:
			return qtMate::material::color::value(qtMate::material::color::Name::Green, shade);
		default:
		{
			auto color = palette().color(QPalette::Text);
			color.setAlpha(140);
			return color;
		}
	}
}

void EventJournalTimeline::paintEvent(QPaintEvent*)
{
	auto painter = QPainter{ this };
	painter.setRenderHint(QPainter::Antialiasing);

	auto const plot = plotRect();

	if (_markers.empty())
	{
		painter.setPen(palette().color(QPalette::Text));
		painter.drawText(rect(), Qt::AlignCenter, "No events to display on the timeline");
		return;
	}

	auto const laneHeight = static_cast<double>(plot.height()) / static_cast<double>(_lanes.size());
	auto gridColor = palette().color(QPalette::Text);
	gridColor.setAlpha(30);

	// Lanes background and labels
	auto laneFont = font();
	laneFont.setPointSizeF(std::max(laneFont.pointSizeF() - 1.0, 7.0));
	painter.setFont(laneFont);
	for (auto laneIndex = std::size_t{ 0 }; laneIndex < _lanes.size(); ++laneIndex)
	{
		auto const laneTop = plot.top() + static_cast<double>(laneIndex) * laneHeight;
		if (laneIndex % 2 == 1)
		{
			auto laneColor = palette().color(QPalette::Text);
			laneColor.setAlpha(10);
			painter.fillRect(QRectF(plot.left(), laneTop, plot.width(), laneHeight), laneColor);
		}
		painter.setPen(palette().color(QPalette::Text));
		painter.drawText(QRectF(2, laneTop, LabelsWidth - 6, laneHeight), Qt::AlignVCenter | Qt::AlignRight, EventJournal::categoryToString(_lanes[laneIndex]));
		painter.setPen(gridColor);
		painter.drawLine(QPointF(plot.left(), laneTop), QPointF(plot.right(), laneTop));
	}
	painter.drawLine(QPointF(plot.left(), plot.bottom()), QPointF(plot.right(), plot.bottom()));

	// Time axis ticks (pick a step giving at least ~90 pixels between ticks)
	constexpr std::array<qint64, 16> TickSteps{ 100, 250, 500, 1000, 2000, 5000, 10000, 30000, 60000, 120000, 300000, 600000, 1800000, 3600000, 7200000, 21600000 };
	auto const span = std::max(_viewEnd - _viewStart, qint64{ 1 });
	auto const msPerPixel = static_cast<double>(span) / plot.width();
	auto tickStep = TickSteps.back();
	for (auto const step : TickSteps)
	{
		if (static_cast<double>(step) / msPerPixel >= 90.0)
		{
			tickStep = step;
			break;
		}
	}
	auto const timeFormat = (tickStep < 1000) ? QString{ "HH:mm:ss.zzz" } : (span > qint64{ 24 } * 3600 * 1000 ? QString{ "dd/MM HH:mm" } : QString{ "HH:mm:ss" });
	for (auto tick = (_viewStart / tickStep) * tickStep; tick <= _viewEnd; tick += tickStep)
	{
		if (tick < _viewStart)
		{
			continue;
		}
		auto const tickX = timeToX(tick);
		painter.setClipRect(QRectF(plot.left(), 0, plot.width(), height()));
		painter.setPen(gridColor);
		painter.drawLine(QPointF(tickX, plot.top()), QPointF(tickX, plot.bottom() + 3));
		// Labels are drawn unclipped (the leftmost one would otherwise be truncated), the space under the lane labels column is empty anyway
		painter.setClipping(false);
		painter.setPen(palette().color(QPalette::Text));
		painter.drawText(QRectF(tickX - 60, plot.bottom() + 2, 120, AxisHeight - 2), Qt::AlignHCenter | Qt::AlignVCenter, QDateTime::fromMSecsSinceEpoch(tick).toString(timeFormat));
	}
	painter.setClipRect(QRectF(plot.left(), 0, plot.width(), height()));

	// Markers
	for (auto const& marker : _markers)
	{
		auto const markerX = timeToX(marker.timestamp);
		if (markerX < plot.left() - MarkerRadius || markerX > plot.right() + MarkerRadius)
		{
			continue;
		}
		auto const markerY = plot.top() + (marker.laneIndex + 0.5) * laneHeight;
		auto const isSelected = _selectedRow && *_selectedRow == marker.row;
		if (isSelected)
		{
			painter.setPen(QPen{ palette().color(QPalette::Highlight), 2.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawEllipse(QPointF(markerX, markerY), MarkerRadius + 3.0, MarkerRadius + 3.0);
		}
		painter.setPen(Qt::NoPen);
		painter.setBrush(severityColor(marker.severity));
		painter.drawEllipse(QPointF(markerX, markerY), MarkerRadius, MarkerRadius);
	}
	painter.setClipping(false);
}

void EventJournalTimeline::wheelEvent(QWheelEvent* event)
{
	if (_markers.empty())
	{
		return;
	}
	auto const delta = event->angleDelta().y();
	if (delta != 0)
	{
		auto const factor = std::pow(1.0015, delta);
		zoom(factor, xToTime(event->position().x()));
	}
	event->accept();
}

void EventJournalTimeline::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		_panLastPosition = event->pos();
		_panned = false;
	}
}

void EventJournalTimeline::mouseMoveEvent(QMouseEvent* event)
{
	if (_panLastPosition)
	{
		auto const deltaX = event->pos().x() - _panLastPosition->x();
		if (deltaX != 0)
		{
			auto const plot = plotRect();
			auto const span = _viewEnd - _viewStart;
			auto const deltaTime = static_cast<qint64>(static_cast<double>(deltaX) * static_cast<double>(span) / plot.width());
			_viewStart -= deltaTime;
			_viewEnd -= deltaTime;
			_autoFit = false;
			_panned = true;
			clampView();
			update();
		}
		_panLastPosition = event->pos();
		return;
	}

	// Hovering: show the event details, including the time since the previous event of the same lane
	if (auto const markerIndex = markerAt(event->pos()))
	{
		auto const& marker = _markers[*markerIndex];
		auto text = QString{ "%1 - %2 / %3" }.arg(QDateTime::fromMSecsSinceEpoch(marker.timestamp).toString("yyyy-MM-dd HH:mm:ss.zzz"), EventJournal::severityToString(marker.severity), EventJournal::categoryToString(_lanes[static_cast<std::size_t>(marker.laneIndex)]));
		if (_model)
		{
			auto const entity = _model->index(marker.row, static_cast<int>(EventJournalModel::Column::Entity)).data().toString();
			auto const summary = _model->index(marker.row, static_cast<int>(EventJournalModel::Column::Summary)).data().toString();
			if (!entity.isEmpty())
			{
				text += QString{ "\n%1" }.arg(entity);
			}
			text += QString{ "\n%1" }.arg(summary);
		}
		if (marker.previousInLane >= 0)
		{
			text += QString{ "\n+%1 since previous %2 event" }.arg(formatDuration(marker.timestamp - marker.previousInLane), EventJournal::categoryToString(_lanes[static_cast<std::size_t>(marker.laneIndex)]));
		}
		QToolTip::showText(event->globalPosition().toPoint(), text, this);
		setCursor(Qt::PointingHandCursor);
	}
	else
	{
		QToolTip::hideText();
		unsetCursor();
	}
}

void EventJournalTimeline::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		_panLastPosition = std::nullopt;
		if (!_panned)
		{
			if (auto const markerIndex = markerAt(event->pos()))
			{
				emit eventClicked(_markers[*markerIndex].row);
			}
		}
	}
}

void EventJournalTimeline::mouseDoubleClickEvent(QMouseEvent*)
{
	_autoFit = true;
	fitToData();
	update();
}

void EventJournalTimeline::leaveEvent(QEvent*)
{
	unsetCursor();
}
