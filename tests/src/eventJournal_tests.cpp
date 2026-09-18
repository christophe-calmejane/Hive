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

/**
* @file eventJournal_tests.cpp
* @author Christophe Calmejane
*/

#include <gtest/gtest.h>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/eventJournal.hpp>
#include <hive/modelsLibrary/helper.hpp>

#include <la/avdecc/internals/entityModelControlValues.hpp>
#include <la/avdecc/internals/entityModelControlValuesTraits.hpp>

#include <nlohmann/json.hpp>

#include <QApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QString>
#ifdef _WIN32
#	pragma warning(push)
#	pragma warning(disable : 4127) // Disable conditional expression is constant
#endif
#include <QTest>
#ifdef _WIN32
#	pragma warning(pop)
#endif

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace
{
constexpr auto TestEntityIDBase = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFF000000u };
constexpr auto TestClockDomainIndex = la::avdecc::entity::model::ClockDomainIndex{ 0u };

// Identities of the 'data/connectionMatrix/9-Normal_Normal-ConnectedNoError_ConnectedNoError.json' test file, whose stream inputs are connected on one entity and not connected on the other (both entities carry a FRAMES_RX counter on every stream input)
constexpr auto ConnectedFixturePath = "data/connectionMatrix/9-Normal_Normal-ConnectedNoError_ConnectedNoError.json";
constexpr auto ConnectedListenerEntityID = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFE0222BFu }; // Its stream inputs are connected to the talker entity
constexpr auto TalkerEntityID = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFE02233Bu }; // Its stream inputs are not connected
constexpr auto TestStreamIndex = la::avdecc::entity::model::StreamIndex{ 0u };
constexpr auto FramesPerSecond = la::avdecc::entity::model::DescriptorCounter{ 8000u }; // One second worth of frames of the simulated stream (AAF class A)
constexpr auto OneSecond = std::chrono::milliseconds{ 1000 };
constexpr auto LearningSamples = std::size_t{ 10u }; // Number of increments the rule needs before evaluating a stream

// Identities of the 'data/networkTopology/1-ZeroPropagationDelay_GrandmasterAdjacency.json' test file (five entities with an AsPath and a propagation delay)
constexpr auto TopologyFixturePath = "data/networkTopology/1-ZeroPropagationDelay_GrandmasterAdjacency.json";
constexpr auto TopologyFixtureEntityCount = std::size_t{ 5u };
constexpr auto TopologyGrandmasterClockIdentity = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFE0000FFu };
constexpr auto TopologyEntityA = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFE000001u }; // Attached to the grandmaster, 20 ns propagation delay

// Journal files of every schema version, recorded (and anonymized) by the version of Hive that introduced the schema
constexpr auto SchemaV1FixturePath = "data/eventJournal/schema_v1.hej"; // 31 events of a real dual interface session recorded by Hive 1.5.0-beta1
constexpr auto SchemaV2FixturePath = "data/eventJournal/schema_v2.hej"; // Session recorded by this test suite (EventJournal_F.Session_WriterRoundTrip) when schema version 2 was introduced

/** Stream data event as journaled, reduced to what the tests check. */
struct StreamDataEvent
{
	QString summary{};
	hive::modelsLibrary::EventJournal::Severity severity{};
	qint64 framesRx{ 0 }; // details.frames_rx
	qint64 increment{ 0 }; // details.increment
	qint64 expectedIncrement{ 0 }; // details.expected_increment
	qint64 elapsedMs{ 0 }; // details.elapsed_ms
	qint64 missingMs{ 0 }; // details.missing_ms
	QString talkerID{}; // details.talker_id
	int talkerStreamIndex{ -1 }; // details.talker_stream_index
};

/** Media clock event as journaled, reduced to what the tests check. */
struct MediaClockEvent
{
	bool isLocked{ false }; // Lock state resulting from the event (true for "Media clock locked", false for "Media clock unlocked")
	bool isInferred{ false }; // details.inferred flag
	qint64 lockedCounter{ 0 }; // details.locked_counter
	qint64 unlockedCounter{ 0 }; // details.unlocked_counter
};

class EventJournal_F : public ::testing::Test
{
public:
	virtual void SetUp() override
	{
		// Session files must not pollute the user's journals directory
		QStandardPaths::setTestModeEnabled(true);

		// The journal session (and its change-detection baselines) deliberately survives controller reloads, so each test uses its own entity to start from a clean baseline
		static auto s_testCount = la::avdecc::UniqueIdentifier::value_type{ 0u };
		_testEntityID = la::avdecc::UniqueIdentifier{ TestEntityIDBase + ++s_testCount };

		// The journal must exist before the controller is created, so it catches the controllerOnline signal that starts the recording session
		auto& journal = hive::modelsLibrary::EventJournal::getInstance();
		// The diagnostic level events are part of what the tests check (the level persists across sessions, so this is a no-op after the first test)
		journal.setRecordingLevel(hive::modelsLibrary::EventJournal::Level::Diagnostic);

		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		try
		{
			controllerManager.createController(la::avdecc::protocol::ProtocolInterface::Type::Virtual, "Unit Tests", 0x0001, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), "en", nullptr);
		}
		catch (la::avdecc::controller::Controller::Exception const&)
		{
			ASSERT_FALSE(true);
		}

