/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "avdecc/helper.hpp"
#include "avdecc/channelConnectionManager.hpp"

#include <optional>

namespace connectionMatrix
{
class EntityNode;

class Node
{
	friend class ModelPrivate;

public:
	enum class Type
	{
		None,

		Entity,

		RedundantOutput,
		RedundantInput,

		RedundantOutputStream,
		RedundantInputStream,

		OutputStream,
		InputStream,

		OutputChannel,
		InputChannel,
	};
	enum class TriState
	{
		Unknown = 0,
		False = 1,
		True = 2,
	};

	virtual ~Node() = default;

	// Returns node type
	Type type() const;

	// Returns true if node type is Entity
	bool isEntityNode() const;

	// Returns true if node type is either RedundantOutput or RedundantInput
	bool isRedundantNode() const;

	// Returns true if node type is either RedundantOutputStream or RedundantInputStream
	bool isRedundantStreamNode() const;

	// Returns true if node type is either RedundantOutputStream, RedundantInputStream, OutputStream or InputStream
	bool isStreamNode() const;

	// Returns true if node type is either OutputChannel or InputChannel
	bool isChannelNode() const;

	// Returns the entity ID
	la::avdecc::UniqueIdentifier const& entityID() const;

	// Returns the parent node
	Node const* parent() const;

	// Returns the parent node
	Node* parent();

	// Returns the EntityNode (top ancestor)
	EntityNode const* entityNode() const;

	// Returns the EntityNode (top ancestor)
	EntityNode* entityNode();

	// Returns true if this node has a parent (false for an entity)
	bool hasParent() const;

	// Returns the name of this node (entity name, stream name, .. etc)
	QString const& name() const;

	// Returns the index of the node in its parent childen
	int index() const;

	// Returns the index child in this node children list, -1 if the child is not related
	int indexOf(Node const* child) const;

	// Returns the child node at index, null if not found
	Node* childAt(int index);

	// Returns the child node at index, null if not found
	Node const* childAt(int index) const;

	// Returns the number of children
	int childrenCount() const;

	// Visitor policy that visit all node types
	struct CompleteHierarchyPolicy
	{
		static bool shouldVisit(Node const* const) noexcept;
	};

	// Visitor policy that visit all relevant nodes in StreamMode
	struct StreamHierarchyPolicy
	{
		static bool shouldVisit(Node const* const node) noexcept;
	};

	// Visitor policy that visit only nodes of StreamNode type
	struct StreamPolicy
	{
		static bool shouldVisit(Node const* const node) noexcept;
	};

	// Visitor policy that visit all relevant nodes in ChannelMode
	struct ChannelHierarchyPolicy
	{
		static bool shouldVisit(Node const* const node) noexcept;
	};

	// Visitor policy that visit only nodes of ChannelNode type
	struct ChannelPolicy
	{
		static bool shouldVisit(Node const* const node) noexcept;
	};

	// Visitor pattern
	using Visitor = std::function<void(Node*)>;

	template<typename Policy = CompleteHierarchyPolicy>
	void accept(Visitor const& visitor, bool const childrenOnly = false) const
	{
		if (!childrenOnly)
		{
			if (Policy::shouldVisit(this))
			{
				visitor(const_cast<Node*>(this));
			}
		}

		for (auto const& child : _children)
		{
			child->accept<Policy>(visitor, false);
		}
	}

protected:
	Node(Type const type, la::avdecc::UniqueIdentifier const& entityID, Node* parent);

	void setName(QString const& name);

protected:
	// Node type
	Type const _type;

	// Associated entity ID
	la::avdecc::UniqueIdentifier const _entityID;

	// Pointer to the parent node (should be null for entities only)
	Node* const _parent;

	// Node name
	QString _name;

	// Holds the children
	std::vector<std::unique_ptr<Node>> _children;
};

class EntityNode : public Node
{
	friend class ModelPrivate;

public:
	static EntityNode* create(la::avdecc::UniqueIdentifier const& entityID, bool const isMilan);

	// Visitor pattern that is called on every stream node that matches avbInterfaceIndex
	using AvbInterfaceIndexVisitor = std::function<void(class StreamNode*)>;
	void accept(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, AvbInterfaceIndexVisitor const& visitor) const;

	bool isMilan() const noexcept;
	la::avdecc::entity::model::ClusterIndex getStreamPortInputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex) const;
	la::avdecc::entity::model::ClusterIndex getStreamPortOutputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex) const;
	la::avdecc::entity::model::AudioMappings const& getInputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortInputIndex) const;
	la::avdecc::entity::model::AudioMappings const& getOutputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortOutputIndex) const;
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings> getInputAudioMappings() const noexcept;
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings> getOutputAudioMappings() const noexcept;

protected:
	EntityNode(la::avdecc::UniqueIdentifier const& entityID, bool const isMilan);
	void setStreamPortInputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterOffset) noexcept;
	void setStreamPortOutputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterOffset) noexcept;
	void setInputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortInputIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept;
	void setOutputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortOutputIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept;

protected:
	bool _isMilan{ false };
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::ClusterIndex> _streamPortInputClusterOffset{};
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::ClusterIndex> _streamPortOutputClusterOffset{};
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings> _inputMappings{};
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings> _outputMappings{};
};

