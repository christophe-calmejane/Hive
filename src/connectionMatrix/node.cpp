/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "node.hpp"

#include <hive/modelsLibrary/helper.hpp>

namespace connectionMatrix
{
/* ************************************************************ */
/* Node                                                         */
/* ************************************************************ */
Node::Type Node::type() const noexcept
{
	return _type;
}

bool Node::isOfflineOutputStreamNode() const noexcept
{
	return _type == Type::OfflineOutputStream;
}

bool Node::isEntityNode() const noexcept
{
	return _type == Type::Entity;
}

bool Node::isRedundantNode() const noexcept
{
	switch (_type)
	{
		case Type::RedundantOutput:
		case Type::RedundantInput:
			return true;
		default:
			return false;
	}
}

bool Node::isRedundantStreamNode() const noexcept
{
	switch (_type)
	{
		case Type::RedundantOutputStream:
		case Type::RedundantInputStream:
			return true;
		default:
			return false;
	}
}

bool Node::isStreamNode() const noexcept
{
	switch (_type)
	{
		case Type::OutputStream:
		case Type::InputStream:
		case Type::RedundantOutputStream:
		case Type::RedundantInputStream:
			return true;
		default:
			return false;
	}
}

bool Node::isChannelNode() const noexcept
{
	switch (_type)
	{
		case Type::OutputChannel:
		case Type::InputChannel:
			return true;
		default:
			return false;
	}
}

la::avdecc::UniqueIdentifier const& Node::entityID() const noexcept
{
	return _entityID;
}

Node const* Node::parent() const noexcept
{
	return _parent;
}

Node* Node::parent() noexcept
{
	return _parent;
}

EntityNode const* Node::entityNode() const noexcept
{
	auto* node = this;

	while (node->_parent != nullptr)
	{
		node = node->_parent;
	}

	return static_cast<EntityNode const*>(node);
}

EntityNode* Node::entityNode() noexcept
{
	// Implemented over entityNode() const overload
	return const_cast<EntityNode*>(static_cast<Node const*>(this)->entityNode());
}

bool Node::hasParent() const noexcept
{
	return _parent != nullptr;
}

QString const& Node::name() const noexcept
{
	return _name;
}

bool Node::selected() const noexcept
{
	return _isSelected;
}

void Node::setSelected(bool const isSelected) noexcept
{
	_isSelected = isSelected;
}

int Node::index() const noexcept
{
	return _parent ? _parent->indexOf(this) : 0;
}

int Node::indexOf(Node const* child) const noexcept
{
	auto const predicate = [child](auto const& item)
	{
		return item.get() == child;
	};
	auto const it = std::find_if(std::begin(_children), std::end(_children), predicate);
	if (!AVDECC_ASSERT_WITH_RET(it != std::end(_children), "not found"))
	{
		return -1;
	}
	auto const index = std::distance(std::begin(_children), it);
	return static_cast<int>(index);
}

Node* Node::childAt(int index) noexcept
{
	if (index < 0 || index >= childrenCount())
	{
		return nullptr;
	}

	return _children.at(index).get();
}

Node const* Node::childAt(int index) const noexcept
{
	if (index < 0 || index >= childrenCount())
	{
		return nullptr;
	}

	return _children.at(index).get();
}

int Node::childrenCount() const noexcept
{
	return static_cast<int>(_children.size());
}

Node::Node(Type const type, la::avdecc::UniqueIdentifier const& entityID, Node* parent) noexcept
	: _type{ type }
	, _entityID{ entityID }
	, _parent{ parent }
	, _name{ hive::modelsLibrary::helper::uniqueIdentifierToString(entityID) }
{
	if (_parent)
	{
		_parent->_children.emplace_back(this);
	}
}

void Node::setName(QString const& name) noexcept
{
	_name = name;
}

OfflineOutputStreamNode* OfflineOutputStreamNode::create() noexcept
{
	return new OfflineOutputStreamNode{};
}

OfflineOutputStreamNode::OfflineOutputStreamNode() noexcept
	: Node{ Type::OfflineOutputStream, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), nullptr }
{
	setName("Offline Streams");
}

