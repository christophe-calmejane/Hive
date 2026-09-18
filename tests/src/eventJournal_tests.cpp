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

#include <cstddef>
#include <vector>

namespace
{
constexpr auto TestEntityIDBase = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFF000000u };
constexpr auto TestClockDomainIndex = la::avdecc::entity::model::ClockDomainIndex{ 0u };

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
		hive::modelsLibrary::ControllerManager::getInstance().destroyController();
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
