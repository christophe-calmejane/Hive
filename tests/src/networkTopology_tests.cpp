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
* @file networkTopology_tests.cpp
* @author Christophe Calmejane
*/

#include <gtest/gtest.h>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/networkTopologyModel.hpp>

#include <QDeadlineTimer>
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
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace
{
// Identities of the 'data/networkTopology/1-ZeroPropagationDelay_GrandmasterAdjacency.json' test file
constexpr auto GrandmasterClockIdentity = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFE0000FFu }; // The grandmaster is a bridge, not an ATDECC entity
constexpr auto InternalBridgeClockIdentity = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFE0000C0u }; // The bridge embedded in the unit of EntityC, only visible from that unit
constexpr auto SharedBridgeClockIdentity = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFE0000D0u }; // The switch EntityD and EntityE are both attached to
constexpr auto EntityANonNullDelay = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFE000001u }; // Attached to the grandmaster, non null propagation delay
constexpr auto EntityBNullDelay = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFE000002u }; // Attached to the grandmaster, null propagation delay
constexpr auto EntityCInternalBridge = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFE000003u }; // Attached to the grandmaster through its own internal bridge, null propagation delay
constexpr auto EntityDSharedBridge = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFE000004u }; // Attached to the shared switch, null propagation delay
constexpr auto EntityESharedBridge = la::avdecc::UniqueIdentifier::value_type{ 0x001122FFFE000005u }; // Attached to the shared switch, non null propagation delay

constexpr auto TestFilePath = "data/networkTopology/1-ZeroPropagationDelay_GrandmasterAdjacency.json";
constexpr auto TestFileEntityCount = std::size_t{ 5u };

class NetworkTopology_F : public ::testing::Test
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
	}

	virtual void TearDown() override
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		controllerManager.destroyController();
	}

	/** Loads a network state file and waits until the topology of all its entities has been computed. */
	void loadNetworkState(QString const& filePath, std::size_t const expectedEntityCount)
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics };
		auto const [err, msg] = controllerManager.loadVirtualEntitiesFromJsonNetworkState(filePath, flags);
		ASSERT_EQ(la::avdecc::jsonSerializer::DeserializationError::NoError, err) << "Failed to load NetworkState file: " << msg;

		// The model captures each entity on a dedicated thread then recomputes the topology on a throttled timer, so the result is only available after a few event loop iterations
		auto const deadline = QDeadlineTimer{ 10000 };
		while (!deadline.hasExpired() && countEntityNodes() != expectedEntityCount)
		{
			QTest::qWait(50);
		}
		ASSERT_EQ(expectedEntityCount, countEntityNodes()) << "Topology was not computed for all the entities of the NetworkState file";
	}

	/** Gets the topology of the first (and, for the test files, only) network. */
	hive::modelsLibrary::NetworkTopologyModel::Topology const& getTopology() const noexcept
	{
		static auto const s_EmptyTopology = hive::modelsLibrary::NetworkTopologyModel::Topology{};
		auto const& networks = _model.networks();
		return networks.empty() ? s_EmptyTopology : networks[0].topology;
	}

	/** Gets the node of the given entity, or std::nullopt if the entity is not part of the topology. */
	std::optional<std::size_t> findEntityNode(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto const& topology = getTopology();
		for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < topology.nodes.size(); ++nodeIndex)
		{
			auto const& node = topology.nodes[nodeIndex];
			if (node.type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity && node.entityID == entityID)
			{
				return nodeIndex;
			}
		}
		return std::nullopt;
	}

	/** Gets the node the given node is attached to (its upstream neighbor), or std::nullopt if the node is a root of the topology. */
	std::optional<std::size_t> findUpstreamNode(std::size_t const nodeIndex) const noexcept
	{
		for (auto const& edge : getTopology().edges)
		{
			if (edge.downstreamNodeIndex == nodeIndex)
			{
				return edge.upstreamNodeIndex;
			}
		}
		return std::nullopt;
	}

	/** Gets the node matching the given gPTP clock identity, or std::nullopt if no node has this identity. */
	std::optional<std::size_t> findNodeByClockIdentity(la::avdecc::UniqueIdentifier const clockIdentity) const noexcept
	{
		auto const& topology = getTopology();
		for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < topology.nodes.size(); ++nodeIndex)
		{
			if (topology.nodes[nodeIndex].clockIdentity == clockIdentity)
			{
				return nodeIndex;
			}
		}
		return std::nullopt;
	}

