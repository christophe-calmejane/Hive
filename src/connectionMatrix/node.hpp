/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

		OfflineOutputStream,

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
	Type type() const noexcept;

	// Returns true if node type is OfflineOutputStream
	bool isOfflineOutputStreamNode() const noexcept;

	// Returns true if node type is Entity
	bool isEntityNode() const noexcept;

	// Returns true if node type is either RedundantOutput or RedundantInput
	bool isRedundantNode() const noexcept;

	// Returns true if node type is either RedundantOutputStream or RedundantInputStream
	bool isRedundantStreamNode() const noexcept;

	// Returns true if node type is either RedundantOutputStream, RedundantInputStream, OutputStream or InputStream
	bool isStreamNode() const noexcept;

	// Returns true if node type is either OutputChannel or InputChannel
	bool isChannelNode() const noexcept;

	// Returns the entity ID
	la::avdecc::UniqueIdentifier const& entityID() const noexcept;

	// Returns the parent node
	Node const* parent() const noexcept;

	// Returns the parent node
	Node* parent() noexcept;

	// Returns the EntityNode (top ancestor)
	EntityNode const* entityNode() const noexcept;

	// Returns the EntityNode (top ancestor)
	EntityNode* entityNode() noexcept;

	// Returns true if this node has a parent (false for an entity)
	bool hasParent() const noexcept;

	// Returns the name of this node (entity name, stream name, .. etc)
	QString const& name() const noexcept;

	// Returns true if the node is "selected"
	bool selected() const noexcept;

	// Change the selected state
	void setSelected(bool const isSelected) noexcept;

	// Returns the index of the node in its parent childen
	int index() const noexcept;

	// Returns the index child in this node children list, -1 if the child is not related
	int indexOf(Node const* child) const noexcept;

	// Returns the child node at index, null if not found
	Node* childAt(int index) noexcept;

	// Returns the child node at index, null if not found
	Node const* childAt(int index) const noexcept;

	// Returns the number of children
	int childrenCount() const noexcept;

	// Returns the read-only list of children
	std::vector<std::unique_ptr<Node>> const& children() const noexcept;

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
	void accept(Visitor const& visitor, bool const childrenOnly = false) const noexcept
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
	Node(Type const type, la::avdecc::UniqueIdentifier const& entityID, Node* parent) noexcept;

	void setName(QString const& name) noexcept;

protected:
	// Node type
	Type const _type;

	// Associated entity ID
	la::avdecc::UniqueIdentifier const _entityID;

	// Pointer to the parent node (should be null for entities only)
	Node* const _parent;

	// Node name
	QString _name;

	// Is Selected
	bool _isSelected{ false };

	// Holds the children
	std::vector<std::unique_ptr<Node>> _children;
};

class OfflineOutputStreamNode : public Node
{
	friend class ModelPrivate;

public:
	static OfflineOutputStreamNode* create() noexcept;

protected:
	OfflineOutputStreamNode() noexcept;
};

class EntityNode : public Node
{
	friend class ModelPrivate;

public:
	static EntityNode* create(la::avdecc::UniqueIdentifier const& entityID, bool const isMilan, bool const isRegisteredUnsol) noexcept;

	// Visitor pattern that is called on every stream node that matches avbInterfaceIndex
	using AvbInterfaceIndexVisitor = std::function<void(class StreamNode*)>;
	void accept(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, AvbInterfaceIndexVisitor const& visitor) const noexcept;

	bool isMilan() const noexcept;
	bool isRegisteredUnsol() const noexcept;
	la::avdecc::entity::model::ClusterIndex getStreamPortInputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex) const;
	la::avdecc::entity::model::ClusterIndex getStreamPortOutputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex) const;
	la::avdecc::entity::model::AudioMappings const& getInputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortInputIndex) const;
	la::avdecc::entity::model::AudioMappings const& getOutputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortOutputIndex) const;
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings> getInputAudioMappings() const noexcept;
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings> getOutputAudioMappings() const noexcept;

protected:
	EntityNode(la::avdecc::UniqueIdentifier const& entityID, bool const isMilan, bool const isRegisteredUnsol) noexcept;
	void setRegisteredUnsol(bool const isRegisteredUnsol) noexcept;
	void setStreamPortInputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterOffset) noexcept;
	void setStreamPortOutputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterOffset) noexcept;
	void setInputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortInputIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept;
	void setOutputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortOutputIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept;

protected:
	bool _isMilan{ false };
	bool _isRegisteredUnsol{ false };
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::ClusterIndex> _streamPortInputClusterOffset{};
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::ClusterIndex> _streamPortOutputClusterOffset{};
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings> _inputMappings{};
	std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings> _outputMappings{};
};

