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

#include <hive/modelsLibrary/eventJournal.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/helper.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include <atomic>
#include <set>
#include <unordered_map>
#include <utility>

namespace hive
{
namespace modelsLibrary
{
namespace
{
constexpr auto SchemaVersion = 1;
constexpr auto MaxJournalFiles = 50; // Maximum number of automatically created session files kept on disk
constexpr auto WriterConnectionName = "HiveEventJournalWriter";

QString categoryToKey(EventJournal::Category const category) noexcept
{
	switch (category)
	{
		case EventJournal::Category::Session:
			return "session";
		case EventJournal::Category::Entity:
			return "entity";
		case EventJournal::Category::Connection:
			return "connection";
		case EventJournal::Category::Counters:
			return "counters";
		case EventJournal::Category::MediaClock:
			return "media_clock";
		case EventJournal::Category::Gptp:
			return "gptp";
		case EventJournal::Category::Link:
			return "link";
		case EventJournal::Category::Latency:
			return "latency";
		case EventJournal::Category::Redundancy:
			return "redundancy";
		default:
			return "unknown";
	}
}

std::optional<EventJournal::Category> categoryFromKey(QString const& key) noexcept
{
	static auto const s_map = std::unordered_map<QString, EventJournal::Category>{
		{ "session", EventJournal::Category::Session },
		{ "entity", EventJournal::Category::Entity },
		{ "connection", EventJournal::Category::Connection },
		{ "counters", EventJournal::Category::Counters },
		{ "media_clock", EventJournal::Category::MediaClock },
		{ "gptp", EventJournal::Category::Gptp },
		{ "link", EventJournal::Category::Link },
		{ "latency", EventJournal::Category::Latency },
		{ "redundancy", EventJournal::Category::Redundancy },
	};
	if (auto const it = s_map.find(key); it != s_map.end())
	{
		return it->second;
	}
	return std::nullopt;
}

QString severityToKey(EventJournal::Severity const severity) noexcept
{
	switch (severity)
	{
		case EventJournal::Severity::Info:
			return "info";
		case EventJournal::Severity::Warning:
			return "warning";
		case EventJournal::Severity::Error:
			return "error";
		case EventJournal::Severity::Recovered:
			return "recovered";
		default:
			return "info";
	}
}

std::optional<EventJournal::Severity> severityFromKey(QString const& key) noexcept
{
	static auto const s_map = std::unordered_map<QString, EventJournal::Severity>{
		{ "info", EventJournal::Severity::Info },
		{ "warning", EventJournal::Severity::Warning },
		{ "error", EventJournal::Severity::Error },
		{ "recovered", EventJournal::Severity::Recovered },
	};
	if (auto const it = s_map.find(key); it != s_map.end())
	{
		return it->second;
	}
	return std::nullopt;
}

QString streamInputCounterName(la::avdecc::entity::StreamInputCounterValidFlag const flag) noexcept
{
	switch (flag)
	{
		case la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked:
			return "Media Locked";
		case la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked:
			return "Media Unlocked";
		case la::avdecc::entity::StreamInputCounterValidFlag::StreamInterrupted:
			return "Stream Interrupted";
		case la::avdecc::entity::StreamInputCounterValidFlag::SeqNumMismatch:
			return "Sequence Number Mismatch";
		case la::avdecc::entity::StreamInputCounterValidFlag::MediaReset:
			return "Media Reset";
		case la::avdecc::entity::StreamInputCounterValidFlag::TimestampUncertain:
			return "Timestamp Uncertain";
		case la::avdecc::entity::StreamInputCounterValidFlag::UnsupportedFormat:
			return "Unsupported Format";
		case la::avdecc::entity::StreamInputCounterValidFlag::LateTimestamp:
			return "Late Timestamp";
		case la::avdecc::entity::StreamInputCounterValidFlag::EarlyTimestamp:
			return "Early Timestamp";
		default:
			return helper::toHexQString(la::avdecc::utils::to_integral(flag), true);
	}
}

QString statisticsCounterName(ControllerManager::StatisticsErrorCounterFlag const flag) noexcept
{
	switch (flag)
	{
		case ControllerManager::StatisticsErrorCounterFlag::AecpRetries:
			return "AECP Retries";
		case ControllerManager::StatisticsErrorCounterFlag::AecpTimeouts:
			return "AECP Timeouts";
		case ControllerManager::StatisticsErrorCounterFlag::AecpUnexpectedResponses:
			return "AECP Unexpected Responses";
		case ControllerManager::StatisticsErrorCounterFlag::AemAecpUnsolicitedLosses:
			return "AEM Unsolicited Response Losses";
		case ControllerManager::StatisticsErrorCounterFlag::MvuAecpUnsolicitedLosses:
			return "MVU Unsolicited Response Losses";
		default:
			return helper::toHexQString(la::avdecc::utils::to_integral(flag), true);
	}
}

QString jsonToString(QJsonObject const& object) noexcept
{
	if (object.isEmpty())
	{
		return {};
	}
	return QString::fromUtf8(QJsonDocument{ object }.toJson(QJsonDocument::Compact));
}
} // namespace

class EventJournal::Private
{
public:
	Private(EventJournal* q) noexcept
		: _q{ q }
	{
	}

	/* ************************************************************ */
	/* Session lifecycle                                            */
	/* ************************************************************ */
	bool isRecording() const noexcept
	{
		return _db.isValid() && _db.isOpen();
	}

