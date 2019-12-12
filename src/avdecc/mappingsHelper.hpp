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

#include "mappingMatrix.hpp"

namespace avdecc
{
namespace mappingsHelper
{
struct NodeMapping
{
	la::avdecc::entity::model::DescriptorIndex descriptorIndex{ la::avdecc::entity::model::DescriptorIndex{ 0u } };
	std::vector<std::uint16_t> channels{};
};
using NodeMappings = std::vector<NodeMapping>;
using HashType = std::uint64_t;
using HashedConnectionsList = std::set<HashType>;

std::pair<NodeMappings, mappingMatrix::Nodes> buildClusterMappings(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::StreamPortNode const& streamPortNode);

template<class StreamNodeType>
std::pair<NodeMappings, mappingMatrix::Nodes> buildStreamMappings(la::avdecc::controller::ControlledEntity const* const controlledEntity, std::vector<StreamNodeType const*> const& streamNodes)
{
	NodeMappings streamMappings;
	mappingMatrix::Nodes streamMatrixNodes;

	// Build list of stream mappings
	for (auto const* streamNode : streamNodes)
	{
		auto streamName = avdecc::helper::objectName(controlledEntity, *streamNode).toStdString();
		auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode->dynamicModel->streamFormat);
		NodeMapping nodeMapping{ streamNode->descriptorIndex };
		mappingMatrix::Node node{ streamName };

		for (auto i = 0u; i < sfi->getChannelsCount(); ++i)
		{
			nodeMapping.channels.push_back(i);
			node.sockets.push_back("Channel " + std::to_string(i));
		}
		streamMappings.push_back(std::move(nodeMapping));
		streamMatrixNodes.push_back(std::move(node));
	}

