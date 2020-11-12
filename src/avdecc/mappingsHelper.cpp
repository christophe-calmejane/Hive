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

#include "mappingsHelper.hpp"

#include <la/avdecc/utils.hpp>

#include <QMessageBox>

namespace avdecc
{
namespace mappingsHelper
{
std::pair<NodeMappings, mappingMatrix::Nodes> buildClusterMappings(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::StreamPortNode const& streamPortNode)
{
	NodeMappings clusterMappings;
	mappingMatrix::Nodes clusterMatrixNodes;

	// Build list of cluster mappings
	for (auto const& clusterKV : streamPortNode.audioClusters)
	{
		auto const clusterIndex = static_cast<la::avdecc::entity::model::ClusterIndex>(clusterKV.first - streamPortNode.staticModel->baseCluster); // Mappings use relative index (see IEEE1722.1 Table 7.33)
		AVDECC_ASSERT(clusterIndex < streamPortNode.staticModel->numberOfClusters, "ClusterIndex invalid");
		auto const& clusterNode = clusterKV.second;
		auto clusterName = hive::modelsLibrary::helper::objectName(controlledEntity, clusterNode).toStdString();
		NodeMapping nodeMapping{ clusterIndex };
		mappingMatrix::Node node{ clusterName };

		for (auto i = 0u; i < clusterNode.staticModel->channelCount; ++i)
		{
			nodeMapping.channels.push_back(i);
			node.sockets.push_back("Channel " + std::to_string(i));
		}
		clusterMappings.push_back(std::move(nodeMapping));
		clusterMatrixNodes.push_back(std::move(node));
	}

	return std::make_pair(clusterMappings, clusterMatrixNodes);
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
				auto const& configurationNode = entity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);
				mappingMatrix::Nodes outputs;
				mappingMatrix::Nodes inputs;
				mappingMatrix::Connections connections;
				avdecc::mappingsHelper::NodeMappings streamMappings;
				avdecc::mappingsHelper::NodeMappings clusterMappings;

				auto const isValidClockDomain = [](auto const& streamPortNode, auto const& streamNode)
				{
					return streamPortNode.staticModel->clockDomainIndex == streamNode.staticModel->clockDomainIndex;
				};

				auto const isValidStreamFormat = [](auto const& streamNode)
				{
					auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode.dynamicModel->streamFormat);

					return sfi->getChannelsCount() > 0;
				};

				if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
				{
					auto const& streamPortNode = entity->getStreamPortInputNode(entityNode.dynamicModel->currentConfiguration, streamPortIndex);
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
					auto clusterResult = avdecc::mappingsHelper::buildClusterMappings(entity, streamPortNode);
					clusterMappings = std::move(clusterResult.first);
					inputs = std::move(clusterResult.second);
					auto streamResult = avdecc::mappingsHelper::buildStreamMappings(entity, streamNodes);
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
					auto const& streamPortNode = entity->getStreamPortOutputNode(entityNode.dynamicModel->currentConfiguration, streamPortIndex);
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
					auto clusterResult = avdecc::mappingsHelper::buildClusterMappings(entity, streamPortNode);
					clusterMappings = std::move(clusterResult.first);
					outputs = std::move(clusterResult.second);
					auto streamResult = avdecc::mappingsHelper::buildStreamMappings(entity, streamNodes);
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

					// Release the controlled entity before starting a long operation (dialog.exec)
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
											avdecc::mappingsHelper::processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortInput>(entityID, streamPortIndex, streamMappings, clusterMappings, connections, dialog.connections());
										}
										else if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
										{
											avdecc::mappingsHelper::processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortOutput>(entityID, streamPortIndex, streamMappings, clusterMappings, connections, dialog.connections());
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

} // namespace mappingsHelper
} // namespace avdecc
