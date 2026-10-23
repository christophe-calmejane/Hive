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

#include <gtest/gtest.h>
#include <hive/modelsLibrary/eventJournal.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/journalAnalysis.hpp>

#include <nlohmann/json.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTimeZone>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace
{
using hive::modelsLibrary::EventJournal;
using hive::modelsLibrary::JournalAnalysis;
using Category = EventJournal::Category;
using Severity = EventJournal::Severity;
using Level = EventJournal::Level;
using EntityID = la::avdecc::UniqueIdentifier;

constexpr auto Base = qint64{ 1789000000000 }; // Some instant in 2026
constexpr auto Second = qint64{ 1000 };
constexpr auto Minute = qint64{ 60000 };
constexpr auto Hour = qint64{ 3600000 };

// A small installation: two grandmasters, one branch switch per plane, two racks of three amplifiers behind it, one unit plugged directly into the branch switch
constexpr auto GrandmasterPrimary = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFE0000A0u };
constexpr auto GrandmasterSecondary = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFE0000B0u };
constexpr auto BranchPrimary = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFE0000A1u };
constexpr auto BranchSecondary = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFE0000B1u };
constexpr auto Rack1Primary = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFE0000A2u };
constexpr auto Rack1Secondary = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFE0000B2u };
constexpr auto Rack2Primary = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFE0000A3u };
constexpr auto Rack2Secondary = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFE0000B3u };
constexpr auto TalkerID = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFF000100u }; // Media clock and audio talker, plugged into the branch switches
constexpr auto DirectID = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFF000200u }; // Plugged into the branch switches, a rack of one
constexpr auto AmplifierBase = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFF000010u }; // Six amplifiers, 3 per rack

/** Identifier of an amplifier (0 to 5, the first three in rack 1). */
EntityID amplifier(std::size_t const index) noexcept
{
	return EntityID{ AmplifierBase + index };
}

/** Clock identity of an entity (FFFE instead of FFFF, as devices derive both from their MAC address). */
EntityID clockIdentity(EntityID const entityID, int const plane) noexcept
{
	return EntityID{ (entityID.getValue() & ~0x0000FFFF00000000u) | 0x0000FFFE00000000u | (plane == 0 ? 0u : 0x8u) };
}

QString idString(EntityID const entityID)
{
	return hive::modelsLibrary::helper::uniqueIdentifierToString(entityID);
}

QString idString(la::avdecc::UniqueIdentifier::value_type const value)
{
	return idString(EntityID{ value });
}

/** Builds a session from events, ordered by timestamp. */
struct SessionBuilder
{
	EventJournal::Session session{};

	void add(qint64 const timestamp, Category const category, Severity const severity, EntityID const entityID, QString const& entityName, QString const& subject, QString const& summary, QJsonObject const& details = {}, Level const level = Level::User)
	{
		auto event = EventJournal::Event{};
		event.timestamp = timestamp;
		event.category = category;
		event.severity = severity;
		event.level = level;
		event.entityID = entityID;
		event.entityName = entityName;
		event.subject = subject;
		event.summary = summary;
		if (!details.isEmpty())
		{
			event.details = QString::fromUtf8(QJsonDocument{ details }.toJson(QJsonDocument::Compact));
		}
		session.events.push_back(event);
	}

	void session_(qint64 const timestamp, QString const& summary, QJsonObject const& details = {}, Level const level = Level::User)
	{
		add(timestamp, Category::Session, Severity::Info, {}, {}, {}, summary, details, level);
	}

	QString name(EntityID const entityID) const
	{
		if (entityID == EntityID{ TalkerID })
		{
			return "Talker";
		}
		if (entityID == EntityID{ DirectID })
		{
			return "Direct Unit";
		}
		return QString{ "Amp %1" }.arg(entityID.getValue() - AmplifierBase + 1u);
	}

	/** 'Entity is online' with the schema version 2 details describing the entity (AsPath on both planes, media clock stream inputs 0 / 1 connected to the talker). */
	void online(qint64 const timestamp, EntityID const entityID, la::avdecc::UniqueIdentifier::value_type const rackPrimary, la::avdecc::UniqueIdentifier::value_type const rackSecondary, bool const isDirect = false)
	{
		auto details = QJsonObject{};
		details["is_controller"] = false;
		auto interfaces = QJsonArray{};
		for (auto plane = 0; plane < 2; ++plane)
		{
			auto interface = QJsonObject{};
			interface["index"] = plane;
			interface["clock_identity"] = idString(clockIdentity(entityID, plane));
			interface["grandmaster_id"] = idString(plane == 0 ? GrandmasterPrimary : GrandmasterSecondary);
			auto path = QJsonArray{};
			path.push_back(idString(plane == 0 ? GrandmasterPrimary : GrandmasterSecondary));
			path.push_back(idString(plane == 0 ? BranchPrimary : BranchSecondary));
			if (!isDirect)
			{
				path.push_back(idString(plane == 0 ? rackPrimary : rackSecondary));
			}
			path.push_back(idString(clockIdentity(entityID, plane)));
			interface["as_path"] = path;
			interfaces.push_back(interface);
		}
		details["avb_interfaces"] = interfaces;
		auto streams = QJsonArray{};
		if (entityID != EntityID{ TalkerID })
		{
			for (auto plane = 0; plane < 2; ++plane)
			{
				auto stream = QJsonObject{};
				stream["index"] = plane;
				stream["name"] = plane == 0 ? "Media Clock Primary" : "Media Clock Secondary";
				stream["avb_interface_index"] = plane;
				stream["talker_entity_id"] = idString(TalkerID);
				stream["talker_stream_index"] = plane;
				streams.push_back(stream);
			}
		}
		details["stream_inputs"] = streams;
		add(timestamp, Category::Entity, Severity::Info, entityID, name(entityID), {}, "Entity is online", details);
	}

	QString streamSubject(int const plane) const
	{
		return QString{ "Input Stream %1 (Media Clock %2)" }.arg(plane).arg(plane == 0 ? "Primary" : "Secondary");
	}

	void mediaUnlocked(qint64 const timestamp, EntityID const entityID, int const plane, bool const inferred = false)
	{
		auto details = QJsonObject{};
		details["talker_id"] = idString(TalkerID);
		details["talker_stream_index"] = plane;
		if (inferred)
		{
			details["inferred"] = true;
		}
		add(timestamp, Category::Counters, Severity::Error, entityID, name(entityID), streamSubject(plane), "Media unlocked", details);
	}

	void mediaLocked(qint64 const timestamp, EntityID const entityID, int const plane, bool const inferred = false)
	{
		auto details = QJsonObject{};
		if (inferred)
		{
			details["inferred"] = true;
		}
		add(timestamp, Category::Counters, Severity::Recovered, entityID, name(entityID), streamSubject(plane), "Media locked", details);
	}

	void clockUnlocked(qint64 const timestamp, EntityID const entityID, bool const inferred = false)
	{
		auto details = QJsonObject{};
		if (inferred)
		{
			details["inferred"] = true;
		}
		add(timestamp, Category::MediaClock, Severity::Error, entityID, name(entityID), "Clock Domain 0", "Media clock unlocked", details);
	}

	void clockLocked(qint64 const timestamp, EntityID const entityID, bool const inferred = false)
	{
		auto details = QJsonObject{};
		if (inferred)
		{
			details["inferred"] = true;
		}
		add(timestamp, Category::MediaClock, Severity::Recovered, entityID, name(entityID), "Clock Domain 0", "Media clock locked", details);
	}