		auto const deadline = QDeadlineTimer{ 5000 };
		while (!deadline.hasExpired() && !journal.isRecording())
		{
			QTest::qWait(10);
		}
		ASSERT_TRUE(journal.isRecording()) << "The journal did not start a recording session when the controller came online";
	}

	virtual void TearDown() override
	{
		hive::modelsLibrary::EventJournal::getInstance().setMonotonicClock({});
		hive::modelsLibrary::ControllerManager::getInstance().destroyController();
	}

	/** Loads the connected fixture in the virtual controller, with the simulated clock installed, and waits until the journal has seeded the baselines of both entities. */
	void loadConnectedFixture()
	{
		loadFixture(ConnectedFixturePath, 2u);
	}

	/** Loads a network state fixture in the virtual controller, with the simulated clock installed, and waits until the journal has processed the online event of every entity. */
	void loadFixture(QString const& filePath, std::size_t const entityCount)
	{
		auto& journal = hive::modelsLibrary::EventJournal::getInstance();
		journal.setMonotonicClock(
			[this]()
			{
				return _now;
			});

		// The session survives controller reloads and the same fixture entities are used by every test: only the events journaled from now on are considered
		_fixtureEventsOffset = journal.currentSessionEvents().size();

		auto const onlineCountBefore = entityOnlineCount();
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const [err, msg] = controllerManager.loadVirtualEntitiesFromJsonNetworkState(filePath, fixtureLoadFlags());
		ASSERT_EQ(la::avdecc::jsonSerializer::DeserializationError::NoError, err) << "Failed to load NetworkState file: " << msg;
		waitForEntitiesOnline(onlineCountBefore + entityCount);
	}

	/** Waits until the journal has processed the given total number of "Entity is online" events. */
	void waitForEntitiesOnline(std::size_t const expectedCount)
	{
		auto const deadline = QDeadlineTimer{ 5000 };
		while (!deadline.hasExpired() && entityOnlineCount() < expectedCount)
		{
			QTest::qWait(10);
		}
		ASSERT_EQ(expectedCount, entityOnlineCount()) << "The fixture entities did not come online";
	}

	/** Flags used to load the fixtures (the same as Hive's network state import, text format). */
	static la::avdecc::entity::model::jsonSerializer::Flags fixtureLoadFlags() noexcept
	{
		return la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics };
	}

	/** Gets the details of the last "Entity is online" event journaled since the fixture was loaded for the given entity (an empty object if none). */
	QJsonObject entityOnlineDetails(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto details = QJsonObject{};
		auto const sessionEvents = hive::modelsLibrary::EventJournal::getInstance().currentSessionEvents();
		for (auto index = _fixtureEventsOffset; index < sessionEvents.size(); ++index)
		{
			auto const& event = sessionEvents[index];
			if (event.category == hive::modelsLibrary::EventJournal::Category::Entity && event.entityID == entityID && event.summary == "Entity is online")
			{
				details = QJsonDocument::fromJson(event.details.toUtf8()).object();
			}
		}
		return details;
	}

	/** Gets the events of the given category journaled since the fixture was loaded for the given entity, in journal order. */
	std::vector<hive::modelsLibrary::EventJournal::Event> eventsSince(hive::modelsLibrary::EventJournal::Category const category, la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto events = std::vector<hive::modelsLibrary::EventJournal::Event>{};
		auto const sessionEvents = hive::modelsLibrary::EventJournal::getInstance().currentSessionEvents();
		for (auto index = _fixtureEventsOffset; index < sessionEvents.size(); ++index)
		{
			if (sessionEvents[index].category == category && sessionEvents[index].entityID == entityID)
			{
				events.push_back(sessionEvents[index]);
			}
		}
		return events;
	}

	/** Gets the FRAMES_RX value the journal seeded as baseline for the given stream (the value stored in the fixture). */
	la::avdecc::entity::model::DescriptorCounter seededFramesRx(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		if (auto controlledEntity = controllerManager.getControlledEntity(entityID))
		{
			try
			{
				auto const& entityNode = controlledEntity->getEntityNode();
				auto const& counters = controlledEntity->getStreamInputNode(entityNode.dynamicModel.currentConfiguration, streamIndex).dynamicModel.counters;
				if (counters)
				{
					return counters->at(la::avdecc::entity::StreamInputCounterValidFlag::FramesRx);
				}
			}
			catch (...)
			{
			}
		}
		return 0u;
	}

	/** Advances the simulated clock then simulates a stream input counters notification carrying the given FRAMES_RX value. */
	void notifyFramesRx(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorCounter const framesRx, std::chrono::milliseconds const elapsed, la::avdecc::entity::model::StreamIndex const streamIndex = TestStreamIndex) noexcept
	{
		_now += elapsed;
		auto counters = la::avdecc::entity::model::StreamInputCounters{};
		counters[la::avdecc::entity::StreamInputCounterValidFlag::FramesRx] = framesRx;
		counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked] = 1u;
		emit hive::modelsLibrary::ControllerManager::getInstance().streamInputCountersChanged(entityID, streamIndex, counters);
	}

	/** Simulates one second of steady streaming: one notification per second, one second worth of frames each, starting from the given value. Returns the last notified value. */
	la::avdecc::entity::model::DescriptorCounter streamSteadily(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorCounter framesRx, std::size_t const notifications) noexcept
	{
		for (auto index = std::size_t{ 0u }; index < notifications; ++index)
		{
			framesRx += FramesPerSecond;
			notifyFramesRx(entityID, framesRx, OneSecond);
		}
		return framesRx;
	}

	/** Loads the fixture and streams steadily on the connected stream until its learning phase is over. Returns the last notified FRAMES_RX value. */
	la::avdecc::entity::model::DescriptorCounter learnConnectedStream()
	{
		loadConnectedFixture();
		auto const framesRx = streamSteadily(la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID }, seededFramesRx(la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID }, TestStreamIndex), LearningSamples);
		EXPECT_EQ(std::size_t{ 1u }, streamDataEvents(la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID }).size()) << "The learning phase should have ended with a single 'monitoring started' event";
		return framesRx;
	}

	/** Gets the stream data events journaled since the fixture was loaded for the given entity, in journal order. */
	std::vector<StreamDataEvent> streamDataEvents(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto events = std::vector<StreamDataEvent>{};
		auto const sessionEvents = hive::modelsLibrary::EventJournal::getInstance().currentSessionEvents();
		for (auto index = _fixtureEventsOffset; index < sessionEvents.size(); ++index)
		{
			auto const& event = sessionEvents[index];
			if (event.category != hive::modelsLibrary::EventJournal::Category::Streaming || event.entityID != entityID)
			{
				continue;
			}
			auto const details = QJsonDocument::fromJson(event.details.toUtf8()).object();
			auto streamDataEvent = StreamDataEvent{};
			streamDataEvent.summary = event.summary;
			streamDataEvent.severity = event.severity;
			streamDataEvent.framesRx = details.value("frames_rx").toVariant().toLongLong();
			streamDataEvent.increment = details.value("increment").toVariant().toLongLong();
			streamDataEvent.expectedIncrement = details.value("expected_increment").toVariant().toLongLong();
			streamDataEvent.elapsedMs = details.value("elapsed_ms").toVariant().toLongLong();
			streamDataEvent.missingMs = details.value("missing_ms").toVariant().toLongLong();
			streamDataEvent.talkerID = details.value("talker_id").toString();
			streamDataEvent.talkerStreamIndex = details.value("talker_stream_index").toInt(-1);
			events.push_back(streamDataEvent);
		}
		return events;
	}

	/** Gets the stream data loss events (the 'monitoring started' one excluded) journaled so far for the given entity. */
	std::vector<StreamDataEvent> streamDataLossEvents(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto events = streamDataEvents(entityID);
		events.erase(std::remove_if(events.begin(), events.end(),
									 [](StreamDataEvent const& event)
									 {
										 return event.severity != hive::modelsLibrary::EventJournal::Severity::Error;
									 }),
			events.end());
		return events;
	}

	/** Gets the summaries of the events of the given category journaled since the fixture was loaded (all entities), in journal order. */
	QStringList summariesSince(hive::modelsLibrary::EventJournal::Category const category) const noexcept
	{
		auto summaries = QStringList{};
		auto const sessionEvents = hive::modelsLibrary::EventJournal::getInstance().currentSessionEvents();
		for (auto index = _fixtureEventsOffset; index < sessionEvents.size(); ++index)
		{
			if (sessionEvents[index].category == category)
			{
				summaries.push_back(sessionEvents[index].summary);
			}
		}
		return summaries;
	}

	/** Counts the "Entity is online" events journaled so far (all entities). */
	std::size_t entityOnlineCount() const noexcept
	{
		auto count = std::size_t{ 0u };
		for (auto const& event : hive::modelsLibrary::EventJournal::getInstance().currentSessionEvents())
		{
			if (event.category == hive::modelsLibrary::EventJournal::Category::Entity && event.summary == "Entity is online")
			{
				++count;
			}
		}
		return count;
	}

	/** Gets the AVB interface information stored in the entity model for the given interface (default constructed if not available). */
	static la::avdecc::entity::model::AvbInterfaceInfo avbInterfaceInfo(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		if (auto controlledEntity = controllerManager.getControlledEntity(entityID))
		{
			try
			{
				auto const& info = controlledEntity->getAvbInterfaceNode(controlledEntity->getCurrentConfigurationIndex(), avbInterfaceIndex).dynamicModel.avbInterfaceInfo;
				if (info)
				{
					return *info;
				}
			}
			catch (...)
			{
			}
		}
		return {};
	}

	/** Gets the patch of an event (an empty array if the event has none). */
	static nlohmann::json eventPatch(hive::modelsLibrary::EventJournal::Event const& event) noexcept
	{
		try
		{
			auto const details = nlohmann::json::parse(event.details.toStdString());
			if (details.contains(hive::modelsLibrary::EventJournal::PatchDetailsKey))
			{
				return details.at(hive::modelsLibrary::EventJournal::PatchDetailsKey);
			}
		}
		catch (...)
		{
		}
		return nlohmann::json::array();
	}

	/** Gets the value of the patch operation of an event whose path ends with the given suffix (null if none), and checks the operation is a 'replace'. */
	static nlohmann::json patchValue(hive::modelsLibrary::EventJournal::Event const& event, std::string const& pathSuffix) noexcept
	{
		for (auto const& operation : eventPatch(event))
		{
			auto const path = operation.value("path", std::string{});
			if (path.size() >= pathSuffix.size() && path.compare(path.size() - pathSuffix.size(), pathSuffix.size(), pathSuffix) == 0)
			{
				EXPECT_EQ(std::string{ "replace" }, operation.value("op", std::string{}));
				return operation.value("value", nlohmann::json{});
			}
		}
		ADD_FAILURE() << "No patch operation ending with " << pathSuffix << " in event '" << event.summary.toStdString() << "': " << event.details.toStdString();
		return {};
	}

	/** Applies the patches of the given events (in order) to an AVE document, failing the test if one does not apply. */
	static nlohmann::json applyPatches(nlohmann::json document, std::vector<hive::modelsLibrary::EventJournal::Event> const& events) noexcept
	{
		for (auto const& event : events)
		{
			auto const patch = eventPatch(event);
			if (patch.empty())
			{
				continue;
			}
			try
			{
				document = document.patch(patch);
			}
			catch (std::exception const& e)
			{
				ADD_FAILURE() << "The patch of event '" << event.summary.toStdString() << "' does not apply: " << e.what() << " (" << patch.dump() << ")";
			}
		}
		return document;
	}

	/** Serialization flags of the snapshots (a full entity export in the .ave binary format). */
	static la::avdecc::entity::model::jsonSerializer::Flags snapshotFlags() noexcept
	{
		auto flags = fixtureLoadFlags();
		flags.set(la::avdecc::entity::model::jsonSerializer::Flag::ProcessDiagnostics);
		flags.set(la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat);
		return flags;
	}

	/** Gets the AVE document of the latest snapshot of the given entity in the current session (exported to a temporary file to read it back). */
	static nlohmann::json snapshotDocument(la::avdecc::UniqueIdentifier const entityID) noexcept
	{
		using EventJournal = hive::modelsLibrary::EventJournal;
		auto const filePath = QDir::temp().filePath("HiveEventJournalTests_changeTracking.hej");
		QFile::remove(filePath);
		if (!EventJournal::getInstance().exportCurrentSession(filePath))
		{
			ADD_FAILURE() << "Cannot export the session";
			return {};
		}
		auto snapshotID = std::optional<qint64>{};
		for (auto const& info : EventJournal::loadSnapshotsIndex(filePath))
		{
			if (info.entityID == entityID)
			{
				snapshotID = info.id;
			}
		}
		auto document = nlohmann::json{};
		if (snapshotID)
		{
			auto const data = EventJournal::loadSnapshotData(filePath, *snapshotID);
			document = nlohmann::json::from_msgpack(data.begin(), data.end());
		}
		else
		{
			ADD_FAILURE() << "No snapshot of the entity";
		}
		QFile::remove(filePath);
		return document;
	}

	/** Gets the AVE document of the given entity as the controller sees it now. */
	static nlohmann::json currentDocument(la::avdecc::UniqueIdentifier const entityID) noexcept
	{
		auto const filePath = QDir::temp().filePath("HiveEventJournalTests_changeTracking.ave");
		QFile::remove(filePath);
		auto const [error, message] = hive::modelsLibrary::ControllerManager::getInstance().serializeControlledEntityAsJson(entityID, filePath, snapshotFlags(), "Unit Tests");
		if (!!error)
		{
			ADD_FAILURE() << "Cannot serialize the entity: " << message;
			return {};
		}
		auto file = QFile{ filePath };
		if (!file.open(QIODevice::ReadOnly))
		{
			ADD_FAILURE() << "Cannot read the serialized entity";
			return {};
		}
		auto const data = file.readAll();
		file.close();
		QFile::remove(filePath);
		return nlohmann::json::from_msgpack(data.begin(), data.end());
	}

	/** Waits until the given number of events of the given category were journaled since the fixture was loaded for the given entity. */
	std::vector<hive::modelsLibrary::EventJournal::Event> waitForEvents(hive::modelsLibrary::EventJournal::Category const category, la::avdecc::UniqueIdentifier const entityID, std::size_t const count) const noexcept
	{
		auto const deadline = QDeadlineTimer{ 5000 };
		while (!deadline.hasExpired() && eventsSince(category, entityID).size() < count)
		{
			QTest::qWait(10);
		}
		auto const events = eventsSince(category, entityID);
		EXPECT_EQ(count, events.size()) << "Unexpected number of " << hive::modelsLibrary::EventJournal::categoryToString(category).toStdString() << " events";
		return events;
	}

	/** Expects the AVE document rebuilt from the snapshot and the given events to match the entity as the controller sees it now (entity model, state and diagnostics; the statistics change with every command and are not compared). */
	static void expectReplayMatches(la::avdecc::UniqueIdentifier const entityID, nlohmann::json const& snapshot, std::vector<hive::modelsLibrary::EventJournal::Event> const& events) noexcept
	{
		auto const replayed = applyPatches(snapshot, events);
		auto const current = currentDocument(entityID);
		for (auto const* const key : { "entity_model", "state", "diagnostics", "compatibility_flags" })
		{
			auto const diff = nlohmann::json::diff(replayed.value(key, nlohmann::json{}), current.value(key, nlohmann::json{}));
			EXPECT_TRUE(diff.empty()) << "The replayed '" << key << "' differs from the current entity: " << diff.dump();
		}
	}

	/** Advances the simulated clock. */
	void advanceClock(std::chrono::milliseconds const elapsed) noexcept
	{
		_now += elapsed;
	}

	/** Simulates a clock domain counters notification for the test entity. */
	void notifyCounters(la::avdecc::entity::model::DescriptorCounter const lockedCounter, la::avdecc::entity::model::DescriptorCounter const unlockedCounter) noexcept
	{
		auto counters = la::avdecc::entity::model::ClockDomainCounters{};
		counters[la::avdecc::entity::ClockDomainCounterValidFlag::Locked] = lockedCounter;
		counters[la::avdecc::entity::ClockDomainCounterValidFlag::Unlocked] = unlockedCounter;
		emit hive::modelsLibrary::ControllerManager::getInstance().clockDomainCountersChanged(_testEntityID, TestClockDomainIndex, counters);
	}

	/** Gets the media clock events journaled so far for the test entity, in journal order. */
	std::vector<MediaClockEvent> mediaClockEvents() const noexcept
	{
		auto events = std::vector<MediaClockEvent>{};
		for (auto const& event : hive::modelsLibrary::EventJournal::getInstance().currentSessionEvents())
		{
			if (event.category != hive::modelsLibrary::EventJournal::Category::MediaClock || event.entityID != _testEntityID)
			{
				continue;
			}
			auto const details = QJsonDocument::fromJson(event.details.toUtf8()).object();
			auto mediaClockEvent = MediaClockEvent{};
			mediaClockEvent.isLocked = event.summary == "Media clock locked";
			mediaClockEvent.isInferred = details.value("inferred").toBool(false);
			mediaClockEvent.lockedCounter = details.value("locked_counter").toVariant().toLongLong();
			mediaClockEvent.unlockedCounter = details.value("unlocked_counter").toVariant().toLongLong();
			EXPECT_TRUE(mediaClockEvent.isLocked || event.summary == "Media clock unlocked") << "Unexpected media clock event summary: " << event.summary.toStdString();
			EXPECT_EQ(mediaClockEvent.isLocked ? hive::modelsLibrary::EventJournal::Severity::Recovered : hive::modelsLibrary::EventJournal::Severity::Error, event.severity);
			events.push_back(mediaClockEvent);
		}
		return events;
	}