/* ************************************************************ */
/* EntityNode                                                   */
/* ************************************************************ */
EntityNode* EntityNode::create(la::avdecc::UniqueIdentifier const& entityID, bool const isMilan) noexcept
{
	return new EntityNode{ entityID, isMilan };
}

void EntityNode::accept(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, AvbInterfaceIndexVisitor const& visitor) const noexcept
{
	Node::accept(
		[&avbInterfaceIndex, &visitor](Node* node)
		{
			if (node->isStreamNode())
			{
				auto* streamNode = static_cast<StreamNode*>(node);
				if (streamNode->avbInterfaceIndex() == avbInterfaceIndex || avbInterfaceIndex == la::avdecc::entity::Entity::GlobalAvbInterfaceIndex)
				{
					visitor(streamNode);
				}
			}
		});
}

EntityNode::EntityNode(la::avdecc::UniqueIdentifier const& entityID, bool const isMilan) noexcept
	: Node{ Type::Entity, entityID, nullptr }
	, _isMilan{ isMilan }
{
}

void EntityNode::setStreamPortInputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterOffset) noexcept
{
	_streamPortInputClusterOffset[streamPortIndex] = clusterOffset;
}

void EntityNode::setStreamPortOutputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterOffset) noexcept
{
	_streamPortOutputClusterOffset[streamPortIndex] = clusterOffset;
}

void EntityNode::setInputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortInputIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept
{
	_inputMappings[streamPortInputIndex] = mappings;
}

void EntityNode::setOutputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortOutputIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept
{
	_outputMappings[streamPortOutputIndex] = mappings;
}

bool EntityNode::isMilan() const noexcept
{
	return _isMilan;
}

la::avdecc::entity::model::ClusterIndex EntityNode::getStreamPortInputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex) const
{
	if (auto const it = _streamPortInputClusterOffset.find(streamPortIndex); it != _streamPortInputClusterOffset.end())
	{
		return it->second;
	}
	throw std::invalid_argument("Invalid StreamPortIndex");
}

la::avdecc::entity::model::ClusterIndex EntityNode::getStreamPortOutputClusterOffset(la::avdecc::entity::model::StreamPortIndex const streamPortIndex) const
{
	if (auto const it = _streamPortOutputClusterOffset.find(streamPortIndex); it != _streamPortOutputClusterOffset.end())
	{
		return it->second;
	}
	throw std::invalid_argument("Invalid StreamPortIndex");
}

la::avdecc::entity::model::AudioMappings const& EntityNode::getInputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortInputIndex) const
{
	if (auto const it = _inputMappings.find(streamPortInputIndex); it != _inputMappings.end())
	{
		return it->second;
	}
	throw std::invalid_argument("Invalid StreamPortIndex");
}

la::avdecc::entity::model::AudioMappings const& EntityNode::getOutputAudioMappings(la::avdecc::entity::model::StreamPortIndex const streamPortOutputIndex) const
{
	if (auto const it = _outputMappings.find(streamPortOutputIndex); it != _outputMappings.end())
	{
		return it->second;
	}
	throw std::invalid_argument("Invalid StreamPortIndex");
}

std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings> EntityNode::getInputAudioMappings() const noexcept
{
	return _inputMappings;
}

std::unordered_map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings> EntityNode::getOutputAudioMappings() const noexcept
{
	return _outputMappings;
}