class RedundantNode : public Node
{
	friend class ModelPrivate;

public:
	static RedundantNode* createOutputNode(EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept;
	static RedundantNode* createInputNode(EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept;

	// Static entity model data
	la::avdecc::controller::model::VirtualIndex const& redundantIndex() const noexcept;

	// Cached data from the controller
	TriState lockedState() const noexcept; // StreamInput only
	bool isStreaming() const noexcept; // StreamOutput only

protected:
	RedundantNode(Type const type, EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept;

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
	static StreamNode* createRedundantOutputNode(RedundantNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept;
	static StreamNode* createRedundantInputNode(RedundantNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept;

	static StreamNode* createOutputNode(EntityNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept;
	static StreamNode* createInputNode(EntityNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept;

	// Static entity model data
	la::avdecc::entity::model::StreamIndex const& streamIndex() const noexcept;
	la::avdecc::entity::model::AvbInterfaceIndex const& avbInterfaceIndex() const noexcept;

	// Cached data from the controller
	la::avdecc::entity::model::StreamFormat streamFormat() const noexcept;
	la::avdecc::entity::model::StreamFormats const& streamFormats() const noexcept;
	la::avdecc::UniqueIdentifier const& grandMasterID() const noexcept;
	std::uint8_t const& grandMasterDomain() const noexcept;
	la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const& interfaceLinkStatus() const noexcept;
	bool isRunning() const noexcept;
	TriState lockedState() const noexcept; // StreamInput only
	bool isLatencyError() const noexcept; // StreamInput only
	bool isStreaming() const noexcept; // StreamOutput only
	la::avdecc::entity::model::StreamInputConnectionInfo const& streamInputConnectionInformation() const noexcept;

protected:
	StreamNode(Type const type, Node& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept;

	void setStreamFormat(la::avdecc::entity::model::StreamFormat const streamFormat) noexcept;
	void setStreamFormats(la::avdecc::entity::model::StreamFormats const& streamFormat) noexcept;
	void setGrandMasterID(la::avdecc::UniqueIdentifier const grandMasterID) noexcept;
	void setGrandMasterDomain(std::uint8_t const grandMasterDomain) noexcept;
	void setInterfaceLinkStatus(la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const interfaceLinkStatus) noexcept;
	void setRunning(bool isRunning) noexcept;
	bool setProbingStatus(la::avdecc::entity::model::ProbingStatus const probingStatus) noexcept; // StreamInput only
	bool setMediaLockedCounter(la::avdecc::entity::model::DescriptorCounter const value) noexcept; // StreamInput only
	bool setMediaUnlockedCounter(la::avdecc::entity::model::DescriptorCounter const value) noexcept; // StreamInput only
	bool setLatencyError(bool const isLatencyError) noexcept; // StreamInput only
	bool setStreamStartCounter(la::avdecc::entity::model::DescriptorCounter const value) noexcept; // StreamOutput only
	bool setStreamStopCounter(la::avdecc::entity::model::DescriptorCounter const value) noexcept; // StreamOutput only
	void setStreamInputConnectionInformation(la::avdecc::entity::model::StreamInputConnectionInfo const& info) noexcept;
	void computeLockedState() noexcept;
	void computeIsStreaming() noexcept;

protected:
	la::avdecc::entity::model::StreamIndex const _streamIndex;
	la::avdecc::entity::model::AvbInterfaceIndex const _avbInterfaceIndex;
	la::avdecc::entity::model::StreamFormat _streamFormat{ la::avdecc::entity::model::StreamFormat::getNullStreamFormat() };
	la::avdecc::entity::model::StreamFormats _streamFormats{};
	la::avdecc::UniqueIdentifier _grandMasterID;
	std::uint8_t _grandMasterDomain;
	la::avdecc::controller::ControlledEntity::InterfaceLinkStatus _interfaceLinkStatus{ la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Unknown };
	bool _isRunning{ true };
	std::optional<la::avdecc::entity::model::ProbingStatus> _probingStatus{ std::nullopt }; // StreamInput only
	std::optional<la::avdecc::entity::model::DescriptorCounter> _mediaLockedCounter{ std::nullopt }; // StreamInput only
	std::optional<la::avdecc::entity::model::DescriptorCounter> _mediaUnlockedCounter{ std::nullopt }; // StreamInput only
	bool _isLatencyError{ false }; // StreamInput only
	std::optional<la::avdecc::entity::model::DescriptorCounter> _streamStartCounter{ std::nullopt }; // StreamOutput only
	std::optional<la::avdecc::entity::model::DescriptorCounter> _streamStopCounter{ std::nullopt }; // StreamOutput only
	la::avdecc::entity::model::StreamInputConnectionInfo _streamInputConnectionInfo{};
	Node::TriState _lockedState{ Node::TriState::Unknown }; // StreamInput only
	bool _isStreaming{ false }; // StreamOutput only
};

class ChannelNode : public Node
{
	friend class ModelPrivate;

public:
	static ChannelNode* createOutputNode(EntityNode& parent, avdecc::ChannelIdentification const& channelIdentification) noexcept;
	static ChannelNode* createInputNode(EntityNode& parent, avdecc::ChannelIdentification const& channelIdentification) noexcept;

	// Static entity model data
	avdecc::ChannelIdentification const& channelIdentification() const noexcept;

	la::avdecc::entity::model::ClusterIndex clusterIndex() const noexcept;
	std::uint16_t channelIndex() const noexcept;

protected:
	ChannelNode(Type const type, Node& parent, avdecc::ChannelIdentification const& channelIdentification) noexcept;

protected:
	avdecc::ChannelIdentification const _channelIdentification;
};

} // namespace connectionMatrix
