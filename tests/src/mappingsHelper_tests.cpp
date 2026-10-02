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
* @file mappingsHelper_tests.cpp
* @author Christophe Calmejane
*/

#include <gtest/gtest.h>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <avdecc/mappingsHelper.hpp>

#include <QApplication>
#include <QStringList>
#ifdef _WIN32
#	pragma warning(push)
#	pragma warning(disable : 4127) // Disable conditional expression is constant
#endif
#include <QTest>
#ifdef _WIN32
#	pragma warning(pop)
#endif

namespace
{
// Entity of the 'data/mappings/DanglingMappings.json' test file: 9 Stream Inputs (the last one being CRF) and a Stream Port Input with 4 single channel Audio Clusters
constexpr auto TestFilePath = "data/mappings/DanglingMappings.json";
constexpr auto EntityID = la::avdecc::UniqueIdentifier::value_type{ 0x001B92FFFF000001u };
constexpr auto StreamPortIndex = la::avdecc::entity::model::StreamPortIndex{ 0u };

class MappingsHelper_F : public ::testing::Test
{
public:
	virtual void SetUp() override
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

		// Create a controller
		try
		{
			controllerManager.createController(la::avdecc::protocol::ProtocolInterface::Type::Virtual, "Unit Tests", 0x0001, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), "en", nullptr);
		}
		catch (la::avdecc::controller::Controller::Exception const&)
		{
			ASSERT_FALSE(true);
		}

		// Load the entity
		auto const flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::IgnoreAEMSanityChecks, la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel };
		auto const [err, msg] = controllerManager.loadVirtualEntityFromJson(TestFilePath, flags);
		ASSERT_EQ(la::avdecc::jsonSerializer::DeserializationError::NoError, err) << "Failed to load entity file: " << msg;
		QTest::qWait(10); // Flush Qt EventLoop
	}

	virtual void TearDown() override
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		controllerManager.destroyController();
	}

	/** Gets the errors of the given Stream Port Input mapping. */
	QStringList getErrors(la::avdecc::entity::model::AudioMapping const& mapping) const noexcept
	{
		auto controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(la::avdecc::UniqueIdentifier{ EntityID });
		if (!controlledEntity)
		{
			return { "Entity not found" };
		}
		return avdecc::mappingsHelper::getAudioMappingErrors(*controlledEntity, la::avdecc::entity::model::DescriptorType::StreamPortInput, StreamPortIndex, mapping);
	}

private:
	int _argc{ 0 };
	QApplication _app{ _argc, nullptr };
};
} // namespace

TEST_F(MappingsHelper_F, ValidMapping)
{
	EXPECT_EQ(QStringList{}, getErrors(la::avdecc::entity::model::AudioMapping{ 0u, 0u, 0u, 0u }));
}

TEST_F(MappingsHelper_F, StreamDoesNotExist)
{
	EXPECT_EQ(QStringList{ "Invalid mapping: Stream Input 9 does not exist" }, getErrors(la::avdecc::entity::model::AudioMapping{ 9u, 0u, 1u, 0u }));
}

TEST_F(MappingsHelper_F, DanglingMapping)
{
	// Stream Input 8 is a CRF stream, which has no audio channel
	EXPECT_EQ(QStringList{ "Dangling mapping: channel 0 does not exist in the current format of Stream Input 8 (channel count: 0)" }, getErrors(la::avdecc::entity::model::AudioMapping{ 8u, 0u, 2u, 0u }));
}

TEST_F(MappingsHelper_F, ClusterChannelDoesNotExist)
{
	EXPECT_EQ(QStringList{ "Invalid mapping: channel 1 does not exist in Audio Cluster 3 (channel count: 1)" }, getErrors(la::avdecc::entity::model::AudioMapping{ 0u, 1u, 3u, 1u }));
}

TEST_F(MappingsHelper_F, ClusterDoesNotExist)
{
	EXPECT_EQ(QStringList{ "Invalid mapping: cluster offset 4 does not exist in Stream Port Input 0 (cluster count: 4)" }, getErrors(la::avdecc::entity::model::AudioMapping{ 0u, 2u, 4u, 0u }));
}

TEST_F(MappingsHelper_F, BothSidesInvalid)
{
	EXPECT_EQ((QStringList{ "Invalid mapping: Stream Input 9 does not exist", "Invalid mapping: cluster offset 4 does not exist in Stream Port Input 0 (cluster count: 4)" }), getErrors(la::avdecc::entity::model::AudioMapping{ 9u, 0u, 4u, 0u }));
}

TEST_F(MappingsHelper_F, InvalidMappingsAreKeptAndEntityRetrograded)
{
	auto controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(la::avdecc::UniqueIdentifier{ EntityID });
	ASSERT_TRUE(!!controlledEntity);

	// The invalid mappings reported by the entity during enumeration are kept in the model, so they can be displayed
	EXPECT_EQ(5u, controlledEntity->getStreamPortInputAudioMappings(StreamPortIndex).size());

	// An entity with mappings referencing something that does not exist in the model is misbehaving, so neither IEEE1722.1 nor Milan compatible
	EXPECT_EQ(la::avdecc::controller::ControlledEntity::CompatibilityFlags{ la::avdecc::controller::ControlledEntity::CompatibilityFlag::Misbehaving }, controlledEntity->getCompatibilityFlags());

	// The reason is available in the Compatibility Change Events, although it was detected before the entity was advertised
	ASSERT_EQ(1u, controlledEntity->getCompatibilityChangedEvents().size());
	EXPECT_EQ("IEEE1722.1-2021 - 7.4.44.2.1/7.2.2", controlledEntity->getCompatibilityChangedEvents()[0].specClause);
}