private:
	int x{ 0 };
	QApplication _app{ x, nullptr };
	la::avdecc::UniqueIdentifier _testEntityID{};
	std::chrono::steady_clock::time_point _now{ std::chrono::steady_clock::time_point{} + std::chrono::hours{ 1 } }; // Simulated monotonic clock
	std::size_t _fixtureEventsOffset{ 0u }; // Number of session events journaled before the fixture was loaded
};
} // namespace

/* *********************************
   Media clock lock tracking
*/
// The first counters notification of a clock domain only seeds the baseline, it is not a transition
TEST_F(EventJournal_F, ClockDomainCounters_FirstNotificationIsSilent)
{
	notifyCounters(5u, 4u);
	EXPECT_TRUE(mediaClockEvents().empty());
}

// A change of the derived lock state is journaled as a single transition, carrying the raw counters
TEST_F(EventJournal_F, ClockDomainCounters_ObservedUnlockAndRelock)
{
	notifyCounters(5u, 4u); // Locked
	notifyCounters(5u, 5u); // Unlocked
	auto events = mediaClockEvents();
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_FALSE(events[0].isLocked);
	EXPECT_FALSE(events[0].isInferred);
	EXPECT_EQ(5, events[0].lockedCounter);
	EXPECT_EQ(5, events[0].unlockedCounter);

	notifyCounters(6u, 5u); // Locked again
	events = mediaClockEvents();
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_TRUE(events[1].isLocked);
	EXPECT_FALSE(events[1].isInferred);
	EXPECT_EQ(6, events[1].lockedCounter);
	EXPECT_EQ(5, events[1].unlockedCounter);
}

// Both counters incremented while the derived state stays locked means the clock unlocked and relocked between the two notifications: both transitions are journaled, flagged as inferred
TEST_F(EventJournal_F, ClockDomainCounters_UnlockAndRelockBetweenNotifications_IsInferred)
{
	notifyCounters(5u, 4u); // Locked
	notifyCounters(6u, 5u); // Still locked, but one more unlock and one more lock happened
	auto const events = mediaClockEvents();
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_FALSE(events[0].isLocked);
	EXPECT_TRUE(events[0].isInferred);
	EXPECT_TRUE(events[1].isLocked);
	EXPECT_TRUE(events[1].isInferred);
	EXPECT_EQ(6, events[1].lockedCounter);
	EXPECT_EQ(5, events[1].unlockedCounter);
}

// Several unlock/relock cycles between two notifications are all replayed
TEST_F(EventJournal_F, ClockDomainCounters_MultipleCyclesBetweenNotifications_AreAllReplayed)
{
	notifyCounters(5u, 4u); // Locked
	notifyCounters(8u, 7u); // Still locked, three cycles later
	auto const events = mediaClockEvents();
	ASSERT_EQ(std::size_t{ 6u }, events.size());
	for (auto index = std::size_t{ 0u }; index < events.size(); ++index)
	{
		EXPECT_EQ(index % 2u == 1u, events[index].isLocked) << "Transition " << index;
		EXPECT_TRUE(events[index].isInferred) << "Transition " << index;
	}
}

// An observed unlock preceded by a full cycle: the cycle is inferred, the final unlock is the observed transition
TEST_F(EventJournal_F, ClockDomainCounters_CycleThenUnlock_OnlyTheCycleIsInferred)
{
	notifyCounters(5u, 4u); // Locked
	notifyCounters(6u, 6u); // Unlocked, after one unlock/relock cycle
	auto const events = mediaClockEvents();
	ASSERT_EQ(std::size_t{ 3u }, events.size());
	EXPECT_FALSE(events[0].isLocked);
	EXPECT_TRUE(events[0].isInferred);
	EXPECT_TRUE(events[1].isLocked);
	EXPECT_TRUE(events[1].isInferred);
	EXPECT_FALSE(events[2].isLocked);
	EXPECT_FALSE(events[2].isInferred);
}

// The same from the unlocked state: a relock/unlock cycle then the observed relock
TEST_F(EventJournal_F, ClockDomainCounters_CycleThenRelock_OnlyTheCycleIsInferred)
{
	notifyCounters(4u, 4u); // Unlocked
	notifyCounters(6u, 5u); // Locked, after one lock/unlock cycle
	auto const events = mediaClockEvents();
	ASSERT_EQ(std::size_t{ 3u }, events.size());
	EXPECT_TRUE(events[0].isLocked);
	EXPECT_TRUE(events[0].isInferred);
	EXPECT_FALSE(events[1].isLocked);
	EXPECT_TRUE(events[1].isInferred);
	EXPECT_TRUE(events[2].isLocked);
	EXPECT_FALSE(events[2].isInferred);
}

// Only the LOCKED counter incrementing while already locked is not a transition
TEST_F(EventJournal_F, ClockDomainCounters_LockedIncrementWhileLocked_IsSilent)
{
	notifyCounters(5u, 4u); // Locked
	notifyCounters(6u, 4u); // Still locked
	EXPECT_TRUE(mediaClockEvents().empty());
}

// A device that only increments UNLOCKED while the derived state stays locked did unlock: the unlock is journaled, and the derived state forces an inferred relock
TEST_F(EventJournal_F, ClockDomainCounters_UnlockedIncrementWhileStayingLocked_IsAnInferredCycle)
{
	notifyCounters(10u, 3u); // Locked
	notifyCounters(10u, 4u); // Still locked by the derived rule
	auto const events = mediaClockEvents();
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_FALSE(events[0].isLocked);
	EXPECT_TRUE(events[0].isInferred);
	EXPECT_TRUE(events[1].isLocked);
	EXPECT_TRUE(events[1].isInferred);
}

// Counters going backwards (device reboot) carry no usable increment: only the resulting lock state is compared
TEST_F(EventJournal_F, ClockDomainCounters_CountersReset_OnlyTheStateIsCompared)
{
	notifyCounters(50u, 49u); // Locked
	notifyCounters(1u, 0u); // Locked, counters restarted
	EXPECT_TRUE(mediaClockEvents().empty());

	notifyCounters(1u, 1u); // Unlocked
	auto const events = mediaClockEvents();
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_FALSE(events[0].isLocked);
	EXPECT_FALSE(events[0].isInferred);
}

// Counters going backwards into a different lock state is journaled as an observed transition
TEST_F(EventJournal_F, ClockDomainCounters_CountersResetIntoAnotherState_IsAnObservedTransition)
{
	notifyCounters(50u, 49u); // Locked
	notifyCounters(0u, 1u); // Unlocked, counters restarted
	auto const events = mediaClockEvents();
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_FALSE(events[0].isLocked);
	EXPECT_FALSE(events[0].isInferred);
}

/* *********************************
   Stream data loss (FRAMES_RX increments)
*/
// While the stream is learning its expected increment, nothing is evaluated: even clearly partial increments give no event
TEST_F(EventJournal_F, StreamData_NoEventDuringLearning)
{
	loadConnectedFixture();
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto framesRx = seededFramesRx(entityID, TestStreamIndex);
	for (auto index = std::size_t{ 0u }; index + 1u < LearningSamples; ++index)
	{
		framesRx += FramesPerSecond * 4u / 10u;
		notifyFramesRx(entityID, framesRx, OneSecond);
	}
	EXPECT_TRUE(streamDataEvents(entityID).empty());
}

// Steady streaming ends the learning phase with a single 'monitoring started' event carrying the learned reference and the talker, then stays silent
TEST_F(EventJournal_F, StreamData_SteadyStreaming_OnlyStartsMonitoring)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto framesRx = learnConnectedStream();
	auto const events = streamDataEvents(entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Stream data monitoring started" }, events[0].summary);
	EXPECT_EQ(hive::modelsLibrary::EventJournal::Severity::Info, events[0].severity);
	EXPECT_EQ(static_cast<qint64>(FramesPerSecond), events[0].expectedIncrement);
	EXPECT_EQ(QString{ "0x001B92FFFE02233B" }, events[0].talkerID);
	EXPECT_EQ(0, events[0].talkerStreamIndex);

	streamSteadily(entityID, framesRx, 50u);
	EXPECT_EQ(std::size_t{ 1u }, streamDataEvents(entityID).size());
}

// A lost unsolicited notification doubles the next increment (and the elapsed time): no event
TEST_F(EventJournal_F, StreamData_LostNotification_IsSilent)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto const framesRx = learnConnectedStream();
	notifyFramesRx(entityID, framesRx + 2u * FramesPerSecond, 2u * OneSecond);
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());
}

// A stalled main thread delivers the queued notifications in a burst, with normal increments but no elapsed time: no event
TEST_F(EventJournal_F, StreamData_QueuedBurst_IsSilent)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto framesRx = learnConnectedStream();
	for (auto index = std::size_t{ 0u }; index < 3u; ++index)
	{
		framesRx += FramesPerSecond;
		notifyFramesRx(entityID, framesRx, std::chrono::milliseconds{ 0 });
	}
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());
}

