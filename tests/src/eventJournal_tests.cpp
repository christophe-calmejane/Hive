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
#include <hive/modelsLibrary/journalReplay.hpp>

#include <la/avdecc/executor.hpp>
#include <la/avdecc/internals/entityModelControlValues.hpp>
#include <la/avdecc/internals/entityModelControlValuesTraits.hpp>
#include <la/avdecc/internals/protocolAemAecpdu.hpp>
#include <la/avdecc/internals/protocolAemPayloadSizes.hpp>
#include <la/avdecc/internals/protocolInterface.hpp>
#include <la/avdecc/internals/serialization.hpp>

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
#include <map>
#include <memory>
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

	/** Gets the events of the given category and summary journaled since the fixture was loaded for the given entity, in journal order. */
	std::vector<hive::modelsLibrary::EventJournal::Event> eventsSince(hive::modelsLibrary::EventJournal::Category const category, la::avdecc::UniqueIdentifier const entityID, QString const& summary) const noexcept
	{
		auto events = eventsSince(category, entityID);
		events.erase(std::remove_if(events.begin(), events.end(),
									 [&summary](hive::modelsLibrary::EventJournal::Event const& event)
									 {
										 return event.summary != summary;
									 }),
			events.end());
		return events;
	}

	/** Gets the details of an event as a JSON object. */
	static QJsonObject eventDetails(hive::modelsLibrary::EventJournal::Event const& event) noexcept
	{
		return QJsonDocument::fromJson(event.details.toUtf8()).object();
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

	/** Expects an AVE document to match the entity as the controller sees it now (entity model, state and diagnostics; the statistics change with every command and are not compared). */
	static void expectDocumentMatchesEntity(nlohmann::json const& document, la::avdecc::UniqueIdentifier const entityID) noexcept
	{
		auto const current = currentDocument(entityID);
		for (auto const* const key : { "entity_model", "state", "diagnostics", "compatibility_flags" })
		{
			auto const diff = nlohmann::json::diff(document.value(key, nlohmann::json{}), current.value(key, nlohmann::json{}));
			EXPECT_TRUE(diff.empty()) << "The replayed '" << key << "' differs from the current entity: " << diff.dump();
		}
	}

	/** Expects the AVE document rebuilt from the snapshot and the given events to match the entity as the controller sees it now. */
	static void expectReplayMatches(la::avdecc::UniqueIdentifier const entityID, nlohmann::json const& snapshot, std::vector<hive::modelsLibrary::EventJournal::Event> const& events) noexcept
	{
		expectDocumentMatchesEntity(applyPatches(snapshot, events), entityID);
	}

	/** Exports the current session to the given file and builds a replay of it (the snapshots are read from the file on demand). */
	static std::unique_ptr<hive::modelsLibrary::JournalReplay> exportAndReplay(QString const& filePath) noexcept
	{
		using EventJournal = hive::modelsLibrary::EventJournal;
		QFile::remove(filePath);
		if (!EventJournal::getInstance().exportCurrentSession(filePath))
		{
			ADD_FAILURE() << "Cannot export the session";
			return {};
		}
		auto session = EventJournal::loadSession(filePath);
		if (!session)
		{
			ADD_FAILURE() << "Cannot load the exported session";
			return {};
		}
		return std::make_unique<hive::modelsLibrary::JournalReplay>(std::move(session->events), EventJournal::loadSnapshotsIndex(filePath),
			[filePath](qint64 const snapshotID)
			{
				return EventJournal::loadSnapshotData(filePath, snapshotID);
			});
	}

	/** Finds the index of the last event of the given entity whose summary starts with the given text (std::nullopt if none). */
	static std::optional<std::size_t> lastEventIndex(std::vector<hive::modelsLibrary::EventJournal::Event> const& events, la::avdecc::UniqueIdentifier const entityID, QString const& summaryPrefix) noexcept
	{
		auto found = std::optional<std::size_t>{};
		for (auto index = std::size_t{ 0u }; index < events.size(); ++index)
		{
			if (events[index].entityID == entityID && events[index].summary.startsWith(summaryPrefix))
			{
				found = index;
			}
		}
		return found;
	}

	/** Unloads the given virtual entity and waits until the journal has journaled it offline. */
	void unloadEntity(la::avdecc::UniqueIdentifier const entityID)
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const offlineCountBefore = eventsSince(hive::modelsLibrary::EventJournal::Category::Entity, entityID, "Entity went offline").size();
		ASSERT_TRUE(controllerManager.unloadVirtualEntity(entityID));
		auto const deadline = QDeadlineTimer{ 5000 };
		while (!deadline.hasExpired() && eventsSince(hive::modelsLibrary::EventJournal::Category::Entity, entityID, "Entity went offline").size() == offlineCountBefore)
		{
			QTest::qWait(10);
		}
		ASSERT_EQ(offlineCountBefore + 1u, eventsSince(hive::modelsLibrary::EventJournal::Category::Entity, entityID, "Entity went offline").size()) << "The entity was not journaled offline";
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
	// The export is journaled in the live session once the copy is taken, so the exported file holds every event but that one
	auto const liveEvents = journal.currentSessionEvents();
	ASSERT_EQ(liveEvents.size(), session->events.size() + 1u);
	EXPECT_EQ(QString{ "Journal exported" }, liveEvents.back().summary);
	EXPECT_EQ(EventJournal::Category::Session, liveEvents.back().category);
	EXPECT_EQ(QString{ "HiveEventJournalTests_schema_v2.hej" }, eventDetails(liveEvents.back()).value("file_name").toString());
	for (auto index = std::size_t{ 0u }; index < session->events.size(); ++index)
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
	ASSERT_TRUE(details.contains("is_controller"));
	EXPECT_FALSE(details.value("is_controller").toBool(true));

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
		// The plane of the stream: the fixture entity has a single AVB interface
		EXPECT_EQ(0, stream.value("avb_interface_index").toInt(-1));
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

	// Propagation delay: the same information and the jitter of the measurement (a few nanoseconds, on every link, all the time) are silent
	auto info = avbInterfaceInfo(entityID, 0u);
	ASSERT_EQ(20u, info.propagationDelay);
	emit controllerManager.avbInterfaceInfoChanged(entityID, 0u, info);
	EXPECT_EQ(std::size_t{ 1u }, eventsSince(EventJournal::Category::Gptp, entityID).size());
	info.propagationDelay = 25u;
	emit controllerManager.avbInterfaceInfoChanged(entityID, 0u, info);
	EXPECT_EQ(std::size_t{ 1u }, eventsSince(EventJournal::Category::Gptp, entityID).size());

	// A delay dropping to zero is the measurement anomaly the Network Graph flags: a user level warning, closed when the delay is measured again (the patch carries the whole information)
	info.propagationDelay = 0u;
	emit controllerManager.avbInterfaceInfoChanged(entityID, 0u, info);
	events = eventsSince(EventJournal::Category::Gptp, entityID);
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_EQ(QString{ "gPTP propagation delay became null (was 25 ns)" }, events[1].summary);
	EXPECT_EQ(EventJournal::Severity::Warning, events[1].severity);
	EXPECT_EQ(EventJournal::Level::User, events[1].level);
	details = QJsonDocument::fromJson(events[1].details.toUtf8()).object();
	EXPECT_EQ(25, details.value("previous_propagation_delay").toInt());
	EXPECT_EQ(0, details.value("new_propagation_delay").toInt());
	EXPECT_EQ(0, patchValue(events[1], "/avb_interface_descriptors/0/dynamic/avb_interface_info").value("propagation_delay", 1));
	info.propagationDelay = 30u;
	emit controllerManager.avbInterfaceInfoChanged(entityID, 0u, info);
	events = eventsSince(EventJournal::Category::Gptp, entityID);
	ASSERT_EQ(std::size_t{ 3u }, events.size());
	EXPECT_EQ(QString{ "gPTP propagation delay measured (30 ns)" }, events[2].summary);
	EXPECT_EQ(EventJournal::Severity::Recovered, events[2].severity);
	EXPECT_EQ(EventJournal::Level::User, events[2].level);
	EXPECT_EQ(30, patchValue(events[2], "/avb_interface_descriptors/0/dynamic/avb_interface_info").value("propagation_delay", 0));

	// Another field of the information (the flags) changed without the delay: a generic diagnostic event
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
	ASSERT_EQ(std::size_t{ 4u }, events.size());
	EXPECT_EQ(QString{ "AVB interface info changed" }, events[3].summary);
	EXPECT_EQ(EventJournal::Level::Diagnostic, events[3].level);

	// An entity enumerated with a null delay (the second entity of the fixture) that gets measured: informative, not a recovery (nothing was journaled before)
	auto const otherEntityID = la::avdecc::UniqueIdentifier{ TopologyEntityA + 1u };
	auto otherInfo = avbInterfaceInfo(otherEntityID, 0u);
	ASSERT_EQ(0u, otherInfo.propagationDelay);
	otherInfo.propagationDelay = 12u;
	emit controllerManager.avbInterfaceInfoChanged(otherEntityID, 0u, otherInfo);
	auto const measuredEvents = eventsSince(EventJournal::Category::Gptp, otherEntityID);
	ASSERT_EQ(std::size_t{ 1u }, measuredEvents.size());
	EXPECT_EQ(QString{ "gPTP propagation delay measured (12 ns)" }, measuredEvents[0].summary);
	EXPECT_EQ(EventJournal::Severity::Info, measuredEvents[0].severity);
	EXPECT_EQ(EventJournal::Level::User, measuredEvents[0].level);
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
	EXPECT_EQ(EventJournal::Level::User, events[0].level);
	for (auto index = std::size_t{ 1u }; index < events.size(); ++index)
	{
		EXPECT_EQ(EventJournal::Level::Diagnostic, events[index].level) << events[index].summary.toStdString();
	}
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
	for (auto const& event : events)
	{
		EXPECT_EQ(EventJournal::Level::User, event.level) << event.summary.toStdString();
	}
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
	EXPECT_EQ(EventJournal::Level::User, controlEvents[0].level);
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

	// Reconnecting journals the talker and the plane of the stream (the streams connected during the session are mapped like the ones listed by the 'Entity is online' details)
	info.state = la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected;
	info.talkerStream = { talkerID, TestStreamIndex };
	emit controllerManager.streamInputConnectionChanged({ entityID, TestStreamIndex }, info);
	events = eventsSince(EventJournal::Category::Connection, entityID);
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_TRUE(events[1].summary.startsWith("Connected to talker"));
	auto const connectedDetails = QJsonDocument::fromJson(events[1].details.toUtf8()).object();
	EXPECT_EQ(QString{ "0x001B92FFFE02233B" }, connectedDetails.value("talker_id").toString());
	EXPECT_EQ(0, connectedDetails.value("avb_interface_index").toInt(-1));

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

/* *********************************
   Counters: per stream media clock lock, link, gPTP grandmaster changes, informational counters
*/
// The MEDIA_LOCKED / MEDIA_UNLOCKED counters of a connected stream drive 'Media unlocked' errors closed by 'Media locked' recoveries, with the transitions inferred from the increments like the clock domains
TEST_F(EventJournal_F, StreamLock_UnlockAndRelock_AreJournaledPerStream)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	// The fixture stream is connected and locked (MEDIA_LOCKED 1, MEDIA_UNLOCKED 0)
	auto counters = la::avdecc::entity::model::StreamInputCounters{};
	{
		auto const controlledEntity = controllerManager.getControlledEntity(entityID);
		ASSERT_TRUE(!!controlledEntity);
		auto const& modelCounters = controlledEntity->getStreamInputNode(0u, TestStreamIndex).dynamicModel.counters;
		ASSERT_TRUE(!!modelCounters);
		counters = *modelCounters;
	}
	ASSERT_EQ(1u, counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked]);
	ASSERT_EQ(0u, counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked]);
	auto const lockEvents = [this, entityID]()
	{
		auto events = eventsSince(EventJournal::Category::Counters, entityID);
		events.erase(std::remove_if(events.begin(), events.end(),
									 [](EventJournal::Event const& event)
									 {
										 return event.summary != "Media unlocked" && event.summary != "Media locked";
									 }),
			events.end());
		return events;
	};

	// Same values as the baseline: nothing
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	EXPECT_TRUE(lockEvents().empty());

	// Observed unlock: an error carrying the raw values and the talker
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked] = 1u;
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	auto events = lockEvents();
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Media unlocked" }, events[0].summary);
	EXPECT_EQ(EventJournal::Severity::Error, events[0].severity);
	EXPECT_EQ(EventJournal::Level::User, events[0].level);
	EXPECT_TRUE(events[0].subject.startsWith("Input Stream 0"));
	auto details = eventDetails(events[0]);
	EXPECT_EQ(1, details.value("media_locked_counter").toInt());
	EXPECT_EQ(1, details.value("media_unlocked_counter").toInt());
	EXPECT_EQ(1, details.value("media_unlocked_increment").toInt());
	EXPECT_FALSE(details.contains("inferred"));
	EXPECT_EQ(hive::modelsLibrary::helper::uniqueIdentifierToString(la::avdecc::UniqueIdentifier{ TalkerEntityID }), details.value("talker_id").toString());

	// The same notification again (the other interface of a redundant controller): nothing
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	EXPECT_EQ(std::size_t{ 1u }, lockEvents().size());

	// Observed relock: the recovery closing the error
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked] = 2u;
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	events = lockEvents();
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_EQ(QString{ "Media locked" }, events[1].summary);
	EXPECT_EQ(EventJournal::Severity::Recovered, events[1].severity);
	EXPECT_EQ(EventJournal::Level::User, events[1].level);
	EXPECT_FALSE(eventDetails(events[1]).contains("inferred"));

	// Unlock and relock between two notifications: both transitions are journaled, flagged as inferred
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked] = 3u;
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked] = 2u;
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	events = lockEvents();
	ASSERT_EQ(std::size_t{ 4u }, events.size());
	EXPECT_EQ(QString{ "Media unlocked" }, events[2].summary);
	EXPECT_EQ(EventJournal::Severity::Error, events[2].severity);
	EXPECT_TRUE(eventDetails(events[2]).value("inferred").toBool());
	EXPECT_EQ(QString{ "Media locked" }, events[3].summary);
	EXPECT_EQ(EventJournal::Severity::Recovered, events[3].severity);
	details = eventDetails(events[3]);
	EXPECT_TRUE(details.value("inferred").toBool());
	EXPECT_EQ(3, details.value("media_locked_counter").toInt());
	EXPECT_EQ(2, details.value("media_unlocked_counter").toInt());
	EXPECT_EQ(1, details.value("media_locked_increment").toInt());
	EXPECT_EQ(1, details.value("media_unlocked_increment").toInt());

	// The error counters of the UI no longer journal MEDIA_UNLOCKED for that stream (the other error counters are unchanged)
	auto errorCounters = hive::modelsLibrary::ControllerManager::StreamInputErrorCounters{};
	errorCounters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked] = 2u;
	errorCounters[la::avdecc::entity::StreamInputCounterValidFlag::StreamInterrupted] = 1u;
	emit controllerManager.streamInputErrorCounterChanged(entityID, TestStreamIndex, errorCounters);
	auto const errorCounterEvents = eventsSince(EventJournal::Category::Counters, entityID, "'Stream Interrupted' error counter increased to 1");
	EXPECT_EQ(std::size_t{ 1u }, errorCounterEvents.size());
	EXPECT_TRUE(eventsSince(EventJournal::Category::Counters, entityID, "'Media Unlocked' error counter increased to 2").empty());
}