	void startSession() noexcept
	{
		stopSession();

		auto const dirPath = EventJournal::journalsDirectory();
		if (!QDir{}.mkpath(dirPath))
		{
			emit _q->recordingFailed(QString{ "Cannot create journal directory: %1" }.arg(dirPath));
			return;
		}
		cleanupOldJournals(dirPath);

		_sessionFilePath = QString{ "%1/EventJournal_%2.%3" }.arg(dirPath).arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")).arg(EventJournal::JournalFileExtension);

		_db = QSqlDatabase::addDatabase("QSQLITE", WriterConnectionName);
		_db.setDatabaseName(_sessionFilePath);
		if (!_db.open())
		{
			auto const reason = _db.lastError().text();
			_db = QSqlDatabase{};
			QSqlDatabase::removeDatabase(WriterConnectionName);
			_sessionFilePath.clear();
			emit _q->recordingFailed(QString{ "Cannot create journal file: %1" }.arg(reason));
			return;
		}

		auto query = QSqlQuery{ _db };
		// WAL mode with NORMAL synchronous gives crash-safe, low latency writes (the whole point of the journal is to survive a crash during a show)
		query.exec("PRAGMA journal_mode=WAL");
		query.exec("PRAGMA synchronous=NORMAL");
		query.exec("CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY NOT NULL, value TEXT)");
		query.exec("CREATE TABLE IF NOT EXISTS entities (entity_id TEXT PRIMARY KEY NOT NULL, name TEXT)");
		query.exec("CREATE TABLE IF NOT EXISTS events (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NOT NULL, category TEXT NOT NULL, severity TEXT NOT NULL, entity_id TEXT, entity_name TEXT, subject TEXT, summary TEXT NOT NULL, details TEXT)");
		query.exec("CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events (timestamp)");

		_insertEventQuery = QSqlQuery{ _db };
		_insertEventQuery.prepare("INSERT INTO events (timestamp, category, severity, entity_id, entity_name, subject, summary, details) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
		_upsertEntityQuery = QSqlQuery{ _db };
		_upsertEntityQuery.prepare("INSERT OR REPLACE INTO entities (entity_id, name) VALUES (?, ?)");
		_upsertMetadataQuery = QSqlQuery{ _db };
		_upsertMetadataQuery.prepare("INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?)");

		setMetadata("schema_version", QString::number(SchemaVersion));
		setMetadata("hive_version", QCoreApplication::applicationVersion());
		setMetadata("computer_name", helper::getComputerName());
		setMetadata("started_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

		addEvent(Category::Session, Severity::Info, {}, {}, {}, "Recording started");
		emit _q->recordingStarted(_sessionFilePath);
	}

	void stopSession() noexcept
	{
		if (isRecording())
		{
			addEvent(Category::Session, Severity::Info, {}, {}, {}, "Recording stopped");
			setMetadata("stopped_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

			{
				// Convert the session file out of WAL mode, so it becomes a self-contained single file that can be opened read-only from anywhere
				auto query = QSqlQuery{ _db };
				query.exec("PRAGMA journal_mode=DELETE");
			}

			// All QSqlQuery objects must be released before the connection can be removed
			_insertEventQuery = QSqlQuery{};
			_upsertEntityQuery = QSqlQuery{};
			_upsertMetadataQuery = QSqlQuery{};
			_db.close();
			_db = QSqlDatabase{};
			QSqlDatabase::removeDatabase(WriterConnectionName);
			_sessionFilePath.clear();
			emit _q->recordingStopped();
		}

		// Reset all session state
		_events.clear();
		_metadata.clear();
		_entityNames.clear();
		clearTrackingState();
	}

	void setMetadata(QString const& key, QString const& value) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		_metadata[key] = value;
		_upsertMetadataQuery.addBindValue(key);
		_upsertMetadataQuery.addBindValue(value);
		_upsertMetadataQuery.exec();
	}

	bool exportSession(QString const& filePath) noexcept
	{
		if (!isRecording())
		{
			return false;
		}
		if (QFile::exists(filePath) && !QFile::remove(filePath))
		{
			return false;
		}
		// VACUUM INTO creates a consistent snapshot of the live database (path cannot be bound, so escape it)
		auto escapedPath = filePath;
		escapedPath.replace("'", "''");
		auto query = QSqlQuery{ _db };
		return query.exec(QString{ "VACUUM INTO '%1'" }.arg(escapedPath));
	}

	/* ************************************************************ */
	/* ControllerManager events handlers                            */
	/* ************************************************************ */
	void handleEntityOnline(la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const enumerationTime) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		auto entityName = QString{};
		auto& manager = ControllerManager::getInstance();
		if (auto controlledEntity = manager.getControlledEntity(entityID))
		{
			entityName = helper::smartEntityName(*controlledEntity);
			seedBaselines(entityID, *controlledEntity);
		}
		_entityNames[entityID] = entityName;
		upsertEntity(entityID, entityName);

		auto details = QJsonObject{};
		details["enumeration_time_ms"] = static_cast<qint64>(enumerationTime.count());
		addEvent(Category::Entity, Severity::Info, entityID, entityName, {}, "Entity is online", details);
	}

	void handleEntityOffline(la::avdecc::UniqueIdentifier const entityID) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		addEvent(Category::Entity, Severity::Warning, entityID, entityNameFor(entityID), {}, "Entity went offline");

		// Remove all tracked state for that entity (states will be re-seeded if it comes back online), but keep its name for future references (eg. as talker of a connection)
		_gptpStates.erase(entityID);
		_linkStatuses.erase(entityID);
		_clockLockStates.erase(entityID);
		_streamConnections.erase(entityID);
		_streamInputErrorCounters.erase(entityID);
		_statisticsErrorCounters.erase(entityID);
		_redundancyWarnings.erase(entityID);
		_latencyErrors.erase(entityID);
		_lostRedundantInterfaces.erase(entityID);
		_unsolRegistrationStates.erase(entityID);
	}

