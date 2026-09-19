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

#include "avePatch.hpp"

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/internals/jsonTypes.hpp>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
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

/** Serialization flags of the entity snapshots: the whole entity (same content as a full entity export), in the .ave binary format. */
la::avdecc::entity::model::jsonSerializer::Flags snapshotSerializationFlags() noexcept
{
	return la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDiagnostics, la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat };
}

/** Builds the JSON representation of a gPTP AsPath (the clock identities from the grandmaster down to the entity). */
QJsonArray asPathToJson(la::avdecc::entity::model::AsPath const& asPath) noexcept
{
	auto array = QJsonArray{};
	for (auto const& clockIdentity : asPath.sequence)
	{
		array.push_back(helper::uniqueIdentifierToString(clockIdentity));
	}
	return array;
}

/** Opens a journal file for reading on the given connection, falling back to a read-write open (which performs WAL recovery) for sessions that were interrupted by a crash. */
bool openJournalForReading(QSqlDatabase& db, QString const& filePath) noexcept
{
	db.setDatabaseName(filePath);
	db.setConnectOptions("QSQLITE_OPEN_READONLY");
	auto isOpen = QFile::exists(filePath) && db.open();
	if (isOpen && !QSqlQuery{ db }.exec("SELECT count(*) FROM events"))
	{
		db.close();
		isOpen = false;
	}
	if (!isOpen)
	{
		db.setConnectOptions();
		isOpen = QFile::exists(filePath) && db.open();
	}
	return isOpen;
}

// Stream data loss rule (see docs/EventJournal.md): a bound Stream Input receiving data increments FRAMES_RX at least once per second and is notified at most once per second (Milan 1.2 tables 5.6 and 5.22), so between two notifications the increment is at least one second worth of frames unless frames were missing
constexpr auto StreamDataLearningWindow = std::size_t{ 30u }; // Number of increments kept per stream to learn the expected increment
constexpr auto StreamDataMinLearningSamples = std::size_t{ 10u }; // Number of learned increments required before a stream is evaluated
constexpr auto StreamDataReferencePercentile = std::size_t{ 10u }; // Percentile of the learned increments used as the expected increment (the low end is the one second worth reference, a device whose notification cadence varies produces larger increments, and a single undetected partial loss must not lower the reference)
constexpr auto StreamDataLossThreshold = 0.5; // An increment below this fraction of the expected increment means frames were missing during the interval
constexpr auto StreamDataMinEvaluationInterval = std::chrono::milliseconds{ 900 }; // Notifications closer than this to the previous one (queued burst, other interface of a redundant controller, non compliant device) are neither evaluated nor learned
constexpr auto StreamDataResyncFactor = std::uint64_t{ 10u }; // An increment above this multiple of the expected increment (counter reset by a rebind, value jump) only resynchronizes the reference sample

// Change tracking events (see docs/EventJournal.md): every dynamic model change is journaled at diagnostic level with a JSON Patch, so the analyser can rebuild the state of an entity at any time of the session from its last snapshot
constexpr auto ControlValuesRateLimitInterval = std::chrono::milliseconds{ 1000 }; // A control journals at most one value change per interval (faders and meters can be very chatty): the last value wins, the coalesced changes are counted
constexpr auto LivenessCounterNames = std::array<char const*, 3>{ "FRAMES_RX", "FRAMES_TX", "TIMESTAMP_VALID" }; // Counters moving on every notification of a streaming device (about 300 notifications per second on the field rig): a notification where only those moved is never journaled as a raw counters event, the FRAMES_RX increments feed the stream data loss rule instead

/** Informational counter (Milan 1.2 Tables 5.6 and 5.7): its serialized name (the la_avdecc JSON name, as in the raw counters events) and its display name. */
struct InformationalCounter
{
	char const* serializedName{ nullptr };
	char const* displayName{ nullptr };
};
// The informational counters get their own readable diagnostic event when they increase (they tell what a talker or a listener did, without being errors): the stream input ones and the stream output ones
constexpr auto StreamInputInformationalCounters = std::array<InformationalCounter, 2>{ { { "MEDIA_RESET", "Media Reset" }, { "TIMESTAMP_UNCERTAIN", "Timestamp Uncertain" } } };
constexpr auto StreamOutputInformationalCounters = std::array<InformationalCounter, 4>{ { { "STREAM_START", "Stream Start" }, { "STREAM_STOP", "Stream Stop" }, { "MEDIA_RESET", "Media Reset" }, { "TIMESTAMP_UNCERTAIN", "Timestamp Uncertain" } } };

/** Returns the display name of an informational counter of the given descriptor type, or nullptr if the counter is not one. */
char const* informationalCounterDisplayName(la::avdecc::entity::model::DescriptorType const descriptorType, std::string const& serializedName) noexcept
{
	auto const find = [&serializedName](auto const& counters) -> char const*
	{
		for (auto const& counter : counters)
		{
			if (serializedName == counter.serializedName)
			{
				return counter.displayName;
			}
		}
		return nullptr;
	};
	switch (descriptorType)
	{
		case la::avdecc::entity::model::DescriptorType::StreamInput:
			return find(StreamInputInformationalCounters);
		case la::avdecc::entity::model::DescriptorType::StreamOutput:
			return find(StreamOutputInformationalCounters);
		default:
			return nullptr;
	}
}

/** Converts a la_avdecc JSON value to its Qt counterpart (QJsonDocument only parses objects and arrays, so the value is wrapped in an array). */
QJsonValue toQJsonValue(json const& value) noexcept
{
	return QJsonDocument::fromJson(QByteArray::fromStdString(json::array({ value }).dump())).array().at(0);
}

/**
* @brief JSON Patch (RFC 6902) of a change tracking event: the operations applying the change to the AVE document of the entity (stored in the EventJournal::PatchDetailsKey details key).
* @details Only 'replace' operations are used: the la_avdecc serializer always writes the targeted keys (an unset optional is written as null), so a replay applies the same operation whatever the previous value. The values are serialized by la_avdecc itself, so they match the snapshots byte for byte.
*/
class Patch
{
public:
	/** Adds a 'replace' operation of the value at the given pointer (no operation if la_avdecc cannot serialize the value). */
	template<typename ValueType>
	void replace(QString const& pointer, ValueType const& value) noexcept
	{
		try
		{
			json const serialized = value; // Must use operator= instead of constructor to force usage of the to_json overload
			auto operation = QJsonObject{};
			operation["op"] = "replace";
			operation["path"] = pointer;
			operation["value"] = toQJsonValue(serialized);
			_operations.push_back(operation);
		}
		catch (...)
		{
			// Unsupported value (eg. stream output counters of an unknown type)
		}
	}

	/** Same as above for a pointer that may not have been resolved (no operation in that case, the event is journaled without a patch). */
	template<typename ValueType>
	void replace(std::optional<QString> const& pointer, ValueType const& value) noexcept
	{
		if (pointer)
		{
			replace(*pointer, value);
		}
	}

	/** Stores the operations in the details of an event (nothing when the patch is empty, so the analyser can tell a change it cannot replay). */
	void storeIn(QJsonObject& details) const noexcept
	{
		if (!_operations.isEmpty())
		{
			details[EventJournal::PatchDetailsKey] = _operations;
		}
	}

private:
	QJsonArray _operations{};
};

/** Serializes a set of counters (any descriptor type) the way the AVE document stores them, flattened to a "counter name to value" object (the stream output counters are nested under their counter type in the AVE document). Returns an empty object if la_avdecc cannot serialize them. */
template<typename CountersType>
json countersToJson(CountersType const& counters) noexcept
{
	try
	{
		json const serialized = counters; // Must use operator= instead of constructor to force usage of the to_json overload
		if (serialized.is_object() && serialized.contains("counters"))
		{
			return serialized.at("counters");
		}
		return serialized;
	}
	catch (...)
	{
		// Stream output counters of an unknown type
		return json::object();
	}
}

/** Names of the acquire states, as written by the la_avdecc controller serializer (avdeccControllerJsonTypes.hpp, not part of the public API). */
QString acquireStateToKey(la::avdecc::controller::model::AcquireState const state) noexcept
{
	using AcquireState = la::avdecc::controller::model::AcquireState;
	switch (state)
	{
		case AcquireState::NotSupported:
			return "NOT_SUPPORTED";
		case AcquireState::NotAcquired:
			return "NOT_ACQUIRED";
		case AcquireState::AcquireInProgress:
			return "ACQUIRE_IN_PROGRESS";
		case AcquireState::Acquired:
			return "ACQUIRED";
		case AcquireState::AcquiredByOther:
			return "ACQUIRED_BY_OTHER";
		case AcquireState::ReleaseInProgress:
			return "RELEASE_IN_PROGRESS";
		default:
			return "UNKNOWN";
	}
}

/** Names of the lock states, as written by the la_avdecc controller serializer. */
QString lockStateToKey(la::avdecc::controller::model::LockState const state) noexcept
{
	using LockState = la::avdecc::controller::model::LockState;
	switch (state)
	{
		case LockState::NotSupported:
			return "NOT_SUPPORTED";
		case LockState::NotLocked:
			return "NOT_LOCKED";
		case LockState::LockInProgress:
			return "LOCK_IN_PROGRESS";
		case LockState::Locked:
			return "LOCKED";
		case LockState::LockedByOther:
			return "LOCKED_BY_OTHER";
		case LockState::UnlockInProgress:
			return "UNLOCK_IN_PROGRESS";
		default:
			return "UNKNOWN";
	}
}

/** Names of the compatibility flags, as written by the la_avdecc controller serializer. */
QJsonArray compatibilityFlagsToJson(la::avdecc::controller::ControlledEntity::CompatibilityFlags const flags) noexcept
{
	using CompatibilityFlag = la::avdecc::controller::ControlledEntity::CompatibilityFlag;
	auto array = QJsonArray{};
	for (auto const flag : flags)
	{
		switch (flag)
		{
			case CompatibilityFlag::IEEE17221:
				array.push_back("IEEE17221");
				break;
			case CompatibilityFlag::Milan:
				array.push_back("MILAN");
				break;
			case CompatibilityFlag::IEEE17221Warning:
				array.push_back("IEEE17221WARNING");
				break;
			case CompatibilityFlag::MilanWarning:
				array.push_back("MILANWARNING");
				break;
			case CompatibilityFlag::Misbehaving:
				array.push_back("MISBEHAVING");
				break;
			default:
				array.push_back("UNKNOWN");
				break;
		}
	}
	return array;
}