// A stream that is not connected unlocks silently, and a lock closing no journaled unlock is diagnostic information rather than a recovery
TEST_F(EventJournal_F, StreamLock_NotConnectedStream_UnlockIsSilent_LockIsInformation)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ TalkerEntityID }; // Its stream inputs are not connected
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	// The fixture stream is not connected and never locked (MEDIA_LOCKED 0, MEDIA_UNLOCKED 0)
	auto counters = la::avdecc::entity::model::StreamInputCounters{};
	{
		auto const controlledEntity = controllerManager.getControlledEntity(entityID);
		ASSERT_TRUE(!!controlledEntity);
		auto const& modelCounters = controlledEntity->getStreamInputNode(0u, TestStreamIndex).dynamicModel.counters;
		ASSERT_TRUE(!!modelCounters);
		counters = *modelCounters;
	}
	ASSERT_EQ(0u, counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked]);
	ASSERT_EQ(0u, counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked]);

	// Lock: information (diagnostic level), no talker
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked] = 1u;
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	auto events = eventsSince(EventJournal::Category::Counters, entityID, "Media locked");
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(EventJournal::Severity::Info, events[0].severity);
	EXPECT_EQ(EventJournal::Level::Diagnostic, events[0].level);
	EXPECT_FALSE(eventDetails(events[0]).contains("talker_id"));

	// Unlock: not an error for a stream that is not connected
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked] = 1u;
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	EXPECT_TRUE(eventsSince(EventJournal::Category::Counters, entityID, "Media unlocked").empty());

	// The same at user level: nothing at all
	EventJournal::getInstance().setRecordingLevel(EventJournal::Level::User);
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked] = 2u;
	counters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked] = 2u;
	emit controllerManager.streamInputCountersChanged(entityID, TestStreamIndex, counters);
	EventJournal::getInstance().setRecordingLevel(EventJournal::Level::Diagnostic);
	EXPECT_EQ(std::size_t{ 1u }, eventsSince(EventJournal::Category::Counters, entityID, "Media locked").size());
	EXPECT_TRUE(eventsSince(EventJournal::Category::Counters, entityID, "Media unlocked").empty());
}

