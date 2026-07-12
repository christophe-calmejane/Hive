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

#include <QObject>
#include <QString>
#include <QMap>

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
	};

	/** Severity of a journaled event. */
	enum class Severity
	{
		Info = 0, /**< Normal, expected event */
		Warning = 1, /**< Abnormal event that may impact the network */
		Error = 2, /**< Abnormal event that most likely impacts the network */
		Recovered = 3, /**< End of a previously reported error condition */
	};

	/** A single journaled event. */
	struct Event
	{
		qint64 timestamp{ 0 }; /**< Milliseconds since Unix epoch, UTC */
		Category category{ Category::Session };
		Severity severity{ Severity::Info };
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

	/**
	* @brief Loads a journal session file.
	* @param[in] filePath Path of the session file to load.
	* @return The loaded session, or std::nullopt if the file could not be read.
	*/
	static std::optional<Session> loadSession(QString const& filePath) noexcept;

	/** Returns true if a session is currently being recorded. */
	bool isRecording() const noexcept;

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

	/* Signals */
	Q_SIGNAL void recordingStarted(QString const& filePath);
	Q_SIGNAL void recordingStopped();
	Q_SIGNAL void recordingFailed(QString const& reason);
	Q_SIGNAL void eventAdded(hive::modelsLibrary::EventJournal::Event const& event);

private:
	EventJournal();
	~EventJournal() override;

	class Private;
	std::unique_ptr<Private> _pImpl;
};

} // namespace modelsLibrary
} // namespace hive