	/** Storm of a rack on a plane: the three amplifiers unlock together every 'period' for 'count' times, starting at 'start'. */
	void rackStorm(qint64 const start, std::size_t const rack, int const plane, std::size_t const count, qint64 const period)
	{
		for (auto i = std::size_t{ 0u }; i < count; ++i)
		{
			for (auto amp = std::size_t{ 0u }; amp < 3u; ++amp)
			{
				mediaUnlocked(start + static_cast<qint64>(i) * period + static_cast<qint64>(amp) * 100, amplifier(rack * 3u + amp), plane);
			}
		}
	}

	void streamDataLoss(qint64 const timestamp, EntityID const entityID, int const plane, qint64 const missingMs, qint64 const elapsedMs)
	{
		auto details = QJsonObject{};
		details["frames_rx"] = 123456;
		details["increment"] = 8000 - missingMs * 8;
		details["expected_increment"] = 8000;
		details["elapsed_ms"] = elapsedMs;
		details["missing_ms"] = missingMs;
		details["talker_id"] = idString(TalkerID);
		details["talker_stream_index"] = plane;
		add(timestamp, Category::Streaming, Severity::Error, entityID, name(entityID), streamSubject(plane), QString{ "Stream data loss: about %1 ms of frames missing" }.arg(missingMs), details);
	}

	void monitoringStarted(qint64 const timestamp, EntityID const entityID, int const plane)
	{
		auto details = QJsonObject{};
		details["expected_increment"] = 8000;
		details["samples"] = 10;
		details["talker_id"] = idString(TalkerID);
		details["talker_stream_index"] = plane;
		add(timestamp, Category::Streaming, Severity::Info, entityID, name(entityID), streamSubject(plane), "Stream data monitoring started", details, Level::Diagnostic);
	}

	/** Every entity online at the start of the session, with the recording events. */
	void standardStart()
	{
		session.metadata["schema_version"] = "2";
		session.metadata["hive_version"] = "1.5.0-test";
		session.metadata["computer_name"] = "Unit Tests";
		session_(Base, "Recording started");
		online(Base + 1 * Second, EntityID{ TalkerID }, 0u, 0u, true);
		online(Base + 2 * Second, EntityID{ DirectID }, 0u, 0u, true);
		for (auto i = std::size_t{ 0u }; i < 6u; ++i)
		{
			online(Base + 3 * Second + static_cast<qint64>(i) * 100, amplifier(i), i < 3u ? Rack1Primary : Rack2Primary, i < 3u ? Rack1Secondary : Rack2Secondary);
		}
	}

	void finish(qint64 const timestamp)
	{
		session_(timestamp, "Controller stopped");
		session_(timestamp + 1, "Recording stopped");
		std::stable_sort(session.events.begin(), session.events.end(),
			[](EventJournal::Event const& lhs, EventJournal::Event const& rhs)
			{
				return lhs.timestamp < rhs.timestamp;
			});
	}
};

JournalAnalysis::Topology topologyOf(EventJournal::Session const& session)
{
	auto topology = JournalAnalysis::Topology{};
	topology.addSession(session);
	return topology;
}
} // namespace

TEST(JournalAnalysis, Topology_RacksAndBranchesFromTheOnlineDetails)
{
	auto builder = SessionBuilder{};
	builder.standardStart();
	builder.finish(Base + Hour);

	auto const topology = topologyOf(builder.session);
	EXPECT_EQ(8u, topology.entities.size());
	EXPECT_TRUE(topology.hasAsPaths());

	// An amplifier: rack switch and branch switch on each plane
	EXPECT_EQ(EntityID{ Rack1Primary }, topology.rackSwitch(amplifier(0u), 0).value());
	EXPECT_EQ(EntityID{ Rack1Secondary }, topology.rackSwitch(amplifier(0u), 1).value());
	EXPECT_EQ(EntityID{ Rack2Primary }, topology.rackSwitch(amplifier(5u), 0).value());
	EXPECT_EQ(EntityID{ BranchPrimary }, topology.branchSwitch(amplifier(0u), 0).value());
	EXPECT_EQ(EntityID{ BranchSecondary }, topology.branchSwitch(amplifier(5u), 1).value());
	// The unit plugged directly into the branch switch is a rack of its own on that switch
	EXPECT_EQ(clockIdentity(EntityID{ DirectID }, 0), topology.rackSwitch(EntityID{ DirectID }, 0).value());
	EXPECT_EQ(EntityID{ BranchPrimary }, topology.branchSwitch(EntityID{ DirectID }, 0).value());
	// Unknown plane or entity
	EXPECT_FALSE(topology.rackSwitch(amplifier(0u), 5).has_value());
	EXPECT_FALSE(topology.rackSwitch(EntityID{ 0x1234u }, 0).has_value());
	// Planes of the streams
	EXPECT_EQ(0, topology.streamPlane(amplifier(0u), 0u));
	EXPECT_EQ(1, topology.streamPlane(amplifier(0u), 1u));
	EXPECT_EQ(JournalAnalysis::UnknownPlane, topology.streamPlane(amplifier(0u), 7u));
	EXPECT_EQ(EntityID{ TalkerID }, topology.entities.at(amplifier(0u)).streamInputs.at(1u).talker->entityID);
	// The upstream switches of a plane are the grandmaster and the branch
	auto const upstream = topology.upstreamSwitches(0);
	ASSERT_EQ(2u, upstream.size());
	EXPECT_EQ(EntityID{ GrandmasterPrimary }, upstream[0]);
	EXPECT_EQ(EntityID{ BranchPrimary }, upstream[1]);
}

TEST(JournalAnalysis, Topology_FromAnEntityDocument)
{
	// The AVE document format of the snapshots and of the network state files (descriptor arrays under the current configuration)
	auto document = nlohmann::json::parse(R"json({
		"adp_information": { "common": { "entity_id": "0x001B92FFFF000300", "controller_capabilities": ["IMPLEMENTED"] } },
		"entity_model": { "entity_descriptor": {
			"dynamic": { "entity_name": "Console", "current_configuration": 0 },
			"configuration_descriptors": [ {
				"avb_interface_descriptors": [
					{ "_index (informative)": 0, "dynamic": { "clock_identity": "0x001B92FFFE000300", "gptp_grandmaster_id": "0x001B92FFFE0000A0", "as_path": ["0x001B92FFFE0000A0", "0x001B92FFFE0000A1", "0x001B92FFFE000300"] } },
					{ "_index (informative)": 1, "dynamic": { "clock_identity": "0x001B92FFFE000308", "gptp_grandmaster_id": "0x001B92FFFE0000B0", "as_path": ["0x001B92FFFE0000B0", "0x001B92FFFE000308"] } }
				],
				"stream_input_descriptors": [
					{ "static": { "avb_interface_index": 0 }, "dynamic": { "connection_state": "CONNECTED", "connected_talker": { "entity_id": "0x001B92FFFF000100", "stream_index": 0 } } },
					{ "static": { "avb_interface_index": 1 }, "dynamic": { "connection_state": "NOT_CONNECTED", "connected_talker": { "entity_id": "0xFFFFFFFFFFFFFFFF", "stream_index": 65535 } } }
				],
				"clock_source_descriptors": [
					{ "static": { "clock_source_location_type": "AUDIO_UNIT", "clock_source_location_index": 0 } },
					{ "static": { "clock_source_location_type": "STREAM_INPUT", "clock_source_location_index": 0 } }
				],
				"clock_domain_descriptors": [ { "dynamic": { "clock_source_index": 1 } } ]
			} ]
		} }
	})json");
	auto topology = JournalAnalysis::Topology{};
	EXPECT_TRUE(topology.addEntityDocument(document));
	auto const entityID = EntityID{ 0x001B92FFFF000300u };
	auto const& entity = topology.entities.at(entityID);
	EXPECT_EQ("Console", entity.name);
	EXPECT_TRUE(entity.isController);
	ASSERT_EQ(2u, entity.interfaces.size());
	EXPECT_EQ(3u, entity.interfaces.at(0u).asPath.size());
	EXPECT_EQ(EntityID{ BranchPrimary }, topology.rackSwitch(entityID, 0).value()); // The branch of the other tests is a plain rack here: nothing is behind it
	EXPECT_EQ(EntityID{ GrandmasterPrimary }, topology.branchSwitch(entityID, 0).value());
	EXPECT_EQ(EntityID{ GrandmasterSecondary }, topology.rackSwitch(entityID, 1).value());
	EXPECT_FALSE(topology.branchSwitch(entityID, 1).has_value());
	EXPECT_EQ(0, topology.streamPlane(entityID, 0u));
	EXPECT_EQ(1, topology.streamPlane(entityID, 1u));
	EXPECT_TRUE(entity.streamInputs.at(0u).talker.has_value());
	EXPECT_FALSE(entity.streamInputs.at(1u).talker.has_value());
	EXPECT_EQ(0u, entity.clockSourceStreams.at(0u));

	// A network state document is an array of such documents
	auto networkState = nlohmann::json::object();
	networkState["entities"] = nlohmann::json::array({ document });
	auto other = JournalAnalysis::Topology{};
	EXPECT_EQ(1u, other.addNetworkState(networkState));
	EXPECT_EQ(0u, other.addNetworkState(nlohmann::json::object()));
	EXPECT_FALSE(other.addEntityDocument(nlohmann::json::object()));
}

