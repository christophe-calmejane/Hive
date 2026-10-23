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

#include <la/avdecc/internals/entityModel.hpp>
#include <la/avdecc/internals/uniqueIdentifier.hpp>

#include <nlohmann/json.hpp>

#include <QString>
#include <QTimeZone>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hive
{
namespace modelsLibrary
{
/**
* @brief Analysis of a journal session: the computations the after show reports need, from the raw events.
* @details Attributes every stream event to a network plane and every entity to its rack and branch switch (Topology), then aggregates at increasing scale (entity and plane hits, rack incidents, storms, runs walking the ports of a branch switch, system wide events), pairs the error and recovered events (clock domain, stream and link durations), measures the both planes coincidences, ranks the entities, racks and branches, and covers the other error families (grandmaster changes, packet loss, controller side, operator actions, stream data losses).
*          UI independent and unit testable: the Journal Analyser shows the Markdown report in its Findings pane (with links to the events) and exports it to a file.
*          The rules are the ones of the analysis prompt (EventJournalAnalysisPrompt.md) and of the reports written by hand before this class existed; the option values are stated in the "How this analysis was produced" section of the report.
* @note Every figure is computed from the events only: a journal recorded before an event family existed has no figure for it, and the report says so instead of guessing.
*/
class JournalAnalysis final
{
public:
	using EventIndex = std::size_t;
	using EventIndexes = std::vector<EventIndex>;
	using EntityID = la::avdecc::UniqueIdentifier;
	using EntityIDs = std::vector<EntityID>;

	/** Network plane of an event, a stream or an interface: the AVB interface index (0 is the primary plane, 1 the secondary one in a Milan redundant installation), or UnknownPlane. */
	using Plane = int;
	static constexpr Plane UnknownPlane = -1;

	/** Returns the name of a plane ("Primary", "Secondary", "Interface N" or "Unknown"). */
	static QString planeName(Plane const plane) noexcept;

	/**
	* @brief Rack and branch mapping of the entities, and plane of their input streams.
	* @details Built from the 'Entity is online' details of a schema version 2 journal (the gPTP AsPath of every interface, the connected streams with their interface), completed or replaced by AVE documents (the entity snapshots of the journal, a network state file, the only source for a schema version 1 journal).
	*          The AsPath of an interface lists the clock identities from the grandmaster down to the entity: the last hop before the entity is its rack switch, the one before is its branch switch (an entity plugged directly into a branch switch is a rack of its own on that switch).
	*/
	struct Topology
	{
		struct Interface
		{
			EntityID clockIdentity{}; /**< Clock identity of the interface (the AsPath usually ends with it) */
			EntityID grandmasterID{}; /**< Grandmaster of the plane as seen by the interface */
			std::vector<EntityID> asPath{}; /**< Clock identities from the grandmaster down to the entity (empty when unknown) */

			/** Returns true if the reported clock identity contradicts the AsPath: it is the one of the grandmaster (the first element of the AsPath) while the AsPath shows the interface is not the grandmaster (a device bug seen in the field). */
			bool hasInconsistentClockIdentity() const noexcept;
		};
		struct StreamInput
		{
			Plane plane{ UnknownPlane }; /**< AVB interface the stream is attached to */
			std::optional<la::avdecc::entity::model::StreamIdentification> talker{}; /**< Talker the stream was connected to (the last known connection) */
		};
		struct Entity
		{
			QString name{};
			bool isController{ false }; /**< The entity implements a controller (another Hive, a management tool) */
			std::map<la::avdecc::entity::model::AvbInterfaceIndex, Interface> interfaces{};
			std::map<la::avdecc::entity::model::StreamIndex, StreamInput> streamInputs{};
			std::map<la::avdecc::entity::model::ClockDomainIndex, la::avdecc::entity::model::StreamIndex> clockSourceStreams{}; /**< Input stream carrying the media clock of each clock domain (when the clock source is an input stream) */
		};
		using Entities = std::unordered_map<EntityID, Entity, EntityID::hash>;

		Entities entities{};

		/** Returns true if at least one interface has an AsPath (the rack and branch computations need it). */
		bool hasAsPaths() const noexcept;

		/** Returns the clock identity of an interface: the reported one, or the one deduced from the end of the AsPath when the reported one is inconsistent (see Interface::hasInconsistentClockIdentity), invalid when unknown. */
		EntityID clockIdentity(EntityID const entityID, Plane const plane) const noexcept;

		/** Returns the hops of an interface from the grandmaster down to the switch the entity is plugged into (the AsPath without the entity's own clock identity), or an empty vector when unknown. */
		std::vector<EntityID> hops(EntityID const entityID, Plane const plane) const noexcept;

		/** Returns the rack of the entity on the given plane: the switch it is plugged into (the last hop), or the entity's own clock identity when that switch also serves other racks (an entity plugged directly into a branch switch or into the grandmaster is a rack of one), or std::nullopt when unknown. */
		std::optional<EntityID> rackSwitch(EntityID const entityID, Plane const plane) const noexcept;

		/** Returns the switch above the rack on the given plane (the one whose port a sweep visits), or std::nullopt when unknown or when the rack switch is the grandmaster. */
		std::optional<EntityID> branchSwitch(EntityID const entityID, Plane const plane) const noexcept;

		/** Returns the switches of the given plane that appear above another switch in an AsPath (they are not rack switches). */
		std::vector<EntityID> upstreamSwitches(Plane const plane) const noexcept;

		/** Returns the plane of an input stream of an entity, or UnknownPlane. */
		Plane streamPlane(EntityID const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept;

		/**
		* @brief Adds what the 'Entity is online' events of a session tell (schema version 2 journals), and follows the 'gPTP AsPath changed' and 'Connected to talker' events.
		* @details The AsPath at the end of the session is kept for an entity whose AsPath changed (a re-patch during the session is a topology event the report lists on its own).
		* @param[in] session The session.
		* @return The number of entities described.
		*/
		std::size_t addSession(EventJournal::Session const& session) noexcept;

		/**
		* @brief Adds an entity from its AVE document (the la_avdecc JSON serializer output: a snapshot of the journal, a virtual entity file).
		* @param[in] document The document.
		* @return True if the document described an entity.
		*/
		bool addEntityDocument(nlohmann::json const& document) noexcept;

		/**
		* @brief Adds the entities of a network state document (the .ans format, an 'entities' array of AVE documents).
		* @param[in] document The document.
		* @return The number of entities added.
		*/
		std::size_t addNetworkState(nlohmann::json const& document) noexcept;
	};

	/** Thresholds of the grouping rules (the defaults are the ones of the hand written reports). */
	struct Options
	{
		qint64 hitWindowMs{ 1000 }; /**< Events of the same entity and plane closer than this form one hit; hits of the same rack and plane closer than this form one incident */
		qint64 stormGapMs{ 60000 }; /**< Incidents of the same rack and plane closer than this form one storm */
		qint64 runGapMs{ 30 * 60000 }; /**< Storms of the same branch and plane closer than this form one run */
		qint64 systemWideGapMs{ 2000 }; /**< Entities hit closer than this belong to the same system wide event candidate */
		qint64 systemWideTailMs{ 15000 }; /**< A later hit of an entity already hit by a system wide event, closer than this to its end, belongs to the event (the relock wave, the second wave of a discontinuity) */
		double systemWideMinShare{ 0.25 }; /**< Share of the entities reporting at least one hit that a system wide event must reach */
		std::size_t systemWideMinEntities{ 4u }; /**< Absolute minimum of entities hit by a system wide event */
		qint64 bothPlanesWindowMs{ 5000 }; /**< A primary and a secondary hit of the same entity closer than this are a both planes pair */
		qint64 simultaneousMs{ 100 }; /**< A both planes pair closer than this is simultaneous (a common source) rather than sequential */
		qint64 accompanyingWindowMs{ 3000 }; /**< A clock domain unlock and a stream hit of the same entity closer than this accompany each other; a stream data loss and a media unlock of the same stream too */
		qint64 grandmasterStormGapMs{ 10000 }; /**< Grandmaster changes closer than this form one storm */
		qint64 waveGapMs{ 5000 }; /**< Entities going online (or offline) closer than this form one wave (a power up or a power down) */
		std::size_t waveMinEntities{ 3u }; /**< Minimum size of a wave */
		qint64 heartbeatLatencyThresholdMs{ 500 }; /**< Heartbeat event loop latency reported as an outlier */
		qint64 longInterruptionMs{ 3000 }; /**< A stream data loss whose elapsed time exceeds this is the resume edge of a long interruption */
		std::size_t rankingSize{ 10u }; /**< Rows of the rankings (worst racks, worst entities, longest unlocks, ...) */
		std::size_t listingSize{ 50u }; /**< Rows of the listings (operator actions, drop outs, both planes pairs, ...) */
	};

	/** A group of events of the same entity and plane closer than Options::hitWindowMs. */
	struct Hit
	{
		qint64 timestamp{ 0 }; /**< First event */
		qint64 endTimestamp{ 0 }; /**< Last event */
		EntityID entityID{};
		Plane plane{ UnknownPlane };
		EventIndexes events{};
		bool isClockDomain{ false }; /**< A clock domain unlock rather than a stream level unlock (only used by the system wide detection) */
	};

	/** Hits of the entities of the same rack switch and plane closer than Options::hitWindowMs. */
	struct Incident
	{
		qint64 timestamp{ 0 };
		qint64 endTimestamp{ 0 };
		EntityID rackSwitch{};
		Plane plane{ UnknownPlane };
		EntityIDs entities{}; /**< Distinct entities, in order of appearance */
		std::size_t rackSize{ 0u }; /**< Entities plugged into that rack switch on that plane */
		EventIndexes events{};
		bool isSystemWide{ false }; /**< Overlaps a system wide event (excluded from the storms and runs) */
	};

	/** Incidents of the same rack switch and plane closer than Options::stormGapMs. */
	struct Storm
	{
		qint64 timestamp{ 0 };
		qint64 endTimestamp{ 0 };
		EntityID rackSwitch{};
		std::optional<EntityID> branchSwitch{};
		Plane plane{ UnknownPlane };
		std::size_t incidents{ 0u };
		EventIndexes events{};
	};

	/** Storms of the same branch switch and plane closer than Options::runGapMs: the signature of a process walking the ports of the switch. */
	struct Run
	{
		struct RackVisit
		{
			EntityID rackSwitch{};
			qint64 timestamp{ 0 }; /**< Start of the main storm of the rack in the run (the one with the most incidents, an isolated incident before or after it is noise) */
			qint64 endTimestamp{ 0 }; /**< End of that storm */
			std::size_t storms{ 0u }; /**< Storms of the rack in the run */
			std::size_t incidents{ 0u }; /**< Incidents of the rack in the run */
			EntityIDs entities{};
		};
		qint64 timestamp{ 0 };
		qint64 endTimestamp{ 0 };
		EntityID branchSwitch{};
		Plane plane{ UnknownPlane };
		std::vector<RackVisit> racks{}; /**< In order of their main storm (a fixed non numeric order is a physical port order) */
		std::size_t storms{ 0u };
		std::size_t incidents{ 0u };
		EventIndexes events{};
	};

	/** Runs of the same branch switch and plane, with their periodicity. */
	struct RunSeries
	{
		EntityID branchSwitch{};
		Plane plane{ UnknownPlane };
		std::vector<std::size_t> runs{}; /**< Indexes in Results::runs, by ascending time */
		std::vector<std::size_t> sweeps{}; /**< The runs visiting at least two racks (the other runs are isolated storms of one rack) */
		std::optional<qint64> minPeriodMs{}; /**< Shortest interval between the starts of two consecutive sweeps (two sweeps at least) */
		std::optional<qint64> maxPeriodMs{};
		std::size_t sameOrderSweeps{ 0u }; /**< Sweeps following the most frequent rack order (every sweep when the port order is stable) */
	};

	/** Entities hit within Options::systemWideGapMs of each other, across several racks: the origin is above the racks (grandmaster, media clock talker, control action). */
	struct SystemWideEvent
	{
		struct BranchShare
		{
			std::optional<EntityID> branchSwitch{};
			Plane plane{ UnknownPlane };
			std::size_t hit{ 0u };
			std::size_t total{ 0u }; /**< Entities of the branch on that plane that reported at least one hit in the session */
		};
		qint64 timestamp{ 0 };
		qint64 endTimestamp{ 0 };
		std::vector<Plane> planes{}; /**< Planes of the stream level hits, ascending */
		EntityIDs entities{}; /**< Distinct entities hit, in order of appearance */
		std::size_t racks{ 0u }; /**< Distinct rack switches hit (one per plane in a redundant installation) */
		std::size_t branches{ 0u }; /**< Distinct branch switches hit */
		std::vector<BranchShare> branchShares{}; /**< Which branches were hit, and how completely */
		EntityIDs spared{}; /**< Entities reporting hits elsewhere in the session but not here */
		EntityIDs clockDomainEntities{}; /**< Entities whose clock domain unlocked during the event */
		std::optional<qint64> minClockDomainUnlockMs{}; /**< Shortest paired clock domain unlock of the event */
		std::optional<qint64> maxClockDomainUnlockMs{};
		std::size_t grandmasterChanges{ 0u }; /**< Accompanying events within Options::accompanyingWindowMs */
		std::size_t linkEvents{ 0u };
		std::size_t packetLossEvents{ 0u };
		std::size_t streamDataLosses{ 0u };
		EventIndexes events{};
	};

	/** A primary and a secondary hit of the same entity closer than Options::bothPlanesWindowMs. */
	struct BothPlanesPair
	{
		EntityID entityID{};
		qint64 timestamp{ 0 }; /**< Earliest of the two hits */
		qint64 delayMs{ 0 }; /**< Time between the two hits */
		bool isSystemWide{ false }; /**< Inside a system wide event */
		EventIndexes events{}; /**< The events of both hits */
	};

	/** Per entity and plane hit counts (the ranking). */
	struct EntityHits
	{
		EntityID entityID{};
		std::size_t total{ 0u };
		std::map<Plane, std::size_t> perPlane{};
		std::size_t clockDomainUnlocks{ 0u }; /**< Unlocks of the clock domains of the entity (a clock domain has no plane) */
	};

	/** Per rack switch and plane incident counts. */
	struct RackHits
	{
		EntityID rackSwitch{};
		std::optional<EntityID> branchSwitch{};
		Plane plane{ UnknownPlane };
		std::size_t incidents{ 0u };
		std::size_t backgroundIncidents{ 0u }; /**< Outside the system wide events */
		EntityIDs entities{}; /**< Entities plugged into the rack on that plane */
	};

	/** Per branch switch and plane incident counts. */
	struct BranchHits
	{
		std::optional<EntityID> branchSwitch{};
		Plane plane{ UnknownPlane };
		std::size_t incidents{ 0u };
		std::size_t backgroundIncidents{ 0u };
		std::size_t racks{ 0u };
	};

	/** An error event paired with the recovered event that closed it (clock domain unlock, stream unlock, link down, entity offline). */
	struct Interval
	{
		EntityID entityID{};
		QString subject{};
		Plane plane{ UnknownPlane };
		qint64 timestamp{ 0 };
		std::optional<qint64> durationMs{}; /**< std::nullopt when the pair was inferred from the counters (duration unknown) or never closed */
		bool isInferred{ false };
		bool isClosed{ false };
		EventIndex startEvent{ 0u };
		std::optional<EventIndex> endEvent{};
	};

	/** Distribution of durations. */
	struct DurationStats
	{
		std::size_t count{ 0u }; /**< Pairs with a known duration */
		std::size_t unknown{ 0u }; /**< Inferred pairs and pairs never closed */
		qint64 minMs{ 0 };
		qint64 medianMs{ 0 };
		qint64 maxMs{ 0 };
		qint64 totalMs{ 0 };
	};

	/** What accompanied the clock domain unlocks at stream level (same entity, within Options::accompanyingWindowMs). */
	struct ClockDomainCompanions
	{
		std::size_t primaryOnly{ 0u };
		std::size_t secondaryOnly{ 0u };
		std::size_t bothPlanes{ 0u };
		std::size_t none{ 0u };
	};

	/** Entities whose clock domain unlocks at the system wide events (a per device property when the same subset unlocks every time). */
	struct ClockDomainGroup
	{
		EntityID entityID{};
		std::size_t events{ 0u }; /**< System wide events with a clock domain unlock of the entity */
	};

	/** Grandmaster changes closer than Options::grandmasterStormGapMs. */
	struct GrandmasterStorm
	{
		struct Flip
		{
			EntityID from{};
			EntityID to{};
			std::size_t count{ 0u };
		};
		qint64 timestamp{ 0 };
		qint64 endTimestamp{ 0 };
		std::vector<Plane> planes{};
		std::size_t changes{ 0u };
		EntityIDs entities{};
		std::vector<Flip> flips{}; /**< By descending count */
		std::size_t packetLossEvents{ 0u }; /**< Within the storm, extended by Options::accompanyingWindowMs */
		std::size_t clockDomainUnlocks{ 0u };
		EventIndexes events{};
	};

	/** Count of an event family per entity, subject and plane (packet loss counters, controller side counters, ...). */
	struct SubjectCount
	{
		EntityID entityID{}; /**< Invalid for session wide subjects */
		QString subject{};
		Plane plane{ UnknownPlane };
		QString name{}; /**< Counter or family name */
		std::size_t count{ 0u };
		std::size_t entities{ 0u }; /**< Distinct entities (for the counts aggregated over the entities) */
		EventIndexes events{};
	};

	/** A wave of entities going online or offline together. */
	struct Wave
	{
		qint64 timestamp{ 0 };
		qint64 endTimestamp{ 0 };
		bool isOnline{ false };
		EntityIDs entities{};
		EventIndexes events{};
	};

	/** An entity that went offline and came back. */
	struct DropOut
	{
		EntityID entityID{};
		qint64 timestamp{ 0 };
		std::optional<qint64> durationMs{}; /**< std::nullopt when the entity never came back */
		bool isRefresh{ false }; /**< Preceded by an 'Entity refresh requested' (Hive re-enumerated the entity, not a network event) */
		EventIndexes events{};
	};

	/** A gap between two heartbeats larger than expected (the controller was frozen or stopped). */
	struct HeartbeatGap
	{
		qint64 timestamp{ 0 };
		qint64 durationMs{ 0 };
		EventIndexes events{};
	};

	/** Coverage of the stream data loss rule for one stream. */
	struct MonitoredStream
	{
		EntityID entityID{};
		QString subject{};
		Plane plane{ UnknownPlane };
		qint64 expectedIncrement{ 0 };
		std::optional<la::avdecc::entity::model::StreamIdentification> talker{};
		bool isCovered{ false }; /**< A 'Stream data monitoring started' event exists (diagnostic level); a stream with losses only has its reference from the losses */
		std::size_t losses{ 0u };
		qint64 totalMissingMs{ 0 };
		EventIndexes events{}; /**< The monitoring started event, then the losses */
	};

	/** A connected stream the stream data loss rule never covered. */
	struct UncoveredStream
	{
		EntityID entityID{};
		la::avdecc::entity::model::StreamIndex streamIndex{ 0u };
		Plane plane{ UnknownPlane };
		std::optional<la::avdecc::entity::model::StreamIdentification> talker{};
	};

	/** The resume edge of a long stream interruption, with its start edge when the journal has it. */
	struct LongInterruption
	{
		EntityID entityID{};
		QString subject{};
		Plane plane{ UnknownPlane };
		qint64 timestamp{ 0 }; /**< Resume edge */
		qint64 elapsedMs{ 0 }; /**< Silence duration */
		std::optional<EventIndex> startEvent{};
		EventIndex resumeEvent{ 0u };
	};

	/** Stream data losses of the listeners of one talker within Options::hitWindowMs: the common hop of the listeners hit names the switch or port that dropped the frames. */
	struct StreamDataIncident
	{
		qint64 timestamp{ 0 };
		qint64 endTimestamp{ 0 };
		la::avdecc::entity::model::StreamIdentification talker{};
		Plane plane{ UnknownPlane };
		EntityIDs listeners{}; /**< Listeners hit */
		std::size_t talkerListeners{ 0u }; /**< Listeners of that talker stream on that plane known to the topology */
		std::optional<EntityID> commonHop{}; /**< Deepest switch common to the AsPath of every listener hit (needs the topology) */
		EntityIDs sparedBehindHop{}; /**< Listeners of the talker behind the common hop that were not hit */
		EventIndexes events{};
	};

	/** Everything computed. */
	struct Results
	{
		// Session shape
		qint64 firstTimestamp{ 0 };
		qint64 lastTimestamp{ 0 };
		std::size_t entitiesCount{ 0u }; /**< Distinct entities with an 'Entity is online' event */
		std::size_t entitiesOnlineAtStart{ 0u }; /**< Online within the first minute of the session */
		EntityIDs controllers{}; /**< Other controllers seen on the network */
		bool isInterrupted{ false }; /**< The last event is not 'Recording stopped' */
		EventIndexes markers{};
		EventIndexes sessionEvents{}; /**< Recording, controller, level, export and clear events */
		EventIndexes controllerInterfaceEvents{};
		std::vector<Wave> waves{};
		std::vector<DropOut> dropOuts{};
		std::vector<HeartbeatGap> heartbeatGaps{};
		std::optional<qint64> heartbeatIntervalMs{}; /**< Median interval between the heartbeats (std::nullopt without heartbeats) */
		EventIndexes heartbeatLatencyOutliers{};
		std::size_t heartbeats{ 0u };

		// Breakdown
		std::map<std::pair<EventJournal::Category, EventJournal::Severity>, std::size_t> categoryCounts{};
		std::map<QString, std::size_t> familyCounts{}; /**< By normalized summary (numbers and names stripped) */
		std::vector<EventJournal::Category> absentCategories{};

		// Media unlock hits (the timing family)
		std::size_t unlockEvents{ 0u };
		std::map<Plane, std::size_t> unlockEventsPerPlane{};
		std::vector<Hit> hits{}; /**< Stream level hits, by ascending time */
		std::map<Plane, std::size_t> hitsPerPlane{};
		std::vector<Incident> incidents{};
		std::map<Plane, std::size_t> incidentsPerPlane{};
		std::size_t fullRackIncidents{ 0u }; /**< Incidents involving every entity of the rack */
		std::vector<Storm> storms{};
		std::vector<Run> runs{};
		std::vector<RunSeries> runSeries{};
		std::vector<SystemWideEvent> systemWideEvents{};
		std::size_t systemWideIncidents{ 0u };
		std::vector<EntityHits> entityRanking{}; /**< By descending total, every entity with a hit or a clock domain unlock */
		std::size_t medianEntityHits{ 0u };
		std::vector<RackHits> rackRanking{}; /**< By descending incidents */
		std::vector<BranchHits> branchRanking{};
		std::vector<BothPlanesPair> bothPlanesPairs{};
		std::size_t primaryOnlyHits{ 0u };
		std::size_t secondaryOnlyHits{ 0u };
		std::optional<qint64> bothPlanesMedianDelayMs{};
		std::optional<qint64> bothPlanesMinDelayMs{};
		std::optional<qint64> bothPlanesMaxDelayMs{};
		std::size_t simultaneousPairs{ 0u };

		// Clock domain
		std::vector<Interval> clockDomainIntervals{};
		DurationStats clockDomainStats{};
		std::vector<std::size_t> longestClockDomainIntervals{}; /**< Indexes in clockDomainIntervals, by descending duration */
		ClockDomainCompanions clockDomainCompanions{};
		std::size_t hitsWithClockDomainUnlock{ 0u }; /**< Stream hits accompanied by a clock domain unlock of the entity */
		std::vector<ClockDomainGroup> clockDomainGroup{}; /**< Entities unlocking their clock domain at the system wide events, by descending count */
		std::size_t systemWideEventsWithClockDomainUnlocks{ 0u };

		// Stream level lock pairs (journals with 'Media unlocked' / 'Media locked')
		std::vector<Interval> streamIntervals{};
		std::map<Plane, DurationStats> streamStatsPerPlane{};
		std::vector<Interval> bothPlanesUnlocked{}; /**< Periods where a primary and a secondary stream of the same entity were unlocked at the same time (subject names the streams) */

		// Other families
		std::vector<GrandmasterStorm> grandmasterStorms{};
		std::size_t grandmasterChanges{ 0u };
		std::size_t grandmasterCounterIncreases{ 0u };
		std::size_t asPathChanges{ 0u };
		std::vector<Interval> linkIntervals{};
		std::vector<Interval> nullPropagationDelayIntervals{}; /**< Links whose propagation delay dropped to zero, closed when measured again */
		std::vector<SubjectCount> packetLossCounts{}; /**< Per counter, entity, subject; by descending count */
		std::map<QString, std::size_t> packetLossPerCounter{};
		std::map<std::pair<QString, Plane>, std::size_t> packetLossPerCounterAndPlane{};
		std::vector<SubjectCount> controllerCounts{}; /**< Controller side counters (Statistics subject), per counter and interface */
		std::size_t unsolicitedNotificationsLost{ 0u }; /**< Unsolicited notifications actually lost, as told by the sequence IDs of the loss events (the counters only count the detections; zero for the journals recorded before the sequence IDs were journaled) */
		std::size_t latencyErrors{ 0u };
		std::size_t redundancyWarnings{ 0u };
		std::map<Plane, std::size_t> offlineOnOneInterface{};
		std::map<Plane, std::size_t> unsolicitedLossesPerInterface{};
		std::size_t transportErrors{ 0u };
		std::size_t connectionChanges{ 0u };
		EventIndexes operatorActions{}; /**< Control events and user level configuration events */

		// Stream data losses (journals with 'streaming' events)
		bool hasStreamDataEvents{ false };
		std::vector<MonitoredStream> monitoredStreams{};
		std::vector<UncoveredStream> uncoveredStreams{};
		std::size_t streamDataLosses{ 0u };
		std::map<Plane, std::size_t> streamDataLossesPerPlane{};
		std::map<qint64, std::size_t> missingMsHistogram{}; /**< Lower bound of the bucket (0, 100, 250, 500, 750) to count */
		std::vector<BothPlanesPair> streamDataBothPlanes{}; /**< Losses of the two planes of the same entity within the same second */
		std::vector<LongInterruption> longInterruptions{};
		std::size_t lossesWithMediaUnlock{ 0u }; /**< Losses accompanied by a media unlock of the same stream */
		std::vector<StreamDataIncident> streamDataIncidents{};
	};

	/** Rendering options of the Markdown report. */
	struct ReportOptions
	{
		QString title{}; /**< Title of the report (the journal file name by default) */
		QTimeZone timeZone{ QTimeZone::systemTimeZone() }; /**< Time zone of every time in the report (the journal stores UTC) */
		bool eventLinks{ false }; /**< Render the times as "event:<index>" links (the Findings pane of the analyser), or as plain text (the exported file) */
	};

	/**
	* @brief Analyzes a session.
	* @param[in] session The session (events by ascending timestamp).
	* @param[in] topology The topology (built from the session and possibly a network state file); an empty topology limits the analysis to what the events tell.
	* @param[in] options The grouping thresholds.
	*/
	JournalAnalysis(EventJournal::Session const& session, Topology const& topology, Options const& options);

	/** Analyzes a session with the default options. */
	JournalAnalysis(EventJournal::Session const& session, Topology const& topology);
	~JournalAnalysis() noexcept;

	// Deleted compiler auto-generated methods
	JournalAnalysis(JournalAnalysis const&) = delete;
	JournalAnalysis(JournalAnalysis&&) = delete;
	JournalAnalysis& operator=(JournalAnalysis const&) = delete;
	JournalAnalysis& operator=(JournalAnalysis&&) = delete;

	/** Returns the results. */
	Results const& results() const noexcept;

	/** Returns the topology used. */
	Topology const& topology() const noexcept;

	/** Returns the options used. */
	Options const& options() const noexcept;

	/** Returns the last name an entity had in the session (or its identifier when it never had one). */
	QString entityName(EntityID const entityID) const noexcept;

	/** Returns the plane of an event, from its subject and details (the topology for the input streams, else the naming of the stream). */
	Plane eventPlane(EventJournal::Event const& event) const noexcept;

	/**
	* @brief Renders the Markdown report: the structure of the hand written analysis reports, with every figure computed here.
	* @param[in] session The session the analysis was made from (the events are quoted by the report).
	* @param[in] options Rendering options.
	* @return The report.
	*/
	QString markdownReport(EventJournal::Session const& session, ReportOptions const& options) const;

	/** Renders the Markdown report with the default rendering options (plain times, system time zone). */
	QString markdownReport(EventJournal::Session const& session) const;

	/** Implementation (shared by the computations and the report rendering, not part of the API). */
	class Private;

private:
	std::unique_ptr<Private> _pImpl;
};

} // namespace modelsLibrary
} // namespace hive
