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

#include <la/avdecc/internals/uniqueIdentifier.hpp>

#include <nlohmann/json.hpp>

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace hive
{
namespace modelsLibrary
{
/**
* @brief Replay engine of a journal session: rebuilds the AVE document of every entity as it was at any event of the session.
* @details The state of an entity at an event is its last snapshot taken at or before that event (EventJournal::SnapshotInfo), plus the patches of the change tracking events journaled between the snapshot and the event (EventJournal::PatchDetailsKey).
*          An entity that went offline after its last snapshot is absent (an entity coming back online gets a new snapshot, which becomes its anchor).
*          The documents are cached per entity and updated incrementally when the cursor moves forward (only the events between the two positions are applied), and only the entities with a snapshot or a relevant event between the two positions are rebuilt when it moves backward, so stepping through the events is cheap.
*          UI independent: the Journal Analyser loads the documents into its virtual controller, the unit tests compare them with the serialization of the live entity.
* @note A patch that does not apply (eg. the name of a descriptor of a non active configuration, which the serializer does not write) is counted as rejected and the entity keeps its previous document: the replay is only as faithful as the journal, and the caller should show the figures.
*/
class JournalReplay final
{
public:
	/** Function loading the serialized entity of a snapshot (EventJournal::loadSnapshotData bound to the journal file), returning an empty array on error. */
	using SnapshotLoader = std::function<QByteArray(qint64 const snapshotID)>;

	/** State of an entity present at the cursor. */
	struct EntityState
	{
		nlohmann::json document{}; /**< AVE document of the entity (la_avdecc JSON serializer output, the .ave format) */
		qint64 snapshotID{ 0 }; /**< Snapshot the document was rebuilt from (EventJournal::SnapshotInfo::id) */
		qint64 snapshotTimestamp{ 0 }; /**< Timestamp of that snapshot, milliseconds since Unix epoch, UTC */
		QString snapshotReason{}; /**< Reason of that snapshot (online, session_start, cleared or manual) */
		std::size_t appliedPatches{ 0u }; /**< Change tracking events applied on top of the snapshot */
		std::size_t rejectedPatches{ 0u }; /**< Change tracking events whose patch could not be applied */
	};

	/** Entities whose state changed with a cursor move. */
	struct Changes
	{
		std::vector<la::avdecc::UniqueIdentifier> updated{}; /**< Entities present at the cursor whose document changed, or that were absent before the move */
		std::vector<la::avdecc::UniqueIdentifier> removed{}; /**< Entities present before the move and absent at the cursor */
	};

	/** Entities present at the cursor, with their state. */
	using EntityStates = std::unordered_map<la::avdecc::UniqueIdentifier, EntityState, la::avdecc::UniqueIdentifier::hash>;

	/**
	* @brief Creates a replay of a loaded session.
	* @param[in] events Events of the session, ordered by ascending timestamp (EventJournal::Session::events).
	* @param[in] snapshots Snapshots index of the session (EventJournal::loadSnapshotsIndex); snapshots of an unknown format are ignored.
	* @param[in] loader Function loading the serialized entity of a snapshot on demand.
	*/
	JournalReplay(std::vector<EventJournal::Event> events, std::vector<EventJournal::SnapshotInfo> snapshots, SnapshotLoader loader);
	~JournalReplay() noexcept;

	// Deleted compiler auto-generated methods
	JournalReplay(JournalReplay const&) = delete;
	JournalReplay(JournalReplay&&) = delete;
	JournalReplay& operator=(JournalReplay const&) = delete;
	JournalReplay& operator=(JournalReplay&&) = delete;

	/** Returns the events of the session. */
	std::vector<EventJournal::Event> const& events() const noexcept;

	/** Returns the number of entities having at least one usable snapshot in the session (0 means the replay can never show anything, eg. a schema version 1 file). */
	std::size_t snapshotEntitiesCount() const noexcept;

	/**
	* @brief Moves the cursor to the given event: every entity gets the state it had right after that event.
	* @param[in] eventIndex Index of the event in events(). Out of range indexes are ignored (the cursor does not move).
	* @return The entities whose state changed compared to the previous cursor position.
	*/
	Changes seekToEvent(std::size_t const eventIndex) noexcept;

	/**
	* @brief Moves the cursor to the last event whose timestamp is at or before the given one.
	* @param[in] timestamp Milliseconds since Unix epoch, UTC. A timestamp preceding the first event empties the state (no entity is present).
	* @return The entities whose state changed compared to the previous cursor position.
	*/
	Changes seekToTimestamp(qint64 const timestamp) noexcept;

	/** Returns the index of the event the cursor is on (std::nullopt before the first seek, or after a seek before the first event). */
	std::optional<std::size_t> cursor() const noexcept;

	/** Returns the entities present at the cursor with their state. */
	EntityStates const& entities() const noexcept;

	/** Returns the state of the given entity at the cursor (nullptr if the entity is absent). */
	EntityState const* entityState(la::avdecc::UniqueIdentifier const entityID) const noexcept;

private:
	class Private;
	std::unique_ptr<Private> _pImpl;
};

} // namespace modelsLibrary
} // namespace hive