// An entity reporting the clock identity of its grandmaster as its own (seen in the field) is mapped with the clock identity deduced from the end of its AsPath: its real clock identity must not be taken as the switch it is plugged into
TEST(JournalAnalysis, Topology_GrandmasterClockIdentityReportedAsOwn)
{
	auto topology = JournalAnalysis::Topology{};
	// Two amplifiers of the same rack, the second one reporting the clock identity of its grandmaster (and ending its AsPath with its real one)
	auto& consistent = topology.entities[amplifier(0u)].interfaces[0u];
	consistent.clockIdentity = clockIdentity(amplifier(0u), 0);
	consistent.asPath = { EntityID{ GrandmasterPrimary }, EntityID{ BranchPrimary }, EntityID{ Rack1Primary }, clockIdentity(amplifier(0u), 0) };
	auto& inconsistent = topology.entities[amplifier(1u)].interfaces[0u];
	inconsistent.clockIdentity = EntityID{ GrandmasterPrimary };
	inconsistent.asPath = { EntityID{ GrandmasterPrimary }, EntityID{ BranchPrimary }, EntityID{ Rack1Primary }, clockIdentity(amplifier(1u), 0) };

	EXPECT_FALSE(consistent.hasInconsistentClockIdentity());
	EXPECT_TRUE(inconsistent.hasInconsistentClockIdentity());
	EXPECT_EQ(clockIdentity(amplifier(0u), 0), topology.clockIdentity(amplifier(0u), 0));
	EXPECT_EQ(clockIdentity(amplifier(1u), 0), topology.clockIdentity(amplifier(1u), 0));
	// Both amplifiers are in the same rack, behind the same branch
	EXPECT_EQ(3u, topology.hops(amplifier(1u), 0).size());
	EXPECT_EQ(EntityID{ Rack1Primary }, topology.rackSwitch(amplifier(1u), 0).value());
	EXPECT_EQ(EntityID{ BranchPrimary }, topology.branchSwitch(amplifier(1u), 0).value());
	EXPECT_EQ(EntityID{ Rack1Primary }, topology.rackSwitch(amplifier(0u), 0).value()) << "The rack switch must not become an upstream switch because of the real clock identity of the inconsistent entity";

	// An amplifier daisy chained behind the first one, reporting the clock identity of its grandmaster and not ending its AsPath with its own: the last element is the first amplifier, it must not be taken as its clock identity
	auto daisyChained = JournalAnalysis::Topology{};
	daisyChained.entities[amplifier(0u)].interfaces[0u] = consistent;
	auto& chained = daisyChained.entities[amplifier(2u)].interfaces[0u];
	chained.clockIdentity = EntityID{ GrandmasterPrimary };
	chained.asPath = { EntityID{ GrandmasterPrimary }, EntityID{ BranchPrimary }, EntityID{ Rack1Primary }, clockIdentity(amplifier(0u), 0) };
	EXPECT_TRUE(chained.hasInconsistentClockIdentity());
	EXPECT_FALSE(daisyChained.clockIdentity(amplifier(2u), 0).isValid());
	// Same rack than a consistent entity daisy chained behind the first amplifier: the switch embedded in that amplifier
	EXPECT_EQ(clockIdentity(amplifier(0u), 0), daisyChained.rackSwitch(amplifier(2u), 0).value());
	EXPECT_EQ(daisyChained.rackSwitch(amplifier(0u), 0).value(), daisyChained.rackSwitch(amplifier(2u), 0).value());
}

TEST(JournalAnalysis, Hits_IncidentsStormsAndRuns)
{
	auto builder = SessionBuilder{};
	builder.standardStart();
	// Two runs of the primary branch, 4 hours apart, each visiting rack 1 then rack 2 (a storm of 8 incidents every 10 s per rack, the second rack 2 minutes after the first)
	for (auto run = 0; run < 2; ++run)
	{
		auto const start = Base + 10 * Minute + run * 4 * Hour;
		builder.rackStorm(start, 0u, 0, 8u, 10 * Second);
		builder.rackStorm(start + 3 * Minute, 1u, 0, 8u, 10 * Second);
	}
	// One isolated hit of one amplifier on the secondary plane, and the unit plugged directly into the branch
	builder.mediaUnlocked(Base + 2 * Hour, amplifier(4u), 1);
	builder.mediaUnlocked(Base + 2 * Hour + 5 * Second, EntityID{ DirectID }, 0);
	builder.finish(Base + 10 * Hour);

	auto const analysis = JournalAnalysis{ builder.session, topologyOf(builder.session) };
	auto const& results = analysis.results();

	EXPECT_EQ(2u * 2u * 8u * 3u + 2u, results.unlockEvents);
	EXPECT_EQ(2u * 2u * 8u * 3u + 2u, results.hits.size()); // The three amplifiers of a rack are three entities: three hits per incident
	EXPECT_EQ(2u * 2u * 8u + 1u, results.incidentsPerPlane.at(0));
	EXPECT_EQ(1u, results.incidentsPerPlane.at(1));
	EXPECT_EQ(2u * 2u * 8u + 1u, results.fullRackIncidents); // The isolated secondary hit does not involve the whole rack; the direct unit is a rack of one, so its incident is full
	EXPECT_TRUE(results.systemWideEvents.empty());
	// Storms: 2 racks x 2 runs, plus the two isolated hits
	EXPECT_EQ(4u + 2u, results.storms.size());
	// Runs: the two sweeps of the primary branch, the isolated secondary hit and the direct unit hit (2 hours away from both sweeps) are runs of their own
	ASSERT_EQ(4u, results.runs.size());
	auto const& firstRun = results.runs[0];
	EXPECT_EQ(EntityID{ BranchPrimary }, firstRun.branchSwitch);
	EXPECT_EQ(0, firstRun.plane);
	ASSERT_EQ(2u, firstRun.racks.size());
	EXPECT_EQ(EntityID{ Rack1Primary }, firstRun.racks[0].rackSwitch);
	EXPECT_EQ(EntityID{ Rack2Primary }, firstRun.racks[1].rackSwitch);
	EXPECT_EQ(8u, firstRun.racks[0].incidents);
	EXPECT_EQ(3u, firstRun.racks[0].entities.size());
	EXPECT_EQ(70 * Second + 200, firstRun.racks[0].endTimestamp - firstRun.racks[0].timestamp);
	EXPECT_EQ(2u, firstRun.storms);
	EXPECT_EQ(16u, firstRun.incidents);
	// Series: the primary branch has two runs 4 hours apart, in the same rack order
	auto const seriesIt = std::find_if(results.runSeries.begin(), results.runSeries.end(),
		[](JournalAnalysis::RunSeries const& series)
		{
			return series.branchSwitch == EntityID{ BranchPrimary } && series.plane == 0;
		});
	ASSERT_NE(results.runSeries.end(), seriesIt);
	EXPECT_EQ(3u, seriesIt->runs.size()); // The two sweeps and the direct unit's own run (same branch and plane, 2 hours away from both)
	EXPECT_EQ(2u, seriesIt->sweeps.size()); // The direct unit's run visits one rack only: an isolated storm, not a sweep
	EXPECT_EQ(4 * Hour, seriesIt->minPeriodMs.value());
	EXPECT_EQ(4 * Hour, seriesIt->maxPeriodMs.value());
	EXPECT_EQ(2u, seriesIt->sameOrderSweeps);

	// Rankings
	ASSERT_FALSE(results.rackRanking.empty());
	EXPECT_EQ(16u, results.rackRanking.front().incidents);
	EXPECT_EQ(3u, results.rackRanking.front().entities.size());
	EXPECT_EQ(EntityID{ BranchPrimary }, results.rackRanking.front().branchSwitch.value());
	ASSERT_FALSE(results.branchRanking.empty());
	EXPECT_EQ(EntityID{ BranchPrimary }, results.branchRanking.front().branchSwitch.value());
	EXPECT_EQ(33u, results.branchRanking.front().incidents);
	EXPECT_EQ(3u, results.branchRanking.front().racks);
	ASSERT_EQ(7u, results.entityRanking.size());
	EXPECT_EQ(17u, results.entityRanking.front().total); // Amplifier 5 has the isolated secondary hit on top of its 16 storm hits
	EXPECT_EQ(amplifier(4u), results.entityRanking.front().entityID);
	EXPECT_EQ(16u, results.medianEntityHits);
	EXPECT_EQ(1u, results.entityRanking.back().total);

	// Both planes: nothing paired (the secondary hit is hours away from any primary hit)
	EXPECT_TRUE(results.bothPlanesPairs.empty());
	EXPECT_EQ(2u * 2u * 8u * 3u + 1u, results.primaryOnlyHits);
	EXPECT_EQ(1u, results.secondaryOnlyHits);
}