/* ************************************************************ */
/* RedundantNode                                                */
/* ************************************************************ */
RedundantNode* RedundantNode::createOutputNode(EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept
{
	return new RedundantNode{ Type::RedundantOutput, parent, redundantIndex };
}

RedundantNode* RedundantNode::createInputNode(EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept
{
	return new RedundantNode{ Type::RedundantInput, parent, redundantIndex };
}

la::avdecc::controller::model::VirtualIndex const& RedundantNode::redundantIndex() const noexcept
{
	return _redundantIndex;
}

Node::TriState RedundantNode::lockedState() const noexcept
{
	return _lockedState;
}

bool RedundantNode::isStreaming() const noexcept
{
	return _isStreaming;
}

void RedundantNode::setLockedState(TriState const lockedState) noexcept
{
	_lockedState = lockedState;
}

void RedundantNode::setIsStreaming(bool const isStreaming) noexcept
{
	_isStreaming = isStreaming;
}

RedundantNode::RedundantNode(Type const type, EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept
	: Node{ type, parent.entityID(), &parent }
	, _redundantIndex{ redundantIndex }
{
}

/* ************************************************************ */
/* StreamNode                                                   */
/* ************************************************************ */
StreamNode* StreamNode::createRedundantOutputNode(RedundantNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept
{
	return new StreamNode{ Type::RedundantOutputStream, parent, streamIndex, avbInterfaceIndex };
}

StreamNode* StreamNode::createRedundantInputNode(RedundantNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept
{
	return new StreamNode{ Type::RedundantInputStream, parent, streamIndex, avbInterfaceIndex };
}

StreamNode* StreamNode::createOutputNode(EntityNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept
{
	return new StreamNode{ Type::OutputStream, parent, streamIndex, avbInterfaceIndex };
}

StreamNode* StreamNode::createInputNode(EntityNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept
{
	return new StreamNode{ Type::InputStream, parent, streamIndex, avbInterfaceIndex };
}

la::avdecc::entity::model::StreamIndex const& StreamNode::streamIndex() const noexcept
{
	return _streamIndex;
}

la::avdecc::entity::model::AvbInterfaceIndex const& StreamNode::avbInterfaceIndex() const noexcept
{
	return _avbInterfaceIndex;
}

la::avdecc::entity::model::StreamFormat StreamNode::streamFormat() const noexcept
{
	return _streamFormat;
}

la::avdecc::entity::model::StreamFormats const& StreamNode::streamFormats() const noexcept
{
	return _streamFormats;
}

la::avdecc::UniqueIdentifier const& StreamNode::grandMasterID() const noexcept
{
	return _grandMasterID;
}

std::uint8_t const& StreamNode::grandMasterDomain() const noexcept
{
	return _grandMasterDomain;
}

la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const& StreamNode::interfaceLinkStatus() const noexcept
{
	return _interfaceLinkStatus;
}

bool StreamNode::isRunning() const noexcept
{
	return _isRunning;
}

Node::TriState StreamNode::lockedState() const noexcept
{
	return _lockedState;
}

bool StreamNode::isLatencyError() const noexcept
{
	return _isLatencyError;
}

bool StreamNode::isStreaming() const noexcept
{
	return _isStreaming;
}

la::avdecc::entity::model::StreamInputConnectionInfo const& StreamNode::streamInputConnectionInformation() const noexcept
{
	return _streamInputConnectionInfo;
}

StreamNode::StreamNode(Type const type, Node& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept
	: Node{ type, parent.entityID(), &parent }
	, _streamIndex{ streamIndex }
	, _avbInterfaceIndex{ avbInterfaceIndex }
{
}

void StreamNode::setStreamFormat(la::avdecc::entity::model::StreamFormat const streamFormat) noexcept
{
	_streamFormat = streamFormat;
}

void StreamNode::setStreamFormats(la::avdecc::entity::model::StreamFormats const& streamFormats) noexcept
{
	_streamFormats = streamFormats;
}

void StreamNode::setGrandMasterID(la::avdecc::UniqueIdentifier const grandMasterID) noexcept
{
	_grandMasterID = grandMasterID;
}

void StreamNode::setGrandMasterDomain(std::uint8_t const grandMasterDomain) noexcept
{
	_grandMasterDomain = grandMasterDomain;
}

void StreamNode::setInterfaceLinkStatus(la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const interfaceLinkStatus) noexcept
{
	_interfaceLinkStatus = interfaceLinkStatus;
}

void StreamNode::setRunning(bool isRunning) noexcept
{
	_isRunning = isRunning;
}

bool StreamNode::setProbingStatus(la::avdecc::entity::model::ProbingStatus const probingStatus) noexcept
{
	auto const changed = _probingStatus != probingStatus;
	_probingStatus = probingStatus;
	return changed;
}

bool StreamNode::setMediaLockedCounter(la::avdecc::entity::model::DescriptorCounter const value) noexcept
{
	auto const changed = _mediaLockedCounter != value;
	_mediaLockedCounter = value;
	return changed;
}

bool StreamNode::setMediaUnlockedCounter(la::avdecc::entity::model::DescriptorCounter const value) noexcept
{
	auto const changed = _mediaUnlockedCounter != value;
	_mediaUnlockedCounter = value;
	return changed;
}

bool StreamNode::setLatencyError(bool const isLatencyError) noexcept
{
	auto const changed = _isLatencyError != isLatencyError;
	_isLatencyError = isLatencyError;
	return changed;
}

bool StreamNode::setStreamStartCounter(la::avdecc::entity::model::DescriptorCounter const value) noexcept
{
	auto const changed = _streamStartCounter != value;
	_streamStartCounter = value;
	return changed;
}

bool StreamNode::setStreamStopCounter(la::avdecc::entity::model::DescriptorCounter const value) noexcept
{
	auto const changed = _streamStopCounter != value;
	_streamStopCounter = value;
	return changed;
}

void StreamNode::setStreamInputConnectionInformation(la::avdecc::entity::model::StreamInputConnectionInfo const& info) noexcept
{
	_streamInputConnectionInfo = info;
}

void StreamNode::computeLockedState() noexcept
{
	// Only if connected
	if (_streamInputConnectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected)
	{
		// If we have ProbingStatus it must be Completed
		if (!_probingStatus || (*_probingStatus == la::avdecc::entity::model::ProbingStatus::Completed))
		{
			// Only if both counters have a valid value
			if (_mediaLockedCounter && _mediaUnlockedCounter)
			{
				if (*_mediaLockedCounter == (*_mediaUnlockedCounter + 1))
				{
					_lockedState = TriState::True;
					return;
				}
				_lockedState = TriState::False;
				return;
			}
		}
	}

	_lockedState = TriState::Unknown;
	return;
}

void StreamNode::computeIsStreaming() noexcept
{
	// Only if both counters have a valid value
	if (_streamStartCounter && _streamStopCounter)
	{
		if (*_streamStartCounter == (*_streamStopCounter + 1))
		{
			_isStreaming = true;
			return;
		}
	}

	_isStreaming = false;
	return;
}

/* ************************************************************ */
/* ChannelNode                                                  */
/* ************************************************************ */
ChannelNode* ChannelNode::createOutputNode(EntityNode& parent, avdecc::ChannelIdentification const& channelIdentification) noexcept
{
	return new ChannelNode{ Type::OutputChannel, parent, channelIdentification };
}

ChannelNode* ChannelNode::createInputNode(EntityNode& parent, avdecc::ChannelIdentification const& channelIdentification) noexcept
{
	return new ChannelNode{ Type::InputChannel, parent, channelIdentification };
}

avdecc::ChannelIdentification const& ChannelNode::channelIdentification() const noexcept
{
	return _channelIdentification;
}

la::avdecc::entity::model::ClusterIndex ChannelNode::clusterIndex() const noexcept
{
	return _channelIdentification.clusterIndex;
}

std::uint16_t ChannelNode::channelIndex() const noexcept
{
	return _channelIdentification.clusterChannel;
}

ChannelNode::ChannelNode(Type const type, Node& parent, avdecc::ChannelIdentification const& channelIdentification) noexcept
	: Node{ type, parent.entityID(), &parent }
	, _channelIdentification{ channelIdentification }
{
}

/* ************************************************************ */
/* Node Policies                                                */
/* ************************************************************ */
bool Node::CompleteHierarchyPolicy::shouldVisit(Node const* const) noexcept
{
	return true;
}

bool Node::StreamHierarchyPolicy::shouldVisit(Node const* const node) noexcept
{
	return node->isOfflineOutputStreamNode() || node->isEntityNode() || node->isRedundantNode() || node->isStreamNode();
}

bool Node::StreamPolicy::shouldVisit(Node const* const node) noexcept
{
	return node->isStreamNode();
}

bool Node::ChannelHierarchyPolicy::shouldVisit(Node const* const node) noexcept
{
	if (node->isEntityNode())
	{
		auto const* const entityNode = static_cast<EntityNode const*>(node);
		// Only accept Milan Entities
		return entityNode->isMilan();
	}
	else
	{
		return node->isChannelNode();
	}
}

bool Node::ChannelPolicy::shouldVisit(Node const* const node) noexcept
{
	return node->isChannelNode();
}

} // namespace connectionMatrix
