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

#include <hive/modelsLibrary/networkTopologyModel.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/helper.hpp>

#include <la/avdecc/internals/streamFormatInfo.hpp>

#include <QTimer>

#include <map>
#include <set>
#include <unordered_map>
#include <utility>

namespace hive
{
namespace modelsLibrary
{
namespace
{
// Debounce delay before rebuilding the topology, so a burst of entity updates triggers a single rebuild
constexpr auto RebuildDebounceDelay = std::chrono::milliseconds{ 500 };

// Information collected for one AVB interface of one entity, input of the topology inference
struct InterfaceInfo
{
	la::avdecc::UniqueIdentifier entityID{};
	la::avdecc::entity::model::AvbInterfaceIndex avbInterfaceIndex{ 0u };
	QString entityName{};
	QString avbInterfaceName{};
	bool isMultiInterface{ false };
	bool isTalker{ false };
	bool isListener{ false };
	bool isStreaming{ false };
	la::avdecc::UniqueIdentifier clockIdentity{};
	la::avdecc::UniqueIdentifier gptpGrandmasterID{};
	std::optional<std::uint8_t> gptpDomainNumber{};
	NetworkTopologyModel::ClockLockState clockLockState{ NetworkTopologyModel::ClockLockState::Unknown };
	std::optional<std::uint32_t> propagationDelay{};
	std::vector<la::avdecc::UniqueIdentifier> asPath{};
	la::avdecc::UniqueIdentifier internalBridgeClockIdentity{}; /**< Clock identity of the entity's internal bridge, for bridged endpoints (propagation delay == 0) */
	std::uint64_t errorCounter{ 0u };
	la::avdecc::entity::model::MilanVersion milanVersion{};
};

// Information collected for one connected stream output, input of the bandwidth accumulation
struct StreamOutputInfo
{
	la::avdecc::UniqueIdentifier talkerEntityID{};
	la::avdecc::entity::model::StreamIndex streamIndex{ 0u };
	la::avdecc::entity::model::AvbInterfaceIndex avbInterfaceIndex{ 0u };
	QString talkerName{};
	QString streamName{};
	la::avdecc::entity::model::StreamFormat streamFormat{};
	bool isRunning{ true };
	bool isClassB{ false };
	std::vector<la::avdecc::entity::model::StreamIdentification> listeners{};
};

// Information collected for one stream input, used to resolve the listener side of a connection
struct StreamInputInfo
{
	la::avdecc::entity::model::AvbInterfaceIndex avbInterfaceIndex{ 0u };
	QString listenerName{};
	QString streamName{};
};

using StreamInputInfos = std::map<std::pair<la::avdecc::UniqueIdentifier, la::avdecc::entity::model::StreamIndex>, StreamInputInfo>;

// Returns the estimated reserved bandwidth (bits per second) of an audio stream, transport overhead included (0 if unknown).
// Class A streams send 8000 packets per second, Class B streams 4000 (IEEE 802.1Q SR classes).
std::uint64_t computeStreamReservedBandwidth(la::avdecc::entity::model::StreamFormat const& streamFormat, bool const isClassB)
{
	auto const formatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
	auto const type = formatInfo->getType();
	if (type != la::avdecc::entity::model::StreamFormatInfo::Type::AAF && type != la::avdecc::entity::model::StreamFormatInfo::Type::IEC_61883_6)
	{
		return 0u;
	}

	auto const sampleRate = static_cast<std::uint64_t>(formatInfo->getSamplingRate().getNominalSampleRate());
	if (sampleRate == 0u)
	{
		return 0u;
	}

	auto const packetRate = std::uint64_t{ isClassB ? 4000u : 8000u };
	auto const samplesPerPacket = (sampleRate + packetRate - 1u) / packetRate;
	auto const bytesPerSample = (formatInfo->getSampleSize() + 7u) / 8u;
	auto const payloadBytes = samplesPerPacket * formatInfo->getChannelsCount() * bytesPerSample;
	// AVTP common stream header is 24 bytes, IEC 61883 adds a 8 bytes CIP header
	auto const avtpHeaderBytes = std::uint64_t{ type == la::avdecc::entity::model::StreamFormatInfo::Type::IEC_61883_6 ? 32u : 24u };
	// Ethernet wire overhead per frame: preamble(7) + SFD(1) + MACs(12) + VLAN(4) + EtherType(2) + FCS(4) + IFG(12)
	constexpr auto ethernetOverheadBytes = std::uint64_t{ 42u };

	return (ethernetOverheadBytes + avtpHeaderBytes + payloadBytes) * 8u * packetRate;
}

// Builds the topology of one network from the interfaces belonging to it.
// Stream connections whose talker or listener is not part of the network are automatically ignored
// (their endpoints cannot be resolved against this network's interfaces).
NetworkTopologyModel::Topology buildTopologyForNetwork(std::vector<InterfaceInfo> const& interfaces, std::vector<StreamOutputInfo> const& streamOutputs, StreamInputInfos const& streamInputs)
{
	using Topology = NetworkTopologyModel::Topology;
	using Node = NetworkTopologyModel::Node;
	using NodeType = NetworkTopologyModel::NodeType;
	using Edge = NetworkTopologyModel::Edge;
	using EdgeKind = NetworkTopologyModel::EdgeKind;
	using Stream = NetworkTopologyModel::Stream;

	auto topology = Topology{};

	// Create one Entity node per AVB interface, indexed by clock identity when valid
	auto nodeIndexByClockIdentity = std::unordered_map<la::avdecc::UniqueIdentifier, std::size_t, la::avdecc::UniqueIdentifier::hash>{};
	for (auto const& info : interfaces)
	{
		auto node = Node{};
		node.type = NodeType::Entity;
		node.clockIdentity = info.clockIdentity;
		node.name = info.entityName;
		node.entityID = info.entityID;
		node.avbInterfaceIndex = info.avbInterfaceIndex;
		node.avbInterfaceName = info.avbInterfaceName;
		node.isMultiInterface = info.isMultiInterface;
		node.isTalker = info.isTalker;
		node.isListener = info.isListener;
		node.isStreaming = info.isStreaming;
		node.gptpGrandmasterID = info.gptpGrandmasterID;
		node.gptpDomainNumber = info.gptpDomainNumber;
		node.clockLockState = info.clockLockState;
		node.propagationDelay = info.propagationDelay;
		node.hasAsPath = info.asPath.size() >= 2;
		node.errorCounter = info.errorCounter;

		auto const nodeIndex = topology.nodes.size();
		topology.nodes.push_back(std::move(node));
		if (info.clockIdentity)
		{
			nodeIndexByClockIdentity.emplace(info.clockIdentity, nodeIndex);
		}
	}

	auto const findOrCreateBridge = [&topology, &nodeIndexByClockIdentity](la::avdecc::UniqueIdentifier const clockIdentity) -> std::size_t
	{
		if (auto const it = nodeIndexByClockIdentity.find(clockIdentity); it != nodeIndexByClockIdentity.end())
		{
			return it->second;
		}
		auto node = Node{};
		node.type = NodeType::InferredBridge;
		node.clockIdentity = clockIdentity;
		node.name = helper::getVendorName(clockIdentity);
		auto const nodeIndex = topology.nodes.size();
		topology.nodes.push_back(std::move(node));
		nodeIndexByClockIdentity.emplace(clockIdentity, nodeIndex);
		return nodeIndex;
	};

	auto edgeKeys = std::set<std::pair<std::size_t, std::size_t>>{};
	auto const addEdge = [&topology, &edgeKeys](std::size_t const upstreamNodeIndex, std::size_t const downstreamNodeIndex, EdgeKind const kind)
	{
		if (upstreamNodeIndex == downstreamNodeIndex)
		{
			return;
		}
		if (edgeKeys.emplace(upstreamNodeIndex, downstreamNodeIndex).second)
		{
			topology.edges.push_back(Edge{ upstreamNodeIndex, downstreamNodeIndex, kind });
		}
	};

	// Walk the AsPath of each interface (from the entity towards the grandmaster), creating InferredBridge nodes
	// for path elements that don't match any known entity, and chaining edges up to the first already known node
	for (auto interfaceIndex = std::size_t{ 0u }; interfaceIndex < interfaces.size(); ++interfaceIndex)
	{
		auto const& info = interfaces[interfaceIndex];
		if (info.asPath.size() < 2)
		{
			continue;
		}

		auto downstreamNodeIndex = interfaceIndex; // Entity nodes have been pushed first, in the same order than 'interfaces'
		for (auto pathPosition = static_cast<int>(info.asPath.size()) - 1; pathPosition >= 0; --pathPosition)
		{
			auto const& hopClockIdentity = info.asPath[static_cast<std::size_t>(pathPosition)];

			// Skip the entity itself and its internal bridge
			if (hopClockIdentity == info.clockIdentity || hopClockIdentity == info.internalBridgeClockIdentity)
			{
				continue;
			}

			if (auto const it = nodeIndexByClockIdentity.find(hopClockIdentity); it != nodeIndexByClockIdentity.end())
			{
				// Hop is already known (another entity, or a bridge created while processing another path): link and stop here,
				// the rest of the path upwards has been (or will be) built when processing that node's own path
				addEdge(it->second, downstreamNodeIndex, EdgeKind::GptpPath);
				break;
			}

			auto const bridgeNodeIndex = findOrCreateBridge(hopClockIdentity);
			addEdge(bridgeNodeIndex, downstreamNodeIndex, EdgeKind::GptpPath);
			downstreamNodeIndex = bridgeNodeIndex;
		}
	}

	// Entities not exposing an AsPath but following an external grandmaster: add a dashed 'grandmaster only' edge
	// so the node is still attached to its clock domain (the physical path is unknown)
	for (auto interfaceIndex = std::size_t{ 0u }; interfaceIndex < interfaces.size(); ++interfaceIndex)
	{
		auto const& info = interfaces[interfaceIndex];
		if (info.asPath.size() >= 2)
		{
			continue;
		}
		if (!info.gptpGrandmasterID || info.gptpGrandmasterID == info.clockIdentity || info.gptpGrandmasterID == info.internalBridgeClockIdentity)
		{
			continue;
		}
		auto const grandmasterNodeIndex = findOrCreateBridge(info.gptpGrandmasterID);
		addEdge(grandmasterNodeIndex, interfaceIndex, EdgeKind::GptpGrandmasterOnly);
	}

	// Mark grandmaster nodes: any node whose clock identity is announced as grandmaster by at least one entity
	{
		auto grandmasterIdentities = std::set<la::avdecc::UniqueIdentifier>{};
		for (auto const& info : interfaces)
		{
			if (info.gptpGrandmasterID)
			{
				grandmasterIdentities.insert(info.gptpGrandmasterID);
			}
		}
		for (auto& node : topology.nodes)
		{
			node.isGrandmaster = grandmasterIdentities.count(node.clockIdentity) > 0;
		}
		// A grandmaster identity may also designate an entity through its internal bridge
		for (auto const& info : interfaces)
		{
			if (info.internalBridgeClockIdentity && grandmasterIdentities.count(info.internalBridgeClockIdentity) > 0)
			{
				if (auto const it = nodeIndexByClockIdentity.find(info.clockIdentity); it != nodeIndexByClockIdentity.end())
				{
					topology.nodes[it->second].isGrandmaster = true;
				}
			}
		}
	}

	// Accumulate the established stream connections on the edges they transit through.
	// The path between talker and listener is computed on the inferred tree (lowest common ancestor),
	// like other AVB controllers do. Endpoints without a usable AsPath are skipped (their position is unknown).
	{
		// Lookup maps: (entityID, avbInterfaceIndex) -> node, parent of each node, (upstream, downstream) -> edge
		auto nodeIndexByEntityInterface = std::map<std::pair<la::avdecc::UniqueIdentifier, la::avdecc::entity::model::AvbInterfaceIndex>, std::size_t>{};
		for (auto interfaceIndex = std::size_t{ 0u }; interfaceIndex < interfaces.size(); ++interfaceIndex)
		{
			nodeIndexByEntityInterface.emplace(std::make_pair(interfaces[interfaceIndex].entityID, interfaces[interfaceIndex].avbInterfaceIndex), interfaceIndex);
		}
		auto parentOf = std::vector<int>(topology.nodes.size(), -1);
		auto edgeIndexByNodes = std::map<std::pair<std::size_t, std::size_t>, std::size_t>{};
		for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < topology.edges.size(); ++edgeIndex)
		{
			auto const& edge = topology.edges[edgeIndex];
			parentOf[edge.downstreamNodeIndex] = static_cast<int>(edge.upstreamNodeIndex);
			edgeIndexByNodes.emplace(std::make_pair(edge.upstreamNodeIndex, edge.downstreamNodeIndex), edgeIndex);
		}

		// Computes the tree path between two nodes: visited nodes (both endpoints included) and traversed edges.
		// Both vectors are left empty if the nodes belong to different trees.
		struct TreePath
		{
			std::vector<std::size_t> nodeIndices{};
			std::vector<std::size_t> edgeIndices{};
		};
		auto const computeTreePath = [&topology, &parentOf, &edgeIndexByNodes](std::size_t const fromNodeIndex, std::size_t const toNodeIndex)
		{
			auto path = TreePath{};

			// Ancestors chain of 'from' (including itself), with their position in the chain
			auto fromChain = std::vector<std::size_t>{};
			auto fromChainPosition = std::unordered_map<std::size_t, std::size_t>{};
			for (auto nodeIndex = static_cast<int>(fromNodeIndex); nodeIndex != -1 && fromChain.size() <= topology.nodes.size(); nodeIndex = parentOf[static_cast<std::size_t>(nodeIndex)])
			{
				fromChainPosition.emplace(static_cast<std::size_t>(nodeIndex), fromChain.size());
				fromChain.push_back(static_cast<std::size_t>(nodeIndex));
			}

			// Walk up from 'to' until we reach a common ancestor
			auto toChain = std::vector<std::size_t>{};
			auto commonAncestorPosition = std::optional<std::size_t>{};
			for (auto nodeIndex = static_cast<int>(toNodeIndex); nodeIndex != -1 && toChain.size() <= topology.nodes.size(); nodeIndex = parentOf[static_cast<std::size_t>(nodeIndex)])
			{
				if (auto const it = fromChainPosition.find(static_cast<std::size_t>(nodeIndex)); it != fromChainPosition.end())
				{
					commonAncestorPosition = it->second;
					break;
				}
				toChain.push_back(static_cast<std::size_t>(nodeIndex));
			}
			if (!commonAncestorPosition)
			{
				return path;
			}

			// Upward part: from -> common ancestor (common ancestor included)
			for (auto chainPosition = std::size_t{ 0u }; chainPosition <= *commonAncestorPosition; ++chainPosition)
			{
				path.nodeIndices.push_back(fromChain[chainPosition]);
				if (chainPosition < *commonAncestorPosition)
				{
					if (auto const it = edgeIndexByNodes.find(std::make_pair(fromChain[chainPosition + 1u], fromChain[chainPosition])); it != edgeIndexByNodes.end())
					{
						path.edgeIndices.push_back(it->second);
					}
				}
			}
			// Downward part: common ancestor -> to
			for (auto chainPosition = toChain.size(); chainPosition > 0u; --chainPosition)
			{
				auto const downstreamNodeIndex = toChain[chainPosition - 1u];
				auto const upstreamNodeIndex = static_cast<std::size_t>(parentOf[downstreamNodeIndex]);
				if (auto const it = edgeIndexByNodes.find(std::make_pair(upstreamNodeIndex, downstreamNodeIndex)); it != edgeIndexByNodes.end())
				{
					path.edgeIndices.push_back(it->second);
				}
				path.nodeIndices.push_back(downstreamNodeIndex);
			}
			return path;
		};

		for (auto const& streamInfo : streamOutputs)
		{
			auto const talkerIt = nodeIndexByEntityInterface.find(std::make_pair(streamInfo.talkerEntityID, streamInfo.avbInterfaceIndex));
			if (talkerIt == nodeIndexByEntityInterface.end() || !topology.nodes[talkerIt->second].hasAsPath)
			{
				continue;
			}
			auto const reservedBandwidth = computeStreamReservedBandwidth(streamInfo.streamFormat, streamInfo.isClassB);

			for (auto const& listenerStream : streamInfo.listeners)
			{
				// Resolve the listener node through the AVB interface of its stream input descriptor
				auto const inputIt = streamInputs.find(std::make_pair(listenerStream.entityID, listenerStream.streamIndex));
				if (inputIt == streamInputs.end())
				{
					continue;
				}
				auto const listenerIt = nodeIndexByEntityInterface.find(std::make_pair(listenerStream.entityID, inputIt->second.avbInterfaceIndex));
				if (listenerIt == nodeIndexByEntityInterface.end() || !topology.nodes[listenerIt->second].hasAsPath)
				{
					continue;
				}

				auto path = computeTreePath(talkerIt->second, listenerIt->second);
				if (path.edgeIndices.empty())
				{
					continue;
				}

				auto stream = Stream{};
				stream.talkerEntityID = streamInfo.talkerEntityID;
				stream.talkerStreamIndex = streamInfo.streamIndex;
				stream.listenerEntityID = listenerStream.entityID;
				stream.listenerStreamIndex = listenerStream.streamIndex;
				stream.description = QString{ "%1:%2 -> %3:%4" }.arg(streamInfo.talkerName, streamInfo.streamName, inputIt->second.listenerName, inputIt->second.streamName);
				if (streamInfo.isClassB)
				{
					stream.description += " [Class B]";
				}
				stream.isRunning = streamInfo.isRunning;
				stream.isClassB = streamInfo.isClassB;
				stream.reservedBandwidth = reservedBandwidth;
				stream.nodeIndices = std::move(path.nodeIndices);
				stream.edgeIndices = std::move(path.edgeIndices);

				auto const streamIndex = topology.streams.size();
				for (auto const edgeIndex : stream.edgeIndices)
				{
					topology.edges[edgeIndex].streamIndices.push_back(streamIndex);
				}
				topology.streams.push_back(std::move(stream));
			}
		}
	}