TEST(JournalAnalysis, SystemWideEvent_HitsEveryRackAtOnce)
{
	auto builder = SessionBuilder{};
	builder.standardStart();
	// Every amplifier and the direct unit hit on both planes within a second, 5 of them losing their clock domain for 2 to 4 seconds, and a grandmaster change 2 seconds before
	auto const start = Base + Hour;
	builder.add(start - 2 * Second, Category::Gptp, Severity::Warning, amplifier(0u), builder.name(amplifier(0u)), "AVB Interface 0", QString{ "gPTP grandmaster changed from %1 to %2" }.arg(idString(GrandmasterPrimary), idString(BranchPrimary)), QJsonObject{ { "previous_grandmaster_id", idString(GrandmasterPrimary) }, { "new_grandmaster_id", idString(BranchPrimary) } });
	for (auto i = std::size_t{ 0u }; i < 6u; ++i)
	{
		builder.mediaUnlocked(start + static_cast<qint64>(i) * 100, amplifier(i), 0);
		builder.mediaUnlocked(start + static_cast<qint64>(i) * 100 + 10, amplifier(i), 1);
		if (i < 5u)
		{
			builder.clockUnlocked(start + static_cast<qint64>(i) * 100 + 20, amplifier(i));
			builder.clockLocked(start + static_cast<qint64>(i) * 100 + 20 + 2 * Second + static_cast<qint64>(i) * 500, amplifier(i));
		}
	}
	builder.mediaUnlocked(start + 700, EntityID{ DirectID }, 0);
	// A background storm of rack 1 an hour later, and a later system wide event where only 3 amplifiers lose their clock domain
	builder.rackStorm(start + Hour, 0u, 1, 5u, 10 * Second);
	auto const second = start + 2 * Hour;
	for (auto i = std::size_t{ 0u }; i < 6u; ++i)
	{
		builder.mediaUnlocked(second + static_cast<qint64>(i) * 50, amplifier(i), 0);
		if (i < 3u)
		{
			builder.clockUnlocked(second + static_cast<qint64>(i) * 50 + 20, amplifier(i));
			builder.clockLocked(second + static_cast<qint64>(i) * 50 + 20 + Second, amplifier(i));
		}
	}
	builder.finish(Base + 4 * Hour);

	auto const analysis = JournalAnalysis{ builder.session, topologyOf(builder.session) };
	auto const& results = analysis.results();

	ASSERT_EQ(2u, results.systemWideEvents.size());
	auto const& event = results.systemWideEvents[0];
	EXPECT_EQ(start, event.timestamp); // The grandmaster change is not a touch
	EXPECT_EQ(7u, event.entities.size());
	ASSERT_EQ(2u, event.planes.size());
	EXPECT_EQ(0, event.planes[0]);
	EXPECT_EQ(1, event.planes[1]);
	EXPECT_EQ(5u, event.racks); // Two racks on each plane, and the direct unit (a rack of one)
	EXPECT_EQ(2u, event.branches);
	EXPECT_EQ(5u, event.clockDomainEntities.size());
	EXPECT_EQ(2 * Second, event.minClockDomainUnlockMs.value());
	EXPECT_EQ(4 * Second, event.maxClockDomainUnlockMs.value());
	EXPECT_EQ(1u, event.grandmasterChanges);
	EXPECT_EQ(0u, event.packetLossEvents);
	EXPECT_TRUE(event.spared.empty()); // Every entity reporting hits in the session was hit
	// Branch shares: on the primary plane, the 7 entities of the primary branch reporting hits were all hit; on the secondary plane, the 6 amplifiers
	auto primaryShare = false;
	for (auto const& share : event.branchShares)
	{
		if (share.plane == 0)
		{
			EXPECT_EQ(EntityID{ BranchPrimary }, share.branchSwitch.value());
			EXPECT_EQ(7u, share.total);
			EXPECT_EQ(7u, share.hit);
			primaryShare = true;
		}
		else
		{
			EXPECT_EQ(6u, share.total);
			EXPECT_EQ(6u, share.hit);
		}
	}
	EXPECT_TRUE(primaryShare);
	// The second one spared the direct unit
	auto const& secondEvent = results.systemWideEvents[1];
	EXPECT_EQ(6u, secondEvent.entities.size());
	ASSERT_EQ(1u, secondEvent.spared.size());
	EXPECT_EQ(EntityID{ DirectID }, secondEvent.spared.front());
	EXPECT_EQ(3u, secondEvent.clockDomainEntities.size());

	// The incidents of the system wide events are excluded from the storms and runs: only the background storm remains
	EXPECT_EQ(12u, results.incidents.size());
	EXPECT_EQ(7u, results.systemWideIncidents); // The five incidents of the first event and the two of the second, leaving the five of the background storm
	ASSERT_EQ(1u, results.storms.size());
	EXPECT_EQ(5u, results.storms.front().incidents);
	EXPECT_EQ(1, results.storms.front().plane);
	ASSERT_EQ(1u, results.runs.size());

	// Both planes: the 6 amplifiers were hit on both planes 10 ms apart, all inside the first system wide event
	ASSERT_EQ(6u, results.bothPlanesPairs.size());
	EXPECT_EQ(10, results.bothPlanesMedianDelayMs.value());
	EXPECT_EQ(6u, results.simultaneousPairs);
	for (auto const& pair : results.bothPlanesPairs)
	{
		EXPECT_TRUE(pair.isSystemWide);
		EXPECT_EQ(2u, pair.events.size());
	}
	EXPECT_EQ(7u, results.primaryOnlyHits); // The second event (6) and the direct unit of the first (1)
	EXPECT_EQ(15u, results.secondaryOnlyHits); // The background storm

	// Clock domain: 8 pairs, the 3 amplifiers of the second event unlock at both system wide events
	EXPECT_EQ(8u, results.clockDomainIntervals.size());
	EXPECT_EQ(8u, results.clockDomainStats.count);
	EXPECT_EQ(Second, results.clockDomainStats.minMs);
	EXPECT_EQ(4 * Second, results.clockDomainStats.maxMs);
	EXPECT_EQ(2u, results.systemWideEventsWithClockDomainUnlocks);
	ASSERT_EQ(5u, results.clockDomainGroup.size());
	EXPECT_EQ(2u, results.clockDomainGroup[0].events);
	EXPECT_EQ(2u, results.clockDomainGroup[2].events);
	EXPECT_EQ(1u, results.clockDomainGroup[3].events);
	// Every clock domain unlock of the first event came with hits on both planes, the ones of the second with the primary only
	EXPECT_EQ(5u, results.clockDomainCompanions.bothPlanes);
	EXPECT_EQ(3u, results.clockDomainCompanions.primaryOnly);
	EXPECT_EQ(0u, results.clockDomainCompanions.none);
	EXPECT_EQ(5u + 5u + 3u, results.hitsWithClockDomainUnlock);
	// The longest unlocks come first
	ASSERT_FALSE(results.longestClockDomainIntervals.empty());
	EXPECT_EQ(4 * Second, results.clockDomainIntervals[results.longestClockDomainIntervals.front()].durationMs.value());
	// The grandmaster change is a storm of one
	ASSERT_EQ(1u, results.grandmasterStorms.size());
	EXPECT_EQ(1u, results.grandmasterStorms.front().changes);
	EXPECT_EQ(5u, results.grandmasterStorms.front().clockDomainUnlocks); // Within the accompanying window
	EXPECT_EQ(EntityID{ BranchPrimary }, results.grandmasterStorms.front().flips.front().to);
}