// An increment at 40 % of the reference means frames were missing: one event with the estimate, the raw values and the talker
TEST_F(EventJournal_F, StreamData_PartialIncrement_IsAnEvent)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto const framesRx = learnConnectedStream();
	auto const increment = FramesPerSecond * 4u / 10u;
	notifyFramesRx(entityID, framesRx + increment, OneSecond);
	auto const events = streamDataLossEvents(entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Stream data loss: about 600 ms of frames missing" }, events[0].summary);
	EXPECT_EQ(static_cast<qint64>(framesRx + increment), events[0].framesRx);
	EXPECT_EQ(static_cast<qint64>(increment), events[0].increment);
	EXPECT_EQ(static_cast<qint64>(FramesPerSecond), events[0].expectedIncrement);
	EXPECT_EQ(1000, events[0].elapsedMs);
	EXPECT_EQ(600, events[0].missingMs);
	EXPECT_EQ(QString{ "0x001B92FFFE02233B" }, events[0].talkerID);
	EXPECT_EQ(0, events[0].talkerStreamIndex);
}

// The alerted increment is not learned, so repeated partial losses keep being reported against the same reference
TEST_F(EventJournal_F, StreamData_RepeatedPartialIncrements_DoNotLowerTheReference)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto framesRx = learnConnectedStream();
	for (auto index = std::size_t{ 0u }; index < 40u; ++index)
	{
		framesRx += FramesPerSecond * 4u / 10u;
		notifyFramesRx(entityID, framesRx, OneSecond);
	}
	auto const events = streamDataLossEvents(entityID);
	ASSERT_EQ(std::size_t{ 40u }, events.size());
	EXPECT_EQ(static_cast<qint64>(FramesPerSecond), events.back().expectedIncrement);
}

// An increment at 60 % of the reference is above the loss threshold: no event
TEST_F(EventJournal_F, StreamData_IncrementAboveThreshold_IsSilent)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto const framesRx = learnConnectedStream();
	notifyFramesRx(entityID, framesRx + FramesPerSecond * 6u / 10u, OneSecond);
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());
}

// A counter reset (value jump) only resynchronizes the reference sample: no event, and the learned reference is kept
TEST_F(EventJournal_F, StreamData_CounterReset_ResynchronizesSilently)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	learnConnectedStream();
	auto framesRx = la::avdecc::entity::model::DescriptorCounter{ 100u };
	notifyFramesRx(entityID, framesRx, OneSecond);
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());

	framesRx = streamSteadily(entityID, framesRx, 1u);
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());

	notifyFramesRx(entityID, framesRx + FramesPerSecond * 4u / 10u, OneSecond);
	auto const events = streamDataLossEvents(entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(static_cast<qint64>(FramesPerSecond), events[0].expectedIncrement);
}

// The counter is 32 bits: a wrap around is a normal increment
TEST_F(EventJournal_F, StreamData_CounterWrap_IsSilent)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	learnConnectedStream();
	auto const beforeWrap = la::avdecc::entity::model::DescriptorCounter{ 0xFFFFFFFFu - 3000u };
	// Jump close to the wrap (resynchronization), then wrap with a normal increment
	notifyFramesRx(entityID, beforeWrap, OneSecond);
	notifyFramesRx(entityID, beforeWrap + FramesPerSecond, OneSecond);
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());

	// The stream is still monitored
	notifyFramesRx(entityID, beforeWrap + FramesPerSecond + FramesPerSecond * 4u / 10u, OneSecond);
	EXPECT_EQ(std::size_t{ 1u }, streamDataLossEvents(entityID).size());
}

// A redundant controller receives every notification once per interface: the duplicate (same value, a few ms later) is not an observation, the stream is still learned and evaluated from the real notifications
TEST_F(EventJournal_F, StreamData_DuplicateNotifications_AreIgnored)
{
	loadConnectedFixture();
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto framesRx = seededFramesRx(entityID, TestStreamIndex);
	for (auto index = std::size_t{ 0u }; index < 40u; ++index)
	{
		framesRx += FramesPerSecond;
		notifyFramesRx(entityID, framesRx, OneSecond - std::chrono::milliseconds{ 3 });
		notifyFramesRx(entityID, framesRx, std::chrono::milliseconds{ 3 });
	}
	auto events = streamDataEvents(entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Stream data monitoring started" }, events[0].summary);
	EXPECT_EQ(static_cast<qint64>(FramesPerSecond), events[0].expectedIncrement);

	// The duplicate processed long after the original (the main thread stalled in between) is still the same observation
	framesRx += FramesPerSecond;
	notifyFramesRx(entityID, framesRx, OneSecond - std::chrono::milliseconds{ 3 });
	notifyFramesRx(entityID, framesRx, OneSecond);
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());

	// A partial increment is still detected against the reference learned from the real notifications
	framesRx += FramesPerSecond * 4u / 10u;
	notifyFramesRx(entityID, framesRx, OneSecond);
	events = streamDataLossEvents(entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(static_cast<qint64>(FramesPerSecond), events[0].expectedIncrement);
}

// The other interface of a redundant controller may carry a counter re-read a few frames later: that notification is neither learned (it would lower the reference) nor evaluated
TEST_F(EventJournal_F, StreamData_NearDuplicateNotifications_AreNotLearned)
{
	loadConnectedFixture();
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto framesRx = seededFramesRx(entityID, TestStreamIndex);
	for (auto index = std::size_t{ 0u }; index < 40u; ++index)
	{
		framesRx += FramesPerSecond - 1u;
		notifyFramesRx(entityID, framesRx, OneSecond - std::chrono::milliseconds{ 3 });
		framesRx += 1u;
		notifyFramesRx(entityID, framesRx, std::chrono::milliseconds{ 3 });
	}
	auto const events = streamDataEvents(entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Stream data monitoring started" }, events[0].summary);
	EXPECT_EQ(static_cast<qint64>(FramesPerSecond - 1u), events[0].expectedIncrement);
}

// A stream that is not connected is never monitored, whatever its counters do
TEST_F(EventJournal_F, StreamData_NotConnectedStream_IsIgnored)
{
	loadConnectedFixture();
	auto const entityID = la::avdecc::UniqueIdentifier{ TalkerEntityID };
	auto const framesRx = streamSteadily(entityID, 0u, LearningSamples + 5u);
	notifyFramesRx(entityID, framesRx + FramesPerSecond * 4u / 10u, OneSecond);
	EXPECT_TRUE(streamDataEvents(entityID).empty());
}

// A device whose notification cadence varies between one and two seconds produces increments between one and two seconds worth: the reference stays at one second worth, no event
TEST_F(EventJournal_F, StreamData_VariableCadence_IsSilent)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto framesRx = learnConnectedStream();
	auto const cadences = std::vector<std::chrono::milliseconds>{ std::chrono::milliseconds{ 1000 }, std::chrono::milliseconds{ 1500 }, std::chrono::milliseconds{ 2000 }, std::chrono::milliseconds{ 1200 }, std::chrono::milliseconds{ 1800 } };
	for (auto index = std::size_t{ 0u }; index < 60u; ++index)
	{
		auto const cadence = cadences[index % cadences.size()];
		framesRx += static_cast<la::avdecc::entity::model::DescriptorCounter>(FramesPerSecond * cadence.count() / 1000);
		notifyFramesRx(entityID, framesRx, cadence);
	}
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());
}

// Resuming after a long silence: the first notification carries only the frames received since the resume, the event tells how long the silence was
TEST_F(EventJournal_F, StreamData_ResumeAfterSilence_IsAnEventWithTheSilenceDuration)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto const framesRx = learnConnectedStream();
	notifyFramesRx(entityID, framesRx + 2000u, std::chrono::milliseconds{ 30000 });
	auto const events = streamDataLossEvents(entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(30000, events[0].elapsedMs);
	EXPECT_EQ(2000, events[0].increment);
	EXPECT_EQ(750, events[0].missingMs);
}

// A connection change restarts the learning (the counters are reset by a rebind and the first increment is partial)
TEST_F(EventJournal_F, StreamData_ConnectionChange_RestartsLearning)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto framesRx = learnConnectedStream();

	auto info = la::avdecc::entity::model::StreamInputConnectionInfo{};
	info.state = la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected;
	info.talkerStream = { la::avdecc::UniqueIdentifier{ TalkerEntityID }, TestStreamIndex };
	emit hive::modelsLibrary::ControllerManager::getInstance().streamInputConnectionChanged({ entityID, TestStreamIndex }, info);

	// The first notification only provides the previous value, the following ones are learned: no event even for partial increments
	framesRx = 0u;
	notifyFramesRx(entityID, framesRx, OneSecond);
	for (auto index = std::size_t{ 0u }; index < LearningSamples - 1u; ++index)
	{
		framesRx += FramesPerSecond * 4u / 10u;
		notifyFramesRx(entityID, framesRx, OneSecond);
	}
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());
	EXPECT_EQ(std::size_t{ 1u }, streamDataEvents(entityID).size()) << "The monitoring must not have restarted yet";
}

// A stream format change restarts the learning (the frame rate may change)
TEST_F(EventJournal_F, StreamData_FormatChange_RestartsLearning)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto framesRx = learnConnectedStream();

	emit hive::modelsLibrary::ControllerManager::getInstance().streamFormatChanged(entityID, la::avdecc::entity::model::DescriptorType::StreamInput, TestStreamIndex, la::avdecc::entity::model::StreamFormat{});

	notifyFramesRx(entityID, framesRx, OneSecond);
	for (auto index = std::size_t{ 0u }; index < LearningSamples - 1u; ++index)
	{
		framesRx += FramesPerSecond * 4u / 10u;
		notifyFramesRx(entityID, framesRx, OneSecond);
	}
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());

	// A stream output format change is not about this stream
	emit hive::modelsLibrary::ControllerManager::getInstance().streamFormatChanged(entityID, la::avdecc::entity::model::DescriptorType::StreamOutput, TestStreamIndex, la::avdecc::entity::model::StreamFormat{});
	framesRx += FramesPerSecond * 4u / 10u;
	notifyFramesRx(entityID, framesRx, OneSecond);
	EXPECT_EQ(std::size_t{ 2u }, streamDataEvents(entityID).size()) << "The learning phase should have ended again";
}

// A notification without FRAMES_RX is ignored
TEST_F(EventJournal_F, StreamData_NotificationWithoutFramesRx_IsIgnored)
{
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	learnConnectedStream();
	auto counters = la::avdecc::entity::model::StreamInputCounters{};
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked] = 1u;
	emit hive::modelsLibrary::ControllerManager::getInstance().streamInputCountersChanged(entityID, TestStreamIndex, counters);
	EXPECT_TRUE(streamDataLossEvents(entityID).empty());
}

