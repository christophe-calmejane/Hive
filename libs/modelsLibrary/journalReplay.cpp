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

#include <hive/modelsLibrary/journalReplay.hpp>

#include <QLatin1String>

#include <algorithm>
#include <iterator>
#include <string>
#include <tuple>
#include <utility>

namespace hive
{
namespace modelsLibrary
{
namespace
{
/** Summary of the event ending the presence of an entity (see EventJournal's handleEntityOffline). */
constexpr auto OfflineSummary = "Entity went offline";

/** Summary of the session event ending the presence of every entity (the controller stopped, the entities are not journaled offline one by one). */
constexpr auto ControllerStoppedSummary = "Controller stopped";

/** Returns true if the snapshots of the given reason describe the entities from the first event of the session (they are written right after that event, see EventJournal's startSession and clearCurrentSession). */
bool isSessionStartReason(QString const& reason) noexcept
{
	return reason == "session_start" || reason == "cleared";
}

/** Returns true if the details of an event may hold a patch: a cheap check before parsing the JSON of every event of the session (a false positive is harmless, the parse decides). */
bool mayHavePatch(QString const& details) noexcept
{
	return details.contains(QLatin1String{ "\"patch\"" });
}

/** Applies the patch of an event to a document. Returns std::nullopt if the event has no patch, false if the patch was rejected (the operations preceding the failing one stay applied). */
std::optional<bool> applyPatch(nlohmann::json& document, EventJournal::Event const& event) noexcept
{
	try
	{
		auto const details = nlohmann::json::parse(event.details.toStdString());
		auto const it = details.find(EventJournal::PatchDetailsKey);
		if (it == details.end() || !it->is_array())
		{
			return std::nullopt;
		}
		document.patch_inplace(*it);
		return true;
	}
	catch (...)
	{
		return false;
	}
}
} // namespace

class JournalReplay::Private final
{
public:
	Private(std::vector<EventJournal::Event> events, std::vector<EventJournal::SnapshotInfo> snapshots, SnapshotLoader loader) noexcept
		: _events{ std::move(events) }
		, _loader{ std::move(loader) }
	{
		// A snapshot describes the entity from its timestamp on, except that the snapshots taken when a session starts (or restarts after a clear) are written a few milliseconds after the first event and describe the entity from that first event on, so the cursor on the first events of the session is not an empty network
		auto const firstTimestamp = _events.empty() ? qint64{ 0 } : _events.front().timestamp;
		for (auto& snapshot : snapshots)
		{
			if (snapshot.format != EventJournal::SnapshotFormat || !snapshot.entityID)
			{
				continue;
			}
			auto const effectiveTimestamp = isSessionStartReason(snapshot.reason) ? std::min(snapshot.timestamp, firstTimestamp) : snapshot.timestamp;
			_tracks[snapshot.entityID].anchors.push_back(Anchor{ std::move(snapshot), effectiveTimestamp });
		}
		for (auto& [entityID, track] : _tracks)
		{
			// The index is ordered by timestamp then id, and clamping to the first timestamp keeps that order, but sort anyway so the binary searches never depend on the reader
			std::stable_sort(track.anchors.begin(), track.anchors.end(),
				[](Anchor const& lhs, Anchor const& rhs)
				{
					return std::tie(lhs.effectiveTimestamp, lhs.info.id) < std::tie(rhs.effectiveTimestamp, rhs.info.id);
				});
		}

		// Events that matter for the state of an entity: the ones carrying a patch, and the ones ending its presence (its own offline event, or the controller stopping)
		for (auto index = std::size_t{ 0u }; index < _events.size(); ++index)
		{
			auto const& event = _events[index];
			if (!event.entityID)
			{
				if (event.category == EventJournal::Category::Session && event.summary == ControllerStoppedSummary)
				{
					_controllerStops.push_back(index);
				}
				continue;
			}
			auto const it = _tracks.find(event.entityID);
			if (it == _tracks.end())
			{
				// Without a snapshot the entity can never be rebuilt, its events are of no use to the replay
				continue;
			}
			if (event.category == EventJournal::Category::Entity && event.summary == OfflineSummary)
			{
				it->second.offlines.push_back(index);
			}
			else if (mayHavePatch(event.details))
			{
				it->second.patches.push_back(index);
			}
		}
	}

	std::vector<EventJournal::Event> const& events() const noexcept
	{
		return _events;
	}