TEST(JournalAnalysis, Intervals_StreamPairsAndInferredEvents)
{
	auto builder = SessionBuilder{};
	builder.standardStart();
	auto const start = Base + Hour;
	// A primary unlock of 3 s and a secondary unlock of 2 s overlapping it by 1 s on the same amplifier, an inferred pair (no duration), an unlock never relocked
	builder.mediaUnlocked(start, amplifier(0u), 0);
	builder.mediaLocked(start + 3 * Second, amplifier(0u), 0);
	builder.mediaUnlocked(start + 2 * Second, amplifier(0u), 1);
	builder.mediaLocked(start + 4 * Second, amplifier(0u), 1);
	builder.mediaUnlocked(start + Minute, amplifier(1u), 0, true);
	builder.mediaLocked(start + Minute, amplifier(1u), 0, true);
	builder.mediaUnlocked(start + 2 * Minute, amplifier(2u), 1);
	// Clock domain: an inferred pair and a link flap
	builder.clockUnlocked(start + Minute, amplifier(1u), true);
	builder.clockLocked(start + Minute, amplifier(1u), true);
	builder.add(start + 3 * Minute, Category::Link, Severity::Error, amplifier(3u), builder.name(amplifier(3u)), "AVB Interface 1", "Link down");
	builder.add(start + 3 * Minute + 5 * Second, Category::Link, Severity::Recovered, amplifier(3u), builder.name(amplifier(3u)), "AVB Interface 1", "Link up");
	// A propagation delay dropping to zero, measured again 2 minutes later
	builder.add(start + 5 * Minute, Category::Gptp, Severity::Warning, amplifier(4u), builder.name(amplifier(4u)), "AVB Interface 0", "gPTP propagation delay became null (was 8 ns)", QJsonObject{ { "previous_propagation_delay", 8 }, { "new_propagation_delay", 0 } });
	builder.add(start + 7 * Minute, Category::Gptp, Severity::Recovered, amplifier(4u), builder.name(amplifier(4u)), "AVB Interface 0", "gPTP propagation delay measured (9 ns)", QJsonObject{ { "previous_propagation_delay", 0 }, { "new_propagation_delay", 9 } });
	// Legacy 'Media Unlocked' error counter (journals recorded before the pairs): a hit without a pair
	builder.add(start + 4 * Minute, Category::Counters, Severity::Error, amplifier(4u), builder.name(amplifier(4u)), builder.streamSubject(0), "'Media Unlocked' error counter increased to 3", QJsonObject{ { "counter", "Media Unlocked" }, { "previous_value", 2 }, { "value", 3 } });
	builder.finish(Base + 2 * Hour);

	auto const analysis = JournalAnalysis{ builder.session, topologyOf(builder.session) };
	auto const& results = analysis.results();

	EXPECT_EQ(5u, results.hits.size());
	ASSERT_EQ(4u, results.streamIntervals.size());
	EXPECT_EQ(3 * Second, results.streamIntervals[0].durationMs.value());
	EXPECT_EQ(2 * Second, results.streamIntervals[1].durationMs.value());
	EXPECT_TRUE(results.streamIntervals[2].isInferred);
	EXPECT_FALSE(results.streamIntervals[2].durationMs.has_value());
	EXPECT_TRUE(results.streamIntervals[2].isClosed);
	EXPECT_FALSE(results.streamIntervals[3].isClosed);
	EXPECT_FALSE(results.streamIntervals[3].durationMs.has_value());
	EXPECT_EQ(1u, results.streamStatsPerPlane.at(0).count);
	EXPECT_EQ(1u, results.streamStatsPerPlane.at(0).unknown);
	EXPECT_EQ(3 * Second, results.streamStatsPerPlane.at(0).medianMs);
	EXPECT_EQ(1u, results.streamStatsPerPlane.at(1).count);
	EXPECT_EQ(1u, results.streamStatsPerPlane.at(1).unknown);
	ASSERT_EQ(1u, results.bothPlanesUnlocked.size());
	EXPECT_EQ(start + 2 * Second, results.bothPlanesUnlocked.front().timestamp);
	EXPECT_EQ(Second, results.bothPlanesUnlocked.front().durationMs.value());
	EXPECT_EQ(amplifier(0u), results.bothPlanesUnlocked.front().entityID);
	// The primary and secondary hits of the first amplifier are 2 s apart: a sequential both planes pair
	ASSERT_EQ(1u, results.bothPlanesPairs.size());
	EXPECT_EQ(2 * Second, results.bothPlanesPairs.front().delayMs);
	EXPECT_EQ(0u, results.simultaneousPairs);
	EXPECT_FALSE(results.bothPlanesPairs.front().isSystemWide);
	// Clock domain and link
	ASSERT_EQ(1u, results.clockDomainIntervals.size());
	EXPECT_TRUE(results.clockDomainIntervals.front().isInferred);
	EXPECT_EQ(0u, results.clockDomainStats.count);
	EXPECT_EQ(1u, results.clockDomainStats.unknown);
	EXPECT_EQ(1u, results.clockDomainCompanions.primaryOnly);
	ASSERT_EQ(1u, results.linkIntervals.size());
	EXPECT_EQ(5 * Second, results.linkIntervals.front().durationMs.value());
	EXPECT_EQ(1, results.linkIntervals.front().plane);
	ASSERT_EQ(1u, results.nullPropagationDelayIntervals.size());
	EXPECT_EQ(2 * Minute, results.nullPropagationDelayIntervals.front().durationMs.value());
	EXPECT_EQ(0, results.nullPropagationDelayIntervals.front().plane);
	EXPECT_EQ(1u, results.familyCounts.at("gPTP propagation delay became null"));
	// The families of the breakdown strip the values
	EXPECT_EQ(1u, results.familyCounts.at("'Media Unlocked' error counter increased"));
	EXPECT_EQ(4u, results.familyCounts.at("Media unlocked"));
	EXPECT_EQ(8u, results.familyCounts.at("Entity is online"));
	EXPECT_EQ(1u, results.categoryCounts.at(std::make_pair(Category::Link, Severity::Error)));
	EXPECT_NE(results.absentCategories.end(), std::find(results.absentCategories.begin(), results.absentCategories.end(), Category::Marker));
	EXPECT_EQ(results.absentCategories.end(), std::find(results.absentCategories.begin(), results.absentCategories.end(), Category::Link));
}