// The LINK_UP / LINK_DOWN counters drive the link events, so a link that went down and came back between two notifications is journaled as an inferred pair
TEST_F(EventJournal_F, Link_CountersDriveTheLinkEvents_WithInferredFlaps)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	// The fixture interface is up (LINK_UP 1, LINK_DOWN 0)
	auto counters = la::avdecc::entity::model::AvbInterfaceCounters{};
	{
		auto const controlledEntity = controllerManager.getControlledEntity(entityID);
		ASSERT_TRUE(!!controlledEntity);
		auto const& modelCounters = controlledEntity->getAvbInterfaceNode(0u, 0u).dynamicModel.counters;
		ASSERT_TRUE(!!modelCounters);
		counters = *modelCounters;
	}
	ASSERT_EQ(1u, counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkUp]);
	ASSERT_EQ(0u, counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkDown]);

	// Same values as the baseline: nothing
	emit controllerManager.avbInterfaceCountersChanged(entityID, 0u, counters);
	EXPECT_TRUE(eventsSince(EventJournal::Category::Link, entityID).empty());

	// Observed link down, then the duplicate notification of the other interface
	counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkDown] = 1u;
	emit controllerManager.avbInterfaceCountersChanged(entityID, 0u, counters);
	emit controllerManager.avbInterfaceCountersChanged(entityID, 0u, counters);
	auto events = eventsSince(EventJournal::Category::Link, entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Link down" }, events[0].summary);
	EXPECT_EQ(EventJournal::Severity::Error, events[0].severity);
	EXPECT_EQ(EventJournal::Level::User, events[0].level);
	EXPECT_EQ(QString{ "AVB Interface 0" }, events[0].subject);
	auto details = eventDetails(events[0]);
	EXPECT_EQ(1, details.value("link_up_counter").toInt());
	EXPECT_EQ(1, details.value("link_down_counter").toInt());
	EXPECT_EQ(1, details.value("link_down_increment").toInt());
	EXPECT_FALSE(details.contains("inferred"));

	// Observed link up
	counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkUp] = 2u;
	emit controllerManager.avbInterfaceCountersChanged(entityID, 0u, counters);
	events = eventsSince(EventJournal::Category::Link, entityID);
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_EQ(QString{ "Link up" }, events[1].summary);
	EXPECT_EQ(EventJournal::Severity::Recovered, events[1].severity);
	EXPECT_FALSE(eventDetails(events[1]).contains("inferred"));

	// Down and up between two notifications: the flap is journaled as an inferred pair
	counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkUp] = 3u;
	counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkDown] = 2u;
	emit controllerManager.avbInterfaceCountersChanged(entityID, 0u, counters);
	events = eventsSince(EventJournal::Category::Link, entityID);
	ASSERT_EQ(std::size_t{ 4u }, events.size());
	EXPECT_EQ(QString{ "Link down" }, events[2].summary);
	EXPECT_TRUE(eventDetails(events[2]).value("inferred").toBool());
	EXPECT_EQ(QString{ "Link up" }, events[3].summary);
	details = eventDetails(events[3]);
	EXPECT_TRUE(details.value("inferred").toBool());
	EXPECT_EQ(3, details.value("link_up_counter").toInt());
	EXPECT_EQ(2, details.value("link_down_counter").toInt());
	EXPECT_EQ(1, details.value("link_up_increment").toInt());
	EXPECT_EQ(1, details.value("link_down_increment").toInt());

	// Counters reset into the same state: nothing
	counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkUp] = 1u;
	counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::LinkDown] = 0u;
	emit controllerManager.avbInterfaceCountersChanged(entityID, 0u, counters);
	EXPECT_EQ(std::size_t{ 4u }, eventsSince(EventJournal::Category::Link, entityID).size());
}

// Every increase of the GPTP_GM_CHANGED counter is journaled with the raw values and the grandmaster known before and after, so a grandmaster that changed and came back between two notifications is visible
TEST_F(EventJournal_F, Gptp_GmChangedCounter_IsJournaledWithTheGrandmasters)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	auto counters = la::avdecc::entity::model::AvbInterfaceCounters{};
	auto grandmasterID = la::avdecc::UniqueIdentifier{};
	{
		auto const controlledEntity = controllerManager.getControlledEntity(entityID);
		ASSERT_TRUE(!!controlledEntity);
		auto const& avbInterfaceNode = controlledEntity->getAvbInterfaceNode(0u, 0u);
		ASSERT_TRUE(!!avbInterfaceNode.dynamicModel.counters);
		counters = *avbInterfaceNode.dynamicModel.counters;
		grandmasterID = avbInterfaceNode.dynamicModel.gptpGrandmasterID;
	}
	auto const baseline = counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::GptpGmChanged];
	auto const gmChangedEvents = [this, entityID]()
	{
		auto events = eventsSince(EventJournal::Category::Gptp, entityID);
		events.erase(std::remove_if(events.begin(), events.end(),
									 [](EventJournal::Event const& event)
									 {
										 return !event.summary.startsWith("'GPTP GM Changed' counter increased");
									 }),
			events.end());
		return events;
	};

	// Two changes counted while the grandmaster is the same before and after: the flip and back is visible from the details
	counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::GptpGmChanged] = baseline + 2u;
	emit controllerManager.avbInterfaceCountersChanged(entityID, 0u, counters);
	emit controllerManager.avbInterfaceCountersChanged(entityID, 0u, counters); // Duplicate
	auto events = gmChangedEvents();
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "'GPTP GM Changed' counter increased to %1" }.arg(baseline + 2u), events[0].summary);
	EXPECT_EQ(EventJournal::Severity::Info, events[0].severity);
	EXPECT_EQ(EventJournal::Level::User, events[0].level);
	EXPECT_EQ(QString{ "AVB Interface 0" }, events[0].subject);
	auto details = eventDetails(events[0]);
	EXPECT_EQ(static_cast<int>(baseline), details.value("previous_value").toInt());
	EXPECT_EQ(static_cast<int>(baseline + 2u), details.value("value").toInt());
	EXPECT_EQ(2, details.value("increment").toInt());
	EXPECT_EQ(hive::modelsLibrary::helper::uniqueIdentifierToString(grandmasterID), details.value("previous_grandmaster_id").toString());
	EXPECT_EQ(hive::modelsLibrary::helper::uniqueIdentifierToString(grandmasterID), details.value("grandmaster_id").toString());

	// A change the controller observed: the event names the grandmasters before and after
	auto const newGrandmasterID = la::avdecc::UniqueIdentifier{ 0x001122FFFE00BBBBu };
	emit controllerManager.gptpChanged(entityID, 0u, newGrandmasterID, 0u);
	counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::GptpGmChanged] = baseline + 3u;
	emit controllerManager.avbInterfaceCountersChanged(entityID, 0u, counters);
	events = gmChangedEvents();
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	details = eventDetails(events[1]);
	EXPECT_EQ(1, details.value("increment").toInt());
	EXPECT_EQ(hive::modelsLibrary::helper::uniqueIdentifierToString(grandmasterID), details.value("previous_grandmaster_id").toString());
	EXPECT_EQ(hive::modelsLibrary::helper::uniqueIdentifierToString(newGrandmasterID), details.value("grandmaster_id").toString());

	// Counters reset: not an increase
	counters[la::avdecc::entity::AvbInterfaceCounterValidFlag::GptpGmChanged] = 0u;
	emit controllerManager.avbInterfaceCountersChanged(entityID, 0u, counters);
	EXPECT_EQ(std::size_t{ 2u }, gmChangedEvents().size());
}

