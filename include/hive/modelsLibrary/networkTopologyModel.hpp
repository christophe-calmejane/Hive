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

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>
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
*          The information the topology is inferred from is cached per entity: a full snapshot of the entity is
*          captured when it comes online (on a dedicated thread, so the model thread never waits on an entity
*          guard), then the cache is updated incrementally from the ControllerManager change signals (only the
*          data carried by each event is updated, without any entity access). Recomputing the topology therefore
*          never queries (nor locks) the controlled entities. The recomputation itself is throttled, and
*          topologyChanged() is emitted when a new topology snapshot is available.
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

	/**
	* @brief One connected stream output and the path it transits through in the topology.
	* @details A stream is multicast: whatever the number of listeners, a single stream transits on the network
	*          (and reserves bandwidth once per traversed link). The stream path is therefore the multicast tree
	*          covering all its resolved listeners, not one path per connection.
	*/
	struct Stream
	{
		la::avdecc::UniqueIdentifier talkerEntityID{};
		la::avdecc::entity::model::StreamIndex talkerStreamIndex{ 0u };
		QString description{}; /**< Human readable description of the stream (talker:stream -> listener:stream, or listeners count when more than one) */
		std::size_t listenerCount{ 0u }; /**< Number of listeners whose path could be resolved on the topology */
		bool isRunning{ true }; /**< False when the stream is connected but not streaming */
		bool isClassB{ false }; /**< SR class of the stream: Class B if the stream only supports Class B, Class A otherwise (IEEE1722.1 default) */
		std::uint64_t reservedBandwidth{ 0u }; /**< Estimated reserved bandwidth (bits per second) of the stream, transport overhead included, computed from the stream format and SR class (0 if unknown) */
		std::size_t talkerNodeIndex{ 0u }; /**< Node of the talker interface the stream originates from */
		std::vector<std::size_t> nodeIndices{}; /**< Multicast tree of the stream: all the nodes it transits through (talker, listeners and intermediates) */
		std::vector<std::size_t> edgeIndices{}; /**< Multicast tree of the stream: all the edges it transits through (each edge carries the stream exactly once) */
	};

	/** Immutable snapshot of the topology of one network. */
	struct Topology
	{
		std::vector<Node> nodes{};
		std::vector<Edge> edges{};
		std::vector<Stream> streams{}; /**< Connected stream outputs whose multicast tree could be resolved on the topology */
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
	// Cached information of one AVB interface of one entity, updated incrementally from the ControllerManager signals
	struct InterfaceCache
	{
		QString name{};
		QString fallbackName{}; /**< Localized default name, used when the object name is cleared (cached so rename events never have to query the entity) */
		la::avdecc::UniqueIdentifier clockIdentity{};
		la::avdecc::UniqueIdentifier gptpGrandmasterID{};
		std::optional<std::uint8_t> gptpDomainNumber{};
		std::optional<std::uint32_t> propagationDelay{};
		std::vector<la::avdecc::UniqueIdentifier> asPath{};
	};
	// Cached information of one stream output of one entity
	struct StreamOutputCache
	{
		la::avdecc::entity::model::AvbInterfaceIndex avbInterfaceIndex{ 0u };
		QString name{};
		QString fallbackName{}; /**< Localized default name, used when the object name is cleared */
		la::avdecc::entity::model::StreamFormat streamFormat{};
		bool isRunning{ true };
		bool isClassB{ false }; /**< SR class of the stream: Class B if the stream only supports Class B, Class A otherwise (IEEE1722.1 default) */
		la::avdecc::entity::model::StreamConnections connections{};
	};
	// Cached information of one stream input of one entity
	struct StreamInputCache
	{
		la::avdecc::entity::model::AvbInterfaceIndex avbInterfaceIndex{ 0u };
		QString name{};
		QString fallbackName{}; /**< Localized default name, used when the object name is cleared */
	};
	// All the information the topology is inferred from, for one entity. Fully read once when the entity comes
	// online (a single entity lock), then updated field by field from the change signals (no lock at all), so
	// topology recomputations never have to query the ControllerManager (which locks every queried entity, way
	// too costly on large networks)
	struct EntityCache
	{
		QString entityName{};
		bool isTalker{ false };
		bool isListener{ false };
		la::avdecc::entity::model::MilanVersion milanVersion{};
		la::avdecc::entity::model::ConfigurationIndex currentConfigurationIndex{ 0u };
		std::map<la::avdecc::entity::model::ClockDomainIndex, ClockLockState> clockLockStates{}; /**< Lock state of each clock domain, the first one is the entity level state (same rule than the Discovered Entities list) */
		std::map<la::avdecc::entity::model::AvbInterfaceIndex, InterfaceCache> interfaces{};
		std::map<la::avdecc::entity::model::StreamIndex, StreamOutputCache> streamOutputs{};
		std::map<la::avdecc::entity::model::StreamIndex, StreamInputCache> streamInputs{};
	};

	/**
	* @brief Reads all the topology related information of a single entity (one entity guard).
	* @details Runs on the snapshot thread: acquiring a ControlledEntityGuard can block for a long time while
	*          the controller is busy (typically during the enumeration of a large network), and the guard
	*          itself is watchdogged, so the model (UI) thread must never wait on it.
	* @return The entity cache, or std::nullopt if the entity is gone or cannot appear in the topology (no AEM).
	*/
	static std::optional<EntityCache> buildEntityCache(la::avdecc::UniqueIdentifier const entityID) noexcept;
	/** Queues an asynchronous snapshot of the given entity on the snapshot thread (coalesced if one is already queued). */
	void requestEntitySnapshot(la::avdecc::UniqueIdentifier const entityID) noexcept;
	/** Gets the cache of an entity, or nullptr if not (yet) available. A snapshot is automatically (re)requested when the entity is online but its snapshot is still in flight, so no event is ever lost. */
	EntityCache* findCache(la::avdecc::UniqueIdentifier const entityID) noexcept;
	/** Starts the throttled topology recomputation timer (no-op if already running, so a continuous event stream cannot starve the recomputation). */
	void scheduleRecompute() noexcept;
	/** Recomputes the networks topology snapshot from the entity caches (no ControllerManager access) and emits topologyChanged(). */
	void recompute() noexcept;

	std::vector<Network> _networks{};
	QTimer* _recomputeTimer{ nullptr };

	std::unordered_map<la::avdecc::UniqueIdentifier, EntityCache, la::avdecc::UniqueIdentifier::hash> _entityCaches{};
	std::unordered_set<la::avdecc::UniqueIdentifier, la::avdecc::UniqueIdentifier::hash> _onlineEntities{}; /**< Entities currently online (their snapshot may still be in flight on the snapshot thread) */

	// Snapshot thread state (see buildEntityCache)
	std::thread _snapshotThread{};
	std::mutex _snapshotMutex{};
	std::condition_variable _snapshotCondition{};
	std::deque<la::avdecc::UniqueIdentifier> _snapshotQueue{};
	std::set<la::avdecc::UniqueIdentifier> _snapshotQueuedIDs{}; /**< IDs currently in _snapshotQueue, to coalesce requests */
	bool _snapshotThreadExit{ false };

	// Error counters are aggregated incrementally from their dedicated signals (they change very frequently)
	std::unordered_map<la::avdecc::UniqueIdentifier, std::unordered_map<la::avdecc::entity::model::DescriptorIndex, std::uint64_t>, la::avdecc::UniqueIdentifier::hash> _streamInputErrorCounters{};
	std::unordered_map<la::avdecc::UniqueIdentifier, std::uint64_t, la::avdecc::UniqueIdentifier::hash> _statisticsErrorCounters{};
};

} // namespace modelsLibrary
} // namespace hive