TEST(JournalAnalysis, OtherFamilies_PacketLossControllerSideAndOperatorActions)
{
	auto builder = SessionBuilder{};
	builder.standardStart();
	auto const start = Base + Hour;
	auto const counterDetails = [](QString const& counter, int const value)
	{
		return QJsonObject{ { "counter", counter }, { "previous_value", value - 1 }, { "value", value } };
	};
	// Packet loss counters on two amplifiers, a grandmaster storm of 4 changes on both planes with 2 flips back, controller side losses per interface, operator actions, a connection change
	builder.add(start, Category::Counters, Severity::Error, amplifier(0u), builder.name(amplifier(0u)), builder.streamSubject(0), "'Sequence Number Mismatch' error counter increased to 1", counterDetails("Sequence Number Mismatch", 1));
	builder.add(start + Second, Category::Counters, Severity::Error, amplifier(0u), builder.name(amplifier(0u)), builder.streamSubject(0), "'Sequence Number Mismatch' error counter increased to 2", counterDetails("Sequence Number Mismatch", 2));
	builder.add(start + Second, Category::Counters, Severity::Error, amplifier(1u), builder.name(amplifier(1u)), builder.streamSubject(1), "'Stream Interrupted' error counter increased to 1", counterDetails("Stream Interrupted", 1));
	for (auto i = 0; i < 4; ++i)
	{
		auto const plane = i % 2;
		auto const from = i < 2 ? (plane == 0 ? GrandmasterPrimary : GrandmasterSecondary) : (plane == 0 ? BranchPrimary : BranchSecondary);
		auto const to = i < 2 ? (plane == 0 ? BranchPrimary : BranchSecondary) : (plane == 0 ? GrandmasterPrimary : GrandmasterSecondary);
		builder.add(start + 2 * Second + i * Second, Category::Gptp, Severity::Warning, amplifier(2u), builder.name(amplifier(2u)), QString{ "AVB Interface %1" }.arg(plane), QString{ "gPTP grandmaster changed from %1 to %2" }.arg(idString(from), idString(to)), QJsonObject{ { "previous_grandmaster_id", idString(from) }, { "new_grandmaster_id", idString(to) } });
	}
	builder.add(start + 10 * Minute, Category::Counters, Severity::Error, amplifier(0u), builder.name(amplifier(0u)), "Statistics", "'AEM Unsolicited Response Losses' error counter increased on the Primary interface (interface total: 1, 3 notifications lost, sequence IDs 41 to 43)", QJsonObject{ { "counter", "AEM Unsolicited Response Losses" }, { "interface", "Primary" }, { "interface_value", 1 }, { "value", 1 }, { "lost_count", 3 }, { "expected_sequence_id", 41 }, { "received_sequence_id", 44 } });
	// A journal recorded before the sequence IDs were journaled: the detection counts, the lost notifications are unknown
	builder.add(start + 10 * Minute, Category::Counters, Severity::Error, amplifier(1u), builder.name(amplifier(1u)), "Statistics", "'AEM Unsolicited Response Losses' error counter increased on the Primary interface (interface total: 1)", QJsonObject{ { "counter", "AEM Unsolicited Response Losses" }, { "interface", "Primary" }, { "interface_value", 1 }, { "value", 1 } });
	builder.add(start + 11 * Minute, Category::Redundancy, Severity::Warning, amplifier(0u), builder.name(amplifier(0u)), "Primary Interface", "No longer receiving unsolicited notifications on the Primary interface");
	builder.add(start + 12 * Minute, Category::Redundancy, Severity::Warning, amplifier(3u), builder.name(amplifier(3u)), "Secondary Interface", "Entity is offline on one redundant interface (still online on the other)");
	builder.add(start + 20 * Minute, Category::Control, Severity::Info, amplifier(0u), builder.name(amplifier(0u)), {}, "Entity acquired by this controller");
	builder.add(start + 21 * Minute, Category::Configuration, Severity::Info, amplifier(0u), builder.name(amplifier(0u)), {}, "Entity name changed to 'Amp 1'", QJsonObject{ { "entity_name", "Amp 1" } });
	builder.add(start + 22 * Minute, Category::Configuration, Severity::Info, amplifier(0u), builder.name(amplifier(0u)), "Control 3", "Control value changed", {}, Level::Diagnostic);
	builder.add(start + 23 * Minute, Category::Connection, Severity::Info, amplifier(0u), builder.name(amplifier(0u)), builder.streamSubject(0), "Connected to talker Talker (output stream 0)", QJsonObject{ { "talker_id", idString(TalkerID) }, { "talker_stream_index", 0 } });
	builder.add(start + 24 * Minute, Category::Latency, Severity::Error, amplifier(0u), builder.name(amplifier(0u)), builder.streamSubject(0), "Stream latency error detected");
	builder.finish(Base + 2 * Hour);

	auto const analysis = JournalAnalysis{ builder.session, topologyOf(builder.session) };
	auto const& results = analysis.results();

	EXPECT_EQ(2u, results.packetLossPerCounter.at("Sequence Number Mismatch"));
	EXPECT_EQ(1u, results.packetLossPerCounter.at("Stream Interrupted"));
	EXPECT_EQ(2u, results.packetLossPerCounterAndPlane.at(std::make_pair(QString{ "Sequence Number Mismatch" }, 0)));
	EXPECT_EQ(1u, results.packetLossPerCounterAndPlane.at(std::make_pair(QString{ "Stream Interrupted" }, 1)));
	ASSERT_EQ(2u, results.packetLossCounts.size());
	EXPECT_EQ(2u, results.packetLossCounts.front().count);
	EXPECT_EQ(amplifier(0u), results.packetLossCounts.front().entityID);

	EXPECT_EQ(4u, results.grandmasterChanges);
	ASSERT_EQ(1u, results.grandmasterStorms.size());
	auto const& storm = results.grandmasterStorms.front();
	EXPECT_EQ(4u, storm.changes);
	EXPECT_EQ(3 * Second, storm.endTimestamp - storm.timestamp);
	ASSERT_EQ(2u, storm.planes.size());
	EXPECT_EQ(1u, storm.entities.size());
	EXPECT_EQ(4u, storm.flips.size());
	EXPECT_EQ(3u, storm.packetLossEvents); // Within the accompanying window before the storm

	ASSERT_EQ(1u, results.controllerCounts.size());
	EXPECT_EQ(2u, results.controllerCounts.front().count);
	EXPECT_EQ(2u, results.controllerCounts.front().entities);
	EXPECT_EQ(0, results.controllerCounts.front().plane);
	EXPECT_EQ("AEM Unsolicited Response Losses", results.controllerCounts.front().name);
	EXPECT_EQ(3u, results.unsolicitedNotificationsLost); // Only the detection telling its sequence IDs says how many notifications were lost
	EXPECT_EQ(1u, results.unsolicitedLossesPerInterface.at(0));
	EXPECT_EQ(1u, results.offlineOnOneInterface.at(1));
	EXPECT_EQ(1u, results.latencyErrors);
	EXPECT_EQ(1u, results.connectionChanges);
	// The acquire and the user level rename are operator actions, the diagnostic control value change is not
	ASSERT_EQ(2u, results.operatorActions.size());
	EXPECT_EQ("Entity acquired by this controller", builder.session.events[results.operatorActions[0]].summary);
	EXPECT_EQ("Entity name changed to 'Amp 1'", builder.session.events[results.operatorActions[1]].summary);
	EXPECT_EQ(1u, results.familyCounts.at("Entity name changed"));
	EXPECT_EQ(1u, results.familyCounts.at("Connected"));
	EXPECT_EQ(4u, results.familyCounts.at("gPTP grandmaster changed"));
}