private:
	std::size_t countEntityNodes() const noexcept
	{
		auto count = std::size_t{ 0u };
		for (auto const& node : getTopology().nodes)
		{
			if (node.type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity)
			{
				++count;
			}
		}
		return count;
	}

	int x{ 0 };
	QApplication _app{ x, nullptr };
	hive::modelsLibrary::NetworkTopologyModel _model{ nullptr };
};
} // namespace

/* *********************************
   Null propagation delay handling
*/
// An entity reporting a null propagation delay while being directly attached to its grandmaster must not be mistaken for an entity attached to an internal bridge: it must stay attached to the grandmaster node
TEST_F(NetworkTopology_F, NullPropagationDelayToGrandmaster_EntityIsAttachedToTheGrandmaster)
{
	loadNetworkState(TestFilePath, TestFileEntityCount);
	if (HasFatalFailure())
	{
		return;
	}

	auto const grandmasterNode = findNodeByClockIdentity(la::avdecc::UniqueIdentifier{ GrandmasterClockIdentity });
	ASSERT_TRUE(grandmasterNode.has_value()) << "The grandmaster must be part of the topology";

	auto const entityNode = findEntityNode(la::avdecc::UniqueIdentifier{ EntityBNullDelay });
	ASSERT_TRUE(entityNode.has_value());
	auto const upstreamNode = findUpstreamNode(*entityNode);
	ASSERT_TRUE(upstreamNode.has_value()) << "The entity must not be disconnected from its clock domain";
	EXPECT_EQ(*grandmasterNode, *upstreamNode);
}

// The same entity must not be flagged as being the grandmaster of the network (it merely is attached to it)
TEST_F(NetworkTopology_F, NullPropagationDelayToGrandmaster_EntityIsNotFlaggedGrandmaster)
{
	loadNetworkState(TestFilePath, TestFileEntityCount);
	if (HasFatalFailure())
	{
		return;
	}

	auto const entityNode = findEntityNode(la::avdecc::UniqueIdentifier{ EntityBNullDelay });
	ASSERT_TRUE(entityNode.has_value());
	EXPECT_FALSE(getTopology().nodes[*entityNode].isGrandmaster);

	auto const grandmasterNode = findNodeByClockIdentity(la::avdecc::UniqueIdentifier{ GrandmasterClockIdentity });
	ASSERT_TRUE(grandmasterNode.has_value());
	EXPECT_TRUE(getTopology().nodes[*grandmasterNode].isGrandmaster);
}

// An entity with a non null propagation delay is the reference behavior: it is attached to the grandmaster the same way
TEST_F(NetworkTopology_F, NonNullPropagationDelayToGrandmaster_EntityIsAttachedToTheGrandmaster)
{
	loadNetworkState(TestFilePath, TestFileEntityCount);
	if (HasFatalFailure())
	{
		return;
	}

	auto const grandmasterNode = findNodeByClockIdentity(la::avdecc::UniqueIdentifier{ GrandmasterClockIdentity });
	ASSERT_TRUE(grandmasterNode.has_value());

	auto const entityNode = findEntityNode(la::avdecc::UniqueIdentifier{ EntityANonNullDelay });
	ASSERT_TRUE(entityNode.has_value());
	auto const upstreamNode = findUpstreamNode(*entityNode);
	ASSERT_TRUE(upstreamNode.has_value());
	EXPECT_EQ(*grandmasterNode, *upstreamNode);
	EXPECT_FALSE(getTopology().nodes[*entityNode].isGrandmaster);
}