	void handleEntityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityName) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		_entityNames[entityID] = entityName;
		upsertEntity(entityID, entityName);
	}

	void handleStreamInputConnectionChanged(la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamInputConnectionInfo const& info) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		using State = la::avdecc::entity::model::StreamInputConnectionInfo::State;

		auto const entityID = stream.entityID;
		auto const streamIndex = stream.streamIndex;
		auto& states = _streamConnections[entityID];
		auto const it = states.find(streamIndex);
		auto const previousInfo = (it != states.end()) ? std::optional<la::avdecc::entity::model::StreamInputConnectionInfo>{ it->second } : std::nullopt;
		states[streamIndex] = info;

		// No baseline for that stream (should not happen, baselines are seeded when the entity comes online): silently store
		if (!previousInfo)
		{
			return;
		}

		auto const wasConnected = previousInfo->state == State::Connected;
		auto const isConnected = info.state == State::Connected;
		// FastConnecting is a transient state, only report actual Connected/NotConnected transitions
		if (isConnected && (!wasConnected || !(previousInfo->talkerStream == info.talkerStream)))
		{
			auto details = QJsonObject{};
			details["talker_id"] = helper::uniqueIdentifierToString(info.talkerStream.entityID);
			details["talker_stream_index"] = static_cast<int>(info.talkerStream.streamIndex);
			addEvent(Category::Connection, Severity::Info, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), QString{ "Connected to talker %1 (output stream %2)" }.arg(entityNameFor(info.talkerStream.entityID)).arg(info.talkerStream.streamIndex), details);
		}
		else if (!isConnected && wasConnected && info.state == State::NotConnected)
		{
			auto details = QJsonObject{};
			details["talker_id"] = helper::uniqueIdentifierToString(previousInfo->talkerStream.entityID);
			details["talker_stream_index"] = static_cast<int>(previousInfo->talkerStream.streamIndex);
			addEvent(Category::Connection, Severity::Info, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), QString{ "Disconnected from talker %1 (output stream %2)" }.arg(entityNameFor(previousInfo->talkerStream.entityID)).arg(previousInfo->talkerStream.streamIndex), details);
		}
	}

	void handleGptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		auto& states = _gptpStates[entityID];
		auto const it = states.find(avbInterfaceIndex);
		auto const newValue = std::make_pair(grandMasterID, grandMasterDomain);
		if (it == states.end())
		{
			states[avbInterfaceIndex] = newValue;
			return;
		}
		if (it->second == newValue)
		{
			return;
		}
		auto const previous = it->second;
		it->second = newValue;

		auto details = QJsonObject{};
		details["previous_grandmaster_id"] = helper::uniqueIdentifierToString(previous.first);
		details["new_grandmaster_id"] = helper::uniqueIdentifierToString(grandMasterID);
		details["previous_domain"] = static_cast<int>(previous.second);
		details["new_domain"] = static_cast<int>(grandMasterDomain);

		auto summary = QString{};
		if (previous.first != grandMasterID)
		{
			summary = QString{ "gPTP grandmaster changed from %1 to %2" }.arg(helper::uniqueIdentifierToString(previous.first), helper::uniqueIdentifierToString(grandMasterID));
		}
		else
		{
			summary = QString{ "gPTP domain changed from %1 to %2" }.arg(previous.second).arg(grandMasterDomain);
		}
		addEvent(Category::Gptp, Severity::Warning, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), summary, details);
	}

	void handleAvbInterfaceLinkStatusChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		using LinkStatus = la::avdecc::controller::ControlledEntity::InterfaceLinkStatus;

		auto& states = _linkStatuses[entityID];
		auto const it = states.find(avbInterfaceIndex);
		auto const previousStatus = (it != states.end()) ? it->second : LinkStatus::Unknown;
		states[avbInterfaceIndex] = linkStatus;

		if (linkStatus == previousStatus)
		{
			return;
		}
		if (linkStatus == LinkStatus::Down)
		{
			addEvent(Category::Link, Severity::Error, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), "Link down");
		}
		else if (linkStatus == LinkStatus::Up && previousStatus == LinkStatus::Down)
		{
			addEvent(Category::Link, Severity::Recovered, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), "Link up");
		}
	}

	void handleClockDomainCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockDomainCounters const& counters) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		auto const lockState = computeClockLockState(counters);
		if (!lockState)
		{
			return;
		}

		auto& states = _clockLockStates[entityID];
		auto const it = states.find(clockDomainIndex);
		if (it == states.end())
		{
			// No baseline (entity without counters at enumeration time): silently store
			states[clockDomainIndex] = *lockState;
			return;
		}
		if (it->second == *lockState)
		{
			return;
		}
		it->second = *lockState;

		if (*lockState)
		{
			addEvent(Category::MediaClock, Severity::Recovered, entityID, entityNameFor(entityID), QString{ "Clock Domain %1" }.arg(clockDomainIndex), "Media clock locked");
		}
		else
		{
			addEvent(Category::MediaClock, Severity::Error, entityID, entityNameFor(entityID), QString{ "Clock Domain %1" }.arg(clockDomainIndex), "Media clock unlocked");
		}
	}

	void handleStreamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, ControllerManager::StreamInputErrorCounters const& errorCounters) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		auto& previousCounters = _streamInputErrorCounters[entityID][descriptorIndex];
		for (auto const& [flag, value] : errorCounters)
		{
			auto const previousIt = previousCounters.find(flag);
			auto const previousValue = (previousIt != previousCounters.end()) ? previousIt->second : la::avdecc::entity::model::DescriptorCounter{ 0u };
			if (previousIt == previousCounters.end() || value > previousValue)
			{
				auto details = QJsonObject{};
				details["counter"] = streamInputCounterName(flag);
				details["previous_value"] = static_cast<qint64>(previousValue);
				details["value"] = static_cast<qint64>(value);
				addEvent(Category::Counters, Severity::Error, entityID, entityNameFor(entityID), inputStreamSubject(entityID, descriptorIndex), QString{ "'%1' error counter increased to %2" }.arg(streamInputCounterName(flag)).arg(value), details);
			}
		}
		// Counters disappearing from the map means the user acknowledged them in the UI: silently forget them
		previousCounters = errorCounters;
	}

	void handleStatisticsErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, ControllerManager::StatisticsErrorCounters const& errorCounters) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		// In redundant (dual interface) mode, statistics error counter increases are journaled from the per-counter signals (handleStatisticsCounterIncreased) so they carry the interface they occurred on
		if (ControllerManager::getInstance().isRedundantController())
		{
			return;
		}

		auto& previousCounters = _statisticsErrorCounters[entityID];
		for (auto const& [flag, value] : errorCounters)
		{
			auto const previousIt = previousCounters.find(flag);
			auto const previousValue = (previousIt != previousCounters.end()) ? previousIt->second : std::uint64_t{ 0u };
			if (previousIt == previousCounters.end() || value > previousValue)
			{
				auto details = QJsonObject{};
				details["counter"] = statisticsCounterName(flag);
				details["previous_value"] = static_cast<qint64>(previousValue);
				details["value"] = static_cast<qint64>(value);
				addEvent(Category::Counters, Severity::Error, entityID, entityNameFor(entityID), "Statistics", QString{ "'%1' error counter increased to %2" }.arg(statisticsCounterName(flag)).arg(value), details);
			}
		}
		// Counters disappearing from the map means the user acknowledged them in the UI: silently forget them
		previousCounters = errorCounters;
	}

	void handleStatisticsCounterIncreased(la::avdecc::UniqueIdentifier const entityID, ControllerManager::StatisticsErrorCounterFlag const flag, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		// Only used in redundant (dual interface) mode, to attribute each error counter increase to the interface it occurred on (single interface mode uses handleStatisticsErrorCounterChanged)
		if (!ControllerManager::getInstance().isRedundantController())
		{
			return;
		}

		auto const interfaceName = helper::interfaceTypeName(interfaceType);
		auto details = QJsonObject{};
		details["counter"] = statisticsCounterName(flag);
		details["interface"] = interfaceName;
		details["interface_value"] = static_cast<qint64>(interfaceValue);
		details["value"] = static_cast<qint64>(value);
		addEvent(Category::Counters, Severity::Error, entityID, entityNameFor(entityID), "Statistics", QString{ "'%1' error counter increased on the %2 interface (interface total: %3)" }.arg(statisticsCounterName(flag)).arg(interfaceName).arg(interfaceValue), details);
	}

	void handleInterfaceTransportError(la::avdecc::controller::InterfaceType const interfaceType) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		// In single interface mode this event is already covered by the session-wide transport error event
		if (!ControllerManager::getInstance().isRedundantController())
		{
			return;
		}

		auto const name = helper::interfaceTypeName(interfaceType);
		auto const isFirstError = _controllerTransportErrors.empty();
		_controllerTransportErrors.insert(interfaceType);
		if (isFirstError)
		{
			addEvent(Category::Redundancy, Severity::Error, {}, {}, QString{ "%1 Interface" }.arg(name), QString{ "Transport error on the controller %1 interface (the other redundant interface is still operational)" }.arg(name));
		}
		else
		{
			addEvent(Category::Redundancy, Severity::Error, {}, {}, QString{ "%1 Interface" }.arg(name), QString{ "Transport error on the controller %1 interface" }.arg(name));
		}
	}

	void handleEntityRedundantInterfaceOffline(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		_lostRedundantInterfaces[entityID].insert(avbInterfaceIndex);
		addEvent(Category::Redundancy, Severity::Warning, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), "Entity is offline on one redundant interface (still online on the other)");
	}

	void handleEntityRedundantInterfaceOnline(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		// Only journal a recovery if that interface was previously reported lost (this notification is also part of the normal discovery sequence)
		if (auto const it = _lostRedundantInterfaces.find(entityID); it != _lostRedundantInterfaces.end() && it->second.erase(avbInterfaceIndex) > 0)
		{
			addEvent(Category::Redundancy, Severity::Recovered, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), "Entity is back online on the redundant interface");
		}
	}

	void handleRedundancyWarningChanged(la::avdecc::UniqueIdentifier const entityID, bool const isRedundancyWarning) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		auto const it = _redundancyWarnings.find(entityID);
		auto const previousWarning = (it != _redundancyWarnings.end()) ? it->second : false;
		_redundancyWarnings[entityID] = isRedundancyWarning;

		if (isRedundancyWarning && !previousWarning)
		{
			addEvent(Category::Redundancy, Severity::Error, entityID, entityNameFor(entityID), {}, "Redundancy warning: redundant interfaces appear to be connected to the same network");
		}
		else if (!isRedundancyWarning && previousWarning)
		{
			addEvent(Category::Redundancy, Severity::Recovered, entityID, entityNameFor(entityID), {}, "Redundancy warning cleared");
		}
	}

	void handleStreamInputLatencyErrorChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isLatencyError) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		auto& states = _latencyErrors[entityID];
		auto const it = states.find(streamIndex);
		auto const previousError = (it != states.end()) ? it->second : false;
		states[streamIndex] = isLatencyError;

		if (isLatencyError && !previousError)
		{
			addEvent(Category::Latency, Severity::Error, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), "Stream latency error detected");
		}
		else if (!isLatencyError && previousError)
		{
			addEvent(Category::Latency, Severity::Recovered, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), "Stream latency error cleared");
		}
	}

	void handleTransportError() noexcept
	{
		if (!isRecording())
		{
			return;
		}
		if (ControllerManager::getInstance().isRedundantController())
		{
			addEvent(Category::Session, Severity::Error, {}, {}, {}, "Network transport error on all controller interfaces (the controller is no longer operational)");
		}
		else
		{
			addEvent(Category::Session, Severity::Error, {}, {}, {}, "Network transport error (the network interface may have gone down)");
		}
	}

	void handleUnsolicitedRegistrationChanged(la::avdecc::UniqueIdentifier const entityID, bool const isSubscribed, bool const triggeredByEntity) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		// isSubscribed is the aggregated state: only journal actual transitions (the signal is emitted for every per-interface change)
		auto const it = _unsolRegistrationStates.find(entityID);
		if (it == _unsolRegistrationStates.end())
		{
			// No baseline for that entity (should not happen, baselines are seeded when the entity comes online): silently store
			_unsolRegistrationStates[entityID] = isSubscribed;
			return;
		}
		if (it->second == isSubscribed)
		{
			return;
		}
		it->second = isSubscribed;

		if (!isSubscribed)
		{
			auto details = QJsonObject{};
			details["triggered_by_entity"] = triggeredByEntity;
			auto const summary = ControllerManager::getInstance().isRedundantController() ? QString{ "No longer subscribed to unsolicited notifications on any interface (entity updates may be missed, consider refreshing the entity)" } : QString{ "No longer subscribed to unsolicited notifications (entity updates may be missed, consider refreshing the entity)" };
			addEvent(Category::Entity, Severity::Error, entityID, entityNameFor(entityID), {}, summary, details);
		}
		else
		{
			addEvent(Category::Entity, Severity::Recovered, entityID, entityNameFor(entityID), {}, "Subscribed to unsolicited notifications again");
		}
	}

	void handleInterfaceUnsolicitedRegistrationChanged(la::avdecc::UniqueIdentifier const entityID, bool const isSubscribed, bool const triggeredByEntity, la::avdecc::controller::InterfaceType const interfaceType) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		// Per-interface events are only relevant in redundant (dual interface) mode: in single interface mode they duplicate the aggregated event
		if (!ControllerManager::getInstance().isRedundantController())
		{
			return;
		}

		auto const name = helper::interfaceTypeName(interfaceType);
		auto details = QJsonObject{};
		details["triggered_by_entity"] = triggeredByEntity;
		if (!isSubscribed)
		{
			auto summary = QString{ "No longer receiving unsolicited notifications on the %1 interface" }.arg(name);
			if (triggeredByEntity)
			{
				summary += " (unregistered by the entity)";
			}
			addEvent(Category::Redundancy, Severity::Warning, entityID, entityNameFor(entityID), QString{ "%1 Interface" }.arg(name), summary, details);
		}
		else
		{
			addEvent(Category::Redundancy, Severity::Recovered, entityID, entityNameFor(entityID), QString{ "%1 Interface" }.arg(name), QString{ "Receiving unsolicited notifications again on the %1 interface" }.arg(name), details);
		}
	}

	/* ************************************************************ */
	/* Internal helpers                                             */
	/* ************************************************************ */
	void addEvent(Category const category, Severity const severity, la::avdecc::UniqueIdentifier const entityID, QString const& entityName, QString const& subject, QString const& summary, QJsonObject const& details = {}) noexcept
	{
		auto event = Event{};
		event.timestamp = QDateTime::currentMSecsSinceEpoch();
		event.category = category;
		event.severity = severity;
		event.entityID = entityID;
		event.entityName = entityName;
		event.subject = subject;
		event.summary = summary;
		event.details = jsonToString(details);

		_insertEventQuery.addBindValue(event.timestamp);
		_insertEventQuery.addBindValue(categoryToKey(category));
		_insertEventQuery.addBindValue(severityToKey(severity));
		_insertEventQuery.addBindValue(entityID ? helper::uniqueIdentifierToString(entityID) : QVariant{ QString{} });
		_insertEventQuery.addBindValue(event.entityName);
		_insertEventQuery.addBindValue(event.subject);
		_insertEventQuery.addBindValue(event.summary);
		_insertEventQuery.addBindValue(event.details);
		_insertEventQuery.exec();

		_events.push_back(event);
		emit _q->eventAdded(event);
	}

	void upsertEntity(la::avdecc::UniqueIdentifier const entityID, QString const& name) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		_upsertEntityQuery.addBindValue(helper::uniqueIdentifierToString(entityID));
		_upsertEntityQuery.addBindValue(name);
		_upsertEntityQuery.exec();
	}

	/** Returns the last known name of the given entity, falling back to its EID. */
	QString entityNameFor(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		if (auto const it = _entityNames.find(entityID); it != _entityNames.end() && !it->second.isEmpty())
		{
			return it->second;
		}
		return helper::uniqueIdentifierToString(entityID);
	}

	/** Returns a designation for the given input stream, including its name if resolvable. */
	QString inputStreamSubject(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept
	{
		auto subject = QString{ "Input Stream %1" }.arg(streamIndex);
		auto& manager = ControllerManager::getInstance();
		if (auto controlledEntity = manager.getControlledEntity(entityID))
		{
			auto const name = helper::inputStreamName(*controlledEntity, streamIndex);
			if (!name.isEmpty())
			{
				subject += QString{ " (%1)" }.arg(name);
			}
		}
		return subject;
	}

	/** Computes the media clock lock state from clock domain counters (same rule as the Discovered Entities list). Returns std::nullopt if the counters are not available. */
	static std::optional<bool> computeClockLockState(la::avdecc::entity::model::ClockDomainCounters const& counters) noexcept
	{
		auto const itLocked = counters.find(la::avdecc::entity::ClockDomainCounterValidFlag::Locked);
		auto const itUnlocked = counters.find(la::avdecc::entity::ClockDomainCounterValidFlag::Unlocked);
		if (itLocked != counters.end() && itUnlocked != counters.end())
		{
			return itLocked->second > itUnlocked->second;
		}
		return std::nullopt;
	}

	/** Seeds the change-detection baselines from the current entity model state, so only actual changes occurring after the entity came online are journaled. */
	void seedBaselines(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept
	{
		// Aggregated unsolicited notifications subscription state (does not depend on the entity having a configuration)
		_unsolRegistrationStates[entityID] = controlledEntity.isSubscribedToUnsolicitedNotifications();

		try
		{
			auto const& configurationNode = controlledEntity.getCurrentConfigurationNode();
			for (auto const& [avbInterfaceIndex, avbInterfaceNode] : configurationNode.avbInterfaces)
			{
				_gptpStates[entityID][avbInterfaceIndex] = std::make_pair(avbInterfaceNode.dynamicModel.gptpGrandmasterID, avbInterfaceNode.dynamicModel.gptpDomainNumber);
				_linkStatuses[entityID][avbInterfaceIndex] = controlledEntity.getAvbInterfaceLinkStatus(avbInterfaceIndex);
			}
			for (auto const& [clockDomainIndex, clockDomainNode] : configurationNode.clockDomains)
			{
				if (clockDomainNode.dynamicModel.counters)
				{
					if (auto const lockState = computeClockLockState(*clockDomainNode.dynamicModel.counters))
					{
						_clockLockStates[entityID][clockDomainIndex] = *lockState;
					}
				}
			}
			for (auto const& [streamIndex, streamNode] : configurationNode.streamInputs)
			{
				_streamConnections[entityID][streamIndex] = streamNode.dynamicModel.connectionInfo;
			}
		}
		catch (...)
		{
			// Entity without a current configuration (or model enumeration error): nothing to seed
		}
	}

	void clearTrackingState() noexcept
	{
		_gptpStates.clear();
		_linkStatuses.clear();
		_clockLockStates.clear();
		_streamConnections.clear();
		_streamInputErrorCounters.clear();
		_statisticsErrorCounters.clear();
		_redundancyWarnings.clear();
		_latencyErrors.clear();
		_lostRedundantInterfaces.clear();
		_unsolRegistrationStates.clear();
		_controllerTransportErrors.clear();
	}

	void cleanupOldJournals(QString const& dirPath) noexcept
	{
		auto files = QDir{ dirPath }.entryInfoList({ QString{ "*.%1" }.arg(EventJournal::JournalFileExtension) }, QDir::Files, QDir::Name);
		// File names are timestamped, so name order is chronological order (also remove the WAL/SHM leftovers of removed sessions)
		while (files.size() >= MaxJournalFiles)
		{
			auto const filePath = files.takeFirst().absoluteFilePath();
			QFile::remove(filePath);
			QFile::remove(filePath + "-wal");
			QFile::remove(filePath + "-shm");
		}
	}

	// Public data (accessed by EventJournal)
	EventJournal* _q{ nullptr };
	QSqlDatabase _db{};
	QSqlQuery _insertEventQuery{};
	QSqlQuery _upsertEntityQuery{};
	QSqlQuery _upsertMetadataQuery{};
	QString _sessionFilePath{};
	std::vector<Event> _events{};
	QMap<QString, QString> _metadata{};

	// Change-detection state
	std::unordered_map<la::avdecc::UniqueIdentifier, QString, la::avdecc::UniqueIdentifier::hash> _entityNames{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::AvbInterfaceIndex, std::pair<la::avdecc::UniqueIdentifier, std::uint8_t>>, la::avdecc::UniqueIdentifier::hash> _gptpStates{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::AvbInterfaceIndex, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus>, la::avdecc::UniqueIdentifier::hash> _linkStatuses{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::ClockDomainIndex, bool>, la::avdecc::UniqueIdentifier::hash> _clockLockStates{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamInputConnectionInfo>, la::avdecc::UniqueIdentifier::hash> _streamConnections{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::DescriptorIndex, ControllerManager::StreamInputErrorCounters>, la::avdecc::UniqueIdentifier::hash> _streamInputErrorCounters{};
	std::unordered_map<la::avdecc::UniqueIdentifier, ControllerManager::StatisticsErrorCounters, la::avdecc::UniqueIdentifier::hash> _statisticsErrorCounters{};
	std::unordered_map<la::avdecc::UniqueIdentifier, bool, la::avdecc::UniqueIdentifier::hash> _redundancyWarnings{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::StreamIndex, bool>, la::avdecc::UniqueIdentifier::hash> _latencyErrors{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::set<la::avdecc::entity::model::AvbInterfaceIndex>, la::avdecc::UniqueIdentifier::hash> _lostRedundantInterfaces{};
	std::unordered_map<la::avdecc::UniqueIdentifier, bool, la::avdecc::UniqueIdentifier::hash> _unsolRegistrationStates{}; // Aggregated subscription state
	std::set<la::avdecc::controller::InterfaceType> _controllerTransportErrors{}; // Controller interfaces having received a fatal transport error
};

/* ************************************************************ */
/* EventJournal                                                 */
/* ************************************************************ */
EventJournal& EventJournal::getInstance() noexcept
{
	static EventJournal s_instance{};
	return s_instance;
}

EventJournal::EventJournal()
	: _pImpl{ std::make_unique<Private>(this) }
{
	auto& manager = ControllerManager::getInstance();

	connect(&manager, &ControllerManager::controllerOnline, this,
		[this]()
		{
			_pImpl->startSession();
		});
	connect(&manager, &ControllerManager::controllerOffline, this,
		[this]()
		{
			_pImpl->stopSession();
		});
	connect(&manager, &ControllerManager::transportError, this,
		[this]()
		{
			_pImpl->handleTransportError();
		});
	connect(&manager, &ControllerManager::entityOnline, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const enumerationTime)
		{
			_pImpl->handleEntityOnline(entityID, enumerationTime);
		});
	connect(&manager, &ControllerManager::entityOffline, this,
		[this](la::avdecc::UniqueIdentifier const entityID)
		{
			_pImpl->handleEntityOffline(entityID);
		});
	connect(&manager, &ControllerManager::entityNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, QString const& entityName)
		{
			_pImpl->handleEntityNameChanged(entityID, entityName);
		});
	connect(&manager, &ControllerManager::streamInputConnectionChanged, this,
		[this](la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamInputConnectionInfo const& info)
		{
			_pImpl->handleStreamInputConnectionChanged(stream, info);
		});
	connect(&manager, &ControllerManager::gptpChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
		{
			_pImpl->handleGptpChanged(entityID, avbInterfaceIndex, grandMasterID, grandMasterDomain);
		});
	connect(&manager, &ControllerManager::avbInterfaceLinkStatusChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus)
		{
			_pImpl->handleAvbInterfaceLinkStatusChanged(entityID, avbInterfaceIndex, linkStatus);
		});
	connect(&manager, &ControllerManager::clockDomainCountersChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockDomainCounters const& counters)
		{
			_pImpl->handleClockDomainCountersChanged(entityID, clockDomainIndex, counters);
		});
	connect(&manager, &ControllerManager::streamInputErrorCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, ControllerManager::StreamInputErrorCounters const& errorCounters)
		{
			_pImpl->handleStreamInputErrorCounterChanged(entityID, descriptorIndex, errorCounters);
		});
	connect(&manager, &ControllerManager::statisticsErrorCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, ControllerManager::StatisticsErrorCounters const& errorCounters)
		{
			_pImpl->handleStatisticsErrorCounterChanged(entityID, errorCounters);
		});
	connect(&manager, &ControllerManager::redundancyWarningChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, bool const isRedundancyWarning)
		{
			_pImpl->handleRedundancyWarningChanged(entityID, isRedundancyWarning);
		});
	connect(&manager, &ControllerManager::interfaceTransportError, this,
		[this](la::avdecc::controller::InterfaceType const interfaceType)
		{
			_pImpl->handleInterfaceTransportError(interfaceType);
		});
	connect(&manager, &ControllerManager::unsolicitedRegistrationChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, bool const isSubscribed, bool const triggeredByEntity)
		{
			_pImpl->handleUnsolicitedRegistrationChanged(entityID, isSubscribed, triggeredByEntity);
		});
	connect(&manager, &ControllerManager::interfaceUnsolicitedRegistrationChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, bool const isSubscribed, bool const triggeredByEntity, la::avdecc::controller::InterfaceType const interfaceType)
		{
			_pImpl->handleInterfaceUnsolicitedRegistrationChanged(entityID, isSubscribed, triggeredByEntity, interfaceType);
		});
	// Per-counter statistics signals, used in redundant (dual interface) mode to attribute error counter increases to the interface they occurred on
	connect(&manager, &ControllerManager::aecpRetryCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			_pImpl->handleStatisticsCounterIncreased(entityID, ControllerManager::StatisticsErrorCounterFlag::AecpRetries, value, interfaceType, interfaceValue);
		});
	connect(&manager, &ControllerManager::aecpTimeoutCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			_pImpl->handleStatisticsCounterIncreased(entityID, ControllerManager::StatisticsErrorCounterFlag::AecpTimeouts, value, interfaceType, interfaceValue);
		});
	connect(&manager, &ControllerManager::aecpUnexpectedResponseCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			_pImpl->handleStatisticsCounterIncreased(entityID, ControllerManager::StatisticsErrorCounterFlag::AecpUnexpectedResponses, value, interfaceType, interfaceValue);
		});
	connect(&manager, &ControllerManager::aemAecpUnsolicitedLossCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			_pImpl->handleStatisticsCounterIncreased(entityID, ControllerManager::StatisticsErrorCounterFlag::AemAecpUnsolicitedLosses, value, interfaceType, interfaceValue);
		});
	connect(&manager, &ControllerManager::mvuAecpUnsolicitedLossCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			_pImpl->handleStatisticsCounterIncreased(entityID, ControllerManager::StatisticsErrorCounterFlag::MvuAecpUnsolicitedLosses, value, interfaceType, interfaceValue);
		});
	connect(&manager, &ControllerManager::entityRedundantInterfaceOffline, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex)
		{
			_pImpl->handleEntityRedundantInterfaceOffline(entityID, avbInterfaceIndex);
		});
	connect(&manager, &ControllerManager::entityRedundantInterfaceOnline, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::Entity::InterfaceInformation const&)
		{
			_pImpl->handleEntityRedundantInterfaceOnline(entityID, avbInterfaceIndex);
		});
	connect(&manager, &ControllerManager::streamInputLatencyErrorChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isLatencyError)
		{
			_pImpl->handleStreamInputLatencyErrorChanged(entityID, streamIndex, isLatencyError);
		});
}