// The informational counters of the streams get a readable diagnostic event per increase (never an error), next to the raw counters event
TEST_F(EventJournal_F, InformationalCounters_GetReadableDiagnosticEvents)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const talkerID = la::avdecc::UniqueIdentifier{ TalkerEntityID };
	auto const listenerID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	// Stream output 0 of the talker (Milan 1.2 counters of the fixture: STREAM_START 6, STREAM_STOP 5, FRAMES_TX 32988, the rest at 0)
	auto outputCounters = std::map<la::avdecc::entity::StreamOutputCounterValidFlagMilan12, la::avdecc::entity::model::DescriptorCounter>{};
	outputCounters[la::avdecc::entity::StreamOutputCounterValidFlagMilan12::StreamStart] = 6u;
	outputCounters[la::avdecc::entity::StreamOutputCounterValidFlagMilan12::StreamStop] = 5u;
	outputCounters[la::avdecc::entity::StreamOutputCounterValidFlagMilan12::MediaReset] = 0u;
	outputCounters[la::avdecc::entity::StreamOutputCounterValidFlagMilan12::TimestampUncertain] = 0u;
	outputCounters[la::avdecc::entity::StreamOutputCounterValidFlagMilan12::FramesTx] = 32988u;
	emit controllerManager.streamOutputCountersChanged(talkerID, TestStreamIndex, la::avdecc::entity::model::StreamOutputCounters{ outputCounters });
	EXPECT_TRUE(eventsSince(EventJournal::Category::Counters, talkerID).empty()) << "The fixture values should match the baseline";

	// The talker stopped and restarted its stream: one readable event per counter, after the raw counters event
	outputCounters[la::avdecc::entity::StreamOutputCounterValidFlagMilan12::StreamStart] = 7u;
	outputCounters[la::avdecc::entity::StreamOutputCounterValidFlagMilan12::StreamStop] = 6u;
	outputCounters[la::avdecc::entity::StreamOutputCounterValidFlagMilan12::FramesTx] = 40000u;
	emit controllerManager.streamOutputCountersChanged(talkerID, TestStreamIndex, la::avdecc::entity::model::StreamOutputCounters{ outputCounters });
	auto events = eventsSince(EventJournal::Category::Counters, talkerID);
	ASSERT_EQ(std::size_t{ 3u }, events.size());
	EXPECT_EQ(QString{ "Counters updated" }, events[0].summary);
	EXPECT_EQ(QString{ "'Stream Start' counter increased to 7" }, events[1].summary);
	EXPECT_EQ(QString{ "'Stream Stop' counter increased to 6" }, events[2].summary);
	for (auto const& event : events)
	{
		EXPECT_EQ(EventJournal::Severity::Info, event.severity);
		EXPECT_EQ(EventJournal::Level::Diagnostic, event.level);
		EXPECT_TRUE(event.subject.startsWith("Output Stream 0"));
	}
	auto const details = eventDetails(events[2]);
	EXPECT_EQ(QString{ "Stream Stop" }, details.value("counter").toString());
	EXPECT_EQ(5, details.value("previous_value").toInt());
	EXPECT_EQ(6, details.value("value").toInt());
	EXPECT_EQ(1, details.value("increment").toInt());

	// A counters reset is not an increase (the raw counters event is still written)
	outputCounters[la::avdecc::entity::StreamOutputCounterValidFlagMilan12::StreamStart] = 1u;
	outputCounters[la::avdecc::entity::StreamOutputCounterValidFlagMilan12::StreamStop] = 0u;
	emit controllerManager.streamOutputCountersChanged(talkerID, TestStreamIndex, la::avdecc::entity::model::StreamOutputCounters{ outputCounters });
	events = eventsSince(EventJournal::Category::Counters, talkerID);
	ASSERT_EQ(std::size_t{ 4u }, events.size());
	EXPECT_EQ(QString{ "Counters updated" }, events[3].summary);

	// Stream input: MEDIA_RESET and TIMESTAMP_UNCERTAIN
	auto inputCounters = la::avdecc::entity::model::StreamInputCounters{};
	{
		auto const controlledEntity = controllerManager.getControlledEntity(listenerID);
		ASSERT_TRUE(!!controlledEntity);
		auto const& modelCounters = controlledEntity->getStreamInputNode(0u, TestStreamIndex).dynamicModel.counters;
		ASSERT_TRUE(!!modelCounters);
		inputCounters = *modelCounters;
	}
	inputCounters[la::avdecc::entity::StreamInputCounterValidFlag::MediaReset] += 1u;
	inputCounters[la::avdecc::entity::StreamInputCounterValidFlag::TimestampUncertain] += 3u;
	emit controllerManager.streamInputCountersChanged(listenerID, TestStreamIndex, inputCounters);
	EXPECT_EQ(std::size_t{ 1u }, eventsSince(EventJournal::Category::Counters, listenerID, "'Media Reset' counter increased to 1").size());
	EXPECT_EQ(std::size_t{ 1u }, eventsSince(EventJournal::Category::Counters, listenerID, "'Timestamp Uncertain' counter increased to 3").size());
	EXPECT_EQ(std::size_t{ 1u }, eventsSince(EventJournal::Category::Counters, listenerID, "Counters updated").size());
	EXPECT_TRUE(eventsSince(EventJournal::Category::Counters, listenerID, "Media unlocked").empty());
}

// The media clock events name the clock source the domain follows, with the input stream and its talker when the source is a stream
TEST_F(EventJournal_F, MediaClock_EventsCarryTheClockSource)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	// The fixture domain follows clock source 3, an input stream clock source on stream input 2, connected to the talker
	auto counters = la::avdecc::entity::model::ClockDomainCounters{};
	auto expectedTalkerStreamIndex = int{ -1 };
	{
		auto const controlledEntity = controllerManager.getControlledEntity(entityID);
		ASSERT_TRUE(!!controlledEntity);
		auto const& configurationNode = controlledEntity->getCurrentConfigurationNode();
		auto const& clockDomainNode = configurationNode.clockDomains.at(TestClockDomainIndex);
		ASSERT_TRUE(!!clockDomainNode.dynamicModel.counters);
		counters = *clockDomainNode.dynamicModel.counters;
		ASSERT_EQ(3u, clockDomainNode.dynamicModel.clockSourceIndex);
		auto const& clockSourceStaticModel = configurationNode.clockSources.at(3u).staticModel;
		ASSERT_EQ(la::avdecc::entity::model::DescriptorType::StreamInput, clockSourceStaticModel.clockSourceLocationType);
		ASSERT_EQ(2u, clockSourceStaticModel.clockSourceLocationIndex);
		expectedTalkerStreamIndex = static_cast<int>(configurationNode.streamInputs.at(2u).dynamicModel.connectionInfo.talkerStream.streamIndex);
	}

	counters[la::avdecc::entity::ClockDomainCounterValidFlag::Unlocked] += 1u;
	emit controllerManager.clockDomainCountersChanged(entityID, TestClockDomainIndex, counters);
	auto const events = eventsSince(EventJournal::Category::MediaClock, entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Media clock unlocked" }, events[0].summary);
	auto const details = eventDetails(events[0]);
	EXPECT_EQ(3, details.value("clock_source_index").toInt());
	EXPECT_EQ(QString{ "INPUT_STREAM" }, details.value("clock_source_type").toString());
	EXPECT_EQ(2, details.value("clock_source_stream_index").toInt());
	EXPECT_EQ(hive::modelsLibrary::helper::uniqueIdentifierToString(la::avdecc::UniqueIdentifier{ TalkerEntityID }), details.value("clock_source_talker_id").toString());
	EXPECT_EQ(expectedTalkerStreamIndex, details.value("clock_source_talker_stream_index").toInt());
}

/* *********************************
   Control: operator changes, acknowledged counters, refresh and identify requests
*/
// The changes an operator makes during a show are journaled at user level (with their patch), the other model changes stay diagnostic
TEST_F(EventJournal_F, Control_OperatorChanges_AreJournaledAtUserLevel)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
	auto& journal = EventJournal::getInstance();

	// User level recording only
	journal.setRecordingLevel(EventJournal::Level::User);
	controllerManager.setEntityName(entityID, "Show name");
	controllerManager.setStreamInputName(entityID, 0u, 0u, "Diagnostic only"); // A descriptor name is diagnostic material
	controllerManager.setAudioUnitSamplingRate(entityID, 0u, la::avdecc::entity::model::SamplingRate{ 0u, 48000u });
	controllerManager.setClockSource(entityID, 0u, 1u);
	controllerManager.stopStreamInput(entityID, 2u);
	auto const events = waitForEvents(EventJournal::Category::Configuration, entityID, 4u);
	journal.setRecordingLevel(EventJournal::Level::Diagnostic);
	ASSERT_EQ(std::size_t{ 4u }, events.size());
	EXPECT_EQ(QString{ "Entity name changed to 'Show name'" }, events[0].summary);
	EXPECT_EQ(QString{ "Sampling rate changed to 48000 Hz" }, events[1].summary);
	EXPECT_TRUE(events[2].summary.startsWith("Clock source changed to Clock Source 1")) << events[2].summary.toStdString();
	EXPECT_EQ(QString{ "Stream stopped" }, events[3].summary);
	for (auto const& event : events)
	{
		EXPECT_EQ(EventJournal::Level::User, event.level) << event.summary.toStdString();
		EXPECT_FALSE(eventPatch(event).empty()) << event.summary.toStdString();
	}

	// Acquire and lock are user level too
	controllerManager.lockEntity(entityID);
	auto const controlEvents = waitForEvents(EventJournal::Category::Control, entityID, 1u);
	ASSERT_EQ(std::size_t{ 1u }, controlEvents.size());
	EXPECT_EQ(QString{ "Entity locked by this controller" }, controlEvents[0].summary);
	EXPECT_EQ(EventJournal::Level::User, controlEvents[0].level);
	controllerManager.unlockEntity(entityID);
	waitForEvents(EventJournal::Category::Control, entityID, 2u);
}