/* *********************************
   Recording level
*/
// Diagnostic level events are only written while diagnostic recording is enabled, the switches are journaled at user level, and enabling it re-announces the covered streams
TEST_F(EventJournal_F, RecordingLevel_DiagnosticEventsFollowTheLevel)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto& journal = EventJournal::getInstance();
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };

	loadConnectedFixture();
	journal.setRecordingLevel(EventJournal::Level::User);
	EXPECT_EQ(EventJournal::Level::User, journal.recordingLevel());
	EXPECT_EQ(QStringList{ "Diagnostic recording disabled" }, summariesSince(EventJournal::Category::Session));

	// The end of the learning phase is a diagnostic event: not written at user level
	auto framesRx = streamSteadily(entityID, seededFramesRx(entityID, TestStreamIndex), LearningSamples + 2u);
	EXPECT_TRUE(streamDataEvents(entityID).empty());

	// Enabling diagnostic recording is journaled, and the stream announces its coverage again at its next notification
	journal.setRecordingLevel(EventJournal::Level::Diagnostic);
	journal.setRecordingLevel(EventJournal::Level::Diagnostic); // No-op
	EXPECT_EQ((QStringList{ "Diagnostic recording disabled", "Diagnostic recording enabled" }), summariesSince(EventJournal::Category::Session));
	framesRx = streamSteadily(entityID, framesRx, 1u);
	auto const events = streamDataEvents(entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Stream data monitoring started" }, events[0].summary);
	for (auto const& event : journal.currentSessionEvents())
	{
		if (event.summary == "Stream data monitoring started")
		{
			EXPECT_EQ(EventJournal::Level::Diagnostic, event.level);
		}
		else if (event.category == EventJournal::Category::Session || event.category == EventJournal::Category::Streaming)
		{
			EXPECT_EQ(EventJournal::Level::User, event.level) << event.summary.toStdString();
		}
	}

	// Losses are user level events: written whatever the level
	journal.setRecordingLevel(EventJournal::Level::User);
	notifyFramesRx(entityID, framesRx + FramesPerSecond * 4u / 10u, OneSecond);
	EXPECT_EQ(std::size_t{ 1u }, streamDataLossEvents(entityID).size());
	journal.setRecordingLevel(EventJournal::Level::Diagnostic);
}

/* *********************************
   Schema compatibility
*/
// A version 1 file (no level column, no snapshots table) loads with every event at user level
TEST_F(EventJournal_F, Session_SchemaV1Fixture_Loads)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const session = EventJournal::loadSession(SchemaV1FixturePath);
	ASSERT_TRUE(session.has_value());
	EXPECT_EQ(1, session->schemaVersion());
	EXPECT_FALSE(session->isNewerThanReader());
	EXPECT_EQ(std::size_t{ 31u }, session->events.size());
	EXPECT_EQ(QString{ "1.5.0-beta1" }, session->metadata.value("hive_version"));
	EXPECT_EQ(QString{ "Test Secondary" }, session->metadata.value("secondary_interface_name"));
	auto categories = QSet<int>{};
	for (auto const& event : session->events)
	{
		EXPECT_EQ(EventJournal::Level::User, event.level);
		categories.insert(static_cast<int>(event.category));
	}
	EXPECT_EQ((QSet<int>{ static_cast<int>(EventJournal::Category::Session), static_cast<int>(EventJournal::Category::Entity), static_cast<int>(EventJournal::Category::Counters), static_cast<int>(EventJournal::Category::MediaClock) }), categories);
	EXPECT_EQ(QString{ "Recording started" }, session->events.front().summary);
	EXPECT_EQ(EventJournal::Severity::Warning, session->events.back().severity);
	EXPECT_EQ(QString{ "Entity went offline" }, session->events.back().summary);
	EXPECT_EQ(6, std::count_if(session->events.begin(), session->events.end(),
								 [](EventJournal::Event const& event)
								 {
									 return event.severity == EventJournal::Severity::Recovered && event.summary == "Media clock locked";
								 }));
}

// A version 2 file loads with the level of every event and the new metadata keys
TEST_F(EventJournal_F, Session_SchemaV2Fixture_Loads)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const session = EventJournal::loadSession(SchemaV2FixturePath);
	ASSERT_TRUE(session.has_value());
	EXPECT_EQ(2, session->schemaVersion());
	EXPECT_FALSE(session->isNewerThanReader());
	EXPECT_EQ(QString{ "diagnostic" }, session->metadata.value("recording_level_at_start"));
	EXPECT_FALSE(session->metadata.value("la_avdecc_version").isEmpty());
	EXPECT_FALSE(session->metadata.value("os").isEmpty());
	auto userCount = std::size_t{ 0u };
	auto diagnosticCount = std::size_t{ 0u };
	for (auto const& event : session->events)
	{
		if (event.level == EventJournal::Level::Diagnostic)
		{
			++diagnosticCount;
			EXPECT_EQ(QString{ "Stream data monitoring started" }, event.summary);
		}
		else
		{
			++userCount;
		}
	}
	EXPECT_EQ(std::size_t{ 1u }, diagnosticCount);
	EXPECT_GT(userCount, std::size_t{ 3u });
}

// A file written by a newer schema still loads (degraded mode): unknown categories and levels fall back, and the session reports it is newer than the reader
TEST_F(EventJournal_F, Session_NewerSchema_LoadsInDegradedMode)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const filePath = QDir::temp().filePath("HiveEventJournalTests_schema_v99.hej");
	QFile::remove(filePath);
	ASSERT_TRUE(QFile::copy(SchemaV2FixturePath, filePath));
	{
		auto db = QSqlDatabase::addDatabase("QSQLITE", "SchemaV99Writer");
		db.setDatabaseName(filePath);
		ASSERT_TRUE(db.open());
		auto query = QSqlQuery{ db };
		ASSERT_TRUE(query.exec("UPDATE metadata SET value = '99' WHERE key = 'schema_version'"));
		ASSERT_TRUE(query.exec("ALTER TABLE events ADD COLUMN future_column TEXT"));
		ASSERT_TRUE(query.exec("INSERT INTO events (timestamp, category, severity, entity_id, entity_name, subject, summary, details, level, future_column) VALUES (9999999999999, 'future_category', 'future_severity', '', '', '', 'Future event', '', 'future_level', 'x')"));
		db.close();
	}
	QSqlDatabase::removeDatabase("SchemaV99Writer");

	auto const session = EventJournal::loadSession(filePath);
	ASSERT_TRUE(session.has_value());
	EXPECT_EQ(99, session->schemaVersion());
	EXPECT_TRUE(session->isNewerThanReader());
	ASSERT_FALSE(session->events.empty());
	auto const& futureEvent = session->events.back();
	EXPECT_EQ(QString{ "Future event" }, futureEvent.summary);
	EXPECT_EQ(EventJournal::Category::Session, futureEvent.category);
	EXPECT_EQ(EventJournal::Severity::Info, futureEvent.severity);
	EXPECT_EQ(EventJournal::Level::User, futureEvent.level);
	QFile::remove(filePath);
}

// The live writer produces a file this reader loads back identically (levels and metadata included); the exported file of this test is the committed schema_v2.hej fixture
TEST_F(EventJournal_F, Session_WriterRoundTrip)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto& journal = EventJournal::getInstance();

	// A few events of both levels
	journal.setRecordingLevel(EventJournal::Level::Diagnostic);
	learnConnectedStream();
	notifyCounters(5u, 4u);
	notifyCounters(5u, 5u);

	auto const filePath = QDir::temp().filePath("HiveEventJournalTests_schema_v2.hej");
	QFile::remove(filePath);
	ASSERT_TRUE(journal.exportCurrentSession(filePath));

	auto const session = EventJournal::loadSession(filePath);
	ASSERT_TRUE(session.has_value());
	EXPECT_EQ(EventJournal::CurrentSchemaVersion, session->schemaVersion());
	EXPECT_EQ(journal.currentSessionMetadata(), session->metadata);
	auto const liveEvents = journal.currentSessionEvents();
	ASSERT_EQ(liveEvents.size(), session->events.size());
	for (auto index = std::size_t{ 0u }; index < liveEvents.size(); ++index)
	{
		EXPECT_EQ(liveEvents[index].timestamp, session->events[index].timestamp);
		EXPECT_EQ(liveEvents[index].category, session->events[index].category);
		EXPECT_EQ(liveEvents[index].severity, session->events[index].severity);
		EXPECT_EQ(liveEvents[index].level, session->events[index].level);
		EXPECT_EQ(liveEvents[index].entityID, session->events[index].entityID);
		EXPECT_EQ(liveEvents[index].summary, session->events[index].summary);
		EXPECT_EQ(liveEvents[index].details, session->events[index].details);
	}
	EXPECT_TRUE(std::any_of(session->events.begin(), session->events.end(),
		[](EventJournal::Event const& event)
		{
			return event.level == EventJournal::Level::Diagnostic;
		}));
	EXPECT_EQ(QString{ "diagnostic" }, session->metadata.value("recording_level_at_start"));
}

