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
#include <unordered_map>
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

	/** Media clock lock state of an entity (from its clock domain counters). */
	enum class ClockLockState
	{
		Unknown = 0, /**< Not reported by the entity */
		Unlocked = 1, /**< Clock domain is not locked */
		Locked = 2, /**< Clock domain is locked on its media clock reference */
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
		bool isTalker{ false }; /**< True if the entity implements a talker with at least one stream output */
		bool isListener{ false }; /**< True if the entity implements a listener with at least one stream input */
		bool isStreaming{ false }; /**< True if the entity currently has at least one connected and running stream output on this interface */
		la::avdecc::UniqueIdentifier gptpGrandmasterID{};
		std::optional<std::uint8_t> gptpDomainNumber{};
		ClockLockState clockLockState{ ClockLockState::Unknown }; /**< Media clock lock state of the entity (entity level, duplicated on each interface node) */
		std::optional<std::uint32_t> propagationDelay{}; /**< Propagation delay (nsec) between this interface and its upstream neighbor */
		bool hasAsPath{ false }; /**< True if the entity exposes a usable AsPath for this interface */
		std::uint64_t errorCounter{ 0u }; /**< Aggregated entity level error counter (stream input errors + statistics errors), duplicated on each interface node of the entity */
		bool isInterconnected{ false }; /**< True when this node reveals a likely interconnection between networks (severe cabling error for redundant networks) */
		QString interconnectionError{}; /**< Human readable description of the interconnection issue (only set when isInterconnected) */
	};

	/** One edge of the topology, from a node to its upstream neighbor (towards the grandmaster). */
	struct Edge
	{
		std::size_t upstreamNodeIndex{ 0u };
		std::size_t downstreamNodeIndex{ 0u };
		EdgeKind kind{ EdgeKind::GptpPath };
		std::vector<std::size_t> streamIndices{}; /**< Indices (in Topology::streams) of the stream connections transiting through this edge */
	};

	/** One established stream connection and the path it transits through in the topology. */
	struct Stream
	{
		la::avdecc::UniqueIdentifier talkerEntityID{};
		la::avdecc::entity::model::StreamIndex talkerStreamIndex{ 0u };
		la::avdecc::UniqueIdentifier listenerEntityID{};
		la::avdecc::entity::model::StreamIndex listenerStreamIndex{ 0u };
		QString description{}; /**< Human readable description of the connection (talker:stream -> listener:stream) */
		bool isRunning{ true }; /**< False when the stream is connected but not streaming */
		bool isClassB{ false }; /**< SR class of the stream: Class B if the stream only supports Class B, Class A otherwise (IEEE1722.1 default) */
		std::uint64_t reservedBandwidth{ 0u }; /**< Estimated reserved bandwidth (bits per second) of the stream, transport overhead included, computed from the stream format and SR class (0 if unknown) */
		std::vector<std::size_t> nodeIndices{}; /**< Path of the stream: nodes from talker to listener (both included) */
		std::vector<std::size_t> edgeIndices{}; /**< Path of the stream: edges from talker to listener */
	};

	/** Immutable snapshot of the topology of one network. */
	struct Topology
	{
		std::vector<Node> nodes{};
		std::vector<Edge> edges{};
		std::vector<Stream> streams{}; /**< Established stream connections whose path could be resolved on the topology */
	};

	/**
	* @brief One network and its topology.
	* @details Networks are identified by the AVB interface index of the entities: interfaces with the same index
	*          belong to the same network (index 0 is the primary network, index 1 the secondary network of
	*          Milan redundant devices). Each network gets its own independent topology.
	*/
	struct Network
	{
		la::avdecc::entity::model::AvbInterfaceIndex avbInterfaceIndex{ 0u };
		la::avdecc::entity::model::MilanVersion milanVersion{}; /**< Highest Milan compatibility version among the entities of this network (used to name the redundant Primary/Secondary networks) */
		Topology topology{};
	};

	NetworkTopologyModel(QObject* parent = nullptr);
	virtual ~NetworkTopologyModel() override;

	/** Gets the current networks snapshot, ordered by AVB interface index (valid until the next topologyChanged() signal). */
	std::vector<Network> const& networks() const noexcept;

	/** Emitted every time a new networks snapshot has been computed. */
	Q_SIGNAL void topologyChanged();

private:
	void rebuild() noexcept;

	std::vector<Network> _networks{};
	QTimer* _rebuildTimer{ nullptr };

	// Error counters are cached incrementally from the ControllerManager signals, so rebuilds don't have to
	// query the manager for every stream of every entity (which is costly on large networks)
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::DescriptorIndex, std::uint64_t>, la::avdecc::UniqueIdentifier::hash> _streamInputErrorCounters{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::uint64_t, la::avdecc::UniqueIdentifier::hash> _statisticsErrorCounters{};
};

} // namespace modelsLibrary
} // namespace hive
