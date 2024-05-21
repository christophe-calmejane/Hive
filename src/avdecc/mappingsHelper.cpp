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

#include "mappingsHelper.hpp"

#include <la/avdecc/utils.hpp>
#include <la/avdecc/internals/protocolAemPayloadSizes.hpp>

#include <QMessageBox>

#include <map>
#include <utility>

namespace avdecc
{
namespace mappingsHelper
{
struct StreamNodeMapping
{
	la::avdecc::entity::model::StreamIndex streamIndex{ la::avdecc::entity::model::getInvalidDescriptorIndex() };
	std::vector<std::uint16_t> channels{};

	// Comparison operator
	friend bool operator==(StreamNodeMapping const& lhs, StreamNodeMapping const& rhs)
	{
		return lhs.streamIndex == rhs.streamIndex && lhs.channels == rhs.channels;
	}
};

struct ClusterNodeMapping
{
	la::avdecc::entity::model::StreamPortIndex streamPortIndex{ la::avdecc::entity::model::getInvalidDescriptorIndex() };
	la::avdecc::entity::model::ClusterIndex clusterOffset{ la::avdecc::entity::model::getInvalidDescriptorIndex() }; // Offset from baseCluster
	std::vector<std::uint16_t> channels{};

	// Comparison operator
	friend bool operator==(ClusterNodeMapping const& lhs, ClusterNodeMapping const& rhs)
	{
		return lhs.streamPortIndex == rhs.streamPortIndex && lhs.clusterOffset == rhs.clusterOffset && lhs.channels == rhs.channels;
	}
};

struct StreamInputNodeUserData
{
	bool isConnected{ false };
};

struct StreamOutputNodeUserData
{
	bool isStreaming{ false };
};

using StreamNodeMappings = std::vector<StreamNodeMapping>; // List of Flow Nodes, with the corresponding Stream data
using ClusterNodeMappings = std::vector<ClusterNodeMapping>; // List of Flow Nodes, with the corresponding Cluster data
using HashType = std::uint64_t;
using HashedConnectionsList = std::set<HashType>;
using StreamPortMappings = std::map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings>;

HashType makeHash(mappingMatrix::Connection const& connection)
{
	return (static_cast<HashType>(connection.first.first & 0xFFFF) << 48) + (static_cast<HashType>(connection.first.second & 0xFFFF) << 32) + (static_cast<HashType>(connection.second.first & 0xFFFF) << 16) + static_cast<HashType>(connection.second.second & 0xFFFF);
}

mappingMatrix::Connection unmakeHash(HashType const& hash)
{
	mappingMatrix::Connection connection;

	connection.first.first = (hash >> 48) & 0xFFFF;
	connection.first.second = (hash >> 32) & 0xFFFF;
	connection.second.first = (hash >> 16) & 0xFFFF;
	connection.second.second = hash & 0xFFFF;

	return connection;
}

HashedConnectionsList hashConnectionsList(mappingMatrix::Connections const& connections)
{
	HashedConnectionsList list;

	for (auto const& c : connections)
	{
		list.insert(makeHash(c));
	}

	return list;
}

HashedConnectionsList substractList(HashedConnectionsList const& a, HashedConnectionsList const& b)
{
	HashedConnectionsList sub{ a };

	for (auto const& v : b)
	{
		sub.erase(v);
	}

	return sub;
}

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
void setStreamSlotIDInConnection(mappingMatrix::Connection& connection, mappingMatrix::SlotID const& slotID)
{
	if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
	{
		connection.first = slotID;
	}
	else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
	{
		connection.second = slotID;
	}
	else
	{
		AVDECC_ASSERT(false, "Unsupported StreamPort type");
	}
}

template<la::avdecc::entity::model::DescriptorType StreamPortType>
void setClusterSlotIDInConnection(mappingMatrix::Connection& connection, mappingMatrix::SlotID const& slotID)
{
	if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
	{
		connection.second = slotID;
	}
	else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
	{
		connection.first = slotID;
	}
	else
	{
		AVDECC_ASSERT(false, "Unsupported StreamPort type");
	}
}

template<la::avdecc::entity::model::DescriptorType StreamPortType>
mappingMatrix::Connection convertFromAudioMapping(StreamNodeMappings const& streamMappings, ClusterNodeMappings const& clusterMappings, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMapping const& mapping) noexcept
{
	auto streamSlotID = mappingMatrix::SlotID{ -1, -1 };
	auto clusterSlotID = mappingMatrix::SlotID{ -1, -1 };

	// Try to find StreamSlotID
	{
		auto pos = mappingMatrix::SlotID::first_type{ 0 };
		// Search for matching StreamIndex
		for (auto const& m : streamMappings)
		{
			if (mapping.streamIndex == m.streamIndex)
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
	// Try to find ClusterSlotID
	{
		auto pos = mappingMatrix::SlotID::first_type{ 0 };
		// Search for matching StreamPortIndex and ClusterOffset
		for (auto const& m : clusterMappings)
		{
			if (streamPortIndex == m.streamPortIndex && mapping.clusterOffset == m.clusterOffset)
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
		auto connection = mappingMatrix::Connection{};
		setStreamSlotIDInConnection<StreamPortType>(connection, streamSlotID);
		setClusterSlotIDInConnection<StreamPortType>(connection, clusterSlotID);
		return connection;
	}

	return mappingMatrix::Connection{ { -1, -1 }, { -1, -1 } };
}

template<la::avdecc::entity::model::DescriptorType StreamPortType>
std::pair<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMapping> convertToAudioMapping(StreamNodeMappings const& streamMappings, ClusterNodeMappings const& clusterMappings, mappingMatrix::Connection const& connection) noexcept
{
	auto const streamSlotID = getStreamSlotIDFromConnection<StreamPortType>(connection);
	auto const clusterSlotID = getClusterSlotIDFromConnection<StreamPortType>(connection);
	auto const& streamMapping = streamMappings[streamSlotID.first];
	auto const& clusterMapping = clusterMappings[clusterSlotID.first];

	return std::make_pair(clusterMapping.streamPortIndex, la::avdecc::entity::model::AudioMapping{ streamMapping.streamIndex, static_cast<std::uint16_t>(streamSlotID.second), clusterMapping.clusterOffset, static_cast<std::uint16_t>(clusterSlotID.second) });
}

template<la::avdecc::entity::model::DescriptorType StreamPortType>
StreamPortMappings convertList(StreamNodeMappings const& streamMappings, ClusterNodeMappings const& clusterMappings, HashedConnectionsList const& list)
{
	auto mappings = StreamPortMappings{};

	for (auto const& l : list)
	{
		auto const c = unmakeHash(l);
		auto [streamPortIndex, mapping] = convertToAudioMapping<StreamPortType>(streamMappings, clusterMappings, c);
		mappings[streamPortIndex].emplace_back(std::move(mapping));
	}

	return mappings;
};

template<class StreamNodeType>
void buildStreamMappings(la::avdecc::controller::ControlledEntity const* const controlledEntity, std::vector<std::pair<std::string, StreamNodeType const*>> streamNodes, StreamNodeMappings& streamMappings, mappingMatrix::Nodes& streamMatrixNodes)
{
	// Build list of stream mappings
	for (auto const& [streamName, streamNode] : streamNodes)
	{
		auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode->dynamicModel.streamFormat);
		StreamNodeMapping nodeMapping{ streamNode->descriptorIndex };
		mappingMatrix::Node node{ streamName };

		for (auto i = 0u; i < sfi->getChannelsCount(); ++i)
		{
			nodeMapping.channels.push_back(i);
			node.sockets.push_back("Channel " + std::to_string(i));
		}
		if constexpr (std::is_same_v<StreamNodeType, la::avdecc::controller::model::StreamInputNode>)
		{
			node.userData = StreamInputNodeUserData{ streamNode->dynamicModel.connectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected };
		}
		else
		{
			auto userData = StreamOutputNodeUserData{};
			if (streamNode->dynamicModel.counters)
			{
				try
				{
					auto const& counters = *streamNode->dynamicModel.counters;
					auto const startValue = counters.at(la::avdecc::entity::StreamOutputCounterValidFlag::StreamStart);
					auto const stopValue = counters.at(la::avdecc::entity::StreamOutputCounterValidFlag::StreamStop);
					userData.isStreaming = startValue > stopValue;
				}
				catch (std::out_of_range const&)
				{
					// Ignore
				}
			}
			node.userData = std::move(userData);
		}
		// Before adding, search if not already present
		if (std::find(streamMappings.begin(), streamMappings.end(), nodeMapping) == streamMappings.end())
		{
			streamMappings.push_back(std::move(nodeMapping));
			streamMatrixNodes.push_back(std::move(node));
		}
	}
}

template<la::avdecc::entity::model::DescriptorType StreamPortType, class StreamNodeType>
void buildConnections(la::avdecc::controller::model::StreamPortNode const& streamPortNode, std::vector<std::pair<std::string, StreamNodeType const*>> streamNodes, StreamNodeMappings const& streamMappings, ClusterNodeMappings const& clusterMappings, mappingMatrix::Connections& connections)
{
	// Build list of current connections
	for (auto const& mapping : streamPortNode.dynamicModel.dynamicAudioMap)
	{
		// Find the existing mapping, as mappingMatrix::SlotID
		mappingMatrix::SlotID streamSlotID{ -1, -1 };
		mappingMatrix::SlotID clusterSlotID{ -1, -1 };

		auto streamIndex{ mapping.streamIndex };

#ifdef ENABLE_AVDECC_FEATURE_REDUNDANCY
		// In case of redundancy, we must check the streamIndex in the mapping is the one matching the primary stream
		{
			auto shouldIgnoreMapping = false;
			for (auto const& [_, streamNode] : streamNodes)
			{
				if (streamIndex == streamNode->descriptorIndex)
				{
					// Found the index, this is the primary stream (because only primary streams are supposed to be in the streamNodes variable), we need to add this connection
					break;
				}
				else if (streamNode->isRedundant)
				{
					// The stream is a redundant one and not the primary stream, validate the redundant pair index
					for (auto const redundantIndex : streamNode->staticModel.redundantStreams)
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

		auto connection = convertFromAudioMapping<StreamPortType>(streamMappings, clusterMappings, streamPortNode.descriptorIndex, mapping);
		if (connection.first.first == -1 || connection.second.first == -1)
		{
			continue;
		}
		connections.emplace_back(std::move(connection));
	}
}

template<la::avdecc::entity::model::DescriptorType StreamPortType>
void processNewConnections(la::avdecc::UniqueIdentifier const entityID, StreamNodeMappings const& streamMappings, ClusterNodeMappings const& clusterMappings, mappingMatrix::Connections const& oldConn, mappingMatrix::Connections const& newConn)
{
	// Build lists of mappings to add/remove
	auto const oldConnections = hashConnectionsList(oldConn);
	auto const newConnections = hashConnectionsList(newConn);

	auto toRemove = StreamPortMappings{};
	auto toAdd = StreamPortMappings{};

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
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	if (!toRemove.empty())
	{
		for (auto const& [streamPortIndex, mappings] : toRemove)
		{
			auto const countMappings = mappings.size();
			auto offset = decltype(countMappings){ 0u };
			while (offset < countMappings)
			{
				auto const m = getMaximumAudioMappings(mappings, offset);
				auto const count = m.size();
				if (!AVDECC_ASSERT_WITH_RET(count != 0, "Should have at least one mapping to change"))
				{
					break;
				}
				offset += count;

				if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
				{
					manager.removeStreamPortInputAudioMappings(entityID, streamPortIndex, m);
				}
				else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
				{
					manager.removeStreamPortOutputAudioMappings(entityID, streamPortIndex, m);
				}
			}
		}
	}
	if (!toAdd.empty())
	{
		for (auto const& [streamPortIndex, mappings] : toAdd)
		{
			auto const countMappings = mappings.size();
			auto offset = decltype(countMappings){ 0u };
			while (offset < countMappings)
			{
				auto const m = getMaximumAudioMappings(mappings, offset);
				auto const count = m.size();
				if (!AVDECC_ASSERT_WITH_RET(count != 0, "Should have at least one mapping to change"))
				{
					break;
				}
				offset += count;

				if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
				{
					manager.addStreamPortInputAudioMappings(entityID, streamPortIndex, m);
				}
				else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
				{
					manager.addStreamPortOutputAudioMappings(entityID, streamPortIndex, m);
				}
			}
		}
	}
}

void buildClusterMappings(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::StreamPortNode const& streamPortNode, ClusterNodeMappings& clusterMappings, mappingMatrix::Nodes& clusterMatrixNodes)
{
	// Build list of cluster mappings
	for (auto const& clusterKV : streamPortNode.audioClusters)
	{
		auto const clusterOffset = static_cast<la::avdecc::entity::model::ClusterIndex>(clusterKV.first - streamPortNode.staticModel.baseCluster); // Mappings use relative index (see IEEE1722.1 Table 7.33)
		AVDECC_ASSERT(clusterOffset < streamPortNode.staticModel.numberOfClusters, "ClusterOffset invalid");
		auto const& clusterNode = clusterKV.second;
		auto clusterName = hive::modelsLibrary::helper::objectName(controlledEntity, clusterNode).toStdString();
		ClusterNodeMapping nodeMapping{ streamPortNode.descriptorIndex, clusterOffset };
		mappingMatrix::Node node{ clusterName };

		for (auto i = 0u; i < clusterNode.staticModel.channelCount; ++i)
		{
			nodeMapping.channels.push_back(i);
			node.sockets.push_back("Channel " + std::to_string(i));
		}
		clusterMappings.push_back(std::move(nodeMapping));
		clusterMatrixNodes.push_back(std::move(node));
	}
}

template<class StreamNodeType, class RedundantStreamNodeType>
std::vector<std::pair<std::string, StreamNodeType const*>> buildStreamsListToDisplay(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::entity::model::StreamIndex const streamIndex, std::map<la::avdecc::entity::model::StreamIndex, StreamNodeType> const& streamNodes, std::map<la::avdecc::controller::model::VirtualIndex, RedundantStreamNodeType> const& redundantStreamNodes, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex)
{
	auto streamNodesToDisplay = std::vector<std::pair<std::string, StreamNodeType const*>>{};

	auto const isValidClockDomain = [](auto const clockDomainIndex, auto const& streamNode)
	{
		return clockDomainIndex == streamNode.staticModel.clockDomainIndex;
	};

	auto const isValidStreamFormat = [](auto const& streamNode)
	{
		auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode.dynamicModel.streamFormat);

		return sfi->getChannelsCount() > 0;
	};

	auto const checkAddStream = [&controlledEntity, &isValidClockDomain, &isValidStreamFormat, &streamNodesToDisplay](auto const streamIndex, auto const clockDomainIndex, auto const& streamNode, auto const& redundantStreamNodes)
	{
		if (isValidStreamFormat(streamNode) && isValidClockDomain(clockDomainIndex, streamNode))
		{
			// Add single Stream
			if (!streamNode.isRedundant)
			{
				auto streamName = hive::modelsLibrary::helper::objectName(controlledEntity, streamNode).toStdString();
				streamNodesToDisplay.push_back(std::make_pair(streamName, &streamNode));
			}
			else
			{
				// Add primary stream of a Redundant Set
				for (auto const& redundantStreamKV : redundantStreamNodes)
				{
					auto const& [redundantStreamIndex, redundantStreamNode] = redundantStreamKV;
					if (redundantStreamNode.primaryStreamIndex == streamIndex)
					{
						auto streamName = std::string("[R] ");
						if (!redundantStreamNode.virtualName.empty())
						{
							streamName += redundantStreamNode.virtualName.str();
						}
						else
						{
							if (redundantStreamNode.descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
							{
								streamName = hive::modelsLibrary::helper::redundantOutputName(redundantStreamIndex).toStdString();
							}
							else if (redundantStreamNode.descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
							{
								streamName = hive::modelsLibrary::helper::redundantInputName(redundantStreamIndex).toStdString();
							}
							else
							{
								AVDECC_ASSERT(false, "Invalide node descriptor type for redundant stream node");
							}
						}
						streamNodesToDisplay.push_back(std::make_pair(streamName, &streamNode));
					}
				}
			}
		}
	};

	if (streamIndex != la::avdecc::entity::model::getInvalidDescriptorIndex())
	{
		auto const& streamNode = streamNodes.at(streamIndex);

		if (isValidStreamFormat(streamNode) && isValidClockDomain(clockDomainIndex, streamNode))
		{
			checkAddStream(streamIndex, clockDomainIndex, streamNode, redundantStreamNodes);
		}
	}
	else
	{
		// Build list of StreamInput (single and primary)
		for (auto const& [strIndex, strNode] : streamNodes)
		{
			checkAddStream(strIndex, clockDomainIndex, strNode, redundantStreamNodes);
		}
	}

	return streamNodesToDisplay;
}

void showMappingsEditor(QObject* obj, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::DescriptorType const streamPortType, std::optional<la::avdecc::entity::model::StreamPortIndex> const streamPortIndex, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept
{
	AVDECC_ASSERT(audioUnitIndex != la::avdecc::entity::model::getInvalidDescriptorIndex(), "Invalid AudioUnitIndex");
	if (AVDECC_ASSERT_WITH_RET(streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput || streamIndex == la::avdecc::entity::model::getInvalidDescriptorIndex(), "StreamPortInput shall not specify a StreamIndex"))
	{
		try
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);

			if (controlledEntity)
			{
				auto const* const entity = controlledEntity.get();
				auto const& entityNode = entity->getEntityNode();
				auto const currentConfigurationIndex = entityNode.dynamicModel.currentConfiguration;
				auto const& configurationNode = entity->getConfigurationNode(currentConfigurationIndex);
				auto const& audioUnitNode = entity->getAudioUnitNode(currentConfigurationIndex, audioUnitIndex);
				auto const clockDomainIndex = audioUnitNode.staticModel.clockDomainIndex;
				mappingMatrix::Nodes outputs;
				mappingMatrix::Nodes inputs;
				mappingMatrix::Connections connections;
				StreamNodeMappings streamMappings;
				ClusterNodeMappings clusterMappings;

				if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
				{
					auto const handleStreamPort = [entity, clockDomainIndex, streamIndex, &configurationNode, &outputs, &inputs, &connections, &streamMappings, &clusterMappings](auto const& streamPortNode)
					{
						if (streamPortNode.staticModel.clockDomainIndex != clockDomainIndex)
						{
							return;
						}
						auto streamNodes = buildStreamsListToDisplay(entity, streamIndex, configurationNode.streamInputs, configurationNode.redundantStreamInputs, clockDomainIndex);

						// Build mappingMatrix vectors
						buildClusterMappings(entity, streamPortNode, clusterMappings, inputs);
						buildStreamMappings(entity, streamNodes, streamMappings, outputs);
						buildConnections<la::avdecc::entity::model::DescriptorType::StreamPortInput>(streamPortNode, streamNodes, streamMappings, clusterMappings, connections);
					};
					if (streamPortIndex)
					{
						auto const& streamPortNode = entity->getStreamPortInputNode(currentConfigurationIndex, *streamPortIndex);
						handleStreamPort(streamPortNode);
					}
					else
					{
						for (auto const& [streamPortIndex, streamPortNode] : audioUnitNode.streamPortInputs)
						{
							if (streamPortNode.staticModel.clockDomainIndex == clockDomainIndex)
							{
								handleStreamPort(streamPortNode);
							}
						}
					}
				}
				else if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
				{
					auto const handleStreamPort = [entity, clockDomainIndex, streamIndex, &configurationNode, &outputs, &inputs, &connections, &streamMappings, &clusterMappings](auto const& streamPortNode)
					{
						if (streamPortNode.staticModel.clockDomainIndex != clockDomainIndex)
						{
							return;
						}
						auto streamNodes = buildStreamsListToDisplay(entity, streamIndex, configurationNode.streamOutputs, configurationNode.redundantStreamOutputs, clockDomainIndex);

						// Build mappingMatrix vectors
						buildClusterMappings(entity, streamPortNode, clusterMappings, outputs);
						buildStreamMappings(entity, streamNodes, streamMappings, inputs);
						buildConnections<la::avdecc::entity::model::DescriptorType::StreamPortOutput>(streamPortNode, streamNodes, streamMappings, clusterMappings, connections);
					};
					if (streamPortIndex)
					{
						auto const& streamPortNode = entity->getStreamPortOutputNode(currentConfigurationIndex, *streamPortIndex);
						handleStreamPort(streamPortNode);
					}
					else
					{
						for (auto const& [streamPortIndex, streamPortNode] : audioUnitNode.streamPortOutputs)
						{
							if (streamPortNode.staticModel.clockDomainIndex == clockDomainIndex)
							{
								handleStreamPort(streamPortNode);
							}
						}
					}
				}
				else
				{
					AVDECC_ASSERT(false, "Should not happen");
				}

				if (!outputs.empty() && !inputs.empty())
				{
					auto smartName = hive::modelsLibrary::helper::smartEntityName(*entity);

					// Release the controlled entity before starting a long operation
					controlledEntity.reset();

					// Get exclusive access
					manager.requestExclusiveAccess(entityID, la::avdecc::controller::Controller::ExclusiveAccessToken::AccessType::Lock,
						[obj, streamMappings = std::move(streamMappings), clusterMappings = std::move(clusterMappings), smartName = std::move(smartName), outputs = std::move(outputs), inputs = std::move(inputs), connections = std::move(connections), entityID, streamPortType, streamPortIndex](auto const /*entityID*/, auto const status, auto&& token)
						{
							// Moving the token to the capture will effectively extend the lifetime of the token, keeping the entity locked until the lambda completes (meaning the dialog has been closed and mappings changed)
							QMetaObject::invokeMethod(obj,
								[status, token = std::move(token), streamMappings = std::move(streamMappings), clusterMappings = std::move(clusterMappings), smartName = std::move(smartName), outputs = std::move(outputs), inputs = std::move(inputs), connections = std::move(connections), entityID, streamPortType, streamPortIndex]()
								{
									// Failed to get the exclusive access
									if (!status || !token)
									{
										// If the device does not support the exclusive access, still proceed
										if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::NotImplemented && status != la::avdecc::entity::ControllerEntity::AemCommandStatus::NotSupported)
										{
											QMessageBox::warning(nullptr, QString(""), QString("Failed to get Exclusive Access on %1:<br>%2").arg(smartName).arg(QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status))));
											return;
										}
									}

									// Create the dialog
									auto title = QString("%1 - %2 Dynamic Mappings").arg(smartName).arg(streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput ? "Output" : "Input");
									auto dialog = mappingMatrix::MappingMatrixDialog{ title, outputs, inputs, connections };

									if (dialog.exec() == QDialog::Accepted)
									{
										if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
										{
											processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortInput>(entityID, streamMappings, clusterMappings, connections, dialog.connections());
										}
										else if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
										{
											processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortOutput>(entityID, streamMappings, clusterMappings, connections, dialog.connections());
										}
									}
								});
						});
				}
				else
				{
					QMessageBox::warning(nullptr, QString(""), QString("No editable channel mappings found for AUDIO_UNIT.%1").arg(audioUnitIndex));
				}
			}
		}
		catch (...)
		{
		}
	}
}

la::avdecc::entity::model::AudioMappings getMaximumAudioMappings(la::avdecc::entity::model::AudioMappings const& mappings, size_t const offset) noexcept
{
	auto const nbMappings = mappings.size();

	if (offset >= nbMappings)
	{
		return {};
	}

	auto const remaining = nbMappings - offset;
	auto const nbCopy = std::min(remaining, la::avdecc::protocol::aemPayload::AecpAemMaxAddRemoveAudioMappings);

	auto const& begin = mappings.begin() + offset;
	return la::avdecc::entity::model::AudioMappings{ begin, begin + nbCopy };
}

template<la::avdecc::entity::model::DescriptorType StreamPortType, bool Remove, typename HandlerType>
void processMappings(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, HandlerType const& handler)
{
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();

	auto const countMappings = mappings.size();
	auto offset = decltype(countMappings){ 0u };
	while (offset < countMappings)
	{
		auto const m = getMaximumAudioMappings(mappings, offset);
		auto const count = m.size();
		if (!AVDECC_ASSERT_WITH_RET(count != 0, "Should have at least one mapping to change"))
		{
			break;
		}
		offset += count;

		if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
		{
			if constexpr (Remove)
			{
				manager.removeStreamPortInputAudioMappings(entityID, streamPortIndex, m, nullptr, handler);
			}
			else
			{
				manager.addStreamPortInputAudioMappings(entityID, streamPortIndex, m, nullptr, handler);
			}
		}
		else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
		{
			if constexpr (Remove)
			{
				manager.removeStreamPortOutputAudioMappings(entityID, streamPortIndex, m, nullptr, handler);
			}
			else
			{
				manager.addStreamPortOutputAudioMappings(entityID, streamPortIndex, m, nullptr, handler);
			}
		}
		else
		{
			AVDECC_ASSERT(false, "Unsupported StreamPort type");
		}
	}
}

void batchAddInputAudioMappings(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, hive::modelsLibrary::ControllerManager::AddStreamPortInputAudioMappingsHandler const& handler) noexcept
{
	processMappings<la::avdecc::entity::model::DescriptorType::StreamPortInput, false>(entityID, streamPortIndex, mappings, handler);
}

void batchAddOutputAudioMappings(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, hive::modelsLibrary::ControllerManager::AddStreamPortInputAudioMappingsHandler const& handler) noexcept
{
	processMappings<la::avdecc::entity::model::DescriptorType::StreamPortOutput, false>(entityID, streamPortIndex, mappings, handler);
}

void batchRemoveInputAudioMappings(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, hive::modelsLibrary::ControllerManager::RemoveStreamPortInputAudioMappingsHandler const& handler) noexcept
{
	processMappings<la::avdecc::entity::model::DescriptorType::StreamPortInput, true>(entityID, streamPortIndex, mappings, handler);
}

void batchRemoveOutputAudioMappings(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, hive::modelsLibrary::ControllerManager::RemoveStreamPortInputAudioMappingsHandler const& handler) noexcept
{
	processMappings<la::avdecc::entity::model::DescriptorType::StreamPortOutput, true>(entityID, streamPortIndex, mappings, handler);
}

} // namespace mappingsHelper
} // namespace avdecc