// The unsolicited notification loss counter only counts the detections: the event tells how many notifications were actually lost and which sequence IDs they had, so the journal can be matched with a network capture
TEST_F(EventJournal_F, Counters_UnsolicitedLoss_TellsWhichNotificationsWereLost)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	// Three AEM unsolicited notifications lost (sequence IDs 41, 42 and 43), detected when the one with sequence ID 44 arrived
	emit controllerManager.aemAecpUnsolicitedLossCounterChanged(entityID, 1u, la::avdecc::controller::InterfaceType::Primary, 1u, 41u, 44u);
	auto events = eventsSince(EventJournal::Category::Counters, entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "'AEM Unsolicited Response Losses' error counter increased to 1 (3 notifications lost, sequence IDs 41 to 43)" }, events[0].summary);
	EXPECT_EQ(EventJournal::Severity::Error, events[0].severity);
	EXPECT_EQ(EventJournal::Level::User, events[0].level);
	EXPECT_EQ(QString{ "Statistics" }, events[0].subject);
	auto details = eventDetails(events[0]);
	EXPECT_EQ(QString{ "AEM Unsolicited Response Losses" }, details.value("counter").toString());
	EXPECT_EQ(1, details.value("value").toInt());
	EXPECT_EQ(3, details.value("lost_count").toInt());
	EXPECT_EQ(41, details.value("expected_sequence_id").toInt());
	EXPECT_EQ(44, details.value("received_sequence_id").toInt());
	EXPECT_FALSE(details.contains("interface")); // Single interface mode: the interface is only told in redundant mode

	// One MVU unsolicited notification lost, the sequence space wrapping at 16 bits
	emit controllerManager.mvuAecpUnsolicitedLossCounterChanged(entityID, 1u, la::avdecc::controller::InterfaceType::Primary, 1u, 65535u, 0u);
	events = eventsSince(EventJournal::Category::Counters, entityID);
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_EQ(QString{ "'MVU Unsolicited Response Losses' error counter increased to 1 (1 notification lost, sequence ID 65535)" }, events[1].summary);
	EXPECT_EQ(1, eventDetails(events[1]).value("lost_count").toInt());

	// The aggregated statistics signal does not journal these two counters a second time (it knows nothing about the lost notifications), the other counters still going through it
	auto statistics = hive::modelsLibrary::ControllerManager::StatisticsErrorCounters{};
	statistics[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AemAecpUnsolicitedLosses] = 1u;
	statistics[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::MvuAecpUnsolicitedLosses] = 1u;
	emit controllerManager.statisticsErrorCounterChanged(entityID, statistics);
	EXPECT_EQ(std::size_t{ 2u }, eventsSince(EventJournal::Category::Counters, entityID).size());
	statistics[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpTimeouts] = 2u;
	emit controllerManager.statisticsErrorCounterChanged(entityID, statistics);
	events = eventsSince(EventJournal::Category::Counters, entityID);
	ASSERT_EQ(std::size_t{ 3u }, events.size());
	EXPECT_EQ(QString{ "'AECP Timeouts' error counter increased to 2" }, events[2].summary);
}

// Acknowledging the error counters in the UI is journaled with the values that were displayed, so the next increase of the same counter (journaled from zero) stays interpretable
TEST_F(EventJournal_F, Control_AcknowledgedCounters_AreJournaled)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	// Two error counters flagged (MEDIA_UNLOCKED is covered by the stream lock rule for this stream, so only STREAM_INTERRUPTED is an error event here)
	auto errorCounters = hive::modelsLibrary::ControllerManager::StreamInputErrorCounters{};
	errorCounters[la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked] = 2u;
	errorCounters[la::avdecc::entity::StreamInputCounterValidFlag::StreamInterrupted] = 1u;
	emit controllerManager.streamInputErrorCounterChanged(entityID, TestStreamIndex, errorCounters);
	EXPECT_EQ(std::size_t{ 1u }, eventsSince(EventJournal::Category::Counters, entityID).size());
	EXPECT_TRUE(eventsSince(EventJournal::Category::Control, entityID).empty());

	// One counter acknowledged
	errorCounters.erase(la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked);
	emit controllerManager.streamInputErrorCounterChanged(entityID, TestStreamIndex, errorCounters);
	auto events = eventsSince(EventJournal::Category::Control, entityID);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Error counters acknowledged: Media Unlocked (2)" }, events[0].summary);
	EXPECT_EQ(EventJournal::Severity::Info, events[0].severity);
	EXPECT_EQ(EventJournal::Level::User, events[0].level);
	EXPECT_TRUE(events[0].subject.startsWith("Input Stream 0"));
	auto const counters = eventDetails(events[0]).value("counters").toArray();
	ASSERT_EQ(1, counters.size());
	EXPECT_EQ(QString{ "Media Unlocked" }, counters[0].toObject().value("counter").toString());
	EXPECT_EQ(2, counters[0].toObject().value("value").toInt());

	// All the counters acknowledged, then nothing left to acknowledge
	emit controllerManager.streamInputErrorCounterChanged(entityID, TestStreamIndex, {});
	emit controllerManager.streamInputErrorCounterChanged(entityID, TestStreamIndex, {});
	events = eventsSince(EventJournal::Category::Control, entityID);
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_EQ(QString{ "Error counters acknowledged: Stream Interrupted (1)" }, events[1].summary);

	// The next increase is journaled from zero
	errorCounters = {};
	errorCounters[la::avdecc::entity::StreamInputCounterValidFlag::StreamInterrupted] = 1u;
	emit controllerManager.streamInputErrorCounterChanged(entityID, TestStreamIndex, errorCounters);
	auto const counterEvents = eventsSince(EventJournal::Category::Counters, entityID);
	ASSERT_EQ(std::size_t{ 2u }, counterEvents.size());
	EXPECT_EQ(QString{ "'Stream Interrupted' error counter increased to 1" }, counterEvents[1].summary);
	EXPECT_EQ(0, eventDetails(counterEvents[1]).value("previous_value").toInt());

	// Statistics counters go through the same rule
	auto statistics = hive::modelsLibrary::ControllerManager::StatisticsErrorCounters{};
	statistics[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpRetries] = 3u;
	emit controllerManager.statisticsErrorCounterChanged(entityID, statistics);
	emit controllerManager.statisticsErrorCounterChanged(entityID, {});
	events = eventsSince(EventJournal::Category::Control, entityID);
	ASSERT_EQ(std::size_t{ 3u }, events.size());
	EXPECT_EQ(QString{ "Error counters acknowledged: AECP Retries (3)" }, events[2].summary);
	EXPECT_EQ(QString{ "Statistics" }, events[2].subject);
}

// Refreshing and identifying an entity leave no trace in its model: the requests are journaled
TEST_F(EventJournal_F, Control_RefreshAndIdentifyRequests_AreJournaled)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	controllerManager.identifyEntity(entityID, std::chrono::milliseconds{ 100 });
	auto events = waitForEvents(EventJournal::Category::Control, entityID, 1u);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Entity identify requested" }, events[0].summary);
	EXPECT_EQ(EventJournal::Level::User, events[0].level);

	// The refresh request comes before the entity goes offline and online again
	auto const onlineCountBefore = entityOnlineCount();
	ASSERT_TRUE(controllerManager.refreshEntity(entityID));
	events = waitForEvents(EventJournal::Category::Control, entityID, 2u);
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	EXPECT_EQ(QString{ "Entity refresh requested" }, events[1].summary);
	waitForEntitiesOnline(onlineCountBefore + 1u);
	auto const entityEvents = eventsSince(EventJournal::Category::Entity, entityID);
	ASSERT_EQ(std::size_t{ 3u }, entityEvents.size());
	EXPECT_EQ(QString{ "Entity went offline" }, entityEvents[1].summary);
	EXPECT_EQ(QString{ "Entity is online" }, entityEvents[2].summary);
	EXPECT_LE(events[1].timestamp, entityEvents[1].timestamp);
}