class RedundantNode : public Node
{
	friend class ModelPrivate;

public:
	static RedundantNode* createOutputNode(EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex);
	static RedundantNode* createInputNode(EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex);

	// Static entity model data
	la::avdecc::controller::model::VirtualIndex const& redundantIndex() const;

	// Cached data from the controller
	TriState lockedState() const; // StreamInput only
	bool isStreaming() const; // StreamOutput only

protected:
	RedundantNode(Type const type, EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex);

	void setLockedState(TriState const lockedState) noexcept; // StreamInput only
	void setIsStreaming(bool const isStreaming) noexcept; // StreamOutput only

protected:
	la::avdecc::controller::model::VirtualIndex const _redundantIndex;
	Node::TriState _lockedState{ Node::TriState::Unknown }; // StreamInput only
	bool _isStreaming{ false }; // StreamOutput only
};

class StreamNode : public Node
{
	friend class ModelPrivate;

public:
	static StreamNode* createRedundantOutputNode(RedundantNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);
	static StreamNode* createRedundantInputNode(RedundantNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);

	static StreamNode* createOutputNode(EntityNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);
	static StreamNode* createInputNode(EntityNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);

	// Static entity model data
	la::avdecc::entity::model::StreamIndex const& streamIndex() const;
	la::avdecc::entity::model::AvbInterfaceIndex const& avbInterfaceIndex() const;

	// Cached data from the controller
	la::avdecc::entity::model::StreamFormat const& streamFormat() const;
	la::avdecc::UniqueIdentifier const& grandMasterID() const;
	std::uint8_t const& grandMasterDomain() const;
	la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const& interfaceLinkStatus() const;
	bool isRunning() const;
	TriState lockedState() const; // StreamInput only
	bool isStreaming() const; // StreamOutput only
	la::avdecc::entity::model::StreamConnectionState const& streamConnectionState() const;

protected:
	StreamNode(Type const type, Node& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);

	void setStreamFormat(la::avdecc::entity::model::StreamFormat const streamFormat);
	void setGrandMasterID(la::avdecc::UniqueIdentifier const grandMasterID);
	void setGrandMasterDomain(std::uint8_t const grandMasterDomain);
	void setInterfaceLinkStatus(la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const interfaceLinkStatus);
	void setRunning(bool isRunning);
	bool setProbingStatus(la::avdecc::entity::model::ProbingStatus const probingStatus); // StreamInput only
	void setMediaLockedCounter(la::avdecc::entity::model::DescriptorCounter const value); // StreamInput only
	void setMediaUnlockedCounter(la::avdecc::entity::model::DescriptorCounter const value); // StreamInput only
	void setStreamStartCounter(la::avdecc::entity::model::DescriptorCounter const value); // StreamOutput only
	void setStreamStopCounter(la::avdecc::entity::model::DescriptorCounter const value); // StreamOutput only
	void setStreamConnectionState(la::avdecc::entity::model::StreamConnectionState const& streamConnectionState);
	void computeLockedState() noexcept;
	void computeIsStreaming() noexcept;

protected:
	la::avdecc::entity::model::StreamIndex const _streamIndex;
	la::avdecc::entity::model::AvbInterfaceIndex const _avbInterfaceIndex;
	la::avdecc::entity::model::StreamFormat _streamFormat{ la::avdecc::entity::model::StreamFormat::getNullStreamFormat() };
	la::avdecc::UniqueIdentifier _grandMasterID;
	std::uint8_t _grandMasterDomain;
	la::avdecc::controller::ControlledEntity::InterfaceLinkStatus _interfaceLinkStatus{ la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Unknown };
	bool _isRunning{ true };
	std::optional<la::avdecc::entity::model::ProbingStatus> _probingStatus{ std::nullopt }; // StreamInput only
	std::optional<la::avdecc::entity::model::DescriptorCounter> _mediaLockedCounter{ std::nullopt }; // StreamInput only
	std::optional<la::avdecc::entity::model::DescriptorCounter> _mediaUnlockedCounter{ std::nullopt }; // StreamInput only
	std::optional<la::avdecc::entity::model::DescriptorCounter> _streamStartCounter{ std::nullopt }; // StreamOutput only
	std::optional<la::avdecc::entity::model::DescriptorCounter> _streamStopCounter{ std::nullopt }; // StreamOutput only
	la::avdecc::entity::model::StreamConnectionState _streamConnectionState{};
	Node::TriState _lockedState{ Node::TriState::Unknown }; // StreamInput only
	bool _isStreaming{ false }; // StreamOutput only
};

class ChannelNode : public Node
{
	friend class ModelPrivate;

public:
	static ChannelNode* createOutputNode(EntityNode& parent, avdecc::ChannelIdentification const& channelIdentification);
	static ChannelNode* createInputNode(EntityNode& parent, avdecc::ChannelIdentification const& channelIdentification);

	// Static entity model data
	avdecc::ChannelIdentification const& channelIdentification() const;

	la::avdecc::entity::model::ClusterIndex clusterIndex() const;
	std::uint16_t channelIndex() const;

protected:
	ChannelNode(Type const type, Node& parent, avdecc::ChannelIdentification const& channelIdentification);

protected:
	avdecc::ChannelIdentification const _channelIdentification;
};

} // namespace connectionMatrix
