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

template<class StreamNodeType>
std::pair<NodeMappings, mappingMatrix::Nodes> buildStreamMappings(la::avdecc::controller::ControlledEntity const* const controlledEntity, std::vector<StreamNodeType const*> const& streamNodes)
{
	NodeMappings streamMappings;
	mappingMatrix::Nodes streamMatrixNodes;

	// Build list of stream mappings
	for (auto const* streamNode : streamNodes)
	{
		auto streamName = hive::modelsLibrary::helper::objectName(controlledEntity, *streamNode).toStdString();
		auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode->dynamicModel.streamFormat);
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
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	if (!toRemove.empty())
	{
		auto const countMappings = toRemove.size();
		auto offset = decltype(countMappings){ 0u };
		while (offset < countMappings)
		{
			auto const mappings = getMaximumAudioMappings(toRemove, offset);
			auto const count = mappings.size();
			if (!AVDECC_ASSERT_WITH_RET(count != 0, "Should have at least one mapping to change"))
			{
				break;
			}
			offset += count;

			if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
			{
				manager.removeStreamPortInputAudioMappings(entityID, streamPortIndex, mappings);
			}
			else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
			{
				manager.removeStreamPortOutputAudioMappings(entityID, streamPortIndex, mappings);
			}
		}
	}
	if (!toAdd.empty())
	{
		auto const countMappings = toAdd.size();
		auto offset = decltype(countMappings){ 0u };
		while (offset < countMappings)
		{
			auto const mappings = getMaximumAudioMappings(toAdd, offset);
			auto const count = mappings.size();
			if (!AVDECC_ASSERT_WITH_RET(count != 0, "Should have at least one mapping to change"))
			{
				break;
			}
			offset += count;

			if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
			{
				manager.addStreamPortInputAudioMappings(entityID, streamPortIndex, mappings);
			}
			else if constexpr (StreamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
			{
				manager.addStreamPortOutputAudioMappings(entityID, streamPortIndex, mappings);
			}
		}
	}
}

std::pair<NodeMappings, mappingMatrix::Nodes> buildClusterMappings(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::StreamPortNode const& streamPortNode)
{
	NodeMappings clusterMappings;
	mappingMatrix::Nodes clusterMatrixNodes;

	// Build list of cluster mappings
	for (auto const& clusterKV : streamPortNode.audioClusters)
	{
		auto const clusterIndex = static_cast<la::avdecc::entity::model::ClusterIndex>(clusterKV.first - streamPortNode.staticModel.baseCluster); // Mappings use relative index (see IEEE1722.1 Table 7.33)
		AVDECC_ASSERT(clusterIndex < streamPortNode.staticModel.numberOfClusters, "ClusterIndex invalid");
		auto const& clusterNode = clusterKV.second;
		auto clusterName = hive::modelsLibrary::helper::objectName(controlledEntity, clusterNode).toStdString();
		NodeMapping nodeMapping{ clusterIndex };
		mappingMatrix::Node node{ clusterName };

		for (auto i = 0u; i < clusterNode.staticModel.channelCount; ++i)
		{
			nodeMapping.channels.push_back(i);
			node.sockets.push_back("Channel " + std::to_string(i));
		}
		clusterMappings.push_back(std::move(nodeMapping));
		clusterMatrixNodes.push_back(std::move(node));
	}

	return std::make_pair(clusterMappings, clusterMatrixNodes);
}

void showMappingsEditor(QObject* obj, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamPortType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept
{
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
				auto const& configurationNode = entity->getConfigurationNode(entityNode.dynamicModel.currentConfiguration);
				mappingMatrix::Nodes outputs;
				mappingMatrix::Nodes inputs;
				mappingMatrix::Connections connections;
				NodeMappings streamMappings;
				NodeMappings clusterMappings;

				auto const isValidClockDomain = [](auto const& streamPortNode, auto const& streamNode)
				{
					return streamPortNode.staticModel.clockDomainIndex == streamNode.staticModel.clockDomainIndex;
				};

				auto const isValidStreamFormat = [](auto const& streamNode)
				{
					auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode.dynamicModel.streamFormat);

					return sfi->getChannelsCount() > 0;
				};

				if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
				{
					auto const& streamPortNode = entity->getStreamPortInputNode(entityNode.dynamicModel.currentConfiguration, streamPortIndex);
					std::vector<la::avdecc::controller::model::StreamInputNode const*> streamNodes;

					if (streamIndex != la::avdecc::entity::model::getInvalidDescriptorIndex())
					{
						// Insert single StreamInput to list
						auto const& streamNode = configurationNode.streamInputs.at(streamIndex);
						if (!streamNode.isRedundant && isValidStreamFormat(streamNode) && isValidClockDomain(streamPortNode, streamNode))
						{
							streamNodes.push_back(&streamNode);
						}

						// Insert single primary stream of a Redundant Set to list
						for (auto const& redundantStreamKV : configurationNode.redundantStreamInputs)
						{
							auto const& redundantStreamNode = redundantStreamKV.second;
							auto const* const primRedundantStreamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);

							// we use the redundantStreams to check if our current streamIndex is either prim or sec of it. For mappings edit, we only use prim though.
							if (redundantStreamNode.redundantStreams.count(streamIndex) && isValidStreamFormat(*primRedundantStreamNode) && isValidClockDomain(streamPortNode, *primRedundantStreamNode))
							{
								streamNodes.push_back(primRedundantStreamNode);
								break;
							}
						}
					}
					else
					{
						// Build list of StreamInput
						for (auto const& streamKV : configurationNode.streamInputs)
						{
							auto const& streamNode = streamKV.second;
							if (!streamNode.isRedundant && isValidStreamFormat(streamNode) && isValidClockDomain(streamPortNode, streamNode))
							{
								streamNodes.push_back(&streamNode);
							}
						}

						// Add primary stream of a Redundant Set
						for (auto const& redundantStreamKV : configurationNode.redundantStreamInputs)
						{
							auto const& redundantStreamNode = redundantStreamKV.second;
							auto const* const streamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);
							if (isValidStreamFormat(*streamNode) && isValidClockDomain(streamPortNode, *streamNode))
							{
								streamNodes.push_back(streamNode);
							}
						}
					}

					// Build mappingMatrix vectors
					auto clusterResult = buildClusterMappings(entity, streamPortNode);
					clusterMappings = std::move(clusterResult.first);
					inputs = std::move(clusterResult.second);
					auto streamResult = buildStreamMappings(entity, streamNodes);
					streamMappings = std::move(streamResult.first);
					outputs = std::move(streamResult.second);
					connections = buildConnections(streamPortNode, streamNodes, streamMappings, clusterMappings,
						[](mappingMatrix::SlotID const streamSlotID, mappingMatrix::SlotID const clusterSlotID)
						{
							return std::make_pair(streamSlotID, clusterSlotID);
						});
				}
				else if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
				{
					auto const& streamPortNode = entity->getStreamPortOutputNode(entityNode.dynamicModel.currentConfiguration, streamPortIndex);
					std::vector<la::avdecc::controller::model::StreamOutputNode const*> streamNodes;

					if (streamIndex != la::avdecc::entity::model::getInvalidDescriptorIndex())
					{
						// Insert single StreamOutput to list
						auto const& streamNode = configurationNode.streamOutputs.at(streamIndex);
						if (!streamNode.isRedundant && isValidStreamFormat(streamNode) && isValidClockDomain(streamPortNode, streamNode))
							streamNodes.push_back(&streamNode);

						// Insert single primary stream of a Redundant Set to list
						for (auto const& redundantStreamKV : configurationNode.redundantStreamOutputs)
						{
							auto const& redundantStreamNode = redundantStreamKV.second;
							auto const* const primRedundantStreamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);

							// we use the redundantStreams to check if our current streamIndex is either prim or sec of it. For mappings edit, we only use prim though.
							if (redundantStreamNode.redundantStreams.count(streamIndex) && isValidStreamFormat(*primRedundantStreamNode) && isValidClockDomain(streamPortNode, *primRedundantStreamNode))
							{
								streamNodes.push_back(primRedundantStreamNode);
								break;
							}
						}
					}
					else
					{
						// Build list of StreamOutput
						for (auto const& streamKV : configurationNode.streamOutputs)
						{
							auto const& streamNode = streamKV.second;
							if (!streamNode.isRedundant && isValidStreamFormat(streamNode) && isValidClockDomain(streamPortNode, streamNode))
							{
								streamNodes.push_back(&streamNode);
							}
						}

						// Add primary stream of a Redundant Set
						for (auto const& redundantStreamKV : configurationNode.redundantStreamOutputs)
						{
							auto const& redundantStreamNode = redundantStreamKV.second;
							auto const* const streamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);
							if (isValidStreamFormat(*streamNode) && isValidClockDomain(streamPortNode, *streamNode))
							{
								streamNodes.push_back(streamNode);
							}
						}
					}

					// Build mappingMatrix vectors
					auto clusterResult = buildClusterMappings(entity, streamPortNode);
					clusterMappings = std::move(clusterResult.first);
					outputs = std::move(clusterResult.second);
					auto streamResult = buildStreamMappings(entity, streamNodes);
					streamMappings = std::move(streamResult.first);
					inputs = std::move(streamResult.second);
					connections = buildConnections(streamPortNode, streamNodes, streamMappings, clusterMappings,
						[](mappingMatrix::SlotID const streamSlotID, mappingMatrix::SlotID const clusterSlotID)
						{
							return std::make_pair(clusterSlotID, streamSlotID);
						});
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
									auto title = QString("%1 - %2.%3 Dynamic Mappings").arg(smartName).arg(avdecc::helper::descriptorTypeToString(streamPortType)).arg(streamPortIndex);
									auto dialog = mappingMatrix::MappingMatrixDialog{ title, outputs, inputs, connections };

									if (dialog.exec() == QDialog::Accepted)
									{
										if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
										{
											processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortInput>(entityID, streamPortIndex, streamMappings, clusterMappings, connections, dialog.connections());
										}
										else if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
										{
											processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortOutput>(entityID, streamPortIndex, streamMappings, clusterMappings, connections, dialog.connections());
										}
									}
								});
						});
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