EventJournal::~EventJournal()
{
	_pImpl->stopSession();
}

QString EventJournal::journalsDirectory() noexcept
{
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/EventJournals";
}

QString EventJournal::categoryToString(Category const category) noexcept
{
	switch (category)
	{
		case Category::Session:
			return "Session";
		case Category::Entity:
			return "Entity";
		case Category::Connection:
			return "Connection";
		case Category::Counters:
			return "Counters";
		case Category::MediaClock:
			return "Media Clock";
		case Category::Gptp:
			return "gPTP";
		case Category::Link:
			return "Link";
		case Category::Latency:
			return "Latency";
		case Category::Redundancy:
			return "Redundancy";
		default:
			return "Unknown";
	}
}

QString EventJournal::severityToString(Severity const severity) noexcept
{
	switch (severity)
	{
		case Severity::Info:
			return "Info";
		case Severity::Warning:
			return "Warning";
		case Severity::Error:
			return "Error";
		case Severity::Recovered:
			return "Recovered";
		default:
			return "Unknown";
	}
}

std::optional<EventJournal::Session> EventJournal::loadSession(QString const& filePath) noexcept
{
	static auto s_readerCount = std::atomic_uint{ 0u };

	auto session = Session{};
	auto success = false;
	auto const connectionName = QString{ "HiveEventJournalReader_%1" }.arg(++s_readerCount);
	{
		auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
		db.setDatabaseName(filePath);
		db.setConnectOptions("QSQLITE_OPEN_READONLY");
		auto isOpen = QFile::exists(filePath) && db.open();
		if (isOpen && !QSqlQuery{ db }.exec("SELECT count(*) FROM events"))
		{
			// Read-only access to a WAL session file (eg. Hive crashed while recording it) may fail: retry in read-write mode, which performs WAL recovery
			db.close();
			isOpen = false;
		}
		if (!isOpen)
		{
			db.setConnectOptions();
			isOpen = QFile::exists(filePath) && db.open();
		}
		if (isOpen)
		{
			auto query = QSqlQuery{ db };
			if (query.exec("SELECT key, value FROM metadata"))
			{
				while (query.next())
				{
					session.metadata[query.value(0).toString()] = query.value(1).toString();
				}
			}
			if (query.exec("SELECT timestamp, category, severity, entity_id, entity_name, subject, summary, details FROM events ORDER BY timestamp, id"))
			{
				success = true;
				while (query.next())
				{
					auto event = Event{};
					event.timestamp = query.value(0).toLongLong();
					event.category = categoryFromKey(query.value(1).toString()).value_or(Category::Session);
					event.severity = severityFromKey(query.value(2).toString()).value_or(Severity::Info);
					auto const entityIDString = query.value(3).toString();
					if (!entityIDString.isEmpty())
					{
						auto ok = false;
						auto const eid = entityIDString.toULongLong(&ok, 16);
						if (ok)
						{
							event.entityID = la::avdecc::UniqueIdentifier{ eid };
						}
					}
					event.entityName = query.value(4).toString();
					event.subject = query.value(5).toString();
					event.summary = query.value(6).toString();
					event.details = query.value(7).toString();
					session.events.push_back(std::move(event));
				}
			}
			db.close();
		}
	}
	QSqlDatabase::removeDatabase(connectionName);

	if (!success)
	{
		return std::nullopt;
	}
	return session;
}

bool EventJournal::isRecording() const noexcept
{
	return _pImpl->isRecording();
}

QString EventJournal::currentSessionFilePath() const noexcept
{
	return _pImpl->_sessionFilePath;
}

std::vector<EventJournal::Event> EventJournal::currentSessionEvents() const noexcept
{
	return _pImpl->_events;
}

QMap<QString, QString> EventJournal::currentSessionMetadata() const noexcept
{
	return _pImpl->_metadata;
}

void EventJournal::setSessionMetadata(QString const& key, QString const& value) noexcept
{
	_pImpl->setMetadata(key, value);
}

bool EventJournal::exportCurrentSession(QString const& filePath) noexcept
{
	return _pImpl->exportSession(filePath);
}

} // namespace modelsLibrary
} // namespace hive