/* *********************************
   Markers, heartbeat and controller side context
*/
// A marker is a user level event whose summary is the text, signed with the computer name
TEST_F(EventJournal_F, Markers_AreJournaledWithTheAuthor)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto& journal = EventJournal::getInstance();
	auto const countBefore = journal.currentSessionEvents().size();

	EXPECT_TRUE(journal.addMarker("  Show start  "));
	EXPECT_FALSE(journal.addMarker(""));
	EXPECT_FALSE(journal.addMarker("   "));
	auto const events = journal.currentSessionEvents();
	ASSERT_EQ(countBefore + 1u, events.size());
	auto const& marker = events.back();
	EXPECT_EQ(EventJournal::Category::Marker, marker.category);
	EXPECT_EQ(EventJournal::Severity::Info, marker.severity);
	EXPECT_EQ(EventJournal::Level::User, marker.level);
	EXPECT_FALSE(marker.entityID);
	EXPECT_EQ(QString{ "Show start" }, marker.summary);
	EXPECT_EQ(journal.currentSessionMetadata().value("computer_name"), eventDetails(marker).value("author").toString());
	EXPECT_EQ(QString{ "Marker" }, EventJournal::categoryToString(marker.category));
}

// The heartbeat is a diagnostic session event written at every period, with the figures accumulated since the previous one
TEST_F(EventJournal_F, Heartbeat_FollowsThePeriod)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto& journal = EventJournal::getInstance();
	loadConnectedFixture();
	auto const heartbeats = [&journal]()
	{
		auto events = std::vector<EventJournal::Event>{};
		for (auto const& event : journal.currentSessionEvents())
		{
			if (event.category == EventJournal::Category::Session && event.summary == "Heartbeat")
			{
				events.push_back(event);
			}
		}
		return events;
	};
	auto const countBefore = heartbeats().size();
	auto const waitForHeartbeats = [&heartbeats](std::size_t const count)
	{
		auto const deadline = QDeadlineTimer{ 2000 };
		while (!deadline.hasExpired() && heartbeats().size() < count)
		{
			QTest::qWait(10);
		}
		return heartbeats();
	};

	journal.setHeartbeatInterval(std::chrono::milliseconds{ 100 });
	auto events = waitForHeartbeats(countBefore + 1u);
	ASSERT_EQ(countBefore + 1u, events.size());
	EXPECT_EQ(EventJournal::Level::Diagnostic, events.back().level);
	auto details = eventDetails(events.back());
	EXPECT_EQ(2, details.value("entities_online").toInt());
	EXPECT_LE(0, details.value("events_since_last").toInt());
	EXPECT_LE(0, details.value("notifications_since_last").toInt());
	EXPECT_LE(0, details.value("event_loop_latency_ms").toInt());
	EXPECT_LT(0, details.value("process_rss_mb").toInt());

	// The next one only counts the events written since the previous heartbeat
	journal.addMarker("Between two heartbeats");
	events = waitForHeartbeats(countBefore + 2u);
	ASSERT_EQ(countBefore + 2u, events.size());
	EXPECT_EQ(1, eventDetails(events.back()).value("events_since_last").toInt());

	// Not written at user level (the period is stretched first, so no heartbeat is in flight when the level changes)
	journal.setHeartbeatInterval(std::chrono::minutes{ 5 });
	journal.setRecordingLevel(EventJournal::Level::User);
	journal.setHeartbeatInterval(std::chrono::milliseconds{ 100 });
	QTest::qWait(350);
	EXPECT_EQ(countBefore + 2u, heartbeats().size());
	journal.setHeartbeatInterval(std::chrono::minutes{ 5 });
	journal.setRecordingLevel(EventJournal::Level::Diagnostic);
}

// The AECP average response time of an entity is journaled at diagnostic level when it moved by 50 ms or more since the last journaled value
TEST_F(EventJournal_F, AecpResponseTime_IsJournaledOnLargeChanges)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
	auto baseline = std::chrono::microseconds{};
	{
		auto const controlledEntity = controllerManager.getControlledEntity(entityID);
		ASSERT_TRUE(!!controlledEntity);
		baseline = controlledEntity->getAecpResponseAverageTime(la::avdecc::controller::InterfaceType::Primary);
	}
	auto const responseTimeEvents = [this, entityID]()
	{
		auto events = eventsSince(EventJournal::Category::Entity, entityID);
		events.erase(std::remove_if(events.begin(), events.end(),
									 [](EventJournal::Event const& event)
									 {
										 return !event.summary.startsWith("AECP response time changed");
									 }),
			events.end());
		return events;
	};

	// Below the threshold: nothing
	emit controllerManager.aecpResponseAverageTimeChanged(entityID, baseline + std::chrono::milliseconds{ 20 }, la::avdecc::controller::InterfaceType::Primary);
	EXPECT_TRUE(responseTimeEvents().empty());

	// Above: journaled with the previous journaled value (the baseline, not the intermediate value)
	emit controllerManager.aecpResponseAverageTimeChanged(entityID, baseline + std::chrono::milliseconds{ 60 }, la::avdecc::controller::InterfaceType::Primary);
	auto events = responseTimeEvents();
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "AECP response time changed to %1 ms" }.arg(std::chrono::duration<double, std::milli>{ baseline + std::chrono::milliseconds{ 60 } }.count(), 0, 'f', 3), events[0].summary);
	EXPECT_EQ(EventJournal::Level::Diagnostic, events[0].level);
	EXPECT_EQ(QString{ "Primary Interface" }, events[0].subject);
	auto const details = eventDetails(events[0]);
	auto const baselineMsec = std::chrono::duration<double, std::milli>{ baseline }.count();
	auto const changedMsec = std::chrono::duration<double, std::milli>{ baseline + std::chrono::milliseconds{ 60 } }.count();
	EXPECT_DOUBLE_EQ(baselineMsec, details.value("previous_average_ms").toDouble());
	EXPECT_DOUBLE_EQ(changedMsec, details.value("average_ms").toDouble());
	EXPECT_EQ(QString{ "Primary" }, details.value("interface").toString());

	// Back to the baseline: journaled again (the difference to the last journaled value is 60 ms)
	emit controllerManager.aecpResponseAverageTimeChanged(entityID, baseline, la::avdecc::controller::InterfaceType::Primary);
	EXPECT_EQ(std::size_t{ 2u }, responseTimeEvents().size());
}