/* *********************************
   Snapshots and entity description
*/
// Every entity coming online gets a snapshot referenced by its 'Entity is online' event, which loads back as the same virtual entity
TEST_F(EventJournal_F, Snapshots_WrittenAtEntityOnline_AndLoadBack)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto& journal = EventJournal::getInstance();
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();

	auto const details = entityOnlineDetails(entityID);
	ASSERT_TRUE(details.contains("snapshot_id"));
	auto const snapshotID = details.value("snapshot_id").toVariant().toLongLong();
	EXPECT_GT(snapshotID, 0);

	auto const filePath = QDir::temp().filePath("HiveEventJournalTests_snapshots.hej");
	QFile::remove(filePath);
	ASSERT_TRUE(journal.exportCurrentSession(filePath));

	auto const index = EventJournal::loadSnapshotsIndex(filePath);
	auto const it = std::find_if(index.begin(), index.end(),
		[snapshotID](EventJournal::SnapshotInfo const& info)
		{
			return info.id == snapshotID;
		});
	ASSERT_NE(index.end(), it);
	EXPECT_EQ(entityID, it->entityID);
	EXPECT_EQ(QString{ "online" }, it->reason);
	EXPECT_EQ(QString{ EventJournal::SnapshotFormat }, it->format);
	EXPECT_GT(it->uncompressedSize, 0);
	EXPECT_GT(it->compressedSize, 0);
	EXPECT_LT(it->compressedSize, it->uncompressedSize);
	std::cout << "Snapshot of " << details.value("entity_model_id").toString().toStdString() << ": " << it->uncompressedSize << " bytes serialized, " << it->compressedSize << " bytes stored, session serialization time " << journal.currentSessionMetadata().value("snapshots_serialization_ms").toStdString() << " ms for " << journal.currentSessionMetadata().value("snapshots_count").toStdString() << " snapshots" << std::endl;

	auto const data = EventJournal::loadSnapshotData(filePath, snapshotID);
	ASSERT_EQ(it->uncompressedSize, data.size());
	EXPECT_TRUE(EventJournal::loadSnapshotData(filePath, snapshotID + 1000).isEmpty());

	// Replace the live entity with the one restored from the snapshot
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
	ASSERT_TRUE(controllerManager.unloadVirtualEntity(entityID));
	auto const deadline = QDeadlineTimer{ 5000 };
	while (!deadline.hasExpired() && !!controllerManager.getControlledEntity(entityID))
	{
		QTest::qWait(10);
	}
	auto const snapshotFilePath = QDir::temp().filePath("HiveEventJournalTests_snapshot.ave");
	{
		auto file = QFile{ snapshotFilePath };
		ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
		ASSERT_EQ(data.size(), file.write(data));
	}
	auto flags = fixtureLoadFlags();
	flags.set(la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat);
	flags.set(la::avdecc::entity::model::jsonSerializer::Flag::ProcessDiagnostics);
	auto const onlineCountBefore = entityOnlineCount();
	auto const [err, msg] = controllerManager.loadVirtualEntityFromJson(snapshotFilePath, flags);
	ASSERT_EQ(la::avdecc::jsonSerializer::DeserializationError::NoError, err) << "Failed to load the snapshot back: " << msg;
	waitForEntitiesOnline(onlineCountBefore + 1u);
	{
		auto const controlledEntity = controllerManager.getControlledEntity(entityID);
		ASSERT_TRUE(!!controlledEntity);
		EXPECT_EQ(std::string{ "P1 152" }, controlledEntity->getEntityNode().dynamicModel.entityName.str());
		EXPECT_EQ(la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected, controlledEntity->getStreamInputNode(0u, TestStreamIndex).dynamicModel.connectionInfo.state);
	}
	QFile::remove(snapshotFilePath);
	QFile::remove(filePath);
}

// The 'Entity is online' details describe the entity (identity, firmware, interfaces, clock sources, connected streams), so the analysis needs no network state dump
TEST_F(EventJournal_F, EntityOnline_DetailsDescribeTheEntity)
{
	loadConnectedFixture();
	auto const details = entityOnlineDetails(la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID });
	EXPECT_EQ(QString{ "0x001B9200030A0000" }, details.value("entity_model_id").toString());
	EXPECT_FALSE(details.value("vendor").toString().isEmpty());
	EXPECT_EQ(QString{ "2.12.0.48" }, details.value("firmware_version").toString());
	EXPECT_TRUE(details.contains("enumeration_time_ms"));

	auto const avbInterfaces = details.value("avb_interfaces").toArray();
	ASSERT_EQ(1, avbInterfaces.size());
	EXPECT_EQ(0, avbInterfaces[0].toObject().value("index").toInt());
	EXPECT_FALSE(avbInterfaces[0].toObject().value("mac").toString().isEmpty());
	EXPECT_FALSE(avbInterfaces[0].toObject().value("clock_identity").toString().isEmpty());
	EXPECT_FALSE(avbInterfaces[0].toObject().value("grandmaster_id").toString().isEmpty());

	auto const clockDomains = details.value("clock_domains").toArray();
	ASSERT_EQ(1, clockDomains.size());
	EXPECT_TRUE(clockDomains[0].toObject().contains("clock_source_index"));

	auto const streamInputs = details.value("stream_inputs").toArray();
	ASSERT_EQ(3, streamInputs.size());
	for (auto const& value : streamInputs)
	{
		auto const stream = value.toObject();
		EXPECT_EQ(QString{ "0x001B92FFFE02233B" }, stream.value("talker_entity_id").toString());
		EXPECT_EQ(stream.value("index").toInt(), stream.value("talker_stream_index").toInt());
		EXPECT_FALSE(stream.value("name").toString().isEmpty());
	}
	// The talker entity has no connected input
	EXPECT_EQ(0, entityOnlineDetails(la::avdecc::UniqueIdentifier{ TalkerEntityID }).value("stream_inputs").toArray().size());

	// AsPath and propagation delay come from the gPTP state of the interfaces
	loadFixture(TopologyFixturePath, TopologyFixtureEntityCount);
	auto const topologyDetails = entityOnlineDetails(la::avdecc::UniqueIdentifier{ TopologyEntityA });
	auto const topologyInterfaces = topologyDetails.value("avb_interfaces").toArray();
	ASSERT_EQ(1, topologyInterfaces.size());
	auto const asPath = topologyInterfaces[0].toObject().value("as_path").toArray();
	ASSERT_EQ(2, asPath.size());
	EXPECT_EQ(QString{ "0x001122FFFE0000FF" }, asPath[0].toString());
	EXPECT_EQ(QString{ "0x001122FFFE000001" }, asPath[1].toString());
	EXPECT_EQ(20, topologyInterfaces[0].toObject().value("propagation_delay").toInt());
}

// An AsPath change is a user level topology event carrying the previous and new paths; a propagation delay change is a diagnostic event
TEST_F(EventJournal_F, Gptp_AsPathAndPropagationDelayChanges_AreJournaled)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ TopologyEntityA };
	loadFixture(TopologyFixturePath, TopologyFixtureEntityCount);
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	// Same path as the baseline: silent
	auto samePath = la::avdecc::entity::model::AsPath{};
	samePath.sequence = { la::avdecc::UniqueIdentifier{ TopologyGrandmasterClockIdentity }, la::avdecc::UniqueIdentifier{ TopologyEntityA } };
	emit controllerManager.asPathChanged(entityID, 0u, samePath);
	EXPECT_TRUE(eventsSince(EventJournal::Category::Gptp, entityID).empty());

	// A new hop appeared between the grandmaster and the entity
	auto newPath = la::avdecc::entity::model::AsPath{};
	newPath.sequence = { la::avdecc::UniqueIdentifier{ TopologyGrandmasterClockIdentity }, la::avdecc::UniqueIdentifier{ 0x001122FFFE0000D0u }, la::avdecc::UniqueIdentifier{ TopologyEntityA } };
	emit controllerManager.asPathChanged(entityID, 0u, newPath);
	auto events = eventsSince(EventJournal::Category::Gptp, entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "gPTP AsPath changed" }, events[0].summary);
	EXPECT_EQ(EventJournal::Severity::Info, events[0].severity);
	EXPECT_EQ(EventJournal::Level::User, events[0].level);
	EXPECT_EQ(QString{ "AVB Interface 0" }, events[0].subject);
	auto details = QJsonDocument::fromJson(events[0].details.toUtf8()).object();
	EXPECT_EQ(2, details.value("previous_as_path").toArray().size());
	ASSERT_EQ(3, details.value("new_as_path").toArray().size());
	EXPECT_EQ(QString{ "0x001122FFFE0000D0" }, details.value("new_as_path").toArray()[1].toString());

	// Propagation delay: same information silent, new delay journaled at diagnostic level
	auto info = avbInterfaceInfo(entityID, 0u);
	ASSERT_EQ(20u, info.propagationDelay);
	emit controllerManager.avbInterfaceInfoChanged(entityID, 0u, info);
	EXPECT_EQ(std::size_t{ 1u }, eventsSince(EventJournal::Category::Gptp, entityID).size());
	info.propagationDelay = 25u;
	emit controllerManager.avbInterfaceInfoChanged(entityID, 0u, info);
	events = eventsSince(EventJournal::Category::Gptp, entityID);
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_EQ(QString{ "gPTP propagation delay changed" }, events[1].summary);
	EXPECT_EQ(EventJournal::Level::Diagnostic, events[1].level);
	details = QJsonDocument::fromJson(events[1].details.toUtf8()).object();
	EXPECT_EQ(20, details.value("previous_propagation_delay").toInt());
	EXPECT_EQ(25, details.value("new_propagation_delay").toInt());
	EXPECT_EQ(25, patchValue(events[1], "/avb_interface_descriptors/0/dynamic/avb_interface_info").value("propagation_delay", 0));

	// Another field of the information (the flags) changed without the delay: a generic event
	if (info.flags.test(la::avdecc::entity::AvbInfoFlag::AvtpDown))
	{
		info.flags.reset(la::avdecc::entity::AvbInfoFlag::AvtpDown);
	}
	else
	{
		info.flags.set(la::avdecc::entity::AvbInfoFlag::AvtpDown);
	}
	emit controllerManager.avbInterfaceInfoChanged(entityID, 0u, info);
	events = eventsSince(EventJournal::Category::Gptp, entityID);
	ASSERT_EQ(std::size_t{ 3u }, events.size());
	EXPECT_EQ(QString{ "AVB interface info changed" }, events[2].summary);
	EXPECT_EQ(EventJournal::Level::Diagnostic, events[2].level);
}

// A manual snapshot stores every online entity and journals a session event
TEST_F(EventJournal_F, Snapshots_Manual_StoresEveryOnlineEntity)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto& journal = EventJournal::getInstance();
	loadConnectedFixture();
	auto const countBefore = journal.currentSessionMetadata().value("snapshots_count").toULongLong();
	EXPECT_EQ(std::size_t{ 2u }, journal.snapshotAllEntities());
	EXPECT_EQ(countBefore + 2u, journal.currentSessionMetadata().value("snapshots_count").toULongLong());
	EXPECT_EQ(QStringList{ "Snapshot of 2 entities taken" }, summariesSince(EventJournal::Category::Session));

	auto const filePath = QDir::temp().filePath("HiveEventJournalTests_manualSnapshots.hej");
	QFile::remove(filePath);
	ASSERT_TRUE(journal.exportCurrentSession(filePath));
	auto const index = EventJournal::loadSnapshotsIndex(filePath);
	EXPECT_EQ(2, std::count_if(index.begin(), index.end(),
								 [](EventJournal::SnapshotInfo const& info)
								 {
									 return info.reason == "manual";
								 }));
	QFile::remove(filePath);
}