	return std::make_pair(streamMappings, streamMatrixNodes);
}

template<class StreamNodeType>
mappingMatrix::Connections buildConnections(la::avdecc::controller::model::StreamPortNode const& streamPortNode, std::vector<StreamNodeType const*> const& streamNodes, NodeMappings const& streamMappings, NodeMappings const& clusterMappings, std::function<mappingMatrix::Connection(mappingMatrix::SlotID const streamSlotID, mappingMatrix::SlotID const clusterSlotID)> const& creationConnectionFunction)
{
	mappingMatrix::Connections connections;

	// Build list of current connections
	for (auto const& mapping : streamPortNode.dynamicModel->dynamicAudioMap)
	{
		// Find the existing mapping, as mappingMatrix::SlotID
		mappingMatrix::SlotID streamSlotID{ -1, -1 };
		mappingMatrix::SlotID clusterSlotID{ -1, -1 };

		auto streamIndex{ mapping.streamIndex };

#ifdef ENABLE_AVDECC_FEATURE_REDUNDANCY
		// In case of redundancy, we must check the streamIndex in the mapping is the one matching the primary stream
		{
			auto shouldIgnoreMapping = false;
			for (auto const* streamNode : streamNodes)
			{
				if (streamIndex == streamNode->descriptorIndex)
				{
					// Found the index, this is the primary stream (because only primary streams are supposed to be in the streamNodes variable), we need to add this connection
					break;
				}
				else if (streamNode->isRedundant)
				{
					// The stream is a redundant one and not the primary stream, validate the redundant pair index
					for (auto const redundantIndex : streamNode->staticModel->redundantStreams)
					{
						if (streamIndex == redundantIndex)
						{
							// This is the secondary connection for the redundant pair, ignore this mapping
							shouldIgnoreMapping = true;
							break;
						}
					}
					if (shouldIgnoreMapping)
					{
						break;
					}
				}
			}

			// Found the secondary connection for the redundant pair, ignore this mapping
			if (shouldIgnoreMapping)
			{
				continue;
			}
		}
#endif // ENABLE_AVDECC_FEATURE_REDUNDANCY

		{
			int pos = 0;
			// Search for descriptorIndex
			for (auto const& m : streamMappings)
			{
				if (streamIndex == m.descriptorIndex)
				{
					// Now search for channel number
					if (mapping.streamChannel < m.channels.size())
					{
						streamSlotID = { pos, mapping.streamChannel };
					}
					break;
				}
				++pos;
			}
		}
		{
			int pos = 0;
			// Search for descriptorIndex
			for (auto const& m : clusterMappings)
			{
				if (mapping.clusterOffset == m.descriptorIndex)
				{
					// Now search for channel number
					if (mapping.clusterChannel < m.channels.size())
					{
						clusterSlotID = { pos, mapping.clusterChannel };
					}
					break;
				}
				++pos;
			}
		}
		if (streamSlotID.first != -1 && clusterSlotID.first != -1)
		{
			connections.push_back(creationConnectionFunction(streamSlotID, clusterSlotID));
		}
	}

	return connections;
}

HashType makeHash(mappingMatrix::Connection const& connection);

mappingMatrix::Connection unmakeHash(HashType const& hash);

HashedConnectionsList hashConnectionsList(mappingMatrix::Connections const& connections);

HashedConnectionsList substractList(HashedConnectionsList const& a, HashedConnectionsList const& b);

template<la::avdecc::entity::model::DescriptorType StreamPortType>
mappingMatrix::SlotID getStreamSlotIDFromConnection(mappingMatrix::Connection const& connection)
{
	if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
	{
		return connection.first;
	}
	else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
	{
		return connection.second;
	}
	else
	{
		AVDECC_ASSERT(false, "Unsupported StreamPort type");
	}
}

template<la::avdecc::entity::model::DescriptorType StreamPortType>
mappingMatrix::SlotID getClusterSlotIDFromConnection(mappingMatrix::Connection const& connection)
{
	if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
	{
		return connection.second;
	}
	else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
	{
		return connection.first;
	}
	else
	{
		AVDECC_ASSERT(false, "Unsupported StreamPort type");
	}
}

template<la::avdecc::entity::model::DescriptorType StreamPortType>
la::avdecc::entity::model::AudioMapping convertToAudioMapping(NodeMappings const& streamMappings, NodeMappings const& clusterMappings, mappingMatrix::Connection const& connection)
{
	auto const streamSlotID = getStreamSlotIDFromConnection<StreamPortType>(connection);
	auto const clusterSlotID = getClusterSlotIDFromConnection<StreamPortType>(connection);
	auto const& streamMapping = streamMappings[streamSlotID.first];
	auto const& clusterMapping = clusterMappings[clusterSlotID.first];

	return la::avdecc::entity::model::AudioMapping{ streamMapping.descriptorIndex, static_cast<std::uint16_t>(streamSlotID.second), clusterMapping.descriptorIndex, static_cast<std::uint16_t>(clusterSlotID.second) };
}

template<la::avdecc::entity::model::DescriptorType StreamPortType>
la::avdecc::entity::model::AudioMappings convertList(NodeMappings const& streamMappings, NodeMappings const& clusterMappings, HashedConnectionsList const& list)
{
	la::avdecc::entity::model::AudioMappings mappings;

	for (auto const& l : list)
	{
		auto const c = unmakeHash(l);
		mappings.push_back(convertToAudioMapping<StreamPortType>(streamMappings, clusterMappings, c));
	}

	return mappings;
};

template<la::avdecc::entity::model::DescriptorType StreamPortType>
void processNewConnections(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, NodeMappings const& streamMappings, NodeMappings const& clusterMappings, mappingMatrix::Connections const& oldConn, mappingMatrix::Connections const& newConn)
{
	// Build lists of mappings to add/remove
	auto const oldConnections = hashConnectionsList(oldConn);
	auto const newConnections = hashConnectionsList(newConn);

	la::avdecc::entity::model::AudioMappings toRemove{};
	la::avdecc::entity::model::AudioMappings toAdd{};

	if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
	{
		toRemove = convertList<la::avdecc::entity::model::DescriptorType::StreamPortInput>(streamMappings, clusterMappings, substractList(oldConnections, newConnections));
		toAdd = convertList<la::avdecc::entity::model::DescriptorType::StreamPortInput>(streamMappings, clusterMappings, substractList(newConnections, oldConnections));
	}
	else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
	{
		toRemove = convertList<la::avdecc::entity::model::DescriptorType::StreamPortOutput>(streamMappings, clusterMappings, substractList(oldConnections, newConnections));
		toAdd = convertList<la::avdecc::entity::model::DescriptorType::StreamPortOutput>(streamMappings, clusterMappings, substractList(newConnections, oldConnections));
	}

	// Remove and Add the mappings
	auto& manager = avdecc::ControllerManager::getInstance();
	if (!toRemove.empty())
	{
		if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
		{
			manager.removeStreamPortInputAudioMappings(entityID, streamPortIndex, toRemove);
		}
		else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
		{
			manager.removeStreamPortOutputAudioMappings(entityID, streamPortIndex, toRemove);
		}
	}
	if (!toAdd.empty())
	{
		if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
		{
			manager.addStreamPortInputAudioMappings(entityID, streamPortIndex, toAdd);
		}
		else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
		{
			manager.addStreamPortOutputAudioMappings(entityID, streamPortIndex, toAdd);
		}
	}
}

} // namespace mappingsHelper
} // namespace avdecc