/* *********************************
   Replay engine (phase 4b)
*/
// The state of an entity at an event is its last snapshot plus the patches journaled since: at the last event it matches the live entity, at its online event it is the bare snapshot, and it follows the cursor both ways
TEST_F(EventJournal_F, Replay_StateFollowsTheCursor)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto const talkerID = la::avdecc::UniqueIdentifier{ TalkerEntityID };
	loadConnectedFixture();
	auto const snapshotID = entityOnlineDetails(entityID).value("snapshot_id").toVariant().toLongLong();
	ASSERT_GT(snapshotID, 0);

	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
	controllerManager.setEntityName(entityID, "Replayed listener");
	controllerManager.setStreamInputName(entityID, 0u, 1u, "Replayed input");
	controllerManager.setClockSource(entityID, TestClockDomainIndex, 1u);
	waitForEvents(EventJournal::Category::Configuration, entityID, 3u);
	// The talker leaves before the end of the session
	unloadEntity(talkerID);

	auto const filePath = QDir::temp().filePath("HiveEventJournalTests_replay.hej");
	auto const replay = exportAndReplay(filePath);
	ASSERT_TRUE(!!replay);
	auto const& events = replay->events();
	ASSERT_FALSE(events.empty());
	EXPECT_FALSE(replay->cursor().has_value());
	EXPECT_GE(replay->snapshotEntitiesCount(), std::size_t{ 2u });

	// At the last event: the listener is the live entity, the talker is gone
	{
		auto const changes = replay->seekToEvent(events.size() - 1u);
		EXPECT_EQ(events.size() - 1u, replay->cursor().value_or(0u));
		EXPECT_NE(changes.updated.end(), std::find(changes.updated.begin(), changes.updated.end(), entityID));
		EXPECT_TRUE(changes.removed.empty());
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		EXPECT_EQ(snapshotID, state->snapshotID);
		EXPECT_EQ(QString{ "online" }, state->snapshotReason);
		EXPECT_GE(state->appliedPatches, std::size_t{ 3u });
		EXPECT_EQ(std::size_t{ 0u }, state->rejectedPatches);
		expectDocumentMatchesEntity(state->document, entityID);
		EXPECT_EQ(nullptr, replay->entityState(talkerID));
		EXPECT_EQ(replay->entities().end(), replay->entities().find(talkerID));
	}

	// At the online event of the listener: the bare snapshot
	auto const onlineIndex = lastEventIndex(events, entityID, "Entity is online");
	ASSERT_TRUE(onlineIndex.has_value());
	auto const talkerOnlineIndex = lastEventIndex(events, talkerID, "Entity is online");
	ASSERT_TRUE(talkerOnlineIndex.has_value());
	auto const renameIndex = lastEventIndex(events, entityID, "Entity name changed to 'Replayed listener'");
	ASSERT_TRUE(renameIndex.has_value());
	auto const talkerOfflineIndex = lastEventIndex(events, talkerID, "Entity went offline");
	ASSERT_TRUE(talkerOfflineIndex.has_value());
	auto snapshotDocument = nlohmann::json{};
	{
		auto const changes = replay->seekToEvent(*onlineIndex);
		EXPECT_NE(changes.updated.end(), std::find(changes.updated.begin(), changes.updated.end(), entityID));
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		EXPECT_EQ(snapshotID, state->snapshotID);
		EXPECT_EQ(std::size_t{ 0u }, state->appliedPatches);
		EXPECT_EQ(std::string{ "P1 152" }, state->document.at("entity_model").at("entity_descriptor").at("dynamic").at("entity_name").get<std::string>());
		snapshotDocument = state->document;
	}
	// Once both are online (the fixture entities come online in no particular order), the talker is there too
	auto const bothOnlineIndex = std::max(*onlineIndex, *talkerOnlineIndex);
	{
		replay->seekToEvent(bothOnlineIndex);
		EXPECT_NE(nullptr, replay->entityState(talkerID));
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		EXPECT_EQ(std::size_t{ 0u }, state->appliedPatches);
	}

	// Moving forward from the same snapshot applies only the patches in between (the rename, then the rest)
	{
		auto const changes = replay->seekToEvent(*renameIndex);
		EXPECT_EQ(std::vector<la::avdecc::UniqueIdentifier>{ entityID }, changes.updated);
		EXPECT_TRUE(changes.removed.empty());
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		EXPECT_EQ(std::size_t{ 1u }, state->appliedPatches);
		EXPECT_EQ(std::string{ "Replayed listener" }, state->document.at("entity_model").at("entity_descriptor").at("dynamic").at("entity_name").get<std::string>());
	}
	{
		// Same position again: nothing changes
		auto const changes = replay->seekToEvent(*renameIndex);
		EXPECT_TRUE(changes.updated.empty());
		EXPECT_TRUE(changes.removed.empty());
	}
	{
		auto const changes = replay->seekToEvent(events.size() - 1u);
		EXPECT_EQ(std::vector<la::avdecc::UniqueIdentifier>{ talkerID }, changes.removed);
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		expectDocumentMatchesEntity(state->document, entityID);
	}

	// Moving backward rebuilds from the snapshot: same document as the first visit
	{
		auto const changes = replay->seekToEvent(*onlineIndex);
		EXPECT_NE(changes.updated.end(), std::find(changes.updated.begin(), changes.updated.end(), entityID));
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		EXPECT_EQ(snapshotDocument, state->document);
	}

	// Stepping through every event gives the same final document as the direct seek (the incremental path is consistent)
	{
		for (auto index = *onlineIndex; index < events.size(); ++index)
		{
			replay->seekToEvent(index);
			if (index == *talkerOfflineIndex)
			{
				EXPECT_EQ(nullptr, replay->entityState(talkerID));
			}
			else if (index >= bothOnlineIndex && index < *talkerOfflineIndex)
			{
				EXPECT_NE(nullptr, replay->entityState(talkerID));
			}
		}
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		expectDocumentMatchesEntity(state->document, entityID);
	}

	// Before the first event: nobody
	{
		auto const changes = replay->seekToTimestamp(events.front().timestamp - 1);
		EXPECT_FALSE(replay->cursor().has_value());
		EXPECT_NE(changes.removed.end(), std::find(changes.removed.begin(), changes.removed.end(), entityID));
		EXPECT_TRUE(replay->entities().empty());
	}
	// By timestamp: the last event at or before the rename
	{
		replay->seekToTimestamp(events[*renameIndex].timestamp);
		ASSERT_TRUE(replay->cursor().has_value());
		EXPECT_GE(*replay->cursor(), *renameIndex);
		EXPECT_EQ(events[*renameIndex].timestamp, events[*replay->cursor()].timestamp);
	}
	// Out of range: ignored
	{
		auto const cursor = replay->cursor();
		EXPECT_TRUE(replay->seekToEvent(events.size()).updated.empty());
		EXPECT_EQ(cursor, replay->cursor());
	}
	QFile::remove(filePath);
}

// An entity that goes offline is absent from the replay until it comes back, and then its new snapshot is the anchor
TEST_F(EventJournal_F, Replay_OfflineEntity_ComesBackWithItsNewSnapshot)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto const firstSnapshotID = entityOnlineDetails(entityID).value("snapshot_id").toVariant().toLongLong();
	ASSERT_GT(firstSnapshotID, 0);

	// Keep the entity to reload it as it is now, then take it offline and bring it back
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
	auto const entityFilePath = QDir::temp().filePath("HiveEventJournalTests_replayEntity.ave");
	QFile::remove(entityFilePath);
	{
		auto const [error, message] = controllerManager.serializeControlledEntityAsJson(entityID, entityFilePath, snapshotFlags(), "Unit Tests");
		ASSERT_FALSE(!!error) << message;
	}
	unloadEntity(entityID);
	auto const onlineCountBefore = entityOnlineCount();
	{
		auto const [error, message] = controllerManager.loadVirtualEntityFromJson(entityFilePath, snapshotFlags());
		ASSERT_EQ(la::avdecc::jsonSerializer::DeserializationError::NoError, error) << "Failed to load the entity back: " << message;
	}
	QFile::remove(entityFilePath);
	waitForEntitiesOnline(onlineCountBefore + 1u);
	auto const secondSnapshotID = entityOnlineDetails(entityID).value("snapshot_id").toVariant().toLongLong();
	ASSERT_GT(secondSnapshotID, firstSnapshotID);
	controllerManager.setEntityName(entityID, "Back online");
	waitForEvents(EventJournal::Category::Configuration, entityID, 1u);

	auto const filePath = QDir::temp().filePath("HiveEventJournalTests_replayOffline.hej");
	auto const replay = exportAndReplay(filePath);
	ASSERT_TRUE(!!replay);
	auto const& events = replay->events();
	auto const offlineIndex = lastEventIndex(events, entityID, "Entity went offline");
	auto const onlineIndex = lastEventIndex(events, entityID, "Entity is online");
	ASSERT_TRUE(offlineIndex.has_value());
	ASSERT_TRUE(onlineIndex.has_value());
	ASSERT_LT(*offlineIndex, *onlineIndex);

	// Present before going offline, from the first snapshot
	{
		replay->seekToEvent(*offlineIndex - 1u);
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		EXPECT_EQ(firstSnapshotID, state->snapshotID);
	}
	// Absent at the offline event
	{
		auto const changes = replay->seekToEvent(*offlineIndex);
		EXPECT_EQ(std::vector<la::avdecc::UniqueIdentifier>{ entityID }, changes.removed);
		EXPECT_EQ(nullptr, replay->entityState(entityID));
	}
	// Back with the new snapshot, and up to date at the end
	{
		auto const changes = replay->seekToEvent(*onlineIndex);
		EXPECT_EQ(std::vector<la::avdecc::UniqueIdentifier>{ entityID }, changes.updated);
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		EXPECT_EQ(secondSnapshotID, state->snapshotID);
		EXPECT_EQ(QString{ "online" }, state->snapshotReason);
		EXPECT_EQ(std::size_t{ 0u }, state->appliedPatches);
	}
	{
		replay->seekToEvent(events.size() - 1u);
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		EXPECT_EQ(secondSnapshotID, state->snapshotID);
		EXPECT_EQ(std::string{ "Back online" }, state->document.at("entity_model").at("entity_descriptor").at("dynamic").at("entity_name").get<std::string>());
		expectDocumentMatchesEntity(state->document, entityID);
	}
	// Straight from the end to before the offline event: rebuilt from the first snapshot
	{
		auto const changes = replay->seekToEvent(*offlineIndex - 1u);
		EXPECT_EQ(std::vector<la::avdecc::UniqueIdentifier>{ entityID }, changes.updated);
		auto const* const state = replay->entityState(entityID);
		ASSERT_NE(nullptr, state);
		EXPECT_EQ(firstSnapshotID, state->snapshotID);
		EXPECT_EQ(std::string{ "P1 152" }, state->document.at("entity_model").at("entity_descriptor").at("dynamic").at("entity_name").get<std::string>());
	}

	// The controller stopping ends the presence of every entity (they are not journaled offline one by one), until they come back online after the restart
	controllerManager.destroyController();
	{
		auto const deadline = QDeadlineTimer{ 5000 };
		while (!deadline.hasExpired() && !summariesSince(EventJournal::Category::Session).contains("Controller stopped"))
		{
			QTest::qWait(10);
		}
		ASSERT_TRUE(summariesSince(EventJournal::Category::Session).contains("Controller stopped"));
	}
	try
	{
		controllerManager.createController(la::avdecc::protocol::ProtocolInterface::Type::Virtual, "Unit Tests", 0x0001, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), "en", nullptr);
	}
	catch (la::avdecc::controller::Controller::Exception const&)
	{
		ASSERT_FALSE(true);
	}
	{
		auto const deadline = QDeadlineTimer{ 5000 };
		while (!deadline.hasExpired() && !summariesSince(EventJournal::Category::Session).contains("Controller restarted"))
		{
			QTest::qWait(10);
		}
		ASSERT_TRUE(summariesSince(EventJournal::Category::Session).contains("Controller restarted"));
	}
	auto const restartedReplay = exportAndReplay(filePath);
	ASSERT_TRUE(!!restartedReplay);
	auto const stopIndex = lastEventIndex(restartedReplay->events(), la::avdecc::UniqueIdentifier{}, "Controller stopped");
	ASSERT_TRUE(stopIndex.has_value());
	restartedReplay->seekToEvent(*stopIndex - 1u);
	EXPECT_NE(nullptr, restartedReplay->entityState(entityID));
	auto const changes = restartedReplay->seekToEvent(*stopIndex);
	EXPECT_EQ(std::size_t{ 2u }, changes.removed.size()); // The listener and the talker
	EXPECT_NE(changes.removed.end(), std::find(changes.removed.begin(), changes.removed.end(), entityID));
	EXPECT_TRUE(restartedReplay->entities().empty());
	restartedReplay->seekToEvent(restartedReplay->events().size() - 1u);
	EXPECT_TRUE(restartedReplay->entities().empty());
	QFile::remove(filePath);
}

