/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#ifndef ENABLE_AVDECC_FEATURE_REDUNDANCY
#	error "Hive requires Redundancy Feature to be enabled in AVDECC Library"
#endif // ENABLE_AVDECC_FEATURE_REDUNDANCY

#include "streamPortDynamicTreeWidgetItem.hpp"
#include "mappingMatrix.hpp"
#include <vector>
#include <utility>
#include <cstdint>
#include <QPushButton>
#include <QMessageBox>

/* ************************************************************ */
/* Internal types and functions                                 */
/* ************************************************************ */
struct NodeMapping
{
	la::avdecc::entity::model::DescriptorIndex descriptorIndex{ la::avdecc::entity::model::DescriptorIndex{ 0u } };
	std::vector<std::uint16_t> channels{};
};
using NodeMappings = std::vector<NodeMapping>;
using HashType = std::uint64_t;
using HashedConnectionsList = std::set<HashType>;

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
		auto clusterName = avdecc::helper::objectName(controlledEntity, clusterNode).toStdString();
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

template<class StreamNodeType>
std::pair<NodeMappings, mappingMatrix::Nodes> buildStreamMappings(la::avdecc::controller::ControlledEntity const* const controlledEntity, std::vector<StreamNodeType const*> const& streamNodes)
{
	NodeMappings streamMappings;
	mappingMatrix::Nodes streamMatrixNodes;

	// Build list of stream mappings
	for (auto const* streamNode : streamNodes)
	{
		auto streamName = avdecc::helper::objectName(controlledEntity, *streamNode).toStdString();
		auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode->dynamicModel->streamInfo.streamFormat);
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

/* ************************************************************ */
/* StreamPortDynamicTreeWidgetItem                              */
/* ************************************************************ */
StreamPortDynamicTreeWidgetItem::StreamPortDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamPortType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::controller::model::StreamPortNodeStaticModel const* const staticModel, la::avdecc::controller::model::StreamPortNodeDynamicModel const* const dynamicModel, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _streamPortType(streamPortType)
	, _streamPortIndex(streamPortIndex)
{
	// StreamPortInfo
	{
		// Create fields
		auto* editMappings = new QTreeWidgetItem(this);
		editMappings->setText(0, "Edit Dynamic Mappings");
		auto* editMappingsButton = new QPushButton("Edit");
		connect(editMappingsButton, &QPushButton::clicked, this, &StreamPortDynamicTreeWidgetItem::editMappingsButtonClicked);
		parent->setItemWidget(editMappings, 1, editMappingsButton);

		auto* mappingsItem = new QTreeWidgetItem(this);
		mappingsItem->setText(0, "Dynamic Mappings");
		_mappingsList = new QListWidget;
		parent->setItemWidget(mappingsItem, 1, _mappingsList);
		_mappingsList->setStyleSheet(".QListWidget{margin-top:4px;margin-bottom:4px}");
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity)
			{
				auto& entity = *controlledEntity;
				auto const* streamPortNode = static_cast<la::avdecc::controller::model::StreamPortNode const*>(nullptr);

				auto const& entityNode = entity.getEntityNode();
				if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
				{
					streamPortNode = &entity.getStreamPortInputNode(entityNode.dynamicModel->currentConfiguration, streamPortIndex);
				}
				else if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
				{
					streamPortNode = &entity.getStreamPortOutputNode(entityNode.dynamicModel->currentConfiguration, streamPortIndex);
				}
				// Update info right now
				updateMappings(streamPortNode->dynamicModel->dynamicAudioMap);
			}
		}
		catch (...)
		{
		}

		auto* clearMappings = new QTreeWidgetItem(this);
		clearMappings->setText(0, "Clear All Dynamic Mappings");
		auto* clearMappingsButton = new QPushButton("Clear");
		connect(clearMappingsButton, &QPushButton::clicked, this, &StreamPortDynamicTreeWidgetItem::clearMappingsButtonClicked);
		parent->setItemWidget(clearMappings, 1, clearMappingsButton);

		// Listen for streamPortAudioMappingsChanged
		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamPortAudioMappingsChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex)
			{
				if (entityID == _entityID && descriptorType == _streamPortType && streamPortIndex == _streamPortIndex)
				{
					auto& manager = avdecc::ControllerManager::getInstance();
					auto controlledEntity = manager.getControlledEntity(entityID);
					if (controlledEntity)
					{
						auto& entity = *controlledEntity;
						auto const* streamPortNode = static_cast<la::avdecc::controller::model::StreamPortNode const*>(nullptr);

						auto const& entityNode = entity.getEntityNode();
						if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
						{
							streamPortNode = &entity.getStreamPortInputNode(entityNode.dynamicModel->currentConfiguration, streamPortIndex);
						}
						else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
						{
							streamPortNode = &entity.getStreamPortOutputNode(entityNode.dynamicModel->currentConfiguration, streamPortIndex);
						}
						// Update mappings
						updateMappings(streamPortNode->dynamicModel->dynamicAudioMap);
					}
				}
			});

		// TODO: Listen for entity offline events and close the popup window
	}
}