/* *********************************
   Change tracking events (phase 4a)
*/
// Object name changes are journaled with a patch of the AVE document, including for the descriptors nested under their parent (the position is the index minus the base of the parent)
TEST_F(EventJournal_F, ChangeTracking_NameChanges_ReplayOntoTheSnapshot)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto const snapshot = snapshotDocument(entityID);
	ASSERT_FALSE(snapshot.empty());

	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
	controllerManager.setEntityName(entityID, "Renamed listener");
	controllerManager.setEntityGroupName(entityID, "Stage left");
	controllerManager.setConfigurationName(entityID, 0u, "Main configuration");
	controllerManager.setStreamInputName(entityID, 0u, 1u, "Second input");
	controllerManager.setStreamOutputName(entityID, 0u, 2u, "Third output");
	controllerManager.setAvbInterfaceName(entityID, 0u, 0u, "Primary port");
	controllerManager.setClockDomainName(entityID, 0u, 0u, "House clock");
	controllerManager.setClockSourceName(entityID, 0u, 1u, "Stream clock");
	controllerManager.setAudioUnitName(entityID, 0u, 0u, "DSP");
	controllerManager.setControlName(entityID, 0u, 0u, "Mute");
	controllerManager.setAudioClusterName(entityID, 0u, 9u, "Output cluster 2"); // Second cluster of the output stream port (base cluster 8)
	controllerManager.setMemoryObjectName(entityID, 0u, 1u, "Firmware");
	auto const events = waitForEvents(EventJournal::Category::Configuration, entityID, 12u);
	ASSERT_EQ(std::size_t{ 12u }, events.size());

	EXPECT_EQ(QString{ "Entity name changed to 'Renamed listener'" }, events[0].summary);
	EXPECT_EQ(EventJournal::Level::Diagnostic, events[0].level);
	EXPECT_EQ(QString{ "Renamed listener" }, events[0].entityName);
	EXPECT_EQ(std::string{ "Renamed listener" }, patchValue(events[0], "/entity_model/entity_descriptor/dynamic/entity_name").get<std::string>());
	EXPECT_EQ(std::string{ "Stage left" }, patchValue(events[1], "/entity_model/entity_descriptor/dynamic/group_name").get<std::string>());
	EXPECT_EQ(QString{ "Configuration 0" }, events[2].subject);
	EXPECT_EQ(std::string{ "Main configuration" }, patchValue(events[2], "/configuration_descriptors/0/dynamic/object_name").get<std::string>());
	EXPECT_EQ(QString{ "Input Stream 1" }, events[3].subject);
	EXPECT_EQ(QString{ "Input Stream name changed to 'Second input'" }, events[3].summary);
	EXPECT_EQ(std::string{ "Second input" }, patchValue(events[3], "/configuration_descriptors/0/stream_input_descriptors/1/dynamic/object_name").get<std::string>());
	EXPECT_EQ(std::string{ "Third output" }, patchValue(events[4], "/configuration_descriptors/0/stream_output_descriptors/2/dynamic/object_name").get<std::string>());
	EXPECT_EQ(std::string{ "Primary port" }, patchValue(events[5], "/configuration_descriptors/0/avb_interface_descriptors/0/dynamic/object_name").get<std::string>());
	EXPECT_EQ(std::string{ "House clock" }, patchValue(events[6], "/configuration_descriptors/0/clock_domain_descriptors/0/dynamic/object_name").get<std::string>());
	EXPECT_EQ(std::string{ "Stream clock" }, patchValue(events[7], "/configuration_descriptors/0/clock_source_descriptors/1/dynamic/object_name").get<std::string>());
	EXPECT_EQ(std::string{ "DSP" }, patchValue(events[8], "/configuration_descriptors/0/audio_unit_descriptors/0/dynamic/object_name").get<std::string>());
	EXPECT_EQ(std::string{ "Mute" }, patchValue(events[9], "/configuration_descriptors/0/control_descriptors/0/dynamic/object_name").get<std::string>());
	EXPECT_EQ(QString{ "Audio Cluster 9" }, events[10].subject);
	EXPECT_EQ(std::string{ "Output cluster 2" }, patchValue(events[10], "/configuration_descriptors/0/audio_unit_descriptors/0/stream_port_output_descriptors/0/audio_cluster_descriptors/1/dynamic/object_name").get<std::string>());
	EXPECT_EQ(std::string{ "Firmware" }, patchValue(events[11], "/configuration_descriptors/0/memory_object_descriptors/1/dynamic/object_name").get<std::string>());

	// The replay of the events onto the snapshot gives the entity as the controller sees it
	expectReplayMatches(entityID, snapshot, events);
}

// Configuration changes (stream format, sampling rate, clock source, stream running) and control actions (lock) are journaled with their patch
TEST_F(EventJournal_F, ChangeTracking_ConfigurationAndControlChanges_ReplayOntoTheSnapshot)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto const snapshot = snapshotDocument(entityID);
	ASSERT_FALSE(snapshot.empty());

	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
	auto newFormat = la::avdecc::entity::model::StreamFormat{};
	{
		auto const controlledEntity = controllerManager.getControlledEntity(entityID);
		ASSERT_TRUE(!!controlledEntity);
		auto const& streamNode = controlledEntity->getStreamInputNode(0u, 0u);
		ASSERT_GE(streamNode.staticModel.formats.size(), std::size_t{ 2u });
		for (auto const& format : streamNode.staticModel.formats)
		{
			if (format != streamNode.dynamicModel.streamFormat)
			{
				newFormat = format;
				break;
			}
		}
	}
	controllerManager.setStreamInputFormat(entityID, 0u, newFormat);
	controllerManager.setAudioUnitSamplingRate(entityID, 0u, la::avdecc::entity::model::SamplingRate{ 0u, 48000u });
	controllerManager.setClockSource(entityID, 0u, 1u);
	controllerManager.stopStreamInput(entityID, 2u);
	auto events = waitForEvents(EventJournal::Category::Configuration, entityID, 4u);
	ASSERT_EQ(std::size_t{ 4u }, events.size());

	EXPECT_EQ(QString{ "Stream format changed to %1" }.arg(hive::modelsLibrary::helper::toHexQString(newFormat.getValue(), true, true)), events[0].summary);
	EXPECT_EQ(EventJournal::Level::Diagnostic, events[0].level);
	EXPECT_TRUE(events[0].subject.startsWith("Input Stream 0"));
	EXPECT_EQ(hive::modelsLibrary::helper::toHexQString(newFormat.getValue(), true, true).toStdString(), patchValue(events[0], "/stream_input_descriptors/0/dynamic/stream_format").get<std::string>());
	EXPECT_EQ(QString{ "Sampling rate changed to 48000 Hz" }, events[1].summary);
	EXPECT_EQ(QString{ "Audio Unit 0" }, events[1].subject);
	EXPECT_EQ(std::string{ "0x0000BB80" }, patchValue(events[1], "/audio_unit_descriptors/0/dynamic/current_sampling_rate").get<std::string>());
	EXPECT_TRUE(events[2].summary.startsWith("Clock source changed to Clock Source 1")) << events[2].summary.toStdString();
	EXPECT_EQ(QString{ "Clock Domain 0" }, events[2].subject);
	EXPECT_EQ(1, patchValue(events[2], "/clock_domain_descriptors/0/dynamic/clock_source_index").get<int>());
	EXPECT_EQ(QString{ "Stream stopped" }, events[3].summary);
	EXPECT_TRUE(events[3].subject.startsWith("Input Stream 2"));
	EXPECT_FALSE(patchValue(events[3], "/stream_input_descriptors/2/dynamic/stream_running").get<bool>());

	// Lock and unlock (the virtual controller answers with the controller EID)
	controllerManager.lockEntity(entityID);
	auto controlEvents = waitForEvents(EventJournal::Category::Control, entityID, 1u);
	ASSERT_EQ(std::size_t{ 1u }, controlEvents.size());
	EXPECT_EQ(QString{ "Entity locked by this controller" }, controlEvents[0].summary);
	EXPECT_EQ(EventJournal::Level::Diagnostic, controlEvents[0].level);
	EXPECT_EQ(std::string{ "LOCKED" }, patchValue(controlEvents[0], "/state/lock_state").get<std::string>());
	EXPECT_EQ(hive::modelsLibrary::helper::uniqueIdentifierToString(controllerManager.getControllerEID()).toStdString(), patchValue(controlEvents[0], "/state/locking_controller_id").get<std::string>());
	auto const lockedDocument = applyPatches(snapshot, controlEvents);
	EXPECT_EQ(nlohmann::json::diff(lockedDocument.at("state"), currentDocument(entityID).at("state")).dump(), std::string{ "[]" });
	controllerManager.unlockEntity(entityID);
	controlEvents = waitForEvents(EventJournal::Category::Control, entityID, 2u);
	ASSERT_EQ(std::size_t{ 2u }, controlEvents.size());
	EXPECT_EQ(QString{ "Entity unlocked" }, controlEvents[1].summary);
	EXPECT_EQ(std::string{ "NOT_LOCKED" }, patchValue(controlEvents[1], "/state/lock_state").get<std::string>());

	// The replay of every event onto the snapshot gives the entity as the controller sees it
	events.insert(events.end(), controlEvents.begin(), controlEvents.end());
	expectReplayMatches(entityID, snapshot, events);
}

// Control value changes are rate limited per control: the first change is journaled at once, the following ones within the interval are coalesced into one event carrying the last value
TEST_F(EventJournal_F, ChangeTracking_ControlValues_AreRateLimited)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	using LinearValues = la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<std::uint8_t>>;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto const snapshot = snapshotDocument(entityID);
	ASSERT_FALSE(snapshot.empty());

	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
	auto const notifyValue = [&controllerManager, entityID](std::uint8_t const value)
	{
		emit controllerManager.controlValuesChanged(entityID, 0u, la::avdecc::entity::model::ControlValues{ LinearValues{ { la::avdecc::entity::model::LinearValueDynamic<std::uint8_t>{ value } } } });
	};

	// First change: journaled at once, with the patch of the values serialized like the AVE document
	notifyValue(10u);
	auto events = eventsSince(EventJournal::Category::Configuration, entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Control value changed" }, events[0].summary);
	EXPECT_EQ(QString{ "Control 0" }, events[0].subject);
	EXPECT_EQ(EventJournal::Level::Diagnostic, events[0].level);
	auto const values = patchValue(events[0], "/configuration_descriptors/0/control_descriptors/0/dynamic/values");
	EXPECT_EQ(std::string{ "CONTROL_LINEAR_UINT8" }, values.value("type", std::string{}));
	EXPECT_EQ(nlohmann::json::array({ 10 }), values.value("values", nlohmann::json{}));
	auto const replayed = applyPatches(snapshot, events);
	EXPECT_EQ(values, replayed.at(nlohmann::json::json_pointer{ "/entity_model/entity_descriptor/configuration_descriptors/0/control_descriptors/0/dynamic/values" }));

	// Changes within the interval are coalesced (the same value again is not a change)
	advanceClock(std::chrono::milliseconds{ 100 });
	notifyValue(11u);
	notifyValue(12u);
	notifyValue(13u);
	notifyValue(13u);
	EXPECT_EQ(std::size_t{ 1u }, eventsSince(EventJournal::Category::Configuration, entityID).size());

	// The flush timer journals the last value once the interval elapsed
	advanceClock(std::chrono::milliseconds{ 1000 });
	auto const deadline = QDeadlineTimer{ 3000 };
	while (!deadline.hasExpired() && eventsSince(EventJournal::Category::Configuration, entityID).size() < 2u)
	{
		QTest::qWait(50);
	}
	events = eventsSince(EventJournal::Category::Configuration, entityID);
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_EQ(QString{ "Control value changed (3 changes coalesced)" }, events[1].summary);
	EXPECT_EQ(nlohmann::json::array({ 13 }), patchValue(events[1], "/control_descriptors/0/dynamic/values").value("values", nlohmann::json{}));
	EXPECT_EQ(3, QJsonDocument::fromJson(events[1].details.toUtf8()).object().value("coalesced").toInt());

	// A change after the interval is journaled at once again, the same value is not
	advanceClock(std::chrono::milliseconds{ 1500 });
	notifyValue(13u);
	notifyValue(14u);
	events = eventsSince(EventJournal::Category::Configuration, entityID);
	ASSERT_EQ(std::size_t{ 3u }, events.size());
	EXPECT_EQ(QString{ "Control value changed" }, events[2].summary);
	EXPECT_EQ(nlohmann::json::array({ 14 }), patchValue(events[2], "/control_descriptors/0/dynamic/values").value("values", nlohmann::json{}));
}

