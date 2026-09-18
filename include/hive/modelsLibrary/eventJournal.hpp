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

#include <la/avdecc/utils.hpp>
#include <la/avdecc/internals/uniqueIdentifier.hpp>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QMap>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace hive
{
namespace modelsLibrary
{
/**
* @brief Journal of important network events, persisted to an SQLite database file.
* @details Listens to ControllerManager signals and records user-relevant events (entities going online/offline,
*          stream connections, error counters, media clock lock changes, gPTP changes, link status changes, ...)
*          into a timestamped session file. A new session file is automatically created each time the controller
*          starts, and closed when it stops. Session files can be exported while recording, and any session file
*          can be loaded back for offline analysis.
* @note All methods must be called from the main thread (events are recorded from ControllerManager signals,
*       which are emitted on the main thread).
*/
class EventJournal : public QObject
{
	Q_OBJECT
public:
	/** File extension used by journal session files (without the leading dot). */
	static constexpr auto JournalFileExtension = "hej";

	/** Schema version written by this version of the journal (see docs/EventJournal.md for the compatibility rules). */
	static constexpr auto CurrentSchemaVersion = 2;

	/** Format identifier of the entity snapshots written by this version: la_avdecc JSON serializer output encoded as MessagePack (the .ave binary format) then zlib compressed (qCompress). */
	static constexpr auto SnapshotFormat = "ave-msgpack-zlib";

	/** Details key of the change tracking events holding the JSON Patch (RFC 6902, 'replace' operations only) that applies the change to the AVE document of the entity (the format of the snapshots, see docs/EventJournal.md). */
	static constexpr auto PatchDetailsKey = "patch";

	/** Functional category of a journaled event, used for filtering. */
	enum class Category
	{
		Session = 0, /**< Journal session lifecycle (recording started/stopped, transport errors) */
		Entity = 1, /**< Entity going online/offline */
		Connection = 2, /**< Stream connection/disconnection */
		Counters = 3, /**< Error counters (the ones Hive flags as errors) */
		MediaClock = 4, /**< Media clock lock state changes */
		Gptp = 5, /**< gPTP grandmaster/domain changes */
		Link = 6, /**< Physical link status changes */
		Latency = 7, /**< Stream latency errors */
		Redundancy = 8, /**< Milan redundancy warnings */
		Streaming = 9, /**< Stream data reception (frames missing, detected from the FRAMES_RX counter increments) */
		Configuration = 10, /**< Entity configuration changes (names, stream formats, sampling rates, clock sources, control values, audio mappings, ...), diagnostic level */
		Control = 11, /**< Controller actions on an entity (acquire, lock) */
	};

	/** Severity of a journaled event. */
	enum class Severity
	{
		Info = 0, /**< Normal, expected event */
		Warning = 1, /**< Abnormal event that may impact the network */
		Error = 2, /**< Abnormal event that most likely impacts the network */
		Recovered = 3, /**< End of a previously reported error condition */
	};

	/** Recording level of a journaled event: who the event is for. The recording level of the session decides which events are written. */
	enum class Level
	{
		User = 0, /**< What the sound engineer needs (the only level recorded unless diagnostic recording is enabled) */
		Diagnostic = 1, /**< Everything needed for deep analysis and for rebuilding the state of the network (much larger files) */
	};

	/** A single journaled event. */
	struct Event
	{
		qint64 timestamp{ 0 }; /**< Milliseconds since Unix epoch, UTC */
		Category category{ Category::Session };
		Severity severity{ Severity::Info };
		Level level{ Level::User };
		la::avdecc::UniqueIdentifier entityID{}; /**< May be invalid for session-wide events */
		QString entityName{}; /**< Name of the entity at the time of the event (may be empty) */
		QString subject{}; /**< Sub-object designation (stream, interface, clock domain, ...) (may be empty) */
		QString summary{}; /**< Human readable description of the event */
		QString details{}; /**< Additional details, JSON object as string (may be empty) */
	};

	/** A complete journal session, as loaded from a session file. */
	struct Session
	{
		QMap<QString, QString> metadata{}; /**< Session metadata (schema_version, hive_version, started_utc, ...) */
		std::vector<Event> events{}; /**< All events, ordered by ascending timestamp */

		/** Returns the schema version the file was written with (1 when the file predates the schema_version metadata key). */
		int schemaVersion() const noexcept;

		/** Returns true if the file was written with a newer schema than this reader understands (its events are loaded anyway, in degraded mode). */
		bool isNewerThanReader() const noexcept;
	};

	/** Index entry of an entity snapshot stored in a journal file (the serialized entity is loaded separately, see loadSnapshotData()). */
	struct SnapshotInfo
	{
		qint64 id{ 0 }; /**< Row id, referenced by the 'snapshot_id' details key of the 'Entity is online' event */
		qint64 timestamp{ 0 }; /**< Milliseconds since Unix epoch, UTC */
		la::avdecc::UniqueIdentifier entityID{};
		QString reason{}; /**< online, session_start or manual */
		QString format{}; /**< Serialization format (see SnapshotFormat) */
		qint64 uncompressedSize{ 0 }; /**< Size of the serialized entity before compression */
		qint64 compressedSize{ 0 }; /**< Size of the stored blob */
	};

	/**
	* @brief Gets the EventJournal singleton.
	* @details The first call instantiates the journal and connects it to the ControllerManager, so this must be
	*          called once at application startup (before the controller is started) to activate journaling.
	* @return Reference to the EventJournal singleton.
	*/
	static EventJournal& getInstance() noexcept;

	/** Returns the directory in which session files are automatically created. */
	static QString journalsDirectory() noexcept;

	/** Returns a human readable name for the given category. */
	static QString categoryToString(Category const category) noexcept;

	/** Returns a human readable name for the given severity. */
	static QString severityToString(Severity const severity) noexcept;

	/** Returns a human readable name for the given level. */
	static QString levelToString(Level const level) noexcept;

	/**
	* @brief Loads a journal session file.
	* @param[in] filePath Path of the session file to load.
	* @return The loaded session, or std::nullopt if the file could not be read.
	*/
	static std::optional<Session> loadSession(QString const& filePath) noexcept;

	/**
	* @brief Loads the index of the entity snapshots stored in a journal file, without their data.
	* @param[in] filePath Path of the session file to read.
	* @return The snapshots ordered by ascending timestamp (empty for files predating schema version 2, or without snapshots).
	*/
	static std::vector<SnapshotInfo> loadSnapshotsIndex(QString const& filePath) noexcept;

	/**
	* @brief Loads and decompresses the serialized entity of a snapshot.
	* @param[in] filePath Path of the session file to read.
	* @param[in] snapshotID Row id of the snapshot (SnapshotInfo::id).
	* @return The serialized entity in the .ave binary format (MessagePack), ready for ControllerManager::loadVirtualEntityFromJson with the BinaryFormat flag, or an empty array on error.
	*/
	static QByteArray loadSnapshotData(QString const& filePath, qint64 const snapshotID) noexcept;

	/** Returns true if a session is currently being recorded. */
	bool isRecording() const noexcept;

	/** Returns the recording level applied to the events being journaled. */
	Level recordingLevel() const noexcept;

	/**
	* @brief Sets the recording level.
	* @details Diagnostic events are only written while the level is Diagnostic, user events are always written. Switching during a session journals a session event, so the analyser knows which periods carry diagnostic data. The level is not persisted by the journal itself (the application restores it from its settings before the controller starts).
	* @param[in] level The recording level to apply from now on.
	*/
	void setRecordingLevel(Level const level) noexcept;

	/** Returns the path of the session file currently being recorded (empty if not recording). */
	QString currentSessionFilePath() const noexcept;

	/** Returns a snapshot of all events recorded in the current session (empty if not recording). */
	std::vector<Event> currentSessionEvents() const noexcept;

	/** Returns the metadata of the current session (empty if not recording). */
	QMap<QString, QString> currentSessionMetadata() const noexcept;

	/**
	* @brief Adds (or replaces) a metadata entry in the current session.
	* @details Used by the application to attach context information to the session (eg. the network interface name).
	*          No-op if not recording.
	* @param[in] key Metadata key.
	* @param[in] value Metadata value.
	*/
	void setSessionMetadata(QString const& key, QString const& value) noexcept;

	/**
	* @brief Exports a consistent snapshot of the current session to the specified file.
	* @param[in] filePath Destination file path (overwritten if it exists).
	* @return True if the export succeeded. Always fails if not recording.
	*/
	bool exportCurrentSession(QString const& filePath) noexcept;

	/**
	* @brief Stores a snapshot of every online entity in the current session (reason 'manual').
	* @details Gives the replay a fresh anchor on demand. Journals a session event with the number of entities snapshotted. No-op if not recording.
	* @return The number of entities snapshotted.
	*/
	std::size_t snapshotAllEntities() noexcept;

	/**
	* @brief Overrides the monotonic clock used to timestamp the arrival of the counter notifications (stream data loss rule).
	* @details Only meant for unit tests, which drive the notifications synchronously and need to control the time elapsed between them.
	* @param[in] clock Function returning the current monotonic time, or an empty function to restore std::chrono::steady_clock.
	*/
	void setMonotonicClock(std::function<std::chrono::steady_clock::time_point()> clock) noexcept;

	/**
	* @brief Clears all events recorded so far in the current session.
	* @details Wipes the events from the session file and from memory, then records a "Journal cleared" marker event.
	*          The tracking baselines are kept, so ongoing states are not re-reported. The session keeps recording.
	*          No-op if not recording.
	*/
	void clearCurrentSession() noexcept;

	/* Signals */
	Q_SIGNAL void recordingStarted(QString const& filePath);
	Q_SIGNAL void recordingStopped();
	Q_SIGNAL void recordingFailed(QString const& reason);
	Q_SIGNAL void eventAdded(hive::modelsLibrary::EventJournal::Event const& event);
	Q_SIGNAL void sessionCleared();
	Q_SIGNAL void recordingLevelChanged(hive::modelsLibrary::EventJournal::Level const level);

private:
	EventJournal();
	~EventJournal() override;

	class Private;
	std::unique_ptr<Private> _pImpl;
};

} // namespace modelsLibrary
} // namespace hive