/** Returns the display name of a descriptor type, used to build the subject of the change tracking events (eg. "Input Stream 2"). */
QString descriptorTypeName(la::avdecc::entity::model::DescriptorType const descriptorType) noexcept
{
	using DescriptorType = la::avdecc::entity::model::DescriptorType;
	switch (descriptorType)
	{
		case DescriptorType::Configuration:
			return "Configuration";
		case DescriptorType::AudioUnit:
			return "Audio Unit";
		case DescriptorType::StreamInput:
			return "Input Stream";
		case DescriptorType::StreamOutput:
			return "Output Stream";
		case DescriptorType::JackInput:
			return "Input Jack";
		case DescriptorType::JackOutput:
			return "Output Jack";
		case DescriptorType::AvbInterface:
			return "AVB Interface";
		case DescriptorType::ClockSource:
			return "Clock Source";
		case DescriptorType::MemoryObject:
			return "Memory Object";
		case DescriptorType::StreamPortInput:
			return "Input Stream Port";
		case DescriptorType::StreamPortOutput:
			return "Output Stream Port";
		case DescriptorType::AudioCluster:
			return "Audio Cluster";
		case DescriptorType::AudioMap:
			return "Audio Map";
		case DescriptorType::Control:
			return "Control";
		case DescriptorType::ClockDomain:
			return "Clock Domain";
		case DescriptorType::Timing:
			return "Timing";
		case DescriptorType::PtpInstance:
			return "PTP Instance";
		case DescriptorType::PtpPort:
			return "PTP Port";
		default:
			return QString::fromStdString(la::avdecc::entity::model::descriptorTypeToString(descriptorType));
	}
}

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
		case EventJournal::Category::Configuration:
			return "configuration";
		case EventJournal::Category::Control:
			return "control";
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
		{ "configuration", EventJournal::Category::Configuration },
		{ "control", EventJournal::Category::Control },
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

	/** Returns true when the diagnostic events are written: the change tracking handlers skip the patch computation (entity lookup, serialization) otherwise, their baselines are still tracked. */
	bool isDiagnosticRecording() const noexcept
	{
		return _recordingLevel == Level::Diagnostic;
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
		_insertSnapshotQuery = QSqlQuery{ _db };
		_insertSnapshotQuery.prepare("INSERT INTO snapshots (timestamp, entity_id, reason, format, data, uncompressed_size) VALUES (?, ?, ?, ?, ?, ?)");
		_snapshotsCount = 0u;
		_snapshotsBytes = 0u;
		_snapshotsSerializationMs = 0;

		setMetadata("schema_version", QString::number(EventJournal::CurrentSchemaVersion));
		setMetadata("hive_version", QCoreApplication::applicationVersion());
		setMetadata("la_avdecc_version", QString::fromStdString(la::avdecc::getVersion()));
		setMetadata("os", QSysInfo::prettyProductName());
		setMetadata("computer_name", helper::getComputerName());
		setMetadata("started_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
		setMetadata("recording_level_at_start", levelToKey(_recordingLevel));
		setMetadata("snapshot_format", EventJournal::SnapshotFormat);

		addEvent(Category::Session, Severity::Info, {}, {}, {}, "Recording started");
		emit _q->recordingStarted(_sessionFilePath);

		// Entities already online when the recording starts get a snapshot too, so the journal is self-contained whatever the startup order
		for (auto const entityID : onlineEntities())
		{
			writeSnapshot(entityID, "session_start");
		}
	}

	void stopSession() noexcept
	{
		if (isRecording())
		{
			flushPendingControlValues(true);
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
			_insertSnapshotQuery = QSqlQuery{};
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

	std::size_t snapshotAllEntities() noexcept
	{
		if (!isRecording())
		{
			return 0u;
		}
		auto count = std::size_t{ 0u };
		for (auto const entityID : onlineEntities())
		{
			if (writeSnapshot(entityID, "manual"))
			{
				++count;
			}
		}
		auto details = QJsonObject{};
		details["entities"] = static_cast<qint64>(count);
		addEvent(Category::Session, Severity::Info, {}, {}, {}, QString{ "Snapshot of %1 entities taken" }.arg(count), details);
		return count;
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
		auto details = QJsonObject{};
		auto& manager = ControllerManager::getInstance();
		if (auto controlledEntity = manager.getControlledEntity(entityID))
		{
			entityName = helper::smartEntityName(*controlledEntity);
			seedBaselines(entityID, *controlledEntity);
			details = entityDescription(*controlledEntity);
		}
		_entityNames[entityID] = entityName;
		upsertEntity(entityID, entityName);

		details["enumeration_time_ms"] = static_cast<qint64>(enumerationTime.count());
		// The snapshot is written once the entity guard above is released (the serializer takes its own)
		if (auto const snapshotID = writeSnapshot(entityID, "online"))
		{
			details["snapshot_id"] = *snapshotID;
		}
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
		_asPaths.erase(entityID);
		_avbInterfaceInfos.erase(entityID);
		_linkStates.erase(entityID);
		_gptpGmChangedStates.erase(entityID);
		_clockLockStates.erase(entityID);
		_streamLockStates.erase(entityID);
		_streamConnections.erase(entityID);
		_streamInputErrorCounters.erase(entityID);
		_streamDataStates.erase(entityID);
		_statisticsErrorCounters.erase(entityID);
		_redundancyWarnings.erase(entityID);
		_latencyErrors.erase(entityID);
		_lostRedundantInterfaces.erase(entityID);
		_unsolRegistrationStates.erase(entityID);
		_lostUnsolInterfaces.erase(entityID);
		_streamOutputConnections.erase(entityID);
		_streamFormats.erase(entityID);
		_streamDynamicInfos.erase(entityID);
		_streamRunningStates.erase(entityID);
		_samplingRates.erase(entityID);
		_clockSources.erase(entityID);
		_mediaClockReferenceInfos.erase(entityID);
		_acquireStates.erase(entityID);
		_lockStates.erase(entityID);
		_compatibilities.erase(entityID);
		_entityCapabilities.erase(entityID);
		_rawCounters.erase(entityID);
		_controlValuesStates.erase(entityID);
	}

	void handleEntityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityName) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		auto const previousName = entityNameFor(entityID);
		_entityNames[entityID] = entityName;
		upsertEntity(entityID, entityName);
		if (!isDiagnosticRecording())
		{
			return;
		}

		auto patch = Patch{};
		patch.replace(QString{ "%1/dynamic/entity_name" }.arg(avePatch::entityDescriptorPointer()), la::avdecc::entity::model::AvdeccFixedString{ entityName.toStdString() });
		auto details = QJsonObject{};
		details["previous_name"] = previousName;
		details["new_name"] = entityName;
		patch.storeIn(details);
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), {}, QString{ "Entity name changed to '%1'" }.arg(entityName), details, Level::Diagnostic);
	}

	void handleEntityGroupNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& groupName) noexcept
	{
		if (!isRecording() || !isDiagnosticRecording())
		{
			return;
		}
		auto patch = Patch{};
		patch.replace(QString{ "%1/dynamic/group_name" }.arg(avePatch::entityDescriptorPointer()), la::avdecc::entity::model::AvdeccFixedString{ groupName.toStdString() });
		auto details = QJsonObject{};
		details["new_name"] = groupName;
		patch.storeIn(details);
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), {}, QString{ "Entity group name changed to '%1'" }.arg(groupName), details, Level::Diagnostic);
	}

	/** Journals the name change of any descriptor (la_avdecc only notifies actual changes, so no value dedup is needed here). */
	void handleObjectNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, QString const& name) noexcept
	{
		if (!isRecording() || !isDiagnosticRecording())
		{
			return;
		}
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, descriptorType, descriptorIndex, "object_name", configurationIndex), la::avdecc::entity::model::AvdeccFixedString{ name.toStdString() });
		auto details = QJsonObject{};
		details["configuration_index"] = static_cast<int>(configurationIndex);
		details["new_name"] = name;
		patch.storeIn(details);
		auto const subject = descriptorType == la::avdecc::entity::model::DescriptorType::Configuration ? QString{ "Configuration %1" }.arg(descriptorIndex) : QString{ "%1 %2" }.arg(descriptorTypeName(descriptorType)).arg(descriptorIndex);
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), subject, QString{ "%1 name changed to '%2'" }.arg(descriptorTypeName(descriptorType), name), details, Level::Diagnostic);
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
		// The patch carries the new connection information, so the twin follows the listener side of the connections
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex, "connected_talker"), info.talkerStream);
		patch.replace(dynamicPointer(entityID, la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex, "connection_state"), info.state);
		// FastConnecting is a transient state, only report actual Connected/NotConnected transitions
		if (isConnected && (!wasConnected || !(previousInfo->talkerStream == info.talkerStream)))
		{
			auto details = QJsonObject{};
			details["talker_id"] = helper::uniqueIdentifierToString(info.talkerStream.entityID);
			details["talker_stream_index"] = static_cast<int>(info.talkerStream.streamIndex);
			patch.storeIn(details);
			addEvent(Category::Connection, Severity::Info, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), QString{ "Connected to talker %1 (output stream %2)" }.arg(entityNameFor(info.talkerStream.entityID)).arg(info.talkerStream.streamIndex), details);
		}
		else if (!isConnected && wasConnected && info.state == State::NotConnected)
		{
			auto details = QJsonObject{};
			details["talker_id"] = helper::uniqueIdentifierToString(previousInfo->talkerStream.entityID);
			details["talker_stream_index"] = static_cast<int>(previousInfo->talkerStream.streamIndex);
			patch.storeIn(details);
			addEvent(Category::Connection, Severity::Info, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), QString{ "Disconnected from talker %1 (output stream %2)" }.arg(entityNameFor(previousInfo->talkerStream.entityID)).arg(previousInfo->talkerStream.streamIndex), details);
		}
	}

	void handleStreamOutputConnectionsChanged(la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamConnections const& connections) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		auto const entityID = stream.entityID;
		if (!trackChange(_streamOutputConnections[entityID], stream.streamIndex, connections))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		// The talker side of the connections is not part of the AVE document (la_avdecc rebuilds it from the listener side when loading a network state), so this event carries no patch: it is the talker's view, useful when a listener is not in the journal
		auto listeners = QJsonArray{};
		for (auto const& listener : connections)
		{
			auto object = QJsonObject{};
			object["entity_id"] = helper::uniqueIdentifierToString(listener.entityID);
			object["stream_index"] = static_cast<int>(listener.streamIndex);
			listeners.push_back(object);
		}
		auto details = QJsonObject{};
		details["listeners"] = listeners;
		addEvent(Category::Connection, Severity::Info, entityID, entityNameFor(entityID), outputStreamSubject(entityID, stream.streamIndex), QString{ "Talker connections changed (%1 listeners)" }.arg(connections.size()), details, Level::Diagnostic);
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
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, la::avdecc::entity::model::DescriptorType::AvbInterface, avbInterfaceIndex, "gptp_grandmaster_id"), grandMasterID);
		patch.replace(dynamicPointer(entityID, la::avdecc::entity::model::DescriptorType::AvbInterface, avbInterfaceIndex, "gptp_domain_number"), grandMasterDomain);
		patch.storeIn(details);

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

	void handleAsPathChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AsPath const& asPath) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		auto& states = _asPaths[entityID];
		auto const it = states.find(avbInterfaceIndex);
		if (it == states.end())
		{
			// No baseline (entity without AsPath at enumeration time): silently store
			states[avbInterfaceIndex] = asPath;
			return;
		}
		if (it->second == asPath)
		{
			return;
		}
		auto const previous = it->second;
		it->second = asPath;

		// The AsPath is the rack and branch mapping of the entity: a change is a topology event, journaled for every reader
		auto details = QJsonObject{};
		details["previous_as_path"] = asPathToJson(previous);
		details["new_as_path"] = asPathToJson(asPath);
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, la::avdecc::entity::model::DescriptorType::AvbInterface, avbInterfaceIndex, "as_path"), asPath);
		patch.storeIn(details);
		addEvent(Category::Gptp, Severity::Info, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), "gPTP AsPath changed", details);
	}

	void handleAvbInterfaceInfoChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceInfo const& info) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		auto& states = _avbInterfaceInfos[entityID];
		auto const it = states.find(avbInterfaceIndex);
		if (it == states.end())
		{
			// No baseline (entity without AVB interface info at enumeration time): silently store
			states[avbInterfaceIndex] = info;
			return;
		}
		if (it->second == info)
		{
			return;
		}
		auto const previous = it->second;
		it->second = info;

		// The whole information (propagation delay, flags, MSRP mappings) is patched, the summary names what changed
		auto details = QJsonObject{};
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, la::avdecc::entity::model::DescriptorType::AvbInterface, avbInterfaceIndex, "avb_interface_info"), info);
		patch.storeIn(details);
		if (previous.propagationDelay != info.propagationDelay)
		{
			// The propagation delay of a link jitters by a few nanoseconds between measurements (5 to 16 ns on the field rig), so its changes are diagnostic material rather than user events
			details["previous_propagation_delay"] = static_cast<qint64>(previous.propagationDelay);
			details["new_propagation_delay"] = static_cast<qint64>(info.propagationDelay);
			addEvent(Category::Gptp, Severity::Info, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), "gPTP propagation delay changed", details, Level::Diagnostic);
		}
		else
		{
			addEvent(Category::Gptp, Severity::Info, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), "AVB interface info changed", details, Level::Diagnostic);
		}
	}

	void handleClockDomainCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockDomainCounters const& counters) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		journalRawCounters(entityID, la::avdecc::entity::model::DescriptorType::ClockDomain, clockDomainIndex, QString{ "Clock Domain %1" }.arg(clockDomainIndex), counters);

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
		auto const replay = replayCounterPairTransitions(it->second, *newState);
		it->second = *newState;
		if (replay.transitions.empty())
		{
			return;
		}

		// The clock source the domain follows at the time of the event (and the input stream and talker behind it), so the analyser can tell which stream a system wide unlock started from
		auto const sourceDetails = clockSourceDetails(entityID, clockDomainIndex);
		for (auto const& transition : replay.transitions)
		{
			auto details = sourceDetails;
			details["locked_counter"] = static_cast<qint64>(newState->activeCounter);
			details["unlocked_counter"] = static_cast<qint64>(newState->inactiveCounter);
			details["locked_increment"] = static_cast<qint64>(replay.activeIncrement);
			details["unlocked_increment"] = static_cast<qint64>(replay.inactiveIncrement);
			if (transition.isInferred)
			{
				details["inferred"] = true;
			}
			if (transition.isActive)
			{
				addEvent(Category::MediaClock, Severity::Recovered, entityID, entityNameFor(entityID), QString{ "Clock Domain %1" }.arg(clockDomainIndex), "Media clock locked", details);
			}
			else
			{
				addEvent(Category::MediaClock, Severity::Error, entityID, entityNameFor(entityID), QString{ "Clock Domain %1" }.arg(clockDomainIndex), "Media clock unlocked", details);
			}
		}
	}

	void handleStreamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat) noexcept
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
		if (!trackChange(_streamFormats[entityID], StreamKey{ descriptorType, streamIndex }, streamFormat))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		auto details = QJsonObject{};
		details["stream_format"] = helper::toHexQString(streamFormat.getValue(), true, true);
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, descriptorType, streamIndex, "stream_format"), streamFormat);
		patch.storeIn(details);
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), streamSubject(entityID, descriptorType, streamIndex), QString{ "Stream format changed to %1" }.arg(details["stream_format"].toString()), details, Level::Diagnostic);
	}

	void handleStreamDynamicInfoChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamDynamicInfo const& info) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		if (!trackChange(_streamDynamicInfos[entityID], StreamKey{ descriptorType, streamIndex }, info))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		auto details = QJsonObject{};
		details["stream_id"] = info.streamID ? QJsonValue{ helper::uniqueIdentifierToString(*info.streamID) } : QJsonValue{};
		details["msrp_accumulated_latency"] = info.msrpAccumulatedLatency ? QJsonValue{ static_cast<qint64>(*info.msrpAccumulatedLatency) } : QJsonValue{};
		details["stream_vlan_id"] = info.streamVlanID ? QJsonValue{ static_cast<int>(*info.streamVlanID) } : QJsonValue{};
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, descriptorType, streamIndex, "stream_dynamic_info"), info);
		patch.storeIn(details);
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), streamSubject(entityID, descriptorType, streamIndex), "Stream dynamic info changed", details, Level::Diagnostic);
	}

	void handleStreamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		if (!trackChange(_streamRunningStates[entityID], StreamKey{ descriptorType, streamIndex }, isRunning))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		auto details = QJsonObject{};
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, descriptorType, streamIndex, "stream_running"), isRunning);
		patch.storeIn(details);
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), streamSubject(entityID, descriptorType, streamIndex), isRunning ? "Stream started" : "Stream stopped", details, Level::Diagnostic);
	}

	void handleAudioUnitSamplingRateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		if (!trackChange(_samplingRates[entityID], audioUnitIndex, samplingRate))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		auto details = QJsonObject{};
		auto const nominalSampleRate = static_cast<qint64>(samplingRate.getNominalSampleRate());
		details["sampling_rate"] = nominalSampleRate;
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, la::avdecc::entity::model::DescriptorType::AudioUnit, audioUnitIndex, "current_sampling_rate"), samplingRate);
		patch.storeIn(details);
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), QString{ "Audio Unit %1" }.arg(audioUnitIndex), QString{ "Sampling rate changed to %1 Hz" }.arg(nominalSampleRate), details, Level::Diagnostic);
	}

	void handleClockSourceChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		if (!trackChange(_clockSources[entityID], clockDomainIndex, clockSourceIndex))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		auto details = QJsonObject{};
		details["clock_source_index"] = static_cast<int>(clockSourceIndex);
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, la::avdecc::entity::model::DescriptorType::ClockDomain, clockDomainIndex, "clock_source_index"), clockSourceIndex);
		patch.storeIn(details);
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), QString{ "Clock Domain %1" }.arg(clockDomainIndex), QString{ "Clock source changed to %1" }.arg(clockSourceSubject(entityID, clockSourceIndex)), details, Level::Diagnostic);
	}

	void handleMediaClockReferenceInfoChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::MediaClockReferenceInfo const& info) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		if (!trackChange(_mediaClockReferenceInfos[entityID], clockDomainIndex, info))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		auto details = QJsonObject{};
		if (info.userMediaClockPriority)
		{
			details["user_media_clock_priority"] = static_cast<int>(*info.userMediaClockPriority);
		}
		if (info.mediaClockDomainName)
		{
			details["media_clock_domain_name"] = QString::fromStdString(info.mediaClockDomainName->str());
		}
		auto patch = Patch{};
		patch.replace(dynamicPointer(entityID, la::avdecc::entity::model::DescriptorType::ClockDomain, clockDomainIndex, "media_clock_reference_info"), info);
		patch.storeIn(details);
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), QString{ "Clock Domain %1" }.arg(clockDomainIndex), "Media clock reference changed", details, Level::Diagnostic);
	}

	void handleStreamPortAudioMappingsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex) noexcept
	{
		if (!isRecording() || !isDiagnosticRecording())
		{
			return;
		}
		// The signal does not carry the mappings: they are read from the entity model (already updated when the queued signal reaches the main thread)
		auto mappings = std::optional<la::avdecc::entity::model::AudioMappings>{};
		auto pointer = std::optional<QString>{};
		if (auto controlledEntity = ControllerManager::getInstance().getControlledEntity(entityID))
		{
			try
			{
				auto const configurationIndex = controlledEntity->getCurrentConfigurationIndex();
				auto const& streamPortNode = descriptorType == la::avdecc::entity::model::DescriptorType::StreamPortInput ? controlledEntity->getStreamPortInputNode(configurationIndex, streamPortIndex) : controlledEntity->getStreamPortOutputNode(configurationIndex, streamPortIndex);
				mappings = streamPortNode.dynamicModel.dynamicAudioMap;
				pointer = avePatch::dynamicModelPointer(*controlledEntity, configurationIndex, descriptorType, streamPortIndex, "dynamic_mappings");
			}
			catch (...)
			{
				// Entity without a current configuration, or unknown stream port
			}
		}
		if (!mappings)
		{
			return;
		}
		auto details = QJsonObject{};
		details["mappings"] = static_cast<qint64>(mappings->size());
		auto patch = Patch{};
		patch.replace(pointer, *mappings);
		patch.storeIn(details);
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), QString{ "%1 %2" }.arg(descriptorTypeName(descriptorType)).arg(streamPortIndex), QString{ "Audio mappings changed (%1 mappings)" }.arg(mappings->size()), details, Level::Diagnostic);
	}

	void handleControlValuesChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlValues const& controlValues) noexcept
	{
		if (!isRecording() || !isDiagnosticRecording())
		{
			return;
		}
		// Serialized the way the AVE document stores the values, through the dynamic model serializer (which dispatches on the value type)
		auto dynamicModel = la::avdecc::entity::model::ControlNodeDynamicModel{};
		dynamicModel.values = controlValues;
		auto values = json{};
		try
		{
			json const serialized = dynamicModel; // Must use operator= instead of constructor to force usage of the to_json overload
			values = serialized.at("values");
		}
		catch (...)
		{
			// Unsupported control value type
			return;
		}

		auto& state = _controlValuesStates[entityID][controlIndex];
		auto const now = _monotonicClock();
		if (state.hasJournaled && state.journaledValues == values && !state.hasPending)
		{
			return;
		}
		// Rate limiting: a control journals at most one change per interval, the following ones are kept pending (last value wins) and flushed by the timer
		if (state.hasJournaled && now - state.journaledAt < ControlValuesRateLimitInterval)
		{
			if (state.hasPending && state.pendingValues == values)
			{
				return;
			}
			state.hasPending = true;
			state.pendingValues = std::move(values);
			++state.coalesced;
			if (!_controlValuesFlushTimer.isActive())
			{
				_controlValuesFlushTimer.start(static_cast<int>(ControlValuesRateLimitInterval.count()));
			}
			return;
		}
		journalControlValues(entityID, controlIndex, state, std::move(values), now);
	}

	/** Journals the pending control values whose rate limiting interval elapsed (all of them when forced, eg. at the end of the session), and re-arms the timer if some remain. */
	void flushPendingControlValues(bool const force) noexcept
	{
		auto const now = _monotonicClock();
		auto remaining = false;
		for (auto& [entityID, states] : _controlValuesStates)
		{
			for (auto& [controlIndex, state] : states)
			{
				if (!state.hasPending)
				{
					continue;
				}
				if (force || now - state.journaledAt >= ControlValuesRateLimitInterval)
				{
					journalControlValues(entityID, controlIndex, state, std::move(state.pendingValues), now);
				}
				else
				{
					remaining = true;
				}
			}
		}
		if (remaining && isRecording())
		{
			_controlValuesFlushTimer.start(static_cast<int>(ControlValuesRateLimitInterval.count()));
		}
	}

	void handleAcquireStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		using AcquireState = la::avdecc::controller::model::AcquireState;
		// The in progress states are our own commands in flight: only the resulting states are journaled
		if (acquireState == AcquireState::AcquireInProgress || acquireState == AcquireState::ReleaseInProgress)
		{
			return;
		}
		if (!trackChange(_acquireStates, entityID, std::make_pair(acquireState, owningEntity)))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		auto details = QJsonObject{};
		details["acquire_state"] = acquireStateToKey(acquireState);
		details["owning_controller_id"] = helper::uniqueIdentifierToString(owningEntity);
		auto patch = Patch{};
		patch.replace(QString{ "%1/acquire_state" }.arg(avePatch::statePointer()), acquireStateToKey(acquireState).toStdString());
		patch.replace(QString{ "%1/owning_controller_id" }.arg(avePatch::statePointer()), owningEntity);
		patch.storeIn(details);
		auto summary = QString{};
		switch (acquireState)
		{
			case AcquireState::Acquired:
				summary = "Entity acquired by this controller";
				break;
			case AcquireState::AcquiredByOther:
				summary = QString{ "Entity acquired by another controller (%1)" }.arg(entityNameFor(owningEntity));
				break;
			case AcquireState::NotAcquired:
				summary = "Entity released";
				break;
			default:
				summary = QString{ "Entity acquire state changed to %1" }.arg(acquireStateToKey(acquireState));
				break;
		}
		addEvent(Category::Control, Severity::Info, entityID, entityNameFor(entityID), {}, summary, details, Level::Diagnostic);
	}

	void handleLockStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		using LockState = la::avdecc::controller::model::LockState;
		if (lockState == LockState::LockInProgress || lockState == LockState::UnlockInProgress)
		{
			return;
		}
		if (!trackChange(_lockStates, entityID, std::make_pair(lockState, lockingEntity)))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		auto details = QJsonObject{};
		details["lock_state"] = lockStateToKey(lockState);
		details["locking_controller_id"] = helper::uniqueIdentifierToString(lockingEntity);
		auto patch = Patch{};
		patch.replace(QString{ "%1/lock_state" }.arg(avePatch::statePointer()), lockStateToKey(lockState).toStdString());
		patch.replace(QString{ "%1/locking_controller_id" }.arg(avePatch::statePointer()), lockingEntity);
		patch.storeIn(details);
		auto summary = QString{};
		switch (lockState)
		{
			case LockState::Locked:
				summary = "Entity locked by this controller";
				break;
			case LockState::LockedByOther:
				summary = QString{ "Entity locked by another controller (%1)" }.arg(entityNameFor(lockingEntity));
				break;
			case LockState::NotLocked:
				summary = "Entity unlocked";
				break;
			default:
				summary = QString{ "Entity lock state changed to %1" }.arg(lockStateToKey(lockState));
				break;
		}
		addEvent(Category::Control, Severity::Info, entityID, entityNameFor(entityID), {}, summary, details, Level::Diagnostic);
	}

	void handleCompatibilityChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags, la::avdecc::entity::model::MilanVersion const& milanVersion) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		if (!trackChange(_compatibilities, entityID, std::make_pair(compatibilityFlags, milanVersion)))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		auto const flagsJson = compatibilityFlagsToJson(compatibilityFlags);
		auto const versionString = QString::fromStdString(static_cast<std::string>(milanVersion));
		auto details = QJsonObject{};
		details["compatibility_flags"] = flagsJson;
		details["milan_version"] = versionString;
		// The compatibility flags are serialized by the controller's private serializer: same names, rebuilt here
		auto patchOperations = QJsonArray{};
		{
			auto operation = QJsonObject{};
			operation["op"] = "replace";
			operation["path"] = avePatch::compatibilityFlagsPointer();
			operation["value"] = flagsJson;
			patchOperations.push_back(operation);
		}
		{
			auto operation = QJsonObject{};
			operation["op"] = "replace";
			operation["path"] = avePatch::milanCompatibilityVersionPointer();
			operation["value"] = versionString;
			patchOperations.push_back(operation);
		}
		details[EventJournal::PatchDetailsKey] = patchOperations;
		auto flagNames = QStringList{};
		for (auto const& value : flagsJson)
		{
			flagNames.push_back(value.toString());
		}
		auto summary = QString{ "Compatibility changed to %1" }.arg(flagNames.isEmpty() ? QString{ "none" } : flagNames.join(", "));
		if (compatibilityFlags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
		{
			summary += QString{ " (Milan %1)" }.arg(versionString);
		}
		addEvent(Category::Entity, Severity::Info, entityID, entityNameFor(entityID), {}, summary, details, Level::Diagnostic);
	}

	void handleEntityCapabilitiesChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::EntityCapabilities const entityCapabilities) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		if (!trackChange(_entityCapabilities, entityID, entityCapabilities))
		{
			return;
		}
		if (!isDiagnosticRecording())
		{
			return;
		}
		json const serializedCapabilities = entityCapabilities; // Must use operator= instead of constructor to force usage of the to_json overload
		auto details = QJsonObject{};
		details["entity_capabilities"] = toQJsonValue(serializedCapabilities);
		auto patch = Patch{};
		patch.replace(avePatch::entityCapabilitiesPointer(), entityCapabilities);
		patch.storeIn(details);
		addEvent(Category::Entity, Severity::Info, entityID, entityNameFor(entityID), {}, "Entity capabilities changed", details, Level::Diagnostic);
	}

	void handleStreamInputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInputCounters const& counters) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		journalRawCounters(entityID, la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex, inputStreamSubject(entityID, streamIndex), counters);
		trackStreamInputLock(entityID, streamIndex, counters);

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

	/**
	* @brief Media clock lock tracking of a stream input from its MEDIA_LOCKED / MEDIA_UNLOCKED counters.
	* @details Same rule as the clock domains (derived state, transitions replayed from the increments, inferred flags), per stream: 'Media unlocked' is an error for a connected stream only (a stream being disconnected unlocks, that is not an error), closed by the 'Media locked' recovery of the same stream.
	*          A lock closing no journaled unlock (the stream was just connected, or its talker just started streaming) is diagnostic information rather than a recovery.
	*/
	void trackStreamInputLock(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInputCounters const& counters) noexcept
	{
		auto const newState = makeStreamLockState(counters);
		if (!newState)
		{
			return;
		}

		auto& states = _streamLockStates[entityID];
		auto const it = states.find(streamIndex);
		if (it == states.end())
		{
			// No baseline (entity without counters at enumeration time): silently store
			states[streamIndex] = StreamLockState{ *newState, false };
			return;
		}
		auto& state = it->second;
		auto const replay = replayCounterPairTransitions(state.lock, *newState);
		state.lock = *newState;
		if (replay.transitions.empty())
		{
			return;
		}

		// The connection state is read live from the entity model (which la_avdecc updates before the queued notification reaches the main thread), the same source as the stream data loss rule and the error counters of the UI
		auto const connectionInfo = liveStreamInputConnectionInfo(entityID, streamIndex);
		auto const isConnected = connectionInfo && connectionInfo->state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected;
		for (auto const& transition : replay.transitions)
		{
			auto details = QJsonObject{};
			details["media_locked_counter"] = static_cast<qint64>(newState->activeCounter);
			details["media_unlocked_counter"] = static_cast<qint64>(newState->inactiveCounter);
			details["media_locked_increment"] = static_cast<qint64>(replay.activeIncrement);
			details["media_unlocked_increment"] = static_cast<qint64>(replay.inactiveIncrement);
			if (transition.isInferred)
			{
				details["inferred"] = true;
			}
			if (isConnected)
			{
				details["talker_id"] = helper::uniqueIdentifierToString(connectionInfo->talkerStream.entityID);
				details["talker_stream_index"] = static_cast<int>(connectionInfo->talkerStream.streamIndex);
			}
			if (!transition.isActive)
			{
				if (!isConnected)
				{
					continue;
				}
				state.hasOpenUnlock = true;
				addEvent(Category::Counters, Severity::Error, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), "Media unlocked", details);
			}
			else if (state.hasOpenUnlock)
			{
				state.hasOpenUnlock = false;
				addEvent(Category::Counters, Severity::Recovered, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), "Media locked", details);
			}
			else
			{
				addEvent(Category::Counters, Severity::Info, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), "Media locked", details, Level::Diagnostic);
			}
		}
	}

	void handleEntityCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::EntityCounters const& counters) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		journalRawCounters(entityID, la::avdecc::entity::model::DescriptorType::Entity, 0u, {}, counters);
	}

	void handleAvbInterfaceCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceCounters const& counters) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		journalRawCounters(entityID, la::avdecc::entity::model::DescriptorType::AvbInterface, avbInterfaceIndex, QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), counters);
		trackLinkState(entityID, avbInterfaceIndex, counters);
		trackGptpGmChanged(entityID, avbInterfaceIndex, counters);
	}

	/**
	* @brief Link tracking of an AVB interface from its LINK_UP / LINK_DOWN counters.
	* @details la_avdecc derives the link status it notifies from the same counters, so the journal tracks them directly: same rule as the clock domains (derived state, transitions replayed from the increments, inferred flags), so a link that went down and came back between two notifications is journaled as an inferred 'Link down' / 'Link up' pair instead of being lost.
	*/
	void trackLinkState(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceCounters const& counters) noexcept
	{
		auto const newState = makeLinkState(counters);
		if (!newState)
		{
			return;
		}

		auto& states = _linkStates[entityID];
		auto const it = states.find(avbInterfaceIndex);
		if (it == states.end())
		{
			// No baseline (entity without counters at enumeration time): silently store
			states[avbInterfaceIndex] = *newState;
			return;
		}
		auto const replay = replayCounterPairTransitions(it->second, *newState);
		it->second = *newState;

		for (auto const& transition : replay.transitions)
		{
			auto details = QJsonObject{};
			details["link_up_counter"] = static_cast<qint64>(newState->activeCounter);
			details["link_down_counter"] = static_cast<qint64>(newState->inactiveCounter);
			details["link_up_increment"] = static_cast<qint64>(replay.activeIncrement);
			details["link_down_increment"] = static_cast<qint64>(replay.inactiveIncrement);
			if (transition.isInferred)
			{
				details["inferred"] = true;
			}
			if (transition.isActive)
			{
				addEvent(Category::Link, Severity::Recovered, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), "Link up", details);
			}
			else
			{
				addEvent(Category::Link, Severity::Error, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), "Link down", details);
			}
		}
	}

	/**
	* @brief GPTP_GM_CHANGED tracking of an AVB interface: every increase is journaled with the raw values and the grandmaster known before and after.
	* @details A grandmaster change the controller observed is already journaled from the AVB interface information (handleGptpChanged), this event adds the count: an increase while the grandmaster is the same before and after (an even increment, typically) means the grandmaster changed and came back between two notifications.
	*          The counters and the AVB interface information are notified separately (in no defined order), so the journal states the facts and leaves the flip and back conclusion to the analysis.
	*/
	void trackGptpGmChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceCounters const& counters) noexcept
	{
		auto const counterIt = counters.find(la::avdecc::entity::AvbInterfaceCounterValidFlag::GptpGmChanged);
		if (counterIt == counters.end())
		{
			return;
		}
		auto const value = counterIt->second;
		auto grandmasterID = std::optional<la::avdecc::UniqueIdentifier>{};
		if (auto const entityIt = _gptpStates.find(entityID); entityIt != _gptpStates.end())
		{
			if (auto const interfaceIt = entityIt->second.find(avbInterfaceIndex); interfaceIt != entityIt->second.end())
			{
				grandmasterID = interfaceIt->second.first;
			}
		}

		auto& states = _gptpGmChangedStates[entityID];
		auto const it = states.find(avbInterfaceIndex);
		if (it == states.end())
		{
			// No baseline (entity without counters at enumeration time): silently store
			states[avbInterfaceIndex] = GptpGmChangedState{ value, grandmasterID };
			return;
		}
		auto const previousState = it->second;
		it->second = GptpGmChangedState{ value, grandmasterID };
		// The same value again is a duplicate notification, a lower value a counters reset: neither is an increase
		if (value <= previousState.counter)
		{
			return;
		}

		auto details = QJsonObject{};
		details["counter"] = "GPTP GM Changed";
		details["previous_value"] = static_cast<qint64>(previousState.counter);
		details["value"] = static_cast<qint64>(value);
		details["increment"] = static_cast<qint64>(value - previousState.counter);
		if (previousState.grandmasterID)
		{
			details["previous_grandmaster_id"] = helper::uniqueIdentifierToString(*previousState.grandmasterID);
		}
		if (grandmasterID)
		{
			details["grandmaster_id"] = helper::uniqueIdentifierToString(*grandmasterID);
		}
		addEvent(Category::Gptp, Severity::Info, entityID, entityNameFor(entityID), QString{ "AVB Interface %1" }.arg(avbInterfaceIndex), QString{ "'GPTP GM Changed' counter increased to %1" }.arg(value), details);
	}

	void handleStreamOutputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamOutputCounters const& counters) noexcept
	{
		if (!isRecording())
		{
			return;
		}
		journalRawCounters(entityID, la::avdecc::entity::model::DescriptorType::StreamOutput, streamIndex, outputStreamSubject(entityID, streamIndex), counters);
	}

	void handleStreamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, ControllerManager::StreamInputErrorCounters const& errorCounters) noexcept
	{
		if (!isRecording())
		{
			return;
		}

		// The MEDIA_UNLOCKED increases of a stream whose lock state is tracked from its counters are journaled by the stream lock rule (trackStreamInputLock), as 'Media unlocked' errors closed by the 'Media locked' recoveries
		auto const isLockTracked = [this, entityID, descriptorIndex]()
		{
			auto const entityIt = _streamLockStates.find(entityID);
			return entityIt != _streamLockStates.end() && entityIt->second.count(descriptorIndex) > 0u;
		}();

		auto& previousCounters = _streamInputErrorCounters[entityID][descriptorIndex];
		for (auto const& [flag, value] : errorCounters)
		{
			if (flag == la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked && isLockTracked)
			{
				continue;
			}
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

		auto details = QJsonObject{};
		auto patch = Patch{};
		patch.replace(QString{ "%1/redundancy_warning" }.arg(avePatch::diagnosticsPointer()), isRedundancyWarning);
		patch.storeIn(details);
		if (isRedundancyWarning && !previousWarning)
		{
			addEvent(Category::Redundancy, Severity::Error, entityID, entityNameFor(entityID), {}, "Redundancy warning: redundant interfaces appear to be connected to the same network", details);
		}
		else if (!isRedundancyWarning && previousWarning)
		{
			addEvent(Category::Redundancy, Severity::Recovered, entityID, entityNameFor(entityID), {}, "Redundancy warning cleared", details);
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

		// The AVE document stores the set of streams in error: patched from the entity diagnostics (already updated when the queued signal reaches the main thread)
		auto details = QJsonObject{};
		if (auto controlledEntity = ControllerManager::getInstance().getControlledEntity(entityID))
		{
			auto patch = Patch{};
			patch.replace(QString{ "%1/stream_input_latency_errors" }.arg(avePatch::diagnosticsPointer()), controlledEntity->getDiagnostics().streamInputOverLatency);
			patch.storeIn(details);
		}
		if (isLatencyError && !previousError)
		{
			addEvent(Category::Latency, Severity::Error, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), "Stream latency error detected", details);
		}
		else if (!isLatencyError && previousError)
		{
			addEvent(Category::Latency, Severity::Recovered, entityID, entityNameFor(entityID), inputStreamSubject(entityID, streamIndex), "Stream latency error cleared", details);
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

		auto details = QJsonObject{};
		auto patch = Patch{};
		patch.replace(QString{ "%1/subscribed_unsol" }.arg(avePatch::statePointer()), isSubscribed);
		patch.storeIn(details);
		if (!isSubscribed)
		{
			details["triggered_by_entity"] = triggeredByEntity;
			auto const summary = ControllerManager::getInstance().isRedundantController() ? QString{ "No longer subscribed to unsolicited notifications on any interface (entity updates may be missed, consider refreshing the entity)" } : QString{ "No longer subscribed to unsolicited notifications (entity updates may be missed, consider refreshing the entity)" };
			addEvent(Category::Entity, Severity::Error, entityID, entityNameFor(entityID), {}, summary, details);
		}
		else
		{
			addEvent(Category::Entity, Severity::Recovered, entityID, entityNameFor(entityID), {}, "Subscribed to unsolicited notifications again", details);
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

	/** Returns a designation for the given output stream, including its name if resolvable. */
	QString outputStreamSubject(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept
	{
		auto subject = QString{ "Output Stream %1" }.arg(streamIndex);
		auto& manager = ControllerManager::getInstance();
		if (auto controlledEntity = manager.getControlledEntity(entityID))
		{
			auto const name = helper::outputStreamName(*controlledEntity, streamIndex);
			if (!name.isEmpty())
			{
				subject += QString{ " (%1)" }.arg(name);
			}
		}
		return subject;
	}

	/** Returns a designation for the given stream (input or output). */
	QString streamSubject(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept
	{
		return descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput ? inputStreamSubject(entityID, streamIndex) : outputStreamSubject(entityID, streamIndex);
	}

	/** Returns a designation for the given clock source, including its name if resolvable. */
	QString clockSourceSubject(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex) const noexcept
	{
		auto subject = QString{ "Clock Source %1" }.arg(clockSourceIndex);
		auto& manager = ControllerManager::getInstance();
		if (auto controlledEntity = manager.getControlledEntity(entityID))
		{
			try
			{
				auto const configurationIndex = controlledEntity->getCurrentConfigurationIndex();
				auto const name = helper::objectName(controlledEntity.get(), configurationIndex, controlledEntity->getClockSourceNode(configurationIndex, clockSourceIndex));
				if (!name.isEmpty())
				{
					subject += QString{ " (%1)" }.arg(name);
				}
			}
			catch (...)
			{
				// Entity without a current configuration, or unknown clock source
			}
		}
		return subject;
	}

	/** Builds the details describing the clock source a clock domain currently follows (index and type, then the input stream and its talker when the source is an input stream), empty if the entity is offline or the model incomplete. */
	static QJsonObject clockSourceDetails(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex) noexcept
	{
		auto details = QJsonObject{};
		auto& manager = ControllerManager::getInstance();
		if (auto controlledEntity = manager.getControlledEntity(entityID))
		{
			try
			{
				auto const& configurationNode = controlledEntity->getCurrentConfigurationNode();
				auto const clockSourceIndex = configurationNode.clockDomains.at(clockDomainIndex).dynamicModel.clockSourceIndex;
				details["clock_source_index"] = static_cast<int>(clockSourceIndex);
				auto const& clockSourceStaticModel = configurationNode.clockSources.at(clockSourceIndex).staticModel;
				json const serializedType = clockSourceStaticModel.clockSourceType; // Must use operator= instead of constructor to force usage of the to_json overload
				details["clock_source_type"] = toQJsonValue(serializedType);
				if (clockSourceStaticModel.clockSourceLocationType == la::avdecc::entity::model::DescriptorType::StreamInput)
				{
					details["clock_source_stream_index"] = static_cast<int>(clockSourceStaticModel.clockSourceLocationIndex);
					auto const& connectionInfo = configurationNode.streamInputs.at(clockSourceStaticModel.clockSourceLocationIndex).dynamicModel.connectionInfo;
					if (connectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected)
					{
						details["clock_source_talker_id"] = helper::uniqueIdentifierToString(connectionInfo.talkerStream.entityID);
						details["clock_source_talker_stream_index"] = static_cast<int>(connectionInfo.talkerStream.streamIndex);
					}
				}
			}
			catch (...)
			{
				// Entity without a current configuration, or clock source not in the model: the details stay partial
			}
		}
		return details;
	}

	/** Returns the pointer of a key of the dynamic model of a descriptor of the given entity, in the given configuration (the current one by default), or std::nullopt if the entity is offline or the descriptor unknown. */
	static std::optional<QString> dynamicPointer(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, char const* const key, std::optional<la::avdecc::entity::model::ConfigurationIndex> const configurationIndex = std::nullopt) noexcept
	{
		if (auto controlledEntity = ControllerManager::getInstance().getControlledEntity(entityID))
		{
			try
			{
				return avePatch::dynamicModelPointer(*controlledEntity, configurationIndex ? *configurationIndex : controlledEntity->getCurrentConfigurationIndex(), descriptorType, descriptorIndex, key);
			}
			catch (...)
			{
				// Entity without a current configuration
			}
		}
		return std::nullopt;
	}

	/** Stores the new value of a tracked item and returns true when it differs from the previous one (or when there was none): the value dedup that makes the change tracking immune to the duplicate notifications a redundant controller receives (one per interface). */
	template<typename MapType, typename KeyType, typename ValueType>
	static bool trackChange(MapType& map, KeyType const& key, ValueType const& value) noexcept
	{
		auto const it = map.find(key);
		if (it != map.end() && it->second == value)
		{
			return false;
		}
		map.insert_or_assign(key, value);
		return true;
	}

	/** Returns the value of a serialized counter, or std::nullopt if the JSON value is not an unsigned number. */
	static std::optional<std::uint64_t> counterValue(json const& value) noexcept
	{
		if (value.is_number_unsigned())
		{
			return value.get<std::uint64_t>();
		}
		return std::nullopt;
	}

	/** Increase of an informational counter found while journaling a raw counters notification (see journalRawCounters). */
	struct InformationalCounterIncrease
	{
		char const* displayName{ nullptr };
		std::uint64_t previousValue{ 0u };
		std::uint64_t value{ 0u };
	};

	/** Journals a raw counters notification at diagnostic level: the values that changed and a patch of the whole set, skipping the notifications that changed nothing (duplicates) or only the liveness counters (FRAMES_RX and the like, see LivenessCounterNames). The informational counters that increased (StreamInputInformationalCounters, StreamOutputInformationalCounters) get an additional readable event each. */
	template<typename CountersType>
	void journalRawCounters(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, QString const& subject, CountersType const& counters) noexcept
	{
		// Nothing to compute unless the event would be written
		if (!isDiagnosticRecording())
		{
			return;
		}
		auto const values = countersToJson(counters);
		auto& previousValues = _rawCounters[entityID][CounterKey{ descriptorType, descriptorIndex }];
		auto changed = QJsonArray{};
		auto onlyLiveness = true;
		auto increases = std::vector<InformationalCounterIncrease>{};
		for (auto const& [name, value] : values.items())
		{
			auto const previousIt = previousValues.find(name);
			if (previousIt != previousValues.end() && *previousIt == value)
			{
				continue;
			}
			auto object = QJsonObject{};
			object["counter"] = QString::fromStdString(name);
			object["previous_value"] = previousIt != previousValues.end() ? toQJsonValue(*previousIt) : QJsonValue{};
			object["value"] = toQJsonValue(value);
			changed.push_back(object);
			onlyLiveness &= std::find(std::begin(LivenessCounterNames), std::end(LivenessCounterNames), name) != std::end(LivenessCounterNames);
			// Only an increase over a known value is informative (a counter going backwards means the device rebooted or its counters were reset)
			if (auto const* const displayName = informationalCounterDisplayName(descriptorType, name))
			{
				auto const previousValue = previousIt != previousValues.end() ? counterValue(*previousIt) : std::nullopt;
				auto const newValue = counterValue(value);
				if (previousValue && newValue && *newValue > *previousValue)
				{
					increases.push_back(InformationalCounterIncrease{ displayName, *previousValue, *newValue });
				}
			}
		}
		// The liveness counters are tracked too, so the first informative change after a long streaming period reports their previous values correctly
		previousValues = values;
		if (changed.isEmpty() || onlyLiveness)
		{
			return;
		}
		auto details = QJsonObject{};
		details["changed"] = changed;
		auto patch = Patch{};
		patch.replace(descriptorType == la::avdecc::entity::model::DescriptorType::Entity ? std::optional<QString>{ QString{ "%1/dynamic/counters" }.arg(avePatch::entityDescriptorPointer()) } : dynamicPointer(entityID, descriptorType, descriptorIndex, "counters"), counters);
		patch.storeIn(details);
		addEvent(Category::Counters, Severity::Info, entityID, entityNameFor(entityID), subject, "Counters updated", details, Level::Diagnostic);

		// The readable events of the informational counters (the raw event above already carries the whole change, these are the ones to search for)
		for (auto const& increase : increases)
		{
			auto increaseDetails = QJsonObject{};
			increaseDetails["counter"] = increase.displayName;
			increaseDetails["previous_value"] = static_cast<qint64>(increase.previousValue);
			increaseDetails["value"] = static_cast<qint64>(increase.value);
			increaseDetails["increment"] = static_cast<qint64>(increase.value - increase.previousValue);
			addEvent(Category::Counters, Severity::Info, entityID, entityNameFor(entityID), subject, QString{ "'%1' counter increased to %2" }.arg(increase.displayName).arg(increase.value), increaseDetails, Level::Diagnostic);
		}
	}

	/** Control values rate limiting state of a control: the last journaled values and time, and the pending values coalesced since. */
	struct ControlValuesState
	{
		bool hasJournaled{ false };
		json journaledValues{};
		std::chrono::steady_clock::time_point journaledAt{};
		bool hasPending{ false };
		json pendingValues{};
		std::size_t coalesced{ 0u }; // Number of changes coalesced into the pending values
	};

	/** Journals the given control values (the coalesced count tells how many changes were skipped since the previous event of that control). */
	void journalControlValues(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, ControlValuesState& state, json values, std::chrono::steady_clock::time_point const now) noexcept
	{
		auto details = QJsonObject{};
		details["values"] = toQJsonValue(values);
		if (state.coalesced > 0u)
		{
			details["coalesced"] = static_cast<qint64>(state.coalesced);
		}
		auto patchOperations = QJsonArray{};
		if (auto const pointer = dynamicPointer(entityID, la::avdecc::entity::model::DescriptorType::Control, controlIndex, "values"))
		{
			auto operation = QJsonObject{};
			operation["op"] = "replace";
			operation["path"] = *pointer;
			operation["value"] = details["values"];
			patchOperations.push_back(operation);
			details[EventJournal::PatchDetailsKey] = patchOperations;
		}
		auto const summary = state.coalesced > 0u ? QString{ "Control value changed (%1 changes coalesced)" }.arg(state.coalesced) : QString{ "Control value changed" };
		state.hasJournaled = true;
		state.journaledValues = std::move(values);
		state.journaledAt = now;
		state.hasPending = false;
		state.pendingValues = json{};
		state.coalesced = 0u;
		addEvent(Category::Configuration, Severity::Info, entityID, entityNameFor(entityID), QString{ "Control %1" }.arg(controlIndex), summary, details, Level::Diagnostic);
	}

	/** State derived from a pair of counters counting the entries into and the exits from a state (LOCKED / UNLOCKED of a clock domain, MEDIA_LOCKED / MEDIA_UNLOCKED of a stream input, LINK_UP / LINK_DOWN of an AVB interface), and the counter values it was derived from. */
	struct CounterPairState
	{
		bool isActive{ false }; // Locked, or link up
		la::avdecc::entity::model::DescriptorCounter activeCounter{ 0u }; // Entries into the state (LOCKED, MEDIA_LOCKED, LINK_UP)
		la::avdecc::entity::model::DescriptorCounter inactiveCounter{ 0u }; // Exits from the state (UNLOCKED, MEDIA_UNLOCKED, LINK_DOWN)
	};

	/** A transition replayed from the counter increments between two notifications (see replayCounterPairTransitions). */
	struct CounterPairTransition
	{
		bool isActive{ false }; // State resulting from the transition
		bool isInferred{ false }; // True when the transition could not have been observed by comparing the states before and after the notification
	};

	/** The transitions replayed from the counter increments between two notifications, with the increments themselves (zero when the counters went backwards). */
	struct CounterPairReplay
	{
		la::avdecc::entity::model::DescriptorCounter activeIncrement{ 0u };
		la::avdecc::entity::model::DescriptorCounter inactiveIncrement{ 0u };
		std::vector<CounterPairTransition> transitions{}; // In order
	};

	/**
	* @brief Replays the transitions implied by the counter increments between two notifications.
	* @details The counters are only notified once in a while, so the state may have flipped and come back (possibly several times) between two notifications without the derived state ever changing.
	*          The transitions are replayed in the only order consistent with an alternating sequence starting from the previous state, and the replay always ends on the state derived from the counters (same rule as the Discovered Entities list), so every exit the device counted ends up in the journal.
	*          A counter going backwards means the device rebooted or its counters were reset: the increments are meaningless, only the resulting state is compared.
	*          A transition is inferred when it could not have been observed by comparing the states before and after the notification: all of them when the state did not change, all but the last one otherwise.
	*/
	static CounterPairReplay replayCounterPairTransitions(CounterPairState const& previousState, CounterPairState const& newState) noexcept
	{
		auto replay = CounterPairReplay{};
		auto const countersReset = newState.activeCounter < previousState.activeCounter || newState.inactiveCounter < previousState.inactiveCounter;
		replay.activeIncrement = countersReset ? la::avdecc::entity::model::DescriptorCounter{ 0u } : newState.activeCounter - previousState.activeCounter;
		replay.inactiveIncrement = countersReset ? la::avdecc::entity::model::DescriptorCounter{ 0u } : newState.inactiveCounter - previousState.inactiveCounter;

		auto isActive = previousState.isActive;
		auto remainingActivations = replay.activeIncrement;
		auto remainingDeactivations = replay.inactiveIncrement;
		while ((isActive && remainingDeactivations > 0u) || (!isActive && remainingActivations > 0u))
		{
			if (isActive)
			{
				--remainingDeactivations;
			}
			else
			{
				--remainingActivations;
			}
			isActive = !isActive;
			replay.transitions.push_back(CounterPairTransition{ isActive, false });
		}
		// Whatever the increments said, the journaled state must end on the state derived from the counters
		if (isActive != newState.isActive)
		{
			isActive = newState.isActive;
			replay.transitions.push_back(CounterPairTransition{ isActive, false });
		}
		auto const stateChanged = previousState.isActive != newState.isActive;
		for (auto index = std::size_t{ 0u }; index < replay.transitions.size(); ++index)
		{
			replay.transitions[index].isInferred = !stateChanged || index + 1u < replay.transitions.size();
		}
		return replay;
	}

	/** Builds the media clock lock state of a clock domain from its counters (the domain is locked when it locked more times than it unlocked, same rule as the Discovered Entities list). Returns std::nullopt if the counters are not available. */
	static std::optional<CounterPairState> makeClockLockState(la::avdecc::entity::model::ClockDomainCounters const& counters) noexcept
	{
		auto const itLocked = counters.find(la::avdecc::entity::ClockDomainCounterValidFlag::Locked);
		auto const itUnlocked = counters.find(la::avdecc::entity::ClockDomainCounterValidFlag::Unlocked);
		if (itLocked != counters.end() && itUnlocked != counters.end())
		{
			return CounterPairState{ itLocked->second > itUnlocked->second, itLocked->second, itUnlocked->second };
		}
		return std::nullopt;
	}

	/** Builds the media clock lock state of a stream input from its MEDIA_LOCKED / MEDIA_UNLOCKED counters (same rule as the clock domains). Returns std::nullopt if either counter is not available. */
	static std::optional<CounterPairState> makeStreamLockState(la::avdecc::entity::model::StreamInputCounters const& counters) noexcept
	{
		auto const itLocked = counters.find(la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked);
		auto const itUnlocked = counters.find(la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked);
		if (itLocked != counters.end() && itUnlocked != counters.end())
		{
			return CounterPairState{ itLocked->second > itUnlocked->second, itLocked->second, itUnlocked->second };
		}
		return std::nullopt;
	}

	/** Builds the link state of an AVB interface from its LINK_UP / LINK_DOWN counters (up when LINK_UP is exactly LINK_DOWN plus one, the rule la_avdecc derives the link status from, Milan 1.3 Clause 5.3.6.3). Returns std::nullopt if either counter is not available. */
	static std::optional<CounterPairState> makeLinkState(la::avdecc::entity::model::AvbInterfaceCounters const& counters) noexcept
	{
		auto const itUp = counters.find(la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkUp);
		auto const itDown = counters.find(la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkDown);
		if (itUp != counters.end() && itDown != counters.end())
		{
			return CounterPairState{ itUp->second == itDown->second + 1u, itUp->second, itDown->second };
		}
		return std::nullopt;
	}

	/** Media clock lock tracking state of a stream input: the lock state derived from its counters, and whether an unlock error is open (journaled and not closed by a relock yet). */
	struct StreamLockState
	{
		CounterPairState lock{};
		bool hasOpenUnlock{ false };
	};

	/** GPTP_GM_CHANGED tracking state of an AVB interface: the counter value and the grandmaster known at the previous counters notification. */
	struct GptpGmChangedState
	{
		la::avdecc::entity::model::DescriptorCounter counter{ 0u };
		std::optional<la::avdecc::UniqueIdentifier> grandmasterID{};
	};

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

	/** Builds the static description of an entity for the details of its 'Entity is online' event (identity, firmware, interfaces with their gPTP state, clock sources, connected streams), so the analysis of a journal needs no external network state dump. */
	static QJsonObject entityDescription(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept
	{
		auto details = QJsonObject{};
		auto const& entity = controlledEntity.getEntity();
		details["entity_model_id"] = helper::uniqueIdentifierToString(entity.getEntityModelID());
		details["vendor"] = helper::getVendorName(entity.getEntityID());
		if (controlledEntity.getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
		{
			auto const milanVersion = controlledEntity.getMilanCompatibilityVersion();
			details["milan_version"] = QString{ "%1.%2" }.arg(milanVersion.getMajorVersion()).arg(milanVersion.getMinorVersion());
		}
		try
		{
			auto const& entityNode = controlledEntity.getEntityNode();
			details["firmware_version"] = QString::fromStdString(entityNode.dynamicModel.firmwareVersion.str());

			auto const& configurationNode = controlledEntity.getCurrentConfigurationNode();
			auto avbInterfaces = QJsonArray{};
			for (auto const& [avbInterfaceIndex, avbInterfaceNode] : configurationNode.avbInterfaces)
			{
				auto const& dynamicModel = avbInterfaceNode.dynamicModel;
				auto object = QJsonObject{};
				object["index"] = static_cast<int>(avbInterfaceIndex);
				object["mac"] = helper::macAddressToString(dynamicModel.macAddress);
				object["clock_identity"] = helper::uniqueIdentifierToString(dynamicModel.clockIdentity);
				object["grandmaster_id"] = helper::uniqueIdentifierToString(dynamicModel.gptpGrandmasterID);
				object["priority1"] = static_cast<int>(dynamicModel.priority1);
				if (dynamicModel.asPath)
				{
					object["as_path"] = asPathToJson(*dynamicModel.asPath);
				}
				if (dynamicModel.avbInterfaceInfo)
				{
					object["propagation_delay"] = static_cast<qint64>(dynamicModel.avbInterfaceInfo->propagationDelay);
				}
				avbInterfaces.push_back(object);
			}
			details["avb_interfaces"] = avbInterfaces;

			auto clockDomains = QJsonArray{};
			for (auto const& [clockDomainIndex, clockDomainNode] : configurationNode.clockDomains)
			{
				auto object = QJsonObject{};
				object["index"] = static_cast<int>(clockDomainIndex);
				object["clock_source_index"] = static_cast<int>(clockDomainNode.dynamicModel.clockSourceIndex);
				// The clock source of a media clock domain is usually an input stream: name it, so the analyser knows which stream carries the clock without decoding the snapshot
				if (auto const clockSourceIt = configurationNode.clockSources.find(clockDomainNode.dynamicModel.clockSourceIndex); clockSourceIt != configurationNode.clockSources.end() && clockSourceIt->second.staticModel.clockSourceLocationType == la::avdecc::entity::model::DescriptorType::StreamInput)
				{
					object["clock_source_stream_index"] = static_cast<int>(clockSourceIt->second.staticModel.clockSourceLocationIndex);
				}
				clockDomains.push_back(object);
			}
			details["clock_domains"] = clockDomains;

			auto streamInputs = QJsonArray{};
			for (auto const& [streamIndex, streamNode] : configurationNode.streamInputs)
			{
				auto const& connectionInfo = streamNode.dynamicModel.connectionInfo;
				if (connectionInfo.state != la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected)
				{
					continue;
				}
				auto object = QJsonObject{};
				object["index"] = static_cast<int>(streamIndex);
				object["name"] = helper::inputStreamName(controlledEntity, streamIndex);
				object["talker_entity_id"] = helper::uniqueIdentifierToString(connectionInfo.talkerStream.entityID);
				object["talker_stream_index"] = static_cast<int>(connectionInfo.talkerStream.streamIndex);
				streamInputs.push_back(object);
			}
			details["stream_inputs"] = streamInputs;
		}
		catch (...)
		{
			// Entity without a current configuration (or model enumeration error): the identity keys are enough
		}
		return details;
	}

	/** Returns the entities currently online in the controller (their guards are released before returning, so the caller can serialize them). */
	static std::vector<la::avdecc::UniqueIdentifier> onlineEntities() noexcept
	{
		auto entityIDs = std::vector<la::avdecc::UniqueIdentifier>{};
		ControllerManager::getInstance().foreachEntity(
			[&entityIDs](la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::ControlledEntity const&)
			{
				entityIDs.push_back(entityID);
			});
		return entityIDs;
	}

	/** Serializes the given entity into the snapshots table (see EventJournal::SnapshotFormat). Returns the row id of the snapshot, or std::nullopt if the entity could not be serialized. */
	std::optional<qint64> writeSnapshot(la::avdecc::UniqueIdentifier const entityID, QString const& reason) noexcept
	{
		if (!isRecording())
		{
			return std::nullopt;
		}

		auto timer = QElapsedTimer{};
		timer.start();
		auto const timestamp = QDateTime::currentMSecsSinceEpoch();

		// la_avdecc only serializes to a file: go through a temporary file (a few milliseconds per entity, measured in the snapshots_serialization_ms metadata)
		auto const tempFilePath = QDir::temp().filePath(QString{ "HiveEventJournalSnapshot_%1.ave" }.arg(QCoreApplication::applicationPid()));
		auto const dumpSource = QString{ "%1 %2 (Event Journal snapshot)" }.arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion());
		auto const [error, message] = ControllerManager::getInstance().serializeControlledEntityAsJson(entityID, tempFilePath, snapshotSerializationFlags(), dumpSource);
		auto uncompressed = QByteArray{};
		if (!error)
		{
			auto file = QFile{ tempFilePath };
			if (file.open(QIODevice::ReadOnly))
			{
				uncompressed = file.readAll();
			}
		}
		QFile::remove(tempFilePath);
		if (uncompressed.isEmpty())
		{
			return std::nullopt;
		}
		auto const compressed = qCompress(uncompressed);

		_insertSnapshotQuery.addBindValue(timestamp);
		_insertSnapshotQuery.addBindValue(helper::uniqueIdentifierToString(entityID));
		_insertSnapshotQuery.addBindValue(reason);
		_insertSnapshotQuery.addBindValue(QString{ EventJournal::SnapshotFormat });
		_insertSnapshotQuery.addBindValue(compressed);
		_insertSnapshotQuery.addBindValue(static_cast<qint64>(uncompressed.size()));
		if (!_insertSnapshotQuery.exec())
		{
			return std::nullopt;
		}
		auto const snapshotID = _insertSnapshotQuery.lastInsertId().toLongLong();

		// Size and time figures, so the cost of the snapshots can be judged on a real session
		++_snapshotsCount;
		_snapshotsBytes += static_cast<std::uint64_t>(compressed.size());
		_snapshotsSerializationMs += timer.elapsed();
		setMetadata("snapshots_count", QString::number(_snapshotsCount));
		setMetadata("snapshots_bytes", QString::number(_snapshotsBytes));
		setMetadata("snapshots_serialization_ms", QString::number(_snapshotsSerializationMs));
		return snapshotID;
	}

	/** Seeds the change-detection baselines from the current entity model state, so only actual changes occurring after the entity came online are journaled. */
	void seedBaselines(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept
	{
		using DescriptorType = la::avdecc::entity::model::DescriptorType;

		// Entity wide states (do not depend on the entity having a configuration)
		_unsolRegistrationStates[entityID] = controlledEntity.isSubscribedToUnsolicitedNotifications();
		_acquireStates[entityID] = std::make_pair(controlledEntity.getAcquireState(), controlledEntity.getOwningControllerID());
		_lockStates[entityID] = std::make_pair(controlledEntity.getLockState(), controlledEntity.getLockingControllerID());
		_compatibilities[entityID] = std::make_pair(controlledEntity.getCompatibilityFlags(), controlledEntity.getMilanCompatibilityVersion());
		_entityCapabilities[entityID] = controlledEntity.getEntity().getEntityCapabilities();
		_rawCounters.erase(entityID);
		_controlValuesStates.erase(entityID);

		try
		{
			auto const& entityNode = controlledEntity.getEntityNode();
			if (entityNode.dynamicModel.counters)
			{
				_rawCounters[entityID][CounterKey{ DescriptorType::Entity, 0u }] = countersToJson(*entityNode.dynamicModel.counters);
			}
			auto const& configurationNode = controlledEntity.getCurrentConfigurationNode();
			for (auto const& [avbInterfaceIndex, avbInterfaceNode] : configurationNode.avbInterfaces)
			{
				_gptpStates[entityID][avbInterfaceIndex] = std::make_pair(avbInterfaceNode.dynamicModel.gptpGrandmasterID, avbInterfaceNode.dynamicModel.gptpDomainNumber);
				if (avbInterfaceNode.dynamicModel.counters)
				{
					if (auto const linkState = makeLinkState(*avbInterfaceNode.dynamicModel.counters))
					{
						_linkStates[entityID][avbInterfaceIndex] = *linkState;
					}
					if (auto const counterIt = avbInterfaceNode.dynamicModel.counters->find(la::avdecc::entity::AvbInterfaceCounterValidFlag::GptpGmChanged); counterIt != avbInterfaceNode.dynamicModel.counters->end())
					{
						_gptpGmChangedStates[entityID][avbInterfaceIndex] = GptpGmChangedState{ counterIt->second, avbInterfaceNode.dynamicModel.gptpGrandmasterID };
					}
				}
				if (avbInterfaceNode.dynamicModel.asPath)
				{
					_asPaths[entityID][avbInterfaceIndex] = *avbInterfaceNode.dynamicModel.asPath;
				}
				if (avbInterfaceNode.dynamicModel.avbInterfaceInfo)
				{
					_avbInterfaceInfos[entityID][avbInterfaceIndex] = *avbInterfaceNode.dynamicModel.avbInterfaceInfo;
				}
				if (avbInterfaceNode.dynamicModel.counters)
				{
					_rawCounters[entityID][CounterKey{ DescriptorType::AvbInterface, avbInterfaceIndex }] = countersToJson(*avbInterfaceNode.dynamicModel.counters);
				}
			}
			for (auto const& [clockDomainIndex, clockDomainNode] : configurationNode.clockDomains)
			{
				if (clockDomainNode.dynamicModel.counters)
				{
					if (auto const lockState = makeClockLockState(*clockDomainNode.dynamicModel.counters))
					{
						_clockLockStates[entityID][clockDomainIndex] = *lockState;
					}
					_rawCounters[entityID][CounterKey{ DescriptorType::ClockDomain, clockDomainIndex }] = countersToJson(*clockDomainNode.dynamicModel.counters);
				}
				_clockSources[entityID][clockDomainIndex] = clockDomainNode.dynamicModel.clockSourceIndex;
				_mediaClockReferenceInfos[entityID][clockDomainIndex] = clockDomainNode.dynamicModel.mediaClockReferenceInfo;
			}
			for (auto const& [audioUnitIndex, audioUnitNode] : configurationNode.audioUnits)
			{
				_samplingRates[entityID][audioUnitIndex] = audioUnitNode.dynamicModel.currentSamplingRate;
			}
			// An entity coming online again starts the stream data loss learning from scratch (its counters may have been reset)
			_streamDataStates.erase(entityID);
			auto const arrival = _monotonicClock();
			for (auto const& [streamIndex, streamNode] : configurationNode.streamInputs)
			{
				_streamConnections[entityID][streamIndex] = streamNode.dynamicModel.connectionInfo;
				_streamFormats[entityID][StreamKey{ DescriptorType::StreamInput, streamIndex }] = streamNode.dynamicModel.streamFormat;
				if (streamNode.dynamicModel.streamDynamicInfo)
				{
					_streamDynamicInfos[entityID][StreamKey{ DescriptorType::StreamInput, streamIndex }] = *streamNode.dynamicModel.streamDynamicInfo;
				}
				if (streamNode.dynamicModel.isStreamRunning)
				{
					_streamRunningStates[entityID][StreamKey{ DescriptorType::StreamInput, streamIndex }] = *streamNode.dynamicModel.isStreamRunning;
				}
				if (streamNode.dynamicModel.counters)
				{
					_rawCounters[entityID][CounterKey{ DescriptorType::StreamInput, streamIndex }] = countersToJson(*streamNode.dynamicModel.counters);
					if (auto const lockState = makeStreamLockState(*streamNode.dynamicModel.counters))
					{
						_streamLockStates[entityID][streamIndex] = StreamLockState{ *lockState, false };
					}
				}
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
			for (auto const& [streamIndex, streamNode] : configurationNode.streamOutputs)
			{
				_streamOutputConnections[entityID][streamIndex] = streamNode.dynamicModel.connections;
				_streamFormats[entityID][StreamKey{ DescriptorType::StreamOutput, streamIndex }] = streamNode.dynamicModel.streamFormat;
				if (streamNode.dynamicModel.streamDynamicInfo)
				{
					_streamDynamicInfos[entityID][StreamKey{ DescriptorType::StreamOutput, streamIndex }] = *streamNode.dynamicModel.streamDynamicInfo;
				}
				if (streamNode.dynamicModel.isStreamRunning)
				{
					_streamRunningStates[entityID][StreamKey{ DescriptorType::StreamOutput, streamIndex }] = *streamNode.dynamicModel.isStreamRunning;
				}
				if (streamNode.dynamicModel.counters)
				{
					_rawCounters[entityID][CounterKey{ DescriptorType::StreamOutput, streamIndex }] = countersToJson(*streamNode.dynamicModel.counters);
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
		_asPaths.clear();
		_avbInterfaceInfos.clear();
		_linkStates.clear();
		_gptpGmChangedStates.clear();
		_clockLockStates.clear();
		_streamLockStates.clear();
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
		_streamOutputConnections.clear();
		_streamFormats.clear();
		_streamDynamicInfos.clear();
		_streamRunningStates.clear();
		_samplingRates.clear();
		_clockSources.clear();
		_mediaClockReferenceInfos.clear();
		_acquireStates.clear();
		_lockStates.clear();
		_compatibilities.clear();
		_entityCapabilities.clear();
		_rawCounters.clear();
		_controlValuesStates.clear();
		_controlValuesFlushTimer.stop();
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
	QSqlQuery _insertSnapshotQuery{};
	std::uint64_t _snapshotsCount{ 0u };
	std::uint64_t _snapshotsBytes{ 0u };
	qint64 _snapshotsSerializationMs{ 0 };
	QString _sessionFilePath{};
	std::vector<Event> _events{};
	QMap<QString, QString> _metadata{};
	Level _recordingLevel{ Level::User };

	// Change-detection state
	std::unordered_map<la::avdecc::UniqueIdentifier, QString, la::avdecc::UniqueIdentifier::hash> _entityNames{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::AvbInterfaceIndex, std::pair<la::avdecc::UniqueIdentifier, std::uint8_t>>, la::avdecc::UniqueIdentifier::hash> _gptpStates{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::AvbInterfaceIndex, la::avdecc::entity::model::AsPath>, la::avdecc::UniqueIdentifier::hash> _asPaths{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::AvbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceInfo>, la::avdecc::UniqueIdentifier::hash> _avbInterfaceInfos{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::AvbInterfaceIndex, CounterPairState>, la::avdecc::UniqueIdentifier::hash> _linkStates{}; // Link state of the AVB interfaces, derived from their LINK_UP / LINK_DOWN counters
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::AvbInterfaceIndex, GptpGmChangedState>, la::avdecc::UniqueIdentifier::hash> _gptpGmChangedStates{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::ClockDomainIndex, CounterPairState>, la::avdecc::UniqueIdentifier::hash> _clockLockStates{}; // Media clock lock state of the clock domains, derived from their LOCKED / UNLOCKED counters
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::StreamIndex, StreamLockState>, la::avdecc::UniqueIdentifier::hash> _streamLockStates{}; // Media clock lock state of the stream inputs, derived from their MEDIA_LOCKED / MEDIA_UNLOCKED counters
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

	// Change tracking state (the twin of every entity, see docs/EventJournal.md): the last journaled value of every tracked item, so duplicates and unchanged notifications are dropped
	using StreamKey = std::pair<la::avdecc::entity::model::DescriptorType, la::avdecc::entity::model::StreamIndex>; // Input or output stream
	using CounterKey = std::pair<la::avdecc::entity::model::DescriptorType, la::avdecc::entity::model::DescriptorIndex>; // Descriptor holding counters (Entity with index 0 for the entity counters)
	std::unordered_map<la::avdecc::UniqueIdentifier, std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamConnections>, la::avdecc::UniqueIdentifier::hash> _streamOutputConnections{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::map<StreamKey, la::avdecc::entity::model::StreamFormat>, la::avdecc::UniqueIdentifier::hash> _streamFormats{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::map<StreamKey, la::avdecc::entity::model::StreamDynamicInfo>, la::avdecc::UniqueIdentifier::hash> _streamDynamicInfos{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::map<StreamKey, bool>, la::avdecc::UniqueIdentifier::hash> _streamRunningStates{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::map<la::avdecc::entity::model::AudioUnitIndex, la::avdecc::entity::model::SamplingRate>, la::avdecc::UniqueIdentifier::hash> _samplingRates{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::map<la::avdecc::entity::model::ClockDomainIndex, la::avdecc::entity::model::ClockSourceIndex>, la::avdecc::UniqueIdentifier::hash> _clockSources{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::map<la::avdecc::entity::model::ClockDomainIndex, la::avdecc::entity::model::MediaClockReferenceInfo>, la::avdecc::UniqueIdentifier::hash> _mediaClockReferenceInfos{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::pair<la::avdecc::controller::model::AcquireState, la::avdecc::UniqueIdentifier>, la::avdecc::UniqueIdentifier::hash> _acquireStates{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::pair<la::avdecc::controller::model::LockState, la::avdecc::UniqueIdentifier>, la::avdecc::UniqueIdentifier::hash> _lockStates{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::pair<la::avdecc::controller::ControlledEntity::CompatibilityFlags, la::avdecc::entity::model::MilanVersion>, la::avdecc::UniqueIdentifier::hash> _compatibilities{};
	std::unordered_map<la::avdecc::UniqueIdentifier, la::avdecc::entity::EntityCapabilities, la::avdecc::UniqueIdentifier::hash> _entityCapabilities{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::map<CounterKey, json>, la::avdecc::UniqueIdentifier::hash> _rawCounters{}; // Last journaled raw counters of every descriptor, as serialized in the AVE document
	std::unordered_map<la::avdecc::UniqueIdentifier, std::map<la::avdecc::entity::model::ControlIndex, ControlValuesState>, la::avdecc::UniqueIdentifier::hash> _controlValuesStates{};
	QTimer _controlValuesFlushTimer{}; // Single shot, journals the pending control values once their rate limiting interval elapsed
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

	_pImpl->_controlValuesFlushTimer.setSingleShot(true);
	connect(&_pImpl->_controlValuesFlushTimer, &QTimer::timeout, this,
		[this]()
		{
			_pImpl->flushPendingControlValues(false);
		});

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
	connect(&manager, &ControllerManager::asPathChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AsPath const& asPath)
		{
			_pImpl->handleAsPathChanged(entityID, avbInterfaceIndex, asPath);
		});
	connect(&manager, &ControllerManager::avbInterfaceInfoChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceInfo const& info)
		{
			_pImpl->handleAvbInterfaceInfoChanged(entityID, avbInterfaceIndex, info);
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
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
		{
			_pImpl->handleStreamFormatChanged(entityID, descriptorType, streamIndex, streamFormat);
		});
	// Change tracking events (diagnostic level), so the analyser can rebuild the state of every entity at any time of the session
	connect(&manager, &ControllerManager::streamOutputConnectionsChanged, this,
		[this](la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamConnections const& connections)
		{
			_pImpl->handleStreamOutputConnectionsChanged(stream, connections);
		});
	connect(&manager, &ControllerManager::streamDynamicInfoChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamDynamicInfo const& info)
		{
			_pImpl->handleStreamDynamicInfoChanged(entityID, descriptorType, streamIndex, info);
		});
	connect(&manager, &ControllerManager::streamRunningChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning)
		{
			_pImpl->handleStreamRunningChanged(entityID, descriptorType, streamIndex, isRunning);
		});
	connect(&manager, &ControllerManager::audioUnitSamplingRateChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate)
		{
			_pImpl->handleAudioUnitSamplingRateChanged(entityID, audioUnitIndex, samplingRate);
		});
	connect(&manager, &ControllerManager::clockSourceChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex)
		{
			_pImpl->handleClockSourceChanged(entityID, clockDomainIndex, clockSourceIndex);
		});
	connect(&manager, &ControllerManager::mediaClockReferenceInfoChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::MediaClockReferenceInfo const& info)
		{
			_pImpl->handleMediaClockReferenceInfoChanged(entityID, clockDomainIndex, info);
		});
	connect(&manager, &ControllerManager::streamPortAudioMappingsChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex)
		{
			_pImpl->handleStreamPortAudioMappingsChanged(entityID, descriptorType, streamPortIndex);
		});
	connect(&manager, &ControllerManager::controlValuesChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlValues const& controlValues)
		{
			_pImpl->handleControlValuesChanged(entityID, controlIndex, controlValues);
		});
	connect(&manager, &ControllerManager::acquireStateChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity)
		{
			_pImpl->handleAcquireStateChanged(entityID, acquireState, owningEntity);
		});
	connect(&manager, &ControllerManager::lockStateChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity)
		{
			_pImpl->handleLockStateChanged(entityID, lockState, lockingEntity);
		});
	connect(&manager, &ControllerManager::compatibilityChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags, la::avdecc::entity::model::MilanVersion const& milanCompatibleVersion)
		{
			_pImpl->handleCompatibilityChanged(entityID, compatibilityFlags, milanCompatibleVersion);
		});
	connect(&manager, &ControllerManager::entityCapabilitiesChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::EntityCapabilities const entityCapabilities)
		{
			_pImpl->handleEntityCapabilitiesChanged(entityID, entityCapabilities);
		});
	connect(&manager, &ControllerManager::entityCountersChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::EntityCounters const& counters)
		{
			_pImpl->handleEntityCountersChanged(entityID, counters);
		});
	connect(&manager, &ControllerManager::avbInterfaceCountersChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceCounters const& counters)
		{
			_pImpl->handleAvbInterfaceCountersChanged(entityID, avbInterfaceIndex, counters);
		});
	connect(&manager, &ControllerManager::streamOutputCountersChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamOutputCounters const& counters)
		{
			_pImpl->handleStreamOutputCountersChanged(entityID, streamIndex, counters);
		});
	// Object names
	connect(&manager, &ControllerManager::entityGroupNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, QString const& entityGroupName)
		{
			_pImpl->handleEntityGroupNameChanged(entityID, entityGroupName);
		});
	connect(&manager, &ControllerManager::configurationNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::Configuration, configurationIndex, name);
		});
	connect(&manager, &ControllerManager::audioUnitNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::AudioUnit, audioUnitIndex, name);
		});
	connect(&manager, &ControllerManager::streamNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, descriptorType, streamIndex, name);
		});
	connect(&manager, &ControllerManager::jackNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::JackIndex const jackIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, descriptorType, jackIndex, name);
		});
	connect(&manager, &ControllerManager::avbInterfaceNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::AvbInterface, avbInterfaceIndex, name);
		});
	connect(&manager, &ControllerManager::clockSourceNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::ClockSource, clockSourceIndex, name);
		});
	connect(&manager, &ControllerManager::memoryObjectNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::MemoryObject, memoryObjectIndex, name);
		});
	connect(&manager, &ControllerManager::audioClusterNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::AudioCluster, audioClusterIndex, name);
		});
	connect(&manager, &ControllerManager::controlNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::Control, controlIndex, name);
		});
	connect(&manager, &ControllerManager::clockDomainNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::ClockDomain, clockDomainIndex, name);
		});
	connect(&manager, &ControllerManager::timingNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::TimingIndex const timingIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::Timing, timingIndex, name);
		});
	connect(&manager, &ControllerManager::ptpInstanceNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpInstanceIndex const ptpInstanceIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::PtpInstance, ptpInstanceIndex, name);
		});
	connect(&manager, &ControllerManager::ptpPortNameChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpPortIndex const ptpPortIndex, QString const& name)
		{
			_pImpl->handleObjectNameChanged(entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::PtpPort, ptpPortIndex, name);
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
		case Category::Configuration:
			return "Configuration";
		case Category::Control:
			return "Control";
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
		if (openJournalForReading(db, filePath))
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

std::vector<EventJournal::SnapshotInfo> EventJournal::loadSnapshotsIndex(QString const& filePath) noexcept
{
	static auto s_readerCount = std::atomic_uint{ 0u };

	auto snapshots = std::vector<SnapshotInfo>{};
	auto const connectionName = QString{ "HiveEventJournalSnapshotsReader_%1" }.arg(++s_readerCount);
	{
		auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
		if (openJournalForReading(db, filePath))
		{
			auto query = QSqlQuery{ db };
			// Files predating schema version 2 have no snapshots table: the query fails and the index stays empty
			if (query.exec("SELECT id, timestamp, entity_id, reason, format, uncompressed_size, length(data) FROM snapshots ORDER BY timestamp, id"))
			{
				while (query.next())
				{
					auto info = SnapshotInfo{};
					info.id = query.value(0).toLongLong();
					info.timestamp = query.value(1).toLongLong();
					auto ok = false;
					auto const eid = query.value(2).toString().toULongLong(&ok, 16);
					if (ok)
					{
						info.entityID = la::avdecc::UniqueIdentifier{ eid };
					}
					info.reason = query.value(3).toString();
					info.format = query.value(4).toString();
					info.uncompressedSize = query.value(5).toLongLong();
					info.compressedSize = query.value(6).toLongLong();
					snapshots.push_back(std::move(info));
				}
			}
			db.close();
		}
	}
	QSqlDatabase::removeDatabase(connectionName);
	return snapshots;
}

QByteArray EventJournal::loadSnapshotData(QString const& filePath, qint64 const snapshotID) noexcept
{
	static auto s_readerCount = std::atomic_uint{ 0u };

	auto data = QByteArray{};
	auto const connectionName = QString{ "HiveEventJournalSnapshotDataReader_%1" }.arg(++s_readerCount);
	{
		auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
		if (openJournalForReading(db, filePath))
		{
			auto query = QSqlQuery{ db };
			query.prepare("SELECT format, data FROM snapshots WHERE id = ?");
			query.addBindValue(snapshotID);
			if (query.exec() && query.next() && query.value(0).toString() == SnapshotFormat)
			{
				data = qUncompress(query.value(1).toByteArray());
			}
			db.close();
		}
	}
	QSqlDatabase::removeDatabase(connectionName);
	return data;
}

bool EventJournal::isRecording() const noexcept
{
	return _pImpl->isRecording();
}

std::size_t EventJournal::snapshotAllEntities() noexcept
{
	return _pImpl->snapshotAllEntities();
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