// Raw counters are journaled at diagnostic level with the changed values and a patch, but not when only the liveness counters moved, nor for a duplicate notification
TEST_F(EventJournal_F, ChangeTracking_RawCounters_SkipLivenessAndDuplicates)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto const snapshot = snapshotDocument(entityID);
	ASSERT_FALSE(snapshot.empty());

	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
	auto counters = la::avdecc::entity::model::StreamInputCounters{};
	{
		auto const controlledEntity = controllerManager.getControlledEntity(entityID);
		ASSERT_TRUE(!!controlledEntity);
		auto const& modelCounters = controlledEntity->getStreamInputNode(0u, TestStreamIndex).dynamicModel.counters;
		ASSERT_TRUE(!!modelCounters);
		counters = *modelCounters;
	}
	auto const countersEvents = [this, entityID]()
	{
		auto events = eventsSince(EventJournal::Category::Counters, entityID);
		events.erase(std::remove_if(events.begin(), events.end(),
									 [](EventJournal::Event const& event)
									 {
										 return event.summary != "Counters updated";
									 }),
			events.end());
		return events;
	};

	// Same values as the baseline: nothing
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	EXPECT_TRUE(countersEvents().empty());

	// Only FRAMES_RX moved: nothing (the increments feed the stream data loss rule)
	counters[la::avdecc::entity::StreamInputCounterValidFlag::FramesRx] += FramesPerSecond;
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	EXPECT_TRUE(countersEvents().empty());

	// An informative counter moved: journaled with the changed values (FRAMES_RX included, it moved since the last journaled set) and the patch of the whole set
	auto const previousMediaUnlocked = counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked];
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked] = previousMediaUnlocked + 1u;
	counters[la::avdecc::entity::StreamInputCounterValidFlag::FramesRx] += FramesPerSecond;
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	auto events = countersEvents();
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(EventJournal::Level::Diagnostic, events[0].level);
	EXPECT_TRUE(events[0].subject.startsWith("Input Stream 0"));
	auto const details = nlohmann::json::parse(events[0].details.toStdString());
	auto changedNames = std::vector<std::string>{};
	for (auto const& changed : details.at("changed"))
	{
		changedNames.push_back(changed.value("counter", std::string{}));
		if (changedNames.back() == "MEDIA_UNLOCKED")
		{
			EXPECT_EQ(previousMediaUnlocked, changed.value("previous_value", 0u));
			EXPECT_EQ(previousMediaUnlocked + 1u, changed.value("value", 0u));
		}
	}
	EXPECT_EQ((std::vector<std::string>{ "FRAMES_RX", "MEDIA_UNLOCKED" }), changedNames);
	auto const patched = applyPatches(snapshot, events);
	EXPECT_EQ(previousMediaUnlocked + 1u, patched.at(nlohmann::json::json_pointer{ "/entity_model/entity_descriptor/configuration_descriptors/0/stream_input_descriptors/0/dynamic/counters/MEDIA_UNLOCKED" }).get<std::uint32_t>());
	EXPECT_EQ(counters[la::avdecc::entity::StreamInputCounterValidFlag::FramesRx], patched.at(nlohmann::json::json_pointer{ "/entity_model/entity_descriptor/configuration_descriptors/0/stream_input_descriptors/0/dynamic/counters/FRAMES_RX" }).get<std::uint32_t>());

	// The same notification again (the other interface of a redundant controller): nothing
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	EXPECT_EQ(std::size_t{ 1u }, countersEvents().size());

	// Not journaled at user level
	EventJournal::getInstance().setRecordingLevel(EventJournal::Level::User);
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked] += 1u;
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	EXPECT_EQ(std::size_t{ 1u }, countersEvents().size());
	EventJournal::getInstance().setRecordingLevel(EventJournal::Level::Diagnostic);

	// Clock domain and AVB interface counters go through the same rule
	auto clockDomainCounters = la::avdecc::entity::model::ClockDomainCounters{};
	clockDomainCounters[la::avdecc::entity::ClockDomainCounterValidFlag::Locked] = 100u;
	clockDomainCounters[la::avdecc::entity::ClockDomainCounterValidFlag::Unlocked] = 99u;
	emit controllerManager.clockDomainCountersChanged(entityID, 0u, clockDomainCounters);
	events = countersEvents();
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_EQ(QString{ "Clock Domain 0" }, events[1].subject);
	EXPECT_EQ(100u, patchValue(events[1], "/clock_domain_descriptors/0/dynamic/counters").value("LOCKED", 0u));
}

// The connection, gPTP and talker connections events carry their patch, and a talker connections notification is only journaled when the listeners changed
TEST_F(EventJournal_F, ChangeTracking_ConnectionAndGptpEvents_CarryTheirPatch)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto const talkerID = la::avdecc::UniqueIdentifier{ TalkerEntityID };
	loadConnectedFixture();
	auto const snapshot = snapshotDocument(entityID);
	ASSERT_FALSE(snapshot.empty());
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	// Listener side: the disconnection patches the connected talker and the connection state
	auto info = la::avdecc::entity::model::StreamInputConnectionInfo{};
	info.state = la::avdecc::entity::model::StreamInputConnectionInfo::State::NotConnected;
	emit controllerManager.streamInputConnectionChanged({ entityID, TestStreamIndex }, info);
	auto events = eventsSince(EventJournal::Category::Connection, entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_TRUE(events[0].summary.startsWith("Disconnected from talker"));
	EXPECT_EQ(EventJournal::Level::User, events[0].level);
	EXPECT_EQ(std::string{ "NOT_CONNECTED" }, patchValue(events[0], "/stream_input_descriptors/0/dynamic/connection_state").get<std::string>());
	EXPECT_EQ(std::string{ "0xFFFFFFFFFFFFFFFF" }, patchValue(events[0], "/stream_input_descriptors/0/dynamic/connected_talker").value("entity_id", std::string{})); // Null identifier
	auto patched = applyPatches(snapshot, events);
	EXPECT_EQ(std::string{ "NOT_CONNECTED" }, patched.at(nlohmann::json::json_pointer{ "/entity_model/entity_descriptor/configuration_descriptors/0/stream_input_descriptors/0/dynamic/connection_state" }).get<std::string>());

	// gPTP: grandmaster and domain
	auto const newGrandmasterID = la::avdecc::UniqueIdentifier{ 0x001122FFFE00AAAAu };
	emit controllerManager.gptpChanged(entityID, 0u, newGrandmasterID, 3u);
	auto gptpEvents = eventsSince(EventJournal::Category::Gptp, entityID);
	ASSERT_EQ(std::size_t{ 1u }, gptpEvents.size());
	EXPECT_EQ(std::string{ "0x001122FFFE00AAAA" }, patchValue(gptpEvents[0], "/avb_interface_descriptors/0/dynamic/gptp_grandmaster_id").get<std::string>());
	EXPECT_EQ(3, patchValue(gptpEvents[0], "/avb_interface_descriptors/0/dynamic/gptp_domain_number").get<int>());
	patched = applyPatches(patched, gptpEvents);
	EXPECT_EQ(std::string{ "0x001122FFFE00AAAA" }, patched.at(nlohmann::json::json_pointer{ "/entity_model/entity_descriptor/configuration_descriptors/0/avb_interface_descriptors/0/dynamic/gptp_grandmaster_id" }).get<std::string>());

	// Talker side: the listeners list is journaled at diagnostic level without a patch (not part of the AVE document), the same list again (the fixture connections, then a duplicate notification) is dropped
	auto connections = la::avdecc::entity::model::StreamConnections{};
	connections.insert({ entityID, TestStreamIndex });
	emit controllerManager.streamOutputConnectionsChanged({ talkerID, TestStreamIndex }, connections);
	EXPECT_TRUE(eventsSince(EventJournal::Category::Connection, talkerID).empty());
	connections.insert({ entityID, la::avdecc::entity::model::StreamIndex{ 2u } });
	emit controllerManager.streamOutputConnectionsChanged({ talkerID, TestStreamIndex }, connections);
	emit controllerManager.streamOutputConnectionsChanged({ talkerID, TestStreamIndex }, connections);
	auto talkerEvents = eventsSince(EventJournal::Category::Connection, talkerID);
	ASSERT_EQ(std::size_t{ 1u }, talkerEvents.size());
	EXPECT_EQ(QString{ "Talker connections changed (2 listeners)" }, talkerEvents[0].summary);
	EXPECT_EQ(EventJournal::Level::Diagnostic, talkerEvents[0].level);
	EXPECT_TRUE(talkerEvents[0].subject.startsWith("Output Stream 0"));
	EXPECT_TRUE(eventPatch(talkerEvents[0]).empty());
	auto const listeners = QJsonDocument::fromJson(talkerEvents[0].details.toUtf8()).object().value("listeners").toArray();
	ASSERT_EQ(2, listeners.size());
	EXPECT_EQ(hive::modelsLibrary::helper::uniqueIdentifierToString(entityID), listeners[0].toObject().value("entity_id").toString());
}