// Clearing the session renews the snapshots (the patches since the previous ones are gone), so the replay of a cleared journal starts from the 'cleared' snapshots at its first event
TEST_F(EventJournal_F, Session_Clear_RenewsTheSnapshots)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto& journal = EventJournal::getInstance();
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	auto const talkerID = la::avdecc::UniqueIdentifier{ TalkerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
	controllerManager.setEntityName(entityID, "Named before the clear");
	waitForEvents(EventJournal::Category::Configuration, entityID, 1u);

	journal.clearCurrentSession();
	auto const events = journal.currentSessionEvents();
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Journal cleared" }, events[0].summary);
	EXPECT_EQ(QString{ "2" }, journal.currentSessionMetadata().value("snapshots_count"));

	auto const filePath = QDir::temp().filePath("HiveEventJournalTests_cleared.hej");
	auto const replay = exportAndReplay(filePath);
	ASSERT_TRUE(!!replay);
	auto const index = EventJournal::loadSnapshotsIndex(filePath);
	ASSERT_EQ(std::size_t{ 2u }, index.size());
	for (auto const& info : index)
	{
		EXPECT_EQ(QString{ "cleared" }, info.reason);
		EXPECT_GE(info.timestamp, events[0].timestamp);
	}

	// The snapshots were written after the first event but describe the entities from that event on
	auto const changes = replay->seekToEvent(0u);
	EXPECT_EQ(std::size_t{ 2u }, changes.updated.size());
	auto const* const state = replay->entityState(entityID);
	ASSERT_NE(nullptr, state);
	EXPECT_EQ(QString{ "cleared" }, state->snapshotReason);
	EXPECT_EQ(std::string{ "Named before the clear" }, state->document.at("entity_model").at("entity_descriptor").at("dynamic").at("entity_name").get<std::string>());
	EXPECT_NE(nullptr, replay->entityState(talkerID));
	QFile::remove(filePath);
}

/* *********************************
   Notification origin (phase 8)
*/
// The events journaled from a notification of la_avdecc carry its origin: an AEM unsolicited notification received on the interface of the controller gives details.origin 'unsolicited', details.interface 'Primary' and the details.sequence_id of the message, while a change made through the virtual controller (no message received) carries no origin
TEST_F(EventJournal_F, NotificationOrigin_IsJournaledWithTheEvent)
{
	using EventJournal = hive::modelsLibrary::EventJournal;
	auto const entityID = la::avdecc::UniqueIdentifier{ ConnectedListenerEntityID };
	loadConnectedFixture();
	auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

	// A device on the virtual bus of the controller (same bus name, same executor), sending an unsolicited SET_NAME response about the fixture entity to the controller: the fixture entities are virtual, but the controller processes a notification received about them like any other
	auto const controllerEID = controllerManager.getControllerEID(la::avdecc::controller::InterfaceType::Primary);
	ASSERT_TRUE(!!controllerEID);
	auto const executorName = std::string{ "avdecc::protocol::PI" }; // The default executor of la_avdecc, registered by the end station of the controller (created without an explicit executor)
	ASSERT_TRUE(la::avdecc::ExecutorManager::getInstance().isExecutorRegistered(executorName));
	auto device = la::avdecc::protocol::ProtocolInterface::create(la::avdecc::protocol::ProtocolInterface::Type::Virtual, "Unit Tests", executorName);
	ASSERT_TRUE(!!device);
	{
		auto payload = la::avdecc::Serializer<la::avdecc::protocol::aemPayload::AecpAemSetNameResponsePayloadSize>{};
		payload << la::avdecc::entity::model::DescriptorType::Entity << la::avdecc::entity::model::DescriptorIndex{ 0u };
		payload << std::uint16_t{ 0u } << la::avdecc::entity::model::ConfigurationIndex{ 0u };
		payload << la::avdecc::entity::model::AvdeccFixedString{ "Unsolicited name" };
		auto unsolicited = la::avdecc::protocol::AemAecpdu{ true };
		unsolicited.setSrcAddress(device->getMacAddress());
		unsolicited.setDestAddress(device->getMacAddress()); // The virtual protocol interfaces created through the public factory share the same MAC address, the one of the controller included
		unsolicited.setStatus(la::avdecc::protocol::AecpStatus::Success);
		unsolicited.setTargetEntityID(entityID);
		unsolicited.setControllerEntityID(controllerEID);
		unsolicited.setSequenceID(42u);
		unsolicited.setCommandType(la::avdecc::protocol::AemCommandType::SetName);
		unsolicited.setUnsolicited(true);
		unsolicited.setCommandSpecificData(payload.data(), payload.usedBytes());
		ASSERT_FALSE(!!device->sendAecpMessage(unsolicited));
	}
	auto events = waitForEvents(EventJournal::Category::Configuration, entityID, 1u);
	ASSERT_EQ(std::size_t{ 1u }, events.size());
	EXPECT_EQ(QString{ "Entity name changed to 'Unsolicited name'" }, events[0].summary);
	auto details = eventDetails(events[0]);
	EXPECT_EQ(QString{ "unsolicited" }, details.value("origin").toString());
	EXPECT_EQ(QString{ "Primary" }, details.value("interface").toString());
	EXPECT_EQ(42, details.value("sequence_id").toInt()); // The sequence_id of the message that carried the change, to match the event with a network capture

	// A change made through the virtual controller is not carried by a message: no origin
	controllerManager.setEntityName(entityID, "Renamed by the test");
	events = waitForEvents(EventJournal::Category::Configuration, entityID, 2u);
	ASSERT_EQ(std::size_t{ 2u }, events.size());
	details = eventDetails(events[1]);
	EXPECT_FALSE(details.contains("origin"));
	EXPECT_FALSE(details.contains("interface"));
	EXPECT_FALSE(details.contains("sequence_id"));

	// Outside of a signal of the controller, nothing is known either
	EXPECT_EQ(la::avdecc::controller::NotificationOrigin::Source::Unknown, controllerManager.currentNotificationOrigin().source);
}
