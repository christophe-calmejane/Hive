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

#include <QApplication>
#include <QDeadlineTimer>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <optional>
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
		auto const flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics };
		auto const [err, msg] = controllerManager.loadVirtualEntitiesFromJsonNetworkState(ConnectedFixturePath, flags);
		ASSERT_EQ(la::avdecc::jsonSerializer::DeserializationError::NoError, err) << "Failed to load NetworkState file: " << msg;

		// The entities come online through queued signals: wait until the journal has processed both
		auto const deadline = QDeadlineTimer{ 5000 };
		while (!deadline.hasExpired() && entityOnlineCount() < onlineCountBefore + 2u)
		{
			QTest::qWait(10);
		}
		ASSERT_EQ(onlineCountBefore + 2u, entityOnlineCount()) << "The fixture entities did not come online";
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