	std::size_t snapshotEntitiesCount() const noexcept
	{
		return _tracks.size();
	}

	Changes seek(std::optional<std::size_t> const cursor) noexcept
	{
		auto changes = Changes{};
		_cursor = cursor;
		if (!cursor)
		{
			for (auto const& [entityID, state] : _states)
			{
				changes.removed.push_back(entityID);
			}
			_states.clear();
			_lastPatches.clear();
			return changes;
		}

		auto const cursorIndex = *cursor;
		auto const cursorTimestamp = _events[cursorIndex].timestamp;
		for (auto const& [entityID, track] : _tracks)
		{
			// Anchor: the last snapshot describing the entity at the cursor
			auto const anchorIt = std::upper_bound(track.anchors.begin(), track.anchors.end(), cursorTimestamp,
				[](qint64 const timestamp, Anchor const& anchor)
				{
					return timestamp < anchor.effectiveTimestamp;
				});
			if (anchorIt == track.anchors.begin())
			{
				remove(entityID, changes);
				continue;
			}
			auto const& anchor = *std::prev(anchorIt);

			// An entity that went offline after the anchor is absent (its next snapshot, if it came back, would have been the anchor)
			if (hasEventBetween(track.offlines, anchor.effectiveTimestamp, cursorIndex) || hasEventBetween(_controllerStops, anchor.effectiveTimestamp, cursorIndex))
			{
				remove(entityID, changes);
				continue;
			}

			// Patches to apply on top of the anchor: journaled at or after it, up to the cursor
			auto const firstPatchIt = std::lower_bound(track.patches.begin(), track.patches.end(), anchor.effectiveTimestamp,
				[this](std::size_t const index, qint64 const timestamp)
				{
					return _events[index].timestamp < timestamp;
				});
			auto const endPatchIt = std::upper_bound(firstPatchIt, track.patches.end(), cursorIndex);
			auto const lastPatch = firstPatchIt == endPatchIt ? std::optional<std::size_t>{} : std::optional<std::size_t>{ *std::prev(endPatchIt) };

			auto const stateIt = _states.find(entityID);
			if (stateIt != _states.end() && stateIt->second.snapshotID == anchor.info.id)
			{
				auto const& cachedLastPatch = _lastPatches[entityID];
				if (cachedLastPatch == lastPatch)
				{
					// Same anchor, same patches: nothing changed for this entity
					continue;
				}
				if (!cachedLastPatch || (lastPatch && *cachedLastPatch < *lastPatch))
				{
					// The cursor moved forward from the same anchor: only the patches journaled since the previous position are applied
					auto const resumeIt = cachedLastPatch ? std::upper_bound(firstPatchIt, endPatchIt, *cachedLastPatch) : firstPatchIt;
					apply(stateIt->second, resumeIt, endPatchIt);
					_lastPatches[entityID] = lastPatch;
					changes.updated.push_back(entityID);
					continue;
				}
			}

			// New anchor, or the cursor moved backward: rebuild from the snapshot
			auto state = EntityState{};
			if (!load(anchor.info, state))
			{
				remove(entityID, changes);
				continue;
			}
			apply(state, firstPatchIt, endPatchIt);
			_states[entityID] = std::move(state);
			_lastPatches[entityID] = lastPatch;
			changes.updated.push_back(entityID);
		}
		return changes;
	}

	std::optional<std::size_t> cursor() const noexcept
	{
		return _cursor;
	}

	EntityStates const& entities() const noexcept
	{
		return _states;
	}

private:
	/** A snapshot of an entity, with the timestamp from which it describes the entity. */
	struct Anchor
	{
		EventJournal::SnapshotInfo info{};
		qint64 effectiveTimestamp{ 0 };
	};

	/** What the replay knows about an entity: its snapshots and the indexes of its relevant events, all in ascending order. */
	struct Track
	{
		std::vector<Anchor> anchors{};
		std::vector<std::size_t> patches{}; /**< Events carrying a patch */
		std::vector<std::size_t> offlines{}; /**< Events ending the presence of the entity */
	};