	return topology;
}
} // namespace

NetworkTopologyModel::NetworkTopologyModel(QObject* parent)
	: QObject{ parent }
{
	_rebuildTimer = new QTimer{ this };
	_rebuildTimer->setSingleShot(true);
	_rebuildTimer->setInterval(RebuildDebounceDelay);
	connect(_rebuildTimer, &QTimer::timeout, this,
		[this]()
		{
			rebuild();
		});

	auto const scheduleRebuild = [this]()
	{
		_rebuildTimer->start();
	};

	// Any change to the information the topology is inferred from triggers a debounced rebuild
	auto& manager = ControllerManager::getInstance();
	connect(&manager, &ControllerManager::controllerOnline, this, scheduleRebuild);
	connect(&manager, &ControllerManager::controllerOffline, this, scheduleRebuild);
	connect(&manager, &ControllerManager::entityOnline, this, scheduleRebuild);
	connect(&manager, &ControllerManager::entityNameChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::avbInterfaceNameChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::gptpChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::asPathChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::avbInterfaceInfoChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::clockDomainCountersChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::streamOutputConnectionsChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::streamRunningChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::streamFormatChanged, this, scheduleRebuild);

	// Error counters are kept in incremental caches (querying the manager for every stream of every entity
	// during a rebuild is too costly on large networks)
	connect(&manager, &ControllerManager::entityOffline, this,
		[this, scheduleRebuild](la::avdecc::UniqueIdentifier const entityID)
		{
			_streamInputErrorCounters.erase(entityID);
			_statisticsErrorCounters.erase(entityID);
			scheduleRebuild();
		});
	connect(&manager, &ControllerManager::streamInputErrorCounterChanged, this,
		[this, scheduleRebuild](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, ControllerManager::StreamInputErrorCounters const& errorCounters)
		{
			auto counter = std::uint64_t{ 0u };
			for (auto const& [flag, value] : errorCounters)
			{
				counter += value;
			}
			_streamInputErrorCounters[entityID][descriptorIndex] = counter;
			scheduleRebuild();
		});
	connect(&manager, &ControllerManager::statisticsErrorCounterChanged, this,
		[this, scheduleRebuild](la::avdecc::UniqueIdentifier const entityID, ControllerManager::StatisticsErrorCounters const& errorCounters)
		{
			auto counter = std::uint64_t{ 0u };
			for (auto const& [flag, value] : errorCounters)
			{
				counter += value;
			}
			_statisticsErrorCounters[entityID] = counter;
			scheduleRebuild();
		});
}

NetworkTopologyModel::~NetworkTopologyModel() = default;

std::vector<NetworkTopologyModel::Network> const& NetworkTopologyModel::networks() const noexcept
{
	return _networks;
}

void NetworkTopologyModel::rebuild() noexcept
{
	// Collect the gPTP information of every AVB interface of every discovered entity, plus the established
	// stream connections (talker side) and the stream inputs (to resolve the listener side of the connections)
	auto interfaces = std::vector<InterfaceInfo>{};
	auto streamOutputs = std::vector<StreamOutputInfo>{};
	auto streamInputs = StreamInputInfos{};
	auto& manager = ControllerManager::getInstance();
	manager.foreachEntity(
		[this, &interfaces, &streamOutputs, &streamInputs](la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::ControlledEntity const& entity)
		{
			try
			{
				auto const& configurationNode = entity.getCurrentConfigurationNode();
				auto const entityName = helper::smartEntityName(entity);
				auto const isMultiInterface = configurationNode.avbInterfaces.size() > 1;
				auto const isTalker = entity.getEntity().getTalkerCapabilities().test(la::avdecc::entity::TalkerCapability::Implemented) && !configurationNode.streamOutputs.empty();
				auto const isListener = entity.getEntity().getListenerCapabilities().test(la::avdecc::entity::ListenerCapability::Implemented) && !configurationNode.streamInputs.empty();
				auto const milanVersion = entity.getMilanCompatibilityVersion();

				// Media clock lock state, from the counters of the first clock domain (same rule than the Discovered Entities list)
				auto clockLockState = NetworkTopologyModel::ClockLockState::Unknown;
				if (!configurationNode.clockDomains.empty())
				{
					auto const& clockDomainNode = configurationNode.clockDomains.begin()->second;
					if (clockDomainNode.dynamicModel.counters)
					{
						auto const& counters = *clockDomainNode.dynamicModel.counters;
						auto const itLocked = counters.find(la::avdecc::entity::ClockDomainCounterValidFlag::Locked);
						auto const itUnlocked = counters.find(la::avdecc::entity::ClockDomainCounterValidFlag::Unlocked);
						if (itLocked != counters.end() && itUnlocked != counters.end())
						{
							clockLockState = itLocked->second > itUnlocked->second ? NetworkTopologyModel::ClockLockState::Locked : NetworkTopologyModel::ClockLockState::Unlocked;
						}
					}
				}

				// Aggregate entity level error counters (statistics + stream input errors) from the incremental caches
				auto errorCounter = std::uint64_t{ 0u };
				if (auto const statisticsIt = _statisticsErrorCounters.find(entityID); statisticsIt != _statisticsErrorCounters.end())
				{
					errorCounter += statisticsIt->second;
				}
				if (auto const streamsIt = _streamInputErrorCounters.find(entityID); streamsIt != _streamInputErrorCounters.end())
				{
					for (auto const& [streamIndex, counter] : streamsIt->second)
					{
						errorCounter += counter;
					}
				}
				for (auto const& [streamIndex, streamNode] : configurationNode.streamInputs)
				{
					streamInputs.emplace(std::make_pair(entityID, streamIndex), StreamInputInfo{ streamNode.staticModel.avbInterfaceIndex, entityName, helper::objectName(&entity, streamNode) });
				}

				// Collect the established stream connections, from the talker side
				for (auto const& [streamIndex, streamNode] : configurationNode.streamOutputs)
				{
					auto const& connections = streamNode.dynamicModel.connections;
					if (connections.empty())
					{
						continue;
					}
					auto streamInfo = StreamOutputInfo{};
					streamInfo.talkerEntityID = entityID;
					streamInfo.streamIndex = streamIndex;
					streamInfo.avbInterfaceIndex = streamNode.staticModel.avbInterfaceIndex;
					streamInfo.talkerName = entityName;
					streamInfo.streamName = helper::objectName(&entity, streamNode);
					streamInfo.streamFormat = streamNode.dynamicModel.streamFormat;
					streamInfo.isRunning = streamNode.dynamicModel.isStreamRunning.value_or(true);
					// IEEE1722.1 default SR class is Class A, only consider Class B when the stream doesn't support Class A
					streamInfo.isClassB = streamNode.staticModel.streamFlags.test(la::avdecc::entity::StreamFlag::ClassB) && !streamNode.staticModel.streamFlags.test(la::avdecc::entity::StreamFlag::ClassA);
					for (auto const& listenerStream : connections)
					{
						streamInfo.listeners.push_back(listenerStream);
					}
					streamOutputs.push_back(std::move(streamInfo));
				}

				for (auto const& [avbInterfaceIndex, avbInterfaceNode] : configurationNode.avbInterfaces)
				{
					auto info = InterfaceInfo{};
					info.entityID = entityID;
					info.avbInterfaceIndex = avbInterfaceIndex;
					info.entityName = entityName;
					info.avbInterfaceName = helper::objectName(&entity, avbInterfaceNode);
					info.isMultiInterface = isMultiInterface;
					info.isTalker = isTalker;
					info.isListener = isListener;
					info.errorCounter = errorCounter;
					info.clockIdentity = avbInterfaceNode.dynamicModel.clockIdentity;
					info.gptpGrandmasterID = avbInterfaceNode.dynamicModel.gptpGrandmasterID;
					info.gptpDomainNumber = avbInterfaceNode.dynamicModel.gptpDomainNumber;
					info.clockLockState = clockLockState;
					info.milanVersion = milanVersion;

					if (avbInterfaceNode.dynamicModel.avbInterfaceInfo)
					{
						info.propagationDelay = avbInterfaceNode.dynamicModel.avbInterfaceInfo->propagationDelay;
					}
					if (avbInterfaceNode.dynamicModel.asPath)
					{
						for (auto const& pathClockIdentity : avbInterfaceNode.dynamicModel.asPath->sequence)
						{
							info.asPath.push_back(pathClockIdentity);
						}
					}

					// Bridged endpoint detection (same heuristic than the one validated in the field by other controllers):
					// a null propagation delay means the interface is directly connected to a bridge embedded in the same unit,
					// that internal bridge is the AsPath element adjacent to the entity and must not be drawn as an external bridge
					if (info.propagationDelay && *info.propagationDelay == 0u && !info.asPath.empty())
					{
						if (info.asPath.back() != info.clockIdentity)
						{
							info.internalBridgeClockIdentity = info.asPath.back();
						}
						else if (info.asPath.size() >= 2)
						{
							info.internalBridgeClockIdentity = info.asPath[info.asPath.size() - 2];
						}
					}

					interfaces.push_back(std::move(info));
				}
			}
			catch (...)
			{
				// Entity has no AEM support or no valid current configuration, it cannot appear in the topology
			}
		});

	// Flag the interfaces currently sending at least one connected and running stream (visual cue on talkers)
	{
		auto streamingInterfaces = std::set<std::pair<la::avdecc::UniqueIdentifier, la::avdecc::entity::model::AvbInterfaceIndex>>{};
		for (auto const& streamInfo : streamOutputs)
		{
			if (streamInfo.isRunning && !streamInfo.listeners.empty())
			{
				streamingInterfaces.emplace(streamInfo.talkerEntityID, streamInfo.avbInterfaceIndex);
			}
		}
		for (auto& info : interfaces)
		{
			info.isStreaming = streamingInterfaces.count(std::make_pair(info.entityID, info.avbInterfaceIndex)) > 0;
		}
	}

	// Partition the interfaces by network (same AVB interface index = same network) and build one topology per network
	auto interfacesByNetwork = std::map<la::avdecc::entity::model::AvbInterfaceIndex, std::vector<InterfaceInfo>>{};
	for (auto& info : interfaces)
	{
		interfacesByNetwork[info.avbInterfaceIndex].push_back(std::move(info));
	}

	auto networks = std::vector<Network>{};
	for (auto const& [avbInterfaceIndex, networkInterfaces] : interfacesByNetwork)
	{
		// A network is a redundant Primary/Secondary network as soon as one Milan redundant device sits on it: keep the highest Milan version among its entities to name it
		auto networkMilanVersion = la::avdecc::entity::model::MilanVersion{};
		for (auto const& info : networkInterfaces)
		{
			networkMilanVersion = std::max(networkMilanVersion, info.milanVersion);
		}
		networks.push_back(Network{ avbInterfaceIndex, networkMilanVersion, buildTopologyForNetwork(networkInterfaces, streamOutputs, streamInputs) });
	}

	// Cross network interconnection detection (severe cabling error for redundant networks):
	// 1. The same clock identity is seen in more than one network (an entity of one network is reachable through
	//    the gPTP information of another network, or appears as an inferred bridge there)
	{
		auto nodesByClockIdentity = std::unordered_map<la::avdecc::UniqueIdentifier, std::vector<std::pair<std::size_t, std::size_t>>, la::avdecc::UniqueIdentifier::hash>{};
		for (auto networkIndex = std::size_t{ 0u }; networkIndex < networks.size(); ++networkIndex)
		{
			auto const& nodes = networks[networkIndex].topology.nodes;
			for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < nodes.size(); ++nodeIndex)
			{
				if (nodes[nodeIndex].clockIdentity)
				{
					nodesByClockIdentity[nodes[nodeIndex].clockIdentity].push_back(std::make_pair(networkIndex, nodeIndex));
				}
			}
		}
		for (auto const& [clockIdentity, nodeRefs] : nodesByClockIdentity)
		{
			auto networksSpanned = std::set<std::size_t>{};
			for (auto const& [networkIndex, nodeIndex] : nodeRefs)
			{
				networksSpanned.insert(networkIndex);
			}
			if (networksSpanned.size() > 1)
			{
				for (auto const& [networkIndex, nodeIndex] : nodeRefs)
				{
					auto& node = networks[networkIndex].topology.nodes[nodeIndex];
					node.isInterconnected = true;
					node.interconnectionError = "Networks are interconnected: this clock identity is seen in more than one network";
				}
			}
		}
	}

	// 2. A node announced as grandmaster by other entities but following another grandmaster itself through a
	//    physical path (in a single gPTP domain this means the networks are looped through this node)
	for (auto& network : networks)
	{
		auto& topology = network.topology;
		auto hasGptpUpstream = std::vector<bool>(topology.nodes.size(), false);
		for (auto const& edge : topology.edges)
		{
			if (edge.kind == EdgeKind::GptpPath)
			{
				hasGptpUpstream[edge.downstreamNodeIndex] = true;
			}
		}
		for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < topology.nodes.size(); ++nodeIndex)
		{
			auto& node = topology.nodes[nodeIndex];
			if (node.isGrandmaster && !node.isInterconnected && hasGptpUpstream[nodeIndex] && node.type == NodeType::Entity && node.gptpGrandmasterID && node.gptpGrandmasterID != node.clockIdentity)
			{
				node.isInterconnected = true;
				node.interconnectionError = "Possible network interconnection: announced as grandmaster by some entities but following another grandmaster itself";
			}
		}
	}

	_networks = std::move(networks);
	emit topologyChanged();
}

} // namespace modelsLibrary
} // namespace hive
