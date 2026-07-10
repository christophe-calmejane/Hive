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

#include <la/avdecc/controller/avdeccController.hpp>

#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

class QTimer;

namespace hive
{
namespace modelsLibrary
{
/**
* @brief Model of the physical network topology, as seen by the discovered entities.
* @details Builds a protocol agnostic graph (forest of trees) of the network: nodes are either discovered
*          entities (one node per AVB interface) or inferred bridges (network nodes that are not ATDECC
*          entities but are known to exist), edges connect a node to its upstream neighbor.
*          The graph is currently inferred from the gPTP information exposed through ATDECC
*          (AsPath, grandmaster ID, propagation delay), but the topology representation itself is protocol
*          agnostic so other discovery sources (eg. LLDP) may be added later without changing consumers.
*          The model automatically rebuilds itself (debounced) when relevant entity information changes,
*          and emits topologyChanged() when a new topology snapshot is available.
* @note All methods and signals must be used from the thread the model lives in.
*/
class NetworkTopologyModel : public QObject
{
	Q_OBJECT
public:
	/** Type of a topology node. */
	enum class NodeType
	{
		Entity = 0, /**< A discovered ATDECC entity (one node per AVB interface) */
		InferredBridge = 1, /**< A network bridge inferred from the discovery protocol (not an ATDECC entity) */
	};

	/** Kind of a topology edge, ie. how it has been discovered. */
	enum class EdgeKind
	{
		GptpPath = 0, /**< Edge inferred from the gPTP AsPath, denotes (almost certain) physical adjacency */
		GptpGrandmasterOnly = 1, /**< Fallback edge for entities not exposing an AsPath: only the grandmaster is known, the physical path is unknown */
	};

	/** One node of the topology. */
	struct Node
	{
		NodeType type{ NodeType::InferredBridge };
		la::avdecc::UniqueIdentifier clockIdentity{}; /**< gPTP clock identity of the node (may be invalid if the entity doesn't expose it) */
		QString name{}; /**< Entity name for Entity nodes, vendor name (from OUI) for InferredBridge nodes */
		bool isGrandmaster{ false }; /**< True if this node is the gPTP grandmaster of at least one entity */
		// Entity specific fields, left to default values for InferredBridge nodes
		la::avdecc::UniqueIdentifier entityID{};
		la::avdecc::entity::model::AvbInterfaceIndex avbInterfaceIndex{ 0u };
		QString avbInterfaceName{};
		bool isMultiInterface{ false }; /**< True if the entity has more than one AVB interface (each interface gets its own node) */
		la::avdecc::UniqueIdentifier gptpGrandmasterID{};
		std::optional<std::uint8_t> gptpDomainNumber{};
		la::avdecc::controller::ControlledEntity::InterfaceLinkStatus linkStatus{ la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Unknown };
		std::optional<std::uint32_t> propagationDelay{}; /**< Propagation delay (nsec) between this interface and its upstream neighbor */
		bool hasAsPath{ false }; /**< True if the entity exposes a usable AsPath for this interface */
		std::uint64_t errorCounter{ 0u }; /**< Aggregated entity level error counter (stream input errors + statistics errors), duplicated on each interface node of the entity */
	};

	/** One edge of the topology, from a node to its upstream neighbor (towards the grandmaster). */
	struct Edge
	{
		std::size_t upstreamNodeIndex{ 0u };
		std::size_t downstreamNodeIndex{ 0u };
		EdgeKind kind{ EdgeKind::GptpPath };
		std::uint32_t streamCount{ 0u }; /**< Number of established stream connections transiting through this edge (in either direction) */
		std::uint64_t streamPayloadBandwidth{ 0u }; /**< Accumulated payload bitrate (bits per second) of the running streams transiting through this edge, transport overhead excluded */
		std::vector<QString> streamDescriptions{}; /**< Human readable description of each stream connection transiting through this edge */
	};

	/** Immutable snapshot of the network topology. */
	struct Topology
	{
		std::vector<Node> nodes{};
		std::vector<Edge> edges{};
	};

	NetworkTopologyModel(QObject* parent = nullptr);
	virtual ~NetworkTopologyModel() override;

	/** Gets the current topology snapshot (valid until the next topologyChanged() signal). */
	Topology const& topology() const noexcept;

	/** Emitted every time a new topology snapshot has been computed. */
	Q_SIGNAL void topologyChanged();

private:
	void rebuild() noexcept;

	Topology _topology{};
	QTimer* _rebuildTimer{ nullptr };
};

} // namespace modelsLibrary
} // namespace hive
