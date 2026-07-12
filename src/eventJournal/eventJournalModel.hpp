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

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QSet>

#include <optional>
#include <vector>

/**
* @brief Table model over the events of a journal session.
* @details Either follows the live recording session (rows are appended as events are journaled, the model resets
*          when a new recording starts), or displays a session loaded from a journal file.
*/
class EventJournalModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	enum class Column
	{
		Timestamp = 0,
		Severity = 1,
		Category = 2,
		Entity = 3,
		Subject = 4,
		Summary = 5,

		Count = 6,
	};

	enum Role
	{
		TimestampRole = Qt::UserRole + 1, /**< qint64, milliseconds since Unix epoch */
		SeverityRole, /**< int, hive::modelsLibrary::EventJournal::Severity */
		CategoryRole, /**< int, hive::modelsLibrary::EventJournal::Category */
	};

	EventJournalModel(QObject* parent = nullptr);

	/** Makes the model follow the live recording session of the EventJournal singleton. */
	void followLiveSession();

	/** Makes the model display the given loaded session. */
	void setSession(hive::modelsLibrary::EventJournal::Session&& session);

	/** Returns the event displayed at the given row. */
	hive::modelsLibrary::EventJournal::Event const& eventAtRow(int const row) const;

	/** Returns the metadata of the displayed session. */
	QMap<QString, QString> const& metadata() const noexcept;

	/** Returns the display name used in the Entity column for the given event. */
	static QString entityDisplayName(hive::modelsLibrary::EventJournal::Event const& event) noexcept;

	// QAbstractTableModel overrides
	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual int columnCount(QModelIndex const& parent = {}) const override;
	virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
	std::vector<hive::modelsLibrary::EventJournal::Event> _events{};
	QMap<QString, QString> _metadata{};
};

/**
* @brief Filter proxy for EventJournalModel.
* @details Combines severity, category, entity, time range and free text (regex) filters.
*/
class EventJournalFilterProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT
public:
	EventJournalFilterProxyModel(QObject* parent = nullptr);

	void setHiddenSeverities(QSet<int> const& severities);
	void setHiddenCategories(QSet<int> const& categories);
	void setHiddenEntities(QSet<QString> const& entityNames);
	void setSearchPattern(QString const& pattern);
	void setTimeRange(std::optional<qint64> const& from, std::optional<qint64> const& to);

	// QSortFilterProxyModel overrides
	virtual bool filterAcceptsRow(int sourceRow, QModelIndex const& sourceParent) const override;

private:
	QSet<int> _hiddenSeverities{};
	QSet<int> _hiddenCategories{};
	QSet<QString> _hiddenEntities{};
	QRegularExpression _searchPattern{};
	std::optional<qint64> _timeFrom{};
	std::optional<qint64> _timeTo{};
};