	/** Returns true if one of the given events (indexes in ascending order) was journaled at or after the given timestamp and at or before the given index. */
	bool hasEventBetween(std::vector<std::size_t> const& indexes, qint64 const fromTimestamp, std::size_t const toIndex) const noexcept
	{
		auto const it = std::lower_bound(indexes.begin(), indexes.end(), fromTimestamp,
			[this](std::size_t const index, qint64 const timestamp)
			{
				return _events[index].timestamp < timestamp;
			});
		return it != indexes.end() && *it <= toIndex;
	}

	/** Loads the document of a snapshot into a state. Returns false if the snapshot cannot be loaded or decoded. */
	bool load(EventJournal::SnapshotInfo const& info, EntityState& state) const noexcept
	{
		auto const data = _loader ? _loader(info.id) : QByteArray{};
		if (data.isEmpty())
		{
			return false;
		}
		try
		{
			state.document = nlohmann::json::from_msgpack(data.begin(), data.end());
		}
		catch (...)
		{
			return false;
		}
		state.snapshotID = info.id;
		state.snapshotTimestamp = info.timestamp;
		state.snapshotReason = info.reason;
		state.appliedPatches = 0u;
		state.rejectedPatches = 0u;
		return true;
	}

	/** Applies the patches of the given events (indexes in ascending order) to a state. */
	void apply(EntityState& state, std::vector<std::size_t>::const_iterator const first, std::vector<std::size_t>::const_iterator const last) const noexcept
	{
		for (auto it = first; it != last; ++it)
		{
			if (auto const applied = applyPatch(state.document, _events[*it]))
			{
				if (*applied)
				{
					++state.appliedPatches;
				}
				else
				{
					++state.rejectedPatches;
				}
			}
		}
	}

	/** Makes an entity absent, reporting it if it was present. */
	void remove(la::avdecc::UniqueIdentifier const entityID, Changes& changes) noexcept
	{
		if (_states.erase(entityID) > 0u)
		{
			changes.removed.push_back(entityID);
		}
		_lastPatches.erase(entityID);
	}

	std::vector<EventJournal::Event> _events{};
	SnapshotLoader _loader{};
	std::unordered_map<la::avdecc::UniqueIdentifier, Track, la::avdecc::UniqueIdentifier::hash> _tracks{};
	std::vector<std::size_t> _controllerStops{}; /**< Events ending the presence of every entity */
	std::optional<std::size_t> _cursor{};
	EntityStates _states{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::optional<std::size_t>, la::avdecc::UniqueIdentifier::hash> _lastPatches{}; /**< Index of the last patch applied to the state of each present entity (std::nullopt when the state is the bare snapshot) */
};

JournalReplay::JournalReplay(std::vector<EventJournal::Event> events, std::vector<EventJournal::SnapshotInfo> snapshots, SnapshotLoader loader)
	: _pImpl{ std::make_unique<Private>(std::move(events), std::move(snapshots), std::move(loader)) }
{
}

JournalReplay::~JournalReplay() noexcept = default;

std::vector<EventJournal::Event> const& JournalReplay::events() const noexcept
{
	return _pImpl->events();
}

std::size_t JournalReplay::snapshotEntitiesCount() const noexcept
{
	return _pImpl->snapshotEntitiesCount();
}

JournalReplay::Changes JournalReplay::seekToEvent(std::size_t const eventIndex) noexcept
{
	if (eventIndex >= _pImpl->events().size())
	{
		return {};
	}
	return _pImpl->seek(eventIndex);
}

JournalReplay::Changes JournalReplay::seekToTimestamp(qint64 const timestamp) noexcept
{
	auto const& events = _pImpl->events();
	auto const it = std::upper_bound(events.begin(), events.end(), timestamp,
		[](qint64 const value, EventJournal::Event const& event)
		{
			return value < event.timestamp;
		});
	if (it == events.begin())
	{
		return _pImpl->seek(std::nullopt);
	}
	return _pImpl->seek(static_cast<std::size_t>(std::distance(events.begin(), std::prev(it))));
}

std::optional<std::size_t> JournalReplay::cursor() const noexcept
{
	return _pImpl->cursor();
}

JournalReplay::EntityStates const& JournalReplay::entities() const noexcept
{
	return _pImpl->entities();
}

JournalReplay::EntityState const* JournalReplay::entityState(la::avdecc::UniqueIdentifier const entityID) const noexcept
{
	auto const& states = _pImpl->entities();
	auto const it = states.find(entityID);
	return it == states.end() ? nullptr : &it->second;
}

} // namespace modelsLibrary
} // namespace hive
