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

#include "loggerModel.hpp"
#include "helper.hpp"

#include <la/avdecc/internals/logItems.hpp>
#include <la/avdecc/controller/internals/logItems.hpp>

#include <unordered_map>
#include <mutex>
#include <utility>
#include <iterator>
#include <QTime>
#include <QFile>
#include <QTextStream>

Q_DECLARE_METATYPE(la::avdecc::logger::Layer)
Q_DECLARE_METATYPE(la::avdecc::logger::Level)
Q_DECLARE_METATYPE(std::string)

enum LoggerModelColumn
{
	Timestamp,
	Layer,
	Level,
	Message,

	Count
};

namespace avdecc
{
class LoggerModelPrivate : public QObject, public la::avdecc::logger::Logger::Observer
{
	Q_OBJECT
public:
	LoggerModelPrivate(LoggerModel* model)
		: q_ptr(model)
	{
		la::avdecc::logger::Logger::getInstance().registerObserver(this);
	}

	~LoggerModelPrivate()
	{
		la::avdecc::logger::Logger::getInstance().unregisterObserver(this);
	}

	int rowCount() const
	{
		return static_cast<int>(_entries.size());
	}

	int columnCount() const
	{
		return LoggerModelColumn::Count;
	}

	QVariant data(QModelIndex const& index, int role) const
	{
		if (role == Qt::DisplayRole)
		{
			auto const& entry = _entries.at(index.row());

			switch (index.column())
			{
				case LoggerModelColumn::Timestamp:
					return entry.timestamp;
				case LoggerModelColumn::Layer:
					return avdecc::helper::loggerLayerToString(entry.layer);
				case LoggerModelColumn::Level:
					return avdecc::helper::loggerLevelToString(entry.level);
				case LoggerModelColumn::Message:
					return entry.message;
				default:
					break;
			}
		}

		return {};
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role) const
	{
		if (orientation == Qt::Horizontal)
		{
			if (role == Qt::DisplayRole)
			{
				switch (section)
				{
					case LoggerModelColumn::Timestamp:
						return "Timestamp";
					case LoggerModelColumn::Layer:
						return "Layer";
					case LoggerModelColumn::Level:
						return "Level";
					case LoggerModelColumn::Message:
						return "Message";
					default:
						break;
				}
			}
		}

		return {};
	}

	Qt::ItemFlags flags(QModelIndex const& /*index*/) const
	{
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	}

	void clear()
	{
		Q_Q(LoggerModel);
		q->beginResetModel();
		_entries.clear();
		q->endResetModel();
	}

	bool save(QString const& filename, LoggerModel::SaveConfiguration const& saveConfiguration) const
	{
		auto file = QFile{ filename };
		if (!file.open(QIODevice::WriteOnly))
		{
			return false;
		}
		auto stream = QTextStream{ &file };

		for (auto const& entry : _entries)
		{
			if (!entry.message.contains(saveConfiguration.search))
			{
				continue;
			}

			auto const level = avdecc::helper::loggerLevelToString(entry.level);
			if (!level.contains(saveConfiguration.level))
			{
				continue;
			}

			auto const layer = avdecc::helper::loggerLayerToString(entry.layer);
			if (!layer.contains(saveConfiguration.layer))
			{
				continue;
			}

			auto elements = QStringList{};

			elements << entry.timestamp;
			elements << layer;
			elements << level;
			elements << entry.message;

			stream << elements.join("\t") << "\n";
		}

		return true;
	}

	virtual void onLogItem(la::avdecc::logger::Level const level, la::avdecc::logger::LogItem const* const item) noexcept override
	{
		// Called from any thread: the entry is queued and the main thread inserts all pending entries at once.
		// One row insertion per entry made a full controller re-enumeration at Trace level (tens of thousands of entries) blow the memory up to several GB: on macOS, every rowsInserted rebuilds the accessibility elements of the whole table when an accessibility client is active (AltTab, VoiceOver...), and the objects only get released when the event loop returns.
		auto const timestamp = QString("%1 - %2").arg(QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString(Qt::ISODate));
		auto flushScheduled = bool{};
		{
			auto const lg = std::lock_guard{ _pendingLock };
			_pendingEntries.push_back({ timestamp, item->getLayer(), level, QString::fromStdString(item->getMessage()) });
			flushScheduled = std::exchange(_flushScheduled, true);
		}
		if (!flushScheduled)
		{
			QMetaObject::invokeMethod(this,
				[this]()
				{
					flushPendingEntries();
				});
		}
	}

private:
	void flushPendingEntries()
	{
		auto pending = decltype(_pendingEntries){};
		{
			auto const lg = std::lock_guard{ _pendingLock };
			pending.swap(_pendingEntries);
			_flushScheduled = false;
		}
		if (pending.empty())
		{
			return;
		}
		Q_Q(LoggerModel);
		auto const count = q->rowCount();
		q->beginInsertRows({}, count, count + static_cast<int>(pending.size()) - 1);
		_entries.insert(_entries.end(), std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
		q->endInsertRows();
	}

	LoggerModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(LoggerModel)

	la::avdecc::logger::Logger* _logger{ nullptr };

	struct LogInfo
	{
		QString timestamp{};
		la::avdecc::logger::Layer layer{};
		la::avdecc::logger::Level level{};
		QString message{};
	};

	std::vector<LogInfo> _entries;
	std::mutex _pendingLock{};
	std::vector<LogInfo> _pendingEntries{}; // Entries logged since the last flush, guarded by _pendingLock
	bool _flushScheduled{ false }; // A flush is posted to the main thread, guarded by _pendingLock
};

LoggerModel::LoggerModel(QObject* parent)
	: QAbstractTableModel(parent)
	, d_ptr(new LoggerModelPrivate(this))
{
}

LoggerModel::~LoggerModel()
{
	delete d_ptr;
}

int LoggerModel::rowCount(QModelIndex const& /*parent*/) const
{
	Q_D(const LoggerModel);
	return d->rowCount();
}

int LoggerModel::columnCount(QModelIndex const& /*parent*/) const
{
	Q_D(const LoggerModel);
	return d->columnCount();
}

QVariant LoggerModel::data(QModelIndex const& index, int role) const
{
	Q_D(const LoggerModel);
	return d->data(index, role);
}

QVariant LoggerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_D(const LoggerModel);
	return d->headerData(section, orientation, role);
}

Qt::ItemFlags LoggerModel::flags(QModelIndex const& index) const
{
	Q_D(const LoggerModel);
	return d->flags(index);
}

void LoggerModel::clear()
{
	Q_D(LoggerModel);
	return d->clear();
}

bool LoggerModel::save(QString const& filename, SaveConfiguration const& saveConfiguration) const
{
	Q_D(const LoggerModel);
	return d->save(filename, saveConfiguration);
}

} // namespace avdecc

#include "loggerModel.moc"