TEST(JournalAnalysis, StreamData_CoverageIncidentsAndInterruptions)
{
	auto builder = SessionBuilder{};
	builder.standardStart();
	auto const start = Base + Hour;
	// Every amplifier's primary stream is covered, the secondary streams of racks are not (except amplifier 0), the direct unit is not covered at all
	for (auto i = std::size_t{ 0u }; i < 6u; ++i)
	{
		builder.monitoringStarted(start, amplifier(i), 0);
	}
	builder.monitoringStarted(start, amplifier(0u), 1);
	// An incident: the three amplifiers of rack 1 lose frames on the primary plane within a second (the common hop is the rack switch), amplifier 0 on both planes within the same second
	builder.streamDataLoss(start + Minute, amplifier(0u), 0, 300, 1000);
	builder.streamDataLoss(start + Minute + 200, amplifier(1u), 0, 350, 1000);
	builder.streamDataLoss(start + Minute + 400, amplifier(2u), 0, 320, 1000);
	builder.streamDataLoss(start + Minute + 500, amplifier(0u), 1, 120, 1000);
	// A media unlock of amplifier 0's primary stream in the same seconds
	builder.mediaUnlocked(start + Minute + 900, amplifier(0u), 0);
	// A long interruption of amplifier 3: a start edge, silence, the resume edge 20 s later
	builder.streamDataLoss(start + 2 * Minute, amplifier(3u), 0, 600, 1000);
	builder.streamDataLoss(start + 2 * Minute + 20 * Second, amplifier(3u), 0, 900, 20 * Second);
	// A loss of amplifier 5 alone (its own link)
	builder.streamDataLoss(start + 3 * Minute, amplifier(5u), 0, 80, 1000);
	builder.finish(Base + 2 * Hour);

	auto const analysis = JournalAnalysis{ builder.session, topologyOf(builder.session) };
	auto const& results = analysis.results();

	EXPECT_TRUE(results.hasStreamDataEvents);
	EXPECT_EQ(7u, results.streamDataLosses);
	EXPECT_EQ(6u, results.streamDataLossesPerPlane.at(0));
	EXPECT_EQ(1u, results.streamDataLossesPerPlane.at(1));
	EXPECT_EQ(1u, results.missingMsHistogram.at(0)); // 80 ms
	EXPECT_EQ(1u, results.missingMsHistogram.at(100)); // 120 ms
	EXPECT_EQ(3u, results.missingMsHistogram.at(250)); // 300, 350, 320 ms
	EXPECT_EQ(1u, results.missingMsHistogram.at(500)); // 600 ms
	EXPECT_EQ(1u, results.missingMsHistogram.at(750)); // 900 ms
	// Coverage: 7 covered streams, the 5 secondary streams of amplifiers 1 to 5 and the 2 streams of the direct unit are not
	auto covered = std::size_t{ 0u };
	for (auto const& stream : results.monitoredStreams)
	{
		if (stream.isCovered)
		{
			++covered;
		}
	}
	EXPECT_EQ(7u, covered);
	EXPECT_EQ(7u, results.uncoveredStreams.size());
	EXPECT_EQ(EntityID{ TalkerID }, results.uncoveredStreams.front().talker->entityID);
	// The stream with the most losses comes first
	ASSERT_FALSE(results.monitoredStreams.empty());
	EXPECT_EQ(amplifier(3u), results.monitoredStreams.front().entityID);
	EXPECT_EQ(2u, results.monitoredStreams.front().losses);
	EXPECT_EQ(1500, results.monitoredStreams.front().totalMissingMs);
	// Both planes of amplifier 0 within the same second
	ASSERT_EQ(1u, results.streamDataBothPlanes.size());
	EXPECT_EQ(amplifier(0u), results.streamDataBothPlanes.front().entityID);
	EXPECT_EQ(500, results.streamDataBothPlanes.front().delayMs);
	// The loss of amplifier 0's primary stream came with its media unlock
	EXPECT_EQ(1u, results.lossesWithMediaUnlock);
	// The long interruption, with its start edge
	ASSERT_EQ(1u, results.longInterruptions.size());
	EXPECT_EQ(20 * Second, results.longInterruptions.front().elapsedMs);
	EXPECT_TRUE(results.longInterruptions.front().startEvent.has_value());
	EXPECT_EQ(start + 2 * Minute, builder.session.events[*results.longInterruptions.front().startEvent].timestamp);
	// Incidents: rack 1 on the primary (common hop = the rack switch, the other 3 listeners of the talker stream are behind the branch, not the rack), amplifier 0 on the secondary, amplifier 3 twice, amplifier 5 once
	ASSERT_EQ(5u, results.streamDataIncidents.size());
	auto const& incident = results.streamDataIncidents.front();
	EXPECT_EQ(3u, incident.listeners.size());
	EXPECT_EQ(7u, incident.talkerListeners); // The six amplifiers and the direct unit listen to the talker's stream 0
	EXPECT_EQ(EntityID{ Rack1Primary }, incident.commonHop.value());
	EXPECT_TRUE(incident.sparedBehindHop.empty());
	EXPECT_EQ(0, incident.plane);
	EXPECT_EQ(EntityID{ TalkerID }, incident.talker.entityID);
	// A single listener: the common hop is its own rack, the other members of the rack were spared
	auto const& single = results.streamDataIncidents.back();
	EXPECT_EQ(1u, single.listeners.size());
	EXPECT_EQ(EntityID{ Rack2Primary }, single.commonHop.value());
	EXPECT_EQ(2u, single.sparedBehindHop.size());
}

