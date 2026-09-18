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

#include <la/avdecc/avdecc.hpp>

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
#include <QSysInfo>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hive
{
namespace modelsLibrary
{
namespace
{
constexpr auto MaxJournalFiles = 50; // Maximum number of automatically created session files kept on disk
constexpr auto WriterConnectionName = "HiveEventJournalWriter";

// Stream data loss rule (see docs/EventJournal.md): a bound Stream Input receiving data increments FRAMES_RX at least once per second and is notified at most once per second (Milan 1.2 tables 5.6 and 5.22), so between two notifications the increment is at least one second worth of frames unless frames were missing
constexpr auto StreamDataLearningWindow = std::size_t{ 30u }; // Number of increments kept per stream to learn the expected increment
constexpr auto StreamDataMinLearningSamples = std::size_t{ 10u }; // Number of learned increments required before a stream is evaluated
constexpr auto StreamDataReferencePercentile = std::size_t{ 10u }; // Percentile of the learned increments used as the expected increment (the low end is the one second worth reference, a device whose notification cadence varies produces larger increments, and a single undetected partial loss must not lower the reference)
constexpr auto StreamDataLossThreshold = 0.5; // An increment below this fraction of the expected increment means frames were missing during the interval
constexpr auto StreamDataMinEvaluationInterval = std::chrono::milliseconds{ 900 }; // Notifications closer than this to the previous one (queued burst, other interface of a redundant controller, non compliant device) are neither evaluated nor learned
constexpr auto StreamDataResyncFactor = std::uint64_t{ 10u }; // An increment above this multiple of the expected increment (counter reset by a rebind, value jump) only resynchronizes the reference sample

/** Builds the subject for an event about one of an entity's redundant AVB interfaces (eg. "Primary Interface" for a Milan redundant device, "AVB Interface 2" otherwise). */
QString redundantInterfaceSubject(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::MilanVersion const& milanVersion) noexcept
{
	if (auto const interfaceType = helper::redundantInterfaceType(avbInterfaceIndex, milanVersion))
	{
		return QString{ "%1 Interface" }.arg(helper::interfaceTypeName(*interfaceType));
	}
	return QString{ "AVB Interface %1" }.arg(avbInterfaceIndex);
}

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
		case EventJournal::Category::Streaming:
			return "streaming";
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
		{ "streaming", EventJournal::Category::Streaming },
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

QString levelToKey(EventJournal::Level const level) noexcept
{
	switch (level)
	{
		case EventJournal::Level::User:
			return "user";
		case EventJournal::Level::Diagnostic:
			return "diagnostic";
		default:
			return "user";
	}
}

std::optional<EventJournal::Level> levelFromKey(QString const& key) noexcept
{
	static auto const s_map = std::unordered_map<QString, EventJournal::Level>{
		{ "user", EventJournal::Level::User },
		{ "diagnostic", EventJournal::Level::Diagnostic },
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

/** Default arrival time source of the counter notifications (the unit tests substitute a simulated clock). */
std::chrono::steady_clock::time_point steadyClockNow() noexcept
{
	return std::chrono::steady_clock::now();
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
		// Schema version 2 (see docs/EventJournal.md): schema changes are additive only, a reader must accept any older file and open a newer one in degraded mode
		query.exec("CREATE TABLE IF NOT EXISTS events (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NOT NULL, category TEXT NOT NULL, severity TEXT NOT NULL, entity_id TEXT, entity_name TEXT, subject TEXT, summary TEXT NOT NULL, details TEXT, level TEXT NOT NULL DEFAULT 'user')");
		query.exec("CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events (timestamp)");
		// Entity snapshots (serialized entity models), written from schema version 2 on
		query.exec("CREATE TABLE IF NOT EXISTS snapshots (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NOT NULL, entity_id TEXT NOT NULL, reason TEXT NOT NULL, format TEXT NOT NULL, data BLOB NOT NULL, uncompressed_size INTEGER NOT NULL)");
		query.exec("CREATE INDEX IF NOT EXISTS idx_snapshots_entity ON snapshots (entity_id, timestamp)");

		_insertEventQuery = QSqlQuery{ _db };
		_insertEventQuery.prepare("INSERT INTO events (timestamp, category, severity, entity_id, entity_name, subject, summary, details, level) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
		_upsertEntityQuery = QSqlQuery{ _db };
		_upsertEntityQuery.prepare("INSERT OR REPLACE INTO entities (entity_id, name) VALUES (?, ?)");
		_upsertMetadataQuery = QSqlQuery{ _db };
		_upsertMetadataQuery.prepare("INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?)");

		setMetadata("schema_version", QString::number(EventJournal::CurrentSchemaVersion));
		setMetadata("hive_version", QCoreApplication::applicationVersion());
		setMetadata("la_avdecc_version", QString::fromStdString(la::avdecc::getVersion()));
		setMetadata("os", QSysInfo::prettyProductName());
		setMetadata("computer_name", helper::getComputerName());
		setMetadata("started_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
		setMetadata("recording_level_at_start", levelToKey(_recordingLevel));

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

	void handleControllerOnline() noexcept
	{
		// Reloading the controller (eg. an interface change) must NOT clear the journal, the user would lose the whole session: keep the current session if one is already recording, only note that the controller restarted
		if (isRecording())
		{
			addEvent(Category::Session, Severity::Info, {}, {}, {}, "Controller restarted");
		}
		else
		{
			startSession();
		}
	}

	void handleControllerOffline() noexcept
	{
		// Do NOT stop the session when the controller goes offline (eg. a reload): the session is only closed when the application exits (destructor), so the journal survives a controller reload. Just note that the controller stopped.
		if (isRecording())
		{
			addEvent(Category::Session, Severity::Info, {}, {}, {}, "Controller stopped");
		}
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

	void clearCurrentSession() noexcept
	{
		if (!isRecording())
		{
			return;
		}
		// Wipe the persisted and in-memory events (the tracking baselines and known entity names are kept, so ongoing states are not re-reported)
		auto query = QSqlQuery{ _db };
		query.exec("DELETE FROM events");
		_events.clear();
		emit _q->sessionCleared();
		// Record a marker so the cleared session is not completely empty
		addEvent(Category::Session, Severity::Info, {}, {}, {}, "Journal cleared");
	}

	void setRecordingLevel(Level const level) noexcept
	{
		if (level == _recordingLevel)
		{
			return;
		}
		_recordingLevel = level;
		if (isRecording())
		{
			// Journaled at user level, so any viewer can tell which periods of the session carry diagnostic data
			addEvent(Category::Session, Severity::Info, {}, {}, {}, level == Level::Diagnostic ? "Diagnostic recording enabled" : "Diagnostic recording disabled");
			if (level == Level::Diagnostic)
			{
				// The streams already monitored announce their coverage again at their next notification, so the diagnostic period starts with the reference of every covered stream
				for (auto& [entityID, states] : _streamDataStates)
				{
					for (auto& [streamIndex, state] : states)
					{
						state.isMonitored = false;
					}
				}
			}
		}
		emit _q->recordingLevelChanged(level);
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
		_streamDataStates.erase(entityID);
		_statisticsErrorCounters.erase(entityID);
		_redundancyWarnings.erase(entityID);
		_latencyErrors.erase(entityID);
		_lostRedundantInterfaces.erase(entityID);
		_unsolRegistrationStates.erase(entityID);
		_lostUnsolInterfaces.erase(entityID);
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

		// The stream input counters are reset by a rebind and the first increment after a connection is partial: the stream data loss rule restarts its learning
		resetStreamDataState(entityID, streamIndex);

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

		auto const newState = makeClockLockState(counters);
		if (!newState)
		{
			return;
		}

		auto& states = _clockLockStates[entityID];
		auto const it = states.find(clockDomainIndex);
		if (it == states.end())
		{
			// No baseline (entity without counters at enumeration time): silently store
			states[clockDomainIndex] = *newState;
			return;
		}
		auto const previousState = it->second;
		it->second = *newState;

		// A counter going backwards means the device rebooted or its counters were reset: the increments are meaningless, only the resulting lock state can be compared
		auto const countersReset = newState->lockedCounter < previousState.lockedCounter || newState->unlockedCounter < previousState.unlockedCounter;
		auto const lockedIncrement = countersReset ? la::avdecc::entity::model::DescriptorCounter{ 0u } : newState->lockedCounter - previousState.lockedCounter;
		auto const unlockedIncrement = countersReset ? la::avdecc::entity::model::DescriptorCounter{ 0u } : newState->unlockedCounter - previousState.unlockedCounter;

		// The counters are only notified once in a while, so the clock may have unlocked and relocked (possibly several times) between two notifications without the derived lock state ever changing.
		// Replay the transitions implied by the increments, in the only order consistent with an alternating unlock/lock sequence starting from the previous state, so every unlock the device counted ends up in the journal.
		auto transitions = std::vector<bool>{}; // Lock state resulting from each transition, in order
		auto isLocked = previousState.isLocked;
		auto remainingLocks = lockedIncrement;
		auto remainingUnlocks = unlockedIncrement;
		while ((isLocked && remainingUnlocks > 0u) || (!isLocked && remainingLocks > 0u))
		{
			if (isLocked)
			{
				--remainingUnlocks;
			}
			else
			{
				--remainingLocks;
			}
			isLocked = !isLocked;
			transitions.push_back(isLocked);
		}
		// Whatever the increments said, the journaled state must end on the lock state derived from the counters (same rule as the Discovered Entities list)
		if (isLocked != newState->isLocked)
		{
			isLocked = newState->isLocked;
			transitions.push_back(isLocked);
		}
		if (transitions.empty())
		{
			return;
		}

		// A transition is inferred when it could not have been observed by comparing the lock states before and after the notification: all of them when the state did not change, all but the last one otherwise
		auto const stateChanged = previousState.isLocked != newState->isLocked;
		for (auto index = std::size_t{ 0u }; index < transitions.size(); ++index)
		{
			auto const isInferred = !stateChanged || index + 1u < transitions.size();
			auto details = QJsonObject{};
			details["locked_counter"] = static_cast<qint64>(newState->lockedCounter);
			details["unlocked_counter"] = static_cast<qint64>(newState->unlockedCounter);
			details["locked_increment"] = static_cast<qint64>(lockedIncrement);
			details["unlocked_increment"] = static_cast<qint64>(unlockedIncrement);
			if (isInferred)
			{
				details["inferred"] = true;
			}
			if (transitions[index])
			{
				addEvent(Category::MediaClock, Severity::Recovered, entityID, entityNameFor(entityID), QString{ "Clock Domain %1" }.arg(clockDomainIndex), "Media clock locked", details);
			}
			else
			{
				addEvent(Category::MediaClock, Severity::Error, entityID, entityNameFor(entityID), QString{ "Clock Domain %1" }.arg(clockDomainIndex), "Media clock unlocked", details);
			}
		}
	}

	void handleStreamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		// The frame rate may change with the format: the stream data loss rule restarts its learning
		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
		{
			resetStreamDataState(entityID, streamIndex);
		}
	}

	void handleStreamInputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInputCounters const& counters) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		auto const framesRxIt = counters.find(la::avdecc::entity::StreamInputCounterValidFlag::FramesRx);
		if (framesRxIt == counters.end())
		{
			return;
		}
		auto const framesRx = framesRxIt->second;
		auto const arrival = _monotonicClock();

		// Only a bound stream receives data: the connection state is read live from the entity model (which la_avdecc updates before the queued notification reaches the main thread), so the last partial second of a stream the user just disconnected is never evaluated
		auto const connectionInfo = liveStreamInputConnectionInfo(entityID, streamIndex);
		if (!connectionInfo || connectionInfo->state != la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected)
		{
			return;
		}

		auto& state = _streamDataStates[entityID][streamIndex];
		if (!state.hasPrevious)
		{
			state.hasPrevious = true;
			state.previousFramesRx = framesRx;
			state.previousArrival = arrival;
			return;
		}
		// DescriptorCounter is 32 bits and wraps every few days at 8000 frames per second, so the increment is computed modulo 2^32
		auto const increment = static_cast<la::avdecc::entity::model::DescriptorCounter>(framesRx - state.previousFramesRx);
		// The same value again is not a new observation: a redundant controller receives every notification once per interface (same payload a few ms apart, or seconds apart when the main thread stalled in between), and a notification triggered by another counter while no frame arrived tells nothing the next one will not tell with a longer elapsed time
		if (increment == 0u)
		{
			return;
		}
		auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(arrival - state.previousArrival);
		state.previousFramesRx = framesRx;
		state.previousArrival = arrival;

		// Notifications closer than the minimum interval (a queued burst after a main thread stall, the other interface of a redundant controller having re-read the counter, a non compliant device) only move the reference sample: they are neither evaluated nor learned, so they can neither raise an event nor lower the reference
		if (elapsed < StreamDataMinEvaluationInterval)
		{
			return;
		}

		auto const expected = expectedStreamDataIncrement(state.increments);
		if (expected)
		{
			// Counter reset by a rebind or value jump: the reference sample is resynchronized, the learned increments are kept (the device cadence did not change)
			if (static_cast<std::uint64_t>(increment) > StreamDataResyncFactor * static_cast<std::uint64_t>(*expected))
			{
				return;
			}
			// Every controller side problem (lost notification, stalled main thread, expired subscription) makes the increment larger or delays it, never smaller: an increment clearly below one second worth of frames means frames were missing during the interval, whatever happened on the controller side
			if (static_cast<double>(increment) < StreamDataLossThreshold * static_cast<double>(*expected))
			{
				// The estimate assumes a one second device cadence (the interval may be longer, but the frames of at most one second of it are counted)
				auto const missingMs = static_cast<qint64>((static_cast<std::uint64_t>(*expected) - static_cast<std::uint64_t>(increment)) * 1000u / static_cast<std::uint64_t>(*expected));
				auto details = streamDataDetails(*connectionInfo, *expected);
				details["frames_rx"] = static_cast<qint64>(framesRx);
				details["increment"] = static_cast<qint64>(increment);
				details["elapsed_ms"] = static_cast<qint64>(elapsed.count());
				details["missing_ms"] = missingMs;
				addEvent(Category::Streaming, Severity::Error, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), QString{ "Stream data loss: about %1 ms of frames missing" }.arg(missingMs), details);
				// The alerted sample is not learned, so repeated partial losses do not lower the reference
				return;
			}
		}

		if (state.increments.size() >= StreamDataLearningWindow)
		{
			state.increments.erase(state.increments.begin());
		}
		state.increments.push_back(increment);

		// End of the learning phase: tell which streams are covered and with which reference (a stream never reaching this point is not monitored, eg. a device counting once per observation interval of one second)
		if (!state.isMonitored)
		{
			if (auto const reference = expectedStreamDataIncrement(state.increments))
			{
				state.isMonitored = true;
				auto details = streamDataDetails(*connectionInfo, *reference);
				details["samples"] = static_cast<qint64>(state.increments.size());
				addEvent(Category::Streaming, Severity::Info, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), "Stream data monitoring started", details, Level::Diagnostic);
			}
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
		addEvent(Category::Redundancy, Severity::Warning, entityID, entityNameFor(entityID), redundantInterfaceSubject(avbInterfaceIndex, milanVersionFor(entityID)), "Entity is offline on one redundant interface (still online on the other)");
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
			addEvent(Category::Redundancy, Severity::Recovered, entityID, entityNameFor(entityID), redundantInterfaceSubject(avbInterfaceIndex, milanVersionFor(entityID)), "Entity is back online on the redundant interface");
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
		auto& lostInterfaces = _lostUnsolInterfaces[entityID];

		if (!isSubscribed)
		{
			// If the entity is currently offline on this redundant interface, the "Entity is offline on one redundant interface" event journaled just before already covers the situation: do not journal a redundant unsolicited-notifications-lost event
			auto const avbInterfaceIndex = static_cast<la::avdecc::entity::model::AvbInterfaceIndex>(interfaceType);
			if (auto const it = _lostRedundantInterfaces.find(entityID); it != _lostRedundantInterfaces.end() && it->second.count(avbInterfaceIndex) > 0)
			{
				return;
			}
			// Only journal (and track) the first transition to unsubscribed
			if (!lostInterfaces.insert(interfaceType).second)
			{
				return;
			}
			auto details = QJsonObject{};
			details["triggered_by_entity"] = triggeredByEntity;
			auto summary = QString{ "No longer receiving unsolicited notifications on the %1 interface" }.arg(name);
			if (triggeredByEntity)
			{
				summary += " (unregistered by the entity)";
			}
			addEvent(Category::Redundancy, Severity::Warning, entityID, entityNameFor(entityID), QString{ "%1 Interface" }.arg(name), summary, details);
		}
		else
		{
			// Only journal a recovery if this interface was previously reported as lost: the first subscription on an interface (eg. the lazy registration on the Secondary interface during the initial discovery sequence) is normal and must not be reported as a recovery
			if (lostInterfaces.erase(interfaceType) == 0)
			{
				return;
			}
			auto details = QJsonObject{};
			details["triggered_by_entity"] = triggeredByEntity;
			addEvent(Category::Redundancy, Severity::Recovered, entityID, entityNameFor(entityID), QString{ "%1 Interface" }.arg(name), QString{ "Receiving unsolicited notifications again on the %1 interface" }.arg(name), details);
		}
	}

	/* ************************************************************ */
	/* Internal helpers                                             */
	/* ************************************************************ */
	void addEvent(Category const category, Severity const severity, la::avdecc::UniqueIdentifier const entityID, QString const& entityName, QString const& subject, QString const& summary, QJsonObject const& details = {}, Level const level = Level::User) noexcept
	{
		// Diagnostic events are only written while diagnostic recording is enabled
		if (level == Level::Diagnostic && _recordingLevel != Level::Diagnostic)
		{
			return;
		}

		auto event = Event{};
		event.timestamp = QDateTime::currentMSecsSinceEpoch();
		event.category = category;
		event.severity = severity;
		event.level = level;
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
		_insertEventQuery.addBindValue(levelToKey(level));
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

	/** Returns the Milan compatibility version of the given entity (default constructed if the entity is not currently known). */
	la::avdecc::entity::model::MilanVersion milanVersionFor(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto& manager = ControllerManager::getInstance();
		if (auto controlledEntity = manager.getControlledEntity(entityID))
		{
			return controlledEntity->getMilanCompatibilityVersion();
		}
		return {};
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

	/** Media clock lock tracking state of a clock domain: the lock state derived from the counters, and the counter values it was derived from. */
	struct ClockLockState
	{
		bool isLocked{ false };
		la::avdecc::entity::model::DescriptorCounter lockedCounter{ 0u };
		la::avdecc::entity::model::DescriptorCounter unlockedCounter{ 0u };
	};

	/** Builds the media clock lock tracking state from clock domain counters (the domain is locked when it locked more times than it unlocked, same rule as the Discovered Entities list). Returns std::nullopt if the counters are not available. */
	static std::optional<ClockLockState> makeClockLockState(la::avdecc::entity::model::ClockDomainCounters const& counters) noexcept
	{
		auto const itLocked = counters.find(la::avdecc::entity::ClockDomainCounterValidFlag::Locked);
		auto const itUnlocked = counters.find(la::avdecc::entity::ClockDomainCounterValidFlag::Unlocked);
		if (itLocked != counters.end() && itUnlocked != counters.end())
		{
			return ClockLockState{ itLocked->second > itUnlocked->second, itLocked->second, itUnlocked->second };
		}
		return std::nullopt;
	}

	/** Stream data reception tracking state of a stream input: the FRAMES_RX value and arrival time of the previous notification, and the learned increments. */
	struct StreamDataState
	{
		bool hasPrevious{ false }; // False until the first notification (or the baseline seeded at entity online) provides a previous value
		la::avdecc::entity::model::DescriptorCounter previousFramesRx{ 0u };
		std::chrono::steady_clock::time_point previousArrival{};
		std::vector<la::avdecc::entity::model::DescriptorCounter> increments{}; // Last StreamDataLearningWindow increments, oldest first
		bool isMonitored{ false }; // True once the end of the learning phase has been journaled
	};

	/** Returns the expected one second worth of frames of a stream from its learned increments (the StreamDataReferencePercentile-th percentile), or std::nullopt while the stream is still learning (not enough increments, or the stream received nothing so far). */
	static std::optional<la::avdecc::entity::model::DescriptorCounter> expectedStreamDataIncrement(std::vector<la::avdecc::entity::model::DescriptorCounter> const& increments) noexcept
	{
		if (increments.size() < StreamDataMinLearningSamples)
		{
			return std::nullopt;
		}
		auto sorted = increments;
		auto const rank = (sorted.size() - 1u) * StreamDataReferencePercentile / 100u;
		std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(rank), sorted.end());
		auto const reference = sorted[rank];
		// A null reference means the stream received nothing while learning (talker stopped): keep learning until frames flow
		if (reference == 0u)
		{
			return std::nullopt;
		}
		return reference;
	}

	/** Returns the connection information of the given input stream as currently stored in the entity model, or std::nullopt if the entity is offline or has no such stream. */
	static std::optional<la::avdecc::entity::model::StreamInputConnectionInfo> liveStreamInputConnectionInfo(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept
	{
		auto& manager = ControllerManager::getInstance();
		if (auto controlledEntity = manager.getControlledEntity(entityID))
		{
			try
			{
				auto const& entityNode = controlledEntity->getEntityNode();
				return controlledEntity->getStreamInputNode(entityNode.dynamicModel.currentConfiguration, streamIndex).dynamicModel.connectionInfo;
			}
			catch (...)
			{
				// Entity without a current configuration, or unknown stream index
			}
		}
		return std::nullopt;
	}

	/** Restarts the stream data loss learning of the given input stream (the next notification only provides the previous value). */
	void resetStreamDataState(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept
	{
		if (auto const it = _streamDataStates.find(entityID); it != _streamDataStates.end())
		{
			it->second.erase(streamIndex);
		}
	}

	/** Builds the details common to the stream data events: the talker (so the analyser joins listener and talker events without a network state dump) and the expected increment. */
	static QJsonObject streamDataDetails(la::avdecc::entity::model::StreamInputConnectionInfo const& connectionInfo, la::avdecc::entity::model::DescriptorCounter const expectedIncrement) noexcept
	{
		auto details = QJsonObject{};
		details["expected_increment"] = static_cast<qint64>(expectedIncrement);
		details["talker_id"] = helper::uniqueIdentifierToString(connectionInfo.talkerStream.entityID);
		details["talker_stream_index"] = static_cast<int>(connectionInfo.talkerStream.streamIndex);
		return details;
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
					if (auto const lockState = makeClockLockState(*clockDomainNode.dynamicModel.counters))
					{
						_clockLockStates[entityID][clockDomainIndex] = *lockState;
					}
				}
			}
			// An entity coming online again starts the stream data loss learning from scratch (its counters may have been reset)
			_streamDataStates.erase(entityID);
			auto const arrival = _monotonicClock();
			for (auto const& [streamIndex, streamNode] : configurationNode.streamInputs)
			{
				_streamConnections[entityID][streamIndex] = streamNode.dynamicModel.connectionInfo;
				// The FRAMES_RX value of a connected stream is the first reference sample of the stream data loss rule (only the increments matter, so a stale value at most makes the first learned increment larger)
				if (streamNode.dynamicModel.connectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected && streamNode.dynamicModel.counters)
				{
					if (auto const framesRxIt = streamNode.dynamicModel.counters->find(la::avdecc::entity::StreamInputCounterValidFlag::FramesRx); framesRxIt != streamNode.dynamicModel.counters->end())
					{
						auto& state = _streamDataStates[entityID][streamIndex];
						state.hasPrevious = true;
						state.previousFramesRx = framesRxIt->second;
						state.previousArrival = arrival;
					}
				}
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
		_streamDataStates.clear();
		_statisticsErrorCounters.clear();
		_redundancyWarnings.clear();
		_latencyErrors.clear();
		_lostRedundantInterfaces.clear();
		_unsolRegistrationStates.clear();
		_lostUnsolInterfaces.clear();
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
	Level _recordingLevel{ Level::User };

	// Change-detection state
	std::unordered_map<la::avdecc::UniqueIdentifier, QString, la::avdecc::UniqueIdentifier::hash> _entityNames{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::AvbInterfaceIndex, std::pair<la::avdecc::UniqueIdentifier, std::uint8_t>>, la::avdecc::UniqueIdentifier::hash> _gptpStates{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::AvbInterfaceIndex, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus>, la::avdecc::UniqueIdentifier::hash> _linkStatuses{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::ClockDomainIndex, ClockLockState>, la::avdecc::UniqueIdentifier::hash> _clockLockStates{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamInputConnectionInfo>, la::avdecc::UniqueIdentifier::hash> _streamConnections{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::DescriptorIndex, ControllerManager::StreamInputErrorCounters>, la::avdecc::UniqueIdentifier::hash> _streamInputErrorCounters{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::StreamIndex, StreamDataState>, la::avdecc::UniqueIdentifier::hash> _streamDataStates{}; // Stream data loss rule state, connected streams with a FRAMES_RX counter only
	std::function<std::chrono::steady_clock::time_point()> _monotonicClock{ &steadyClockNow }; // Arrival time source of the counter notifications (overridden by the unit tests)
	std::unordered_map<la::avdecc::UniqueIdentifier, ControllerManager::StatisticsErrorCounters, la::avdecc::UniqueIdentifier::hash> _statisticsErrorCounters{};
	std::unordered_map<la::avdecc::UniqueIdentifier, bool, la::avdecc::UniqueIdentifier::hash> _redundancyWarnings{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::StreamIndex, bool>, la::avdecc::UniqueIdentifier::hash> _latencyErrors{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::set<la::avdecc::entity::model::AvbInterfaceIndex>, la::avdecc::UniqueIdentifier::hash> _lostRedundantInterfaces{};
	std::unordered_map<la::avdecc::UniqueIdentifier, bool, la::avdecc::UniqueIdentifier::hash> _unsolRegistrationStates{}; // Aggregated subscription state
	std::unordered_map<la::avdecc::UniqueIdentifier, std::set<la::avdecc::controller::InterfaceType>, la::avdecc::UniqueIdentifier::hash> _lostUnsolInterfaces{}; // Redundant interfaces currently reported as no longer receiving unsolicited notifications
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
			_pImpl->handleControllerOnline();
		});
	connect(&manager, &ControllerManager::controllerOffline, this,
		[this]()
		{
			_pImpl->handleControllerOffline();
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
	connect(&manager, &ControllerManager::streamInputCountersChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInputCounters const& counters)
		{
			_pImpl->handleStreamInputCountersChanged(entityID, streamIndex, counters);
		});
	connect(&manager, &ControllerManager::streamFormatChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const)
		{
			_pImpl->handleStreamFormatChanged(entityID, descriptorType, streamIndex);
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
		case Category::Streaming:
			return "Streaming";
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

QString EventJournal::levelToString(Level const level) noexcept
{
	switch (level)
	{
		case Level::User:
			return "User";
		case Level::Diagnostic:
			return "Diagnostic";
		default:
			return "Unknown";
	}
}

int EventJournal::Session::schemaVersion() const noexcept
{
	auto ok = false;
	auto const version = metadata.value("schema_version", "1").toInt(&ok);
	return ok ? version : 1;
}

bool EventJournal::Session::isNewerThanReader() const noexcept
{
	return schemaVersion() > CurrentSchemaVersion;
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
			// The level column only exists from schema version 2 on (a version 1 file only holds user level events)
			auto hasLevelColumn = false;
			if (query.exec("PRAGMA table_info(events)"))
			{
				while (query.next())
				{
					hasLevelColumn |= query.value(1).toString() == "level";
				}
			}
			auto const eventsQuery = hasLevelColumn ? QString{ "SELECT timestamp, category, severity, entity_id, entity_name, subject, summary, details, level FROM events ORDER BY timestamp, id" } : QString{ "SELECT timestamp, category, severity, entity_id, entity_name, subject, summary, details FROM events ORDER BY timestamp, id" };
			if (query.exec(eventsQuery))
			{
				success = true;
				while (query.next())
				{
					auto event = Event{};
					event.timestamp = query.value(0).toLongLong();
					event.category = categoryFromKey(query.value(1).toString()).value_or(Category::Session);
					event.severity = severityFromKey(query.value(2).toString()).value_or(Severity::Info);
					if (hasLevelColumn)
					{
						event.level = levelFromKey(query.value(8).toString()).value_or(Level::User);
					}
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

EventJournal::Level EventJournal::recordingLevel() const noexcept
{
	return _pImpl->_recordingLevel;
}

void EventJournal::setRecordingLevel(Level const level) noexcept
{
	_pImpl->setRecordingLevel(level);
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

void EventJournal::setMonotonicClock(std::function<std::chrono::steady_clock::time_point()> clock) noexcept
{
	_pImpl->_monotonicClock = clock ? std::move(clock) : std::function<std::chrono::steady_clock::time_point()>{ &steadyClockNow };
}

void EventJournal::clearCurrentSession() noexcept
{
	_pImpl->clearCurrentSession();
}

} // namespace modelsLibrary
} // namespace hive
