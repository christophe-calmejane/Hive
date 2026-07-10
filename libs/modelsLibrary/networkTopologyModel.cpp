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

#include <QTimer>

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
	la::avdecc::UniqueIdentifier clockIdentity{};
	la::avdecc::UniqueIdentifier gptpGrandmasterID{};
	std::optional<std::uint8_t> gptpDomainNumber{};
	la::avdecc::controller::ControlledEntity::InterfaceLinkStatus linkStatus{ la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Unknown };
	std::optional<std::uint32_t> propagationDelay{};
	std::vector<la::avdecc::UniqueIdentifier> asPath{};
	la::avdecc::UniqueIdentifier internalBridgeClockIdentity{}; /**< Clock identity of the entity's internal bridge, for bridged endpoints (propagation delay == 0) */
	std::uint64_t errorCounter{ 0u };
};
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
	connect(&manager, &ControllerManager::entityOffline, this, scheduleRebuild);
	connect(&manager, &ControllerManager::entityNameChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::avbInterfaceNameChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::gptpChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::asPathChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::avbInterfaceInfoChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::avbInterfaceLinkStatusChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::streamInputErrorCounterChanged, this, scheduleRebuild);
	connect(&manager, &ControllerManager::statisticsErrorCounterChanged, this, scheduleRebuild);
}

NetworkTopologyModel::~NetworkTopologyModel() = default;

NetworkTopologyModel::Topology const& NetworkTopologyModel::topology() const noexcept
{
	return _topology;
}

void NetworkTopologyModel::rebuild() noexcept
{
	auto topology = Topology{};

	// Collect the gPTP information of every AVB interface of every discovered entity
	auto interfaces = std::vector<InterfaceInfo>{};
	auto& manager = ControllerManager::getInstance();
	manager.foreachEntity(
		[&interfaces, &manager](la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::ControlledEntity const& entity)
		{
			try
			{
				auto const& configurationNode = entity.getCurrentConfigurationNode();
				auto const entityName = helper::smartEntityName(entity);
				auto const isMultiInterface = configurationNode.avbInterfaces.size() > 1;

				// Aggregate entity level error counters (statistics + stream input errors)
				auto errorCounter = std::uint64_t{ 0u };
				for (auto const& [flag, value] : manager.getStatisticsCounters(entityID))
				{
					errorCounter += value;
				}
				for (auto const& [streamIndex, streamNode] : configurationNode.streamInputs)
				{
					for (auto const& [flag, value] : manager.getStreamInputErrorCounters(entityID, streamIndex))
					{
						errorCounter += value;
					}
				}

				for (auto const& [avbInterfaceIndex, avbInterfaceNode] : configurationNode.avbInterfaces)
				{
					auto info = InterfaceInfo{};
					info.entityID = entityID;
					info.avbInterfaceIndex = avbInterfaceIndex;
					info.entityName = entityName;
					info.avbInterfaceName = helper::objectName(&entity, avbInterfaceNode);
					info.isMultiInterface = isMultiInterface;
					info.errorCounter = errorCounter;
					info.clockIdentity = avbInterfaceNode.dynamicModel.clockIdentity;
					info.gptpGrandmasterID = avbInterfaceNode.dynamicModel.gptpGrandmasterID;
					info.gptpDomainNumber = avbInterfaceNode.dynamicModel.gptpDomainNumber;
					info.linkStatus = entity.getAvbInterfaceLinkStatus(avbInterfaceIndex);

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
		node.gptpGrandmasterID = info.gptpGrandmasterID;
		node.gptpDomainNumber = info.gptpDomainNumber;
		node.linkStatus = info.linkStatus;
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

	_topology = std::move(topology);
	emit topologyChanged();
}

} // namespace modelsLibrary
} // namespace hive
