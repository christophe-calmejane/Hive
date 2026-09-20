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

#include <hive/modelsLibrary/journalAnalysis.hpp>

#include <QString>

#include <cstddef>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hive
{
namespace modelsLibrary
{
/** Implementation of JournalAnalysis: the computations (journalAnalysis.cpp) and the Markdown rendering (journalAnalysisReport.cpp) share it. */
class JournalAnalysis::Private final
{
public:
	/** What an event is for the analysis (the summaries and details decoded once). */
	enum class Kind
	{
		Other,
		RecordingStarted,
		RecordingStopped,
		ControllerStopped,
		ControllerRestarted,
		SessionOther, /**< Journal cleared or exported, recording level changes, snapshots */
		Heartbeat,
		Marker,
		EntityOnline,
		EntityOffline,
		RefreshRequested,
		ControllerInterface, /**< The network interfaces of the controller computer */
		MediaUnlock, /**< Stream level unlock: 'Media unlocked', or the 'Media Unlocked' error counter of older journals */
		MediaLock, /**< Stream level relock closing an unlock */
		ClockUnlock,
		ClockLock,
		PacketLoss, /**< Stream input error counters of the packet family */
		ControllerCounter, /**< Controller side error counters (Statistics subject) */
		LinkDown,
		LinkUp,
		GrandmasterChange,
		GrandmasterCounter,
		AsPathChange,
		StreamDataLoss,
		StreamDataMonitoring,
		Latency,
		OfflineOnOneInterface,
		BackOnlineOnInterface,
		UnsolicitedLoss,
		UnsolicitedBack,
		TransportError,
		RedundancyWarning,
		Connection, /**< User level stream connection changes */
		OperatorAction, /**< Control events and user level configuration changes */
		ChangeTracking, /**< Diagnostic level change tracking events */
	};

	/** What the analysis needs from an event, decoded once. */
	struct Info
	{
		Kind kind{ Kind::Other };
		Plane plane{ UnknownPlane }; /**< Plane of the event (see eventPlane) */
		Plane detailsPlane{ UnknownPlane }; /**< Plane named by the details (controller side counters) */
		bool flag{ false }; /**< inferred (lock and link events), is_controller (entity online) */
		bool isPaired{ false }; /**< A stream level unlock that has a relock event ('Media unlocked' summary) */
		QString name{}; /**< Counter name */
		qint64 value{ 0 }; /**< missing_ms (stream data loss), event_loop_latency_ms (heartbeat) */
		qint64 secondaryValue{ 0 }; /**< elapsed_ms (stream data loss), entities_online (heartbeat) */
		qint64 increment{ 0 }; /**< Stream data loss increment */
		qint64 expected{ 0 }; /**< Stream data expected increment */
		EntityID from{}; /**< Previous grandmaster, or the talker of a stream data event */
		EntityID to{}; /**< New grandmaster */
		int talkerStreamIndex{ -1 };
	};

	Private(EventJournal::Session const& session, Topology const& topology, Options const& options);

	/** Decodes an event. */
	static Info parse(EventJournal::Event const& event) noexcept;

	/** Returns the plane of an event from its subject (the topology for the input streams, the naming of the stream otherwise) and the plane its details name. */
	Plane eventPlane(EventJournal::Event const& event, Plane const detailsPlane) const noexcept;

	/** Returns the last name an entity had. */
	QString entityName(EntityID const entityID) const noexcept;

	/** Returns the rack of an entity on a plane (the first known interface for a clock domain event without plane), cached. */
	std::optional<EntityID> rackOf(EntityID const entityID, Plane const plane) const noexcept;

	/** Returns the branch of an entity on a plane, cached. */
	std::optional<EntityID> branchOf(EntityID const entityID, Plane const plane) const noexcept;

	/** Returns true if the period overlaps a system wide event (extended by the hit window). */
	bool isInsideSystemWideEvent(qint64 const timestamp, qint64 const endTimestamp) const noexcept;

	Topology topology{};
	Options options{};
	Results results{};
	std::vector<Info> infos{}; /**< Parallel to the events */
	std::unordered_map<EntityID, QString, EntityID::hash> names{};
	std::vector<Hit> clockHits{}; /**< Clock domain unlocks grouped like the stream hits (system wide detection, entity ranking) */
	std::map<std::pair<EntityID, Plane>, std::size_t> hitsPerEntityPlane{};
	mutable std::map<std::pair<EntityID, Plane>, std::optional<EntityID>> rackCache{};
	mutable std::map<std::pair<EntityID, Plane>, std::optional<EntityID>> branchCache{};

private:
	void computeSessionShape(std::vector<EventJournal::Event> const& events);
	void computeBreakdown(std::vector<EventJournal::Event> const& events);
	void computeHits(std::vector<EventJournal::Event> const& events);
	void computeIntervals(std::vector<EventJournal::Event> const& events);
	void computeSystemWideEvents(std::vector<EventJournal::Event> const& events);
	void computeIncidents(std::vector<EventJournal::Event> const& events);
	void computeStorms();
	void computeRuns();
	void computeRankings();
	void computeBothPlanes();
	void computeClockDomain(std::vector<EventJournal::Event> const& events);
	void computeOtherFamilies(std::vector<EventJournal::Event> const& events);
	void computeStreamData(std::vector<EventJournal::Event> const& events);
};

} // namespace modelsLibrary
} // namespace hive