// A null propagation delay to an AsPath element that is not the grandmaster still denotes a bridge embedded in the unit of the entity: no node is created for it and the entity is attached to the upstream neighbor of that internal bridge
TEST_F(NetworkTopology_F, NullPropagationDelayToInternalBridge_InternalBridgeIsNotDrawn)
{
	loadNetworkState(TestFilePath, TestFileEntityCount);
	if (HasFatalFailure())
	{
		return;
	}

	EXPECT_FALSE(findNodeByClockIdentity(la::avdecc::UniqueIdentifier{ InternalBridgeClockIdentity }).has_value()) << "The internal bridge of an entity must not be drawn as an external bridge";

	auto const grandmasterNode = findNodeByClockIdentity(la::avdecc::UniqueIdentifier{ GrandmasterClockIdentity });
	ASSERT_TRUE(grandmasterNode.has_value());

	auto const entityNode = findEntityNode(la::avdecc::UniqueIdentifier{ EntityCInternalBridge });
	ASSERT_TRUE(entityNode.has_value());
	auto const upstreamNode = findUpstreamNode(*entityNode);
	ASSERT_TRUE(upstreamNode.has_value());
	EXPECT_EQ(*grandmasterNode, *upstreamNode);
}

// A null propagation delay to an AsPath element that another entity also sees denotes a real switch shared by several units, not a bridge embedded in this one: the entity keeps its real position, attached to that switch
TEST_F(NetworkTopology_F, NullPropagationDelayToSharedBridge_EntityIsAttachedToTheSharedBridge)
{
	loadNetworkState(TestFilePath, TestFileEntityCount);
	if (HasFatalFailure())
	{
		return;
	}

	auto const sharedBridgeNode = findNodeByClockIdentity(la::avdecc::UniqueIdentifier{ SharedBridgeClockIdentity });
	ASSERT_TRUE(sharedBridgeNode.has_value()) << "A switch seen by several entities must be part of the topology";

	// The entity with the null propagation delay, and the one with a valid delay, are both attached to that switch
	for (auto const entityID : { EntityDSharedBridge, EntityESharedBridge })
	{
		auto const entityNode = findEntityNode(la::avdecc::UniqueIdentifier{ entityID });
		ASSERT_TRUE(entityNode.has_value());
		auto const upstreamNode = findUpstreamNode(*entityNode);
		ASSERT_TRUE(upstreamNode.has_value());
		EXPECT_EQ(*sharedBridgeNode, *upstreamNode);
	}
}

// Every null propagation delay is reported as suspicious (only an internal bridge link is expected to have one), except the ones explained by a bridge embedded in the same unit
TEST_F(NetworkTopology_F, NullPropagationDelay_IsFlaggedSuspiciousUnlessExplainedByAnInternalBridge)
{
	loadNetworkState(TestFilePath, TestFileEntityCount);
	if (HasFatalFailure())
	{
		return;
	}

	auto const expectedFlags = std::vector<std::pair<la::avdecc::UniqueIdentifier::value_type, bool>>{
		{ EntityANonNullDelay, false }, // Valid propagation delay
		{ EntityBNullDelay, true }, // Null propagation delay to the grandmaster
		{ EntityCInternalBridge, false }, // Null propagation delay to its own internal bridge
		{ EntityDSharedBridge, true }, // Null propagation delay to a shared switch
		{ EntityESharedBridge, false }, // Valid propagation delay
	};
	for (auto const& [entityID, expectedFlag] : expectedFlags)
	{
		auto const entityNode = findEntityNode(la::avdecc::UniqueIdentifier{ entityID });
		ASSERT_TRUE(entityNode.has_value());
		EXPECT_EQ(expectedFlag, getTopology().nodes[*entityNode].hasSuspiciousPropagationDelay) << "Wrong suspicious propagation delay flag for entity " << la::avdecc::utils::toHexString(entityID, true, true);
	}
}

