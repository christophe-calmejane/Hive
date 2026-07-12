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

#include "eventJournalModel.hpp"

#include <hive/modelsLibrary/helper.hpp>

#include <QtMate/material/color.hpp>

#include <QDateTime>

using EventJournal = hive::modelsLibrary::EventJournal;

/* ************************************************************ */
/* EventJournalModel                                            */
/* ************************************************************ */
EventJournalModel::EventJournalModel(QObject* parent)
	: QAbstractTableModel{ parent }
{
}

void EventJournalModel::followLiveSession()
{
	auto& journal = EventJournal::getInstance();

	connect(&journal, &EventJournal::eventAdded, this,
		[this](EventJournal::Event const& event)
		{
			auto const row = static_cast<int>(_events.size());
			beginInsertRows({}, row, row);
			_events.push_back(event);
			endInsertRows();
		});
	connect(&journal, &EventJournal::recordingStarted, this,
		[this](QString const&)
		{
			auto& journal = EventJournal::getInstance();
			beginResetModel();
			_events = journal.currentSessionEvents();
			_metadata = journal.currentSessionMetadata();
			endResetModel();
		});

	beginResetModel();
	_events = journal.currentSessionEvents();
	_metadata = journal.currentSessionMetadata();
	endResetModel();
}

void EventJournalModel::setSession(EventJournal::Session&& session)
{
	beginResetModel();
	_events = std::move(session.events);
	_metadata = std::move(session.metadata);
	endResetModel();
}

EventJournal::Event const& EventJournalModel::eventAtRow(int const row) const
{
	return _events.at(static_cast<std::size_t>(row));
}

QMap<QString, QString> const& EventJournalModel::metadata() const noexcept
{
	return _metadata;
}

QString EventJournalModel::entityDisplayName(EventJournal::Event const& event) noexcept
{
	if (!event.entityName.isEmpty())
	{
		return event.entityName;
	}
	if (event.entityID)
	{
		return hive::modelsLibrary::helper::uniqueIdentifierToString(event.entityID);
	}
	return {};
}

int EventJournalModel::rowCount(QModelIndex const& parent) const
{
	if (parent.isValid())
	{
		return 0;
	}
	return static_cast<int>(_events.size());
}

int EventJournalModel::columnCount(QModelIndex const& parent) const
{
	if (parent.isValid())
	{
		return 0;
	}
	return static_cast<int>(Column::Count);
}

QVariant EventJournalModel::data(QModelIndex const& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
	{
		return {};
	}

	auto const& event = _events[static_cast<std::size_t>(index.row())];

	switch (role)
	{
		case Qt::DisplayRole:
		{
			switch (static_cast<Column>(index.column()))
			{
				case Column::Timestamp:
					return QDateTime::fromMSecsSinceEpoch(event.timestamp).toString("yyyy-MM-dd HH:mm:ss.zzz");
				case Column::Severity:
					return EventJournal::severityToString(event.severity);
				case Column::Category:
					return EventJournal::categoryToString(event.category);
				case Column::Entity:
					return entityDisplayName(event);
				case Column::Subject:
					return event.subject;
				case Column::Summary:
					return event.summary;
				default:
					return {};
			}
		}
		case Qt::ForegroundRole:
		{
			auto const colorName = qtMate::material::color::backgroundColorName();
			auto const shade = qtMate::material::color::colorSchemeShade();
			switch (event.severity)
			{
				case EventJournal::Severity::Error:
					return qtMate::material::color::foregroundErrorColorValue(colorName, shade);
				case EventJournal::Severity::Warning:
					return qtMate::material::color::foregroundWarningColorValue(colorName, shade);
				case EventJournal::Severity::Recovered:
					return qtMate::material::color::value(qtMate::material::color::Name::Green, shade);
				default:
					return {};
			}
		}
		case TimestampRole:
			return event.timestamp;
		case SeverityRole:
			return static_cast<int>(event.severity);
		case CategoryRole:
			return static_cast<int>(event.category);
		default:
			return {};
	}
}

QVariant EventJournalModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (static_cast<Column>(section))
		{
			case Column::Timestamp:
				return "Time";
			case Column::Severity:
				return "Severity";
			case Column::Category:
				return "Category";
			case Column::Entity:
				return "Entity";
			case Column::Subject:
				return "Subject";
			case Column::Summary:
				return "Summary";
			default:
				return {};
		}
	}
	return QAbstractTableModel::headerData(section, orientation, role);
}

/* ************************************************************ */
/* EventJournalFilterProxyModel                                 */
/* ************************************************************ */
EventJournalFilterProxyModel::EventJournalFilterProxyModel(QObject* parent)
	: QSortFilterProxyModel{ parent }
{
}

void EventJournalFilterProxyModel::setHiddenSeverities(QSet<int> const& severities)
{
	_hiddenSeverities = severities;
	invalidateFilter();
}

void EventJournalFilterProxyModel::setHiddenCategories(QSet<int> const& categories)
{
	_hiddenCategories = categories;
	invalidateFilter();
}

void EventJournalFilterProxyModel::setHiddenEntities(QSet<QString> const& entityNames)
{
	_hiddenEntities = entityNames;
	invalidateFilter();
}

void EventJournalFilterProxyModel::setSearchPattern(QString const& pattern)
{
	_searchPattern = QRegularExpression{ pattern, QRegularExpression::CaseInsensitiveOption };
	invalidateFilter();
}

void EventJournalFilterProxyModel::setTimeRange(std::optional<qint64> const& from, std::optional<qint64> const& to)
{
	_timeFrom = from;
	_timeTo = to;
	invalidateFilter();
}

bool EventJournalFilterProxyModel::filterAcceptsRow(int sourceRow, QModelIndex const& sourceParent) const
{
	auto const* const model = sourceModel();
	auto const index = model->index(sourceRow, 0, sourceParent);

	if (_hiddenSeverities.contains(model->data(index, EventJournalModel::SeverityRole).toInt()))
	{
		return false;
	}
	if (_hiddenCategories.contains(model->data(index, EventJournalModel::CategoryRole).toInt()))
	{
		return false;
	}

	auto const timestamp = model->data(index, EventJournalModel::TimestampRole).toLongLong();
	if (_timeFrom && timestamp < *_timeFrom)
	{
		return false;
	}
	if (_timeTo && timestamp > *_timeTo)
	{
		return false;
	}

	auto const entityName = model->data(model->index(sourceRow, static_cast<int>(EventJournalModel::Column::Entity), sourceParent)).toString();
	if (!_hiddenEntities.isEmpty() && _hiddenEntities.contains(entityName))
	{
		return false;
	}

	if (!_searchPattern.pattern().isEmpty() && _searchPattern.isValid())
	{
		auto const subject = model->data(model->index(sourceRow, static_cast<int>(EventJournalModel::Column::Subject), sourceParent)).toString();
		auto const summary = model->data(model->index(sourceRow, static_cast<int>(EventJournalModel::Column::Summary), sourceParent)).toString();
		if (!_searchPattern.match(entityName).hasMatch() && !_searchPattern.match(subject).hasMatch() && !_searchPattern.match(summary).hasMatch())
		{
			return false;
		}
	}

	return true;
}