void StreamPortDynamicTreeWidgetItem::editMappingsButtonClicked()
{
	// TODO: Avoir access au controller pour faire un acquire de l'entite (sans utiliser les callbacks du ControllerManager, car sinon ca lance une popup globale si le acquire echoue)
	// Si acquire echoue, alors on indique un message comme quoi il est necessaire de pouvoir acquire l'entite afin de changer le mapping sans etre derange
	// Si acquire reussi, alors on peut ouvrir la fenetre et faire le mapping
	// TODO: Plus tard, il faudra faire un Lock, au lieu de Acquire (ou mieux faire une fonction "GetExclusiveToken" qui acquire ou lock en fonction du type d'entite, une fois le token relache et si c'est le dernier, ca release l'entite). Peut etre le mettre dans la lib controller comme ca on gere directement le resend du lock !!
	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(_entityID);

		if (controlledEntity)
		{
			auto const* const entity = controlledEntity.get();
			auto const& entityNode = entity->getEntityNode();
			auto const& configurationNode = entity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);
			mappingMatrix::Nodes outputs;
			mappingMatrix::Nodes inputs;
			mappingMatrix::Connections connections;
			NodeMappings streamMappings;
			NodeMappings clusterMappings;

			auto const isValidStream = [](auto const* const streamNode)
			{
				auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode->dynamicModel->streamInfo.streamFormat);
				auto const formatType = sfi->getType();

				if (formatType == la::avdecc::entity::model::StreamFormatInfo::Type::None || formatType == la::avdecc::entity::model::StreamFormatInfo::Type::Unsupported || formatType == la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference)
					return false;

				return true;
			};

			if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
			{
				auto const& streamPortNode = entity->getStreamPortInputNode(entityNode.dynamicModel->currentConfiguration, _streamPortIndex);
				std::vector<la::avdecc::controller::model::StreamInputNode const*> streamNodes;

				// Build list of StreamInput
				for (auto const& streamKV : configurationNode.streamInputs)
				{
					auto const& streamNode = streamKV.second;
					if (!streamNode.isRedundant && isValidStream(&streamNode))
					{
						streamNodes.push_back(&streamNode);
					}
				}

				// Add primary stream of a Redundant Set
				for (auto const& redundantStreamKV : configurationNode.redundantStreamInputs)
				{
					auto const& redundantStreamNode = redundantStreamKV.second;
					auto const* const streamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);
					if (isValidStream(streamNode))
					{
						streamNodes.push_back(streamNode);
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
			else if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
			{
				auto const& streamPortNode = entity->getStreamPortOutputNode(entityNode.dynamicModel->currentConfiguration, _streamPortIndex);
				std::vector<la::avdecc::controller::model::StreamOutputNode const*> streamNodes;

				// Build list of StreamOutput
				for (auto const& streamKV : configurationNode.streamOutputs)
				{
					auto const& streamNode = streamKV.second;
					if (!streamNode.isRedundant && isValidStream(&streamNode))
					{
						streamNodes.push_back(&streamNode);
					}
				}

				// Add primary stream of a Redundant Set
				for (auto const& redundantStreamKV : configurationNode.redundantStreamOutputs)
				{
					auto const& redundantStreamNode = redundantStreamKV.second;
					auto const* const streamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);
					if (isValidStream(streamNode))
					{
						streamNodes.push_back(streamNode);
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
				mappingMatrix::MappingMatrixDialog dialog(outputs, inputs, connections);

				// Release the controlled entity before starting a long operation (dialog.exec)
				controlledEntity.reset();

				if (dialog.exec() == QDialog::Accepted)
				{
					if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
					{
						processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortInput>(_entityID, _streamPortIndex, streamMappings, clusterMappings, connections, dialog.connections());
					}
					else if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
					{
						processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortOutput>(_entityID, _streamPortIndex, streamMappings, clusterMappings, connections, dialog.connections());
					}
				}
			}
		}
	}
	catch (...)
	{
	}
}

void StreamPortDynamicTreeWidgetItem::clearMappingsButtonClicked()
{
	// TODO: Avoir access au controller pour faire un acquire de l'entite (sans utiliser les callbacks du ControllerManager, car sinon ca lance une popup globale si le acquire echoue)
	// Si acquire echoue, alors on indique un message comme quoi il est necessaire de pouvoir acquire l'entite afin de changer le mapping sans etre derange
	// Si acquire reussi, alors on peut ouvrir la fenetre et faire le mapping
	// TODO: Plus tard, il faudra faire un Lock, au lieu de Acquire (ou mieux faire une fonction "GetExclusiveToken" qui acquire ou lock en fonction du type d'entite, une fois le token relache et si c'est le dernier, ca release l'entite). Peut etre le mettre dans la lib controller comme ca on gere directement le resend du lock !!
	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(_entityID);
		if (controlledEntity)
		{
			auto& entity = *controlledEntity;
			auto const* streamPortNode = static_cast<la::avdecc::controller::model::StreamPortNode const*>(nullptr);

			auto const& entityNode = entity.getEntityNode();
			if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
			{
				streamPortNode = &entity.getStreamPortInputNode(entityNode.dynamicModel->currentConfiguration, _streamPortIndex);
				manager.removeStreamPortInputAudioMappings(_entityID, _streamPortIndex, streamPortNode->dynamicModel->dynamicAudioMap);
			}
			else if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
			{
				streamPortNode = &entity.getStreamPortOutputNode(entityNode.dynamicModel->currentConfiguration, _streamPortIndex);
				manager.removeStreamPortOutputAudioMappings(_entityID, _streamPortIndex, streamPortNode->dynamicModel->dynamicAudioMap);
			}
		}
	}
	catch (...)
	{
	}
}

void StreamPortDynamicTreeWidgetItem::updateMappings(la::avdecc::entity::model::AudioMappings const& mappings)
{
	_mappingsList->clear();

	for (auto const& mapping : mappings)
	{
		_mappingsList->addItem(QString("%1.%2 > %3.%4").arg(mapping.streamIndex).arg(mapping.streamChannel).arg(mapping.clusterOffset).arg(mapping.clusterChannel));
	}

	_mappingsList->sortItems();
}