/* *********************************
   Stream reserved bandwidth
*/
namespace
{
// Stream formats used by the reserved bandwidth tests
constexpr auto Crf48kHz_1TimestampPerPdu_500PacketsPerSecond = la::avdecc::entity::model::StreamFormat::value_type{ 0x041060010000BB80u }; // CRF_AUDIO_SAMPLE, 48kHz base frequency, timestamp interval of 96 events
constexpr auto Crf48kHz_1TimestampPerPdu_300PacketsPerSecond = la::avdecc::entity::model::StreamFormat::value_type{ 0x0410A0010000BB80u }; // Same, with a timestamp interval of 160 events
constexpr auto Crf48kHz_6TimestampsPerPdu = la::avdecc::entity::model::StreamFormat::value_type{ 0x041060060000BB80u }; // Same, packing 6 timestamps in each AVTPDU
constexpr auto AafPcm32_48kHz_1Channel = la::avdecc::entity::model::StreamFormat::value_type{ 0x0205022000406000u };
constexpr auto AafPcm32_48kHz_8Channels = la::avdecc::entity::model::StreamFormat::value_type{ 0x0205022002006000u };
constexpr auto AafPcm24_48kHz_8Channels = la::avdecc::entity::model::StreamFormat::value_type{ 0x0205031802006000u };
constexpr auto AafPcm32_96kHz_2Channels = la::avdecc::entity::model::StreamFormat::value_type{ 0x020702200080C000u };

std::uint64_t computeReservedBandwidth(la::avdecc::entity::model::StreamFormat::value_type const streamFormat, bool const isClassB = false) noexcept
{
	return hive::modelsLibrary::NetworkTopologyModel::computeStreamReservedBandwidth(la::avdecc::entity::model::StreamFormat{ streamFormat }, isClassB);
}
} // namespace

// MSRP reserves one frame per SR class measurement interval, so the bandwidth reserved by a media clock stream doesn't depend on the packet rate defined by its format: a CRF stream only sends a few hundred packets per second but still reserves the 5632 kb/s given by the Milan specification (Milan 1.2, table 4.4)
TEST(NetworkTopology, CrfStreamBandwidth_DoesNotDependOnThePacketRate)
{
	constexpr auto ExpectedBandwidth = std::uint64_t{ 5632000u };

	EXPECT_EQ(ExpectedBandwidth, computeReservedBandwidth(Crf48kHz_1TimestampPerPdu_500PacketsPerSecond));
	EXPECT_EQ(ExpectedBandwidth, computeReservedBandwidth(Crf48kHz_1TimestampPerPdu_300PacketsPerSecond)) << "A slower CRF stream carrying the same AVTPDU must reserve the exact same bandwidth";
}

// A CRF AVTPDU is smaller than the smallest tagged Ethernet frame, the reservation must be computed on the padded frame (68 bytes) and not on the useful payload
TEST(NetworkTopology, CrfStreamBandwidth_AccountsForTheMinimumFrameSize)
{
	// 6 timestamps per PDU is a 69 bytes AVTPDU (Milan margin included), ie. a 91 bytes frame that doesn't need any padding
	EXPECT_EQ(7104000u, computeReservedBandwidth(Crf48kHz_6TimestampsPerPdu));
}

// Reference values of the Milan specification (Milan 1.2, table 4.4): AAF PCM32 48kHz is 1536*N + 4288 kb/s, AAF PCM24 48kHz is 1152*N + 4288 kb/s and AAF PCM32 96kHz is 3072*N + 4288 kb/s
TEST(NetworkTopology, AudioStreamBandwidth_MatchesTheMilanReferenceValues)
{
	EXPECT_EQ(5824000u, computeReservedBandwidth(AafPcm32_48kHz_1Channel));
	EXPECT_EQ(16576000u, computeReservedBandwidth(AafPcm32_48kHz_8Channels));
	EXPECT_EQ(13504000u, computeReservedBandwidth(AafPcm24_48kHz_8Channels));
	EXPECT_EQ(10432000u, computeReservedBandwidth(AafPcm32_96kHz_2Channels));
}

// The Class B measurement interval is twice the Class A one: an audio stream packs twice as many samples in each of its AVTPDUs, a media clock stream keeps the same AVTPDU
TEST(NetworkTopology, StreamBandwidth_AccountsForTheClassBMeasurementInterval)
{
	EXPECT_EQ(14432000u, computeReservedBandwidth(AafPcm32_48kHz_8Channels, true));
	EXPECT_EQ(2816000u, computeReservedBandwidth(Crf48kHz_1TimestampPerPdu_500PacketsPerSecond, true));
}

// A stream format that is not supported (or not known) must not reserve any bandwidth, so it is not displayed at all
TEST(NetworkTopology, StreamBandwidth_IsNullForAnUnsupportedFormat)
{
	EXPECT_EQ(0u, computeReservedBandwidth(la::avdecc::entity::model::StreamFormat::value_type{ 0u }));
}