TEST(JournalAnalysis, SessionShape_WavesDropOutsHeartbeatsAndMarkers)
{
	auto builder = SessionBuilder{};
	builder.standardStart();
	auto const start = Base + Hour;
	// A drop out of amplifier 2 (30 s), a refresh of amplifier 3 (not a drop out), markers, heartbeats every 5 minutes with a 20 minute gap and one latency outlier, a power down wave at the end
	builder.add(start, Category::Entity, Severity::Warning, amplifier(2u), builder.name(amplifier(2u)), {}, "Entity went offline");
	builder.online(start + 30 * Second, amplifier(2u), Rack1Primary, Rack1Secondary);
	builder.add(start + Minute, Category::Control, Severity::Info, amplifier(3u), builder.name(amplifier(3u)), {}, "Entity refresh requested");
	builder.add(start + Minute + Second, Category::Entity, Severity::Warning, amplifier(3u), builder.name(amplifier(3u)), {}, "Entity went offline");
	builder.online(start + Minute + 10 * Second, amplifier(3u), Rack2Primary, Rack2Secondary);
	builder.add(start + 2 * Minute, Category::Marker, Severity::Info, {}, {}, {}, "Show start", QJsonObject{ { "author", "Unit Tests" } });
	builder.add(start + 50 * Minute, Category::Marker, Severity::Info, {}, {}, {}, "Glitch heard", QJsonObject{ { "author", "Unit Tests" } });
	for (auto i = 0; i < 10; ++i)
	{
		auto const timestamp = Base + (i < 5 ? i * 5 * Minute : (i + 3) * 5 * Minute);
		builder.session_(timestamp + 100, "Heartbeat", QJsonObject{ { "entities_online", 8 }, { "events_since_last", 3 }, { "notifications_since_last", 100 }, { "event_loop_latency_ms", i == 7 ? 800 : 5 }, { "process_rss_mb", 200 } }, Level::Diagnostic);
	}
	builder.add(Base + 3 * Hour, Category::Link, Severity::Error, {}, {}, "Primary Interface", "Controller interface link down", QJsonObject{ { "interface_id", "en5" }, { "interface_name", "Ethernet" } });
	for (auto i = std::size_t{ 0u }; i < 6u; ++i)
	{
		builder.add(Base + 3 * Hour + Second + static_cast<qint64>(i) * 500, Category::Entity, Severity::Warning, amplifier(i), builder.name(amplifier(i)), {}, "Entity went offline");
	}
	builder.finish(Base + 3 * Hour + Minute);
	// An interrupted session ends without 'Recording stopped'
	builder.session.events.pop_back();

	auto const analysis = JournalAnalysis{ builder.session, topologyOf(builder.session) };
	auto const& results = analysis.results();

	EXPECT_EQ(8u, results.entitiesCount);
	EXPECT_EQ(8u, results.entitiesOnlineAtStart);
	EXPECT_TRUE(results.isInterrupted);
	EXPECT_TRUE(results.controllers.empty());
	ASSERT_EQ(2u, results.markers.size());
	EXPECT_EQ("Show start", builder.session.events[results.markers[0]].summary);
	EXPECT_EQ(1u, results.controllerInterfaceEvents.size());
	// Waves: the power up at the start (8 entities within 3 s) and the power down at the end (6 amplifiers)
	ASSERT_EQ(2u, results.waves.size());
	EXPECT_TRUE(results.waves[0].isOnline);
	EXPECT_EQ(8u, results.waves[0].entities.size());
	EXPECT_FALSE(results.waves[1].isOnline);
	EXPECT_EQ(6u, results.waves[1].entities.size());
	// Drop outs: amplifier 2 for 30 s, amplifier 3 flagged as a refresh
	ASSERT_EQ(2u, results.dropOuts.size());
	EXPECT_EQ(amplifier(2u), results.dropOuts[0].entityID);
	EXPECT_EQ(30 * Second, results.dropOuts[0].durationMs.value());
	EXPECT_FALSE(results.dropOuts[0].isRefresh);
	EXPECT_TRUE(results.dropOuts[1].isRefresh);
	EXPECT_EQ(9 * Second, results.dropOuts[1].durationMs.value());
	// Heartbeats: 10 of them, 5 minutes apart, one gap of 20 minutes, one latency outlier
	EXPECT_EQ(10u, results.heartbeats);
	EXPECT_EQ(5 * Minute, results.heartbeatIntervalMs.value());
	ASSERT_EQ(1u, results.heartbeatGaps.size());
	EXPECT_EQ(20 * Minute, results.heartbeatGaps.front().durationMs);
	ASSERT_EQ(1u, results.heartbeatLatencyOutliers.size());
	// The session events (recording, controller) are listed, the heartbeats are not
	EXPECT_EQ(2u, results.sessionEvents.size());
	// The names come from the events
	EXPECT_EQ("Amp 1", analysis.entityName(amplifier(0u)));
	EXPECT_EQ(idString(EntityID{ 0x1234u }), analysis.entityName(EntityID{ 0x1234u }));
	// Planes of the events
	EXPECT_EQ(0, analysis.eventPlane(builder.session.events[results.controllerInterfaceEvents.front()]));
}

TEST(JournalAnalysis, Report_HasTheSectionsAndTheEventLinks)
{
	auto builder = SessionBuilder{};
	builder.standardStart();
	auto const start = Base + Hour;
	builder.rackStorm(start, 0u, 0, 3u, 10 * Second);
	builder.clockUnlocked(start + 100, amplifier(0u));
	builder.clockLocked(start + 1500, amplifier(0u));
	builder.add(start + 2 * Minute, Category::Marker, Severity::Info, {}, {}, {}, "Glitch heard", QJsonObject{ { "author", "Unit Tests" } });
	builder.finish(Base + 2 * Hour);

	auto const analysis = JournalAnalysis{ builder.session, topologyOf(builder.session) };
	auto options = JournalAnalysis::ReportOptions{};
	options.title = "Unit test report";
	options.timeZone = QTimeZone::utc();
	options.eventLinks = true;
	auto const report = analysis.markdownReport(builder.session, options);

	EXPECT_TRUE(report.startsWith("# Unit test report\n"));
	for (auto const* const section : { "## Markers", "## Session shape", "## Event breakdown", "## Topology", "## Media clock unlocks at stream level", "### System wide events", "### Runs: storms walking the ports of a branch switch", "### Rack and branch ranking", "### Entity ranking", "## Clock domain", "## Both planes", "## Other error families", "### Grandmaster changes", "### Packet loss counters", "### Controller side", "### Operator actions and connection changes", "## Stream data losses", "## How this analysis was produced" })
	{
		EXPECT_TRUE(report.contains(QString{ "\n%1\n" }.arg(section))) << section;
	}
	// The marker is linked to its event, the times are UTC
	auto const markerIndex = analysis.results().markers.front();
	EXPECT_TRUE(report.contains(QString{ "[%1](event:%2) | Glitch heard" }.arg(QDateTime::fromMSecsSinceEpoch(start + 2 * Minute, QTimeZone::utc()).toString("HH:mm:ss")).arg(markerIndex)));
	// The rack of the storm is named after its amplifiers
	EXPECT_TRUE(report.contains("Amp 1/2/3"));
	EXPECT_TRUE(report.contains("(UTC+0"));
	// No em dash anywhere
	EXPECT_FALSE(report.contains(QChar{ 0x2014 }));

	// Without the links, the times are plain
	options.eventLinks = false;
	auto const plain = analysis.markdownReport(builder.session, options);
	EXPECT_FALSE(plain.contains("](event:"));
	EXPECT_TRUE(plain.contains("| Glitch heard |"));

	// An empty session has a report too
	auto const empty = JournalAnalysis{ EventJournal::Session{}, JournalAnalysis::Topology{} };
	EXPECT_TRUE(empty.markdownReport(EventJournal::Session{}).contains("no event"));
}

// The interfaces reporting the clock identity of their grandmaster as their own are listed in the topology section, and still mapped to their rack
TEST(JournalAnalysis, Report_ListsTheInconsistentClockIdentities)
{
	auto builder = SessionBuilder{};
	builder.standardStart();
	builder.finish(Base + Hour);
	auto topology = topologyOf(builder.session);
	topology.entities.at(amplifier(1u)).interfaces.at(0u).clockIdentity = EntityID{ GrandmasterPrimary };

	auto const analysis = JournalAnalysis{ builder.session, topology };
	auto options = JournalAnalysis::ReportOptions{};
	options.timeZone = QTimeZone::utc();
	auto const report = analysis.markdownReport(builder.session, options);

	EXPECT_TRUE(report.contains("Inconsistent clock identity reported by 1 interface(s)"));
	EXPECT_TRUE(report.contains(QString{ "Amp 2 (%1: reports %2, deduced %3)" }.arg(JournalAnalysis::planeName(0), idString(GrandmasterPrimary), idString(clockIdentity(amplifier(1u), 0)))));
	EXPECT_TRUE(report.contains("Amp 1/2/3"));
}
