/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ENABLE_AVDECC_FEATURE_REDUNDANCY
#error "Hive requires Redundancy Feature to be enabled in AVDECC Library"
#endif // ENABLE_AVDECC_FEATURE_REDUNDANCY

#include "streamPortDynamicTreeWidgetItem.hpp"
#include "mappingMatrix.hpp"

#include <QPushButton>
#include <QMessageBox>

StreamPortDynamicTreeWidgetItem::StreamPortDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamPortType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::controller::model::StreamPortNodeStaticModel const* const staticModel, la::avdecc::controller::model::StreamPortNodeDynamicModel const* const dynamicModel, QTreeWidget *parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _streamPortType(streamPortType)
	, _streamPortIndex(streamPortIndex)
{
	// StreamPortInfo
	{
		// Create fields
		auto* editMappings = new QTreeWidgetItem(this);
		editMappings->setText(0, "Edit dynamic mapping");
		auto* editMappingsButton = new QPushButton("Edit");
		connect(editMappingsButton, &QPushButton::clicked, this, &StreamPortDynamicTreeWidgetItem::editMappingsButtonClicked);
		parent->setItemWidget(editMappings, 1, editMappingsButton);

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
			struct NodeMapping
			{
				la::avdecc::entity::model::DescriptorIndex descriptorIndex{ la::avdecc::entity::model::DescriptorIndex{ 0u } };
				std::vector<std::uint16_t> channels{};
			};

			auto const& entityNode = controlledEntity->getEntityNode();
			auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);
			mappingMatrix::Outputs outputs;
			mappingMatrix::Inputs inputs;
			mappingMatrix::Connections connections;
			std::vector<NodeMapping> streamMappings;
			std::vector<NodeMapping> clusterMappings;

			auto const buildVectors = [&controlledEntity, &connections, &streamMappings, &clusterMappings](la::avdecc::controller::model::StreamPortNode const& streamPortNode, std::vector<la::avdecc::controller::model::StreamInputNode const*> const& streamNodes, mappingMatrix::Inputs& clusterMatrixNodes, mappingMatrix::Outputs& streamMatrixNodes, std::function<mappingMatrix::Connection(mappingMatrix::SlotID const streamSlotID, mappingMatrix::SlotID const clusterSlotID)> const& creationConnectionFunction)
			{
				// Build list of cluster mappings
				for (auto const& clusterKV : streamPortNode.audioClusters)
				{
					auto const clusterIndex = clusterKV.first;
					auto const& clusterNode = clusterKV.second;
					auto clusterName = avdecc::helper::objectName(controlledEntity.get(), clusterNode).toStdString();
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

				// Build list of stream mappings
				for (auto const* streamNode : streamNodes)
				{
					auto streamName = avdecc::helper::objectName(controlledEntity.get(), *streamNode).toStdString();
					auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode->dynamicModel->currentFormat);
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

				// Build list of current connections
				for (auto const& mapping : streamPortNode.dynamicModel->dynamicAudioMap)
				{
					// Find the existing mapping
					mappingMatrix::SlotID streamSlotID{ -1, -1 };
					mappingMatrix::SlotID clusterSlotID{ -1, -1 };

					auto streamIndex{ mapping.streamIndex };

#ifdef ENABLE_AVDECC_FEATURE_REDUNDANCY
					// In case of redundancy, we must check the streamIndex in the mapping is the one matching the primary stream (or convert it)
					{
						for (auto const* streamNode : streamNodes)
						{
							if (streamIndex == streamNode->descriptorIndex)
							{
								// Found the index, nothing to do
								break;
							}
							else if(streamNode->isRedundant)
							{
								// Index not found and we have a redundant stream, check all association if we have it
								for (auto const redundantIndex : streamNode->staticModel->redundantStreams)
								{
									if (streamIndex == redundantIndex)
									{
										// Replace mapping's streamIndex with this stream's index (streamNodes only contains single and primary streams)
										streamIndex = streamNode->descriptorIndex;
										break;
									}
								}
							}
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
			};

			auto const isValidStream = [](auto const* const streamNode)
			{
				auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode->dynamicModel->currentFormat);
				auto const formatType = sfi->getType();

				if (formatType == la::avdecc::entity::model::StreamFormatInfo::Type::None || formatType == la::avdecc::entity::model::StreamFormatInfo::Type::Unsupported || formatType == la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference)
					return false;

				return true;
			};

			if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
			{
				auto const& streamPortNode = controlledEntity->getStreamPortInputNode(entityNode.dynamicModel->currentConfiguration, _streamPortIndex);
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
				buildVectors(streamPortNode, streamNodes, inputs, outputs, [](mappingMatrix::SlotID const streamSlotID, mappingMatrix::SlotID const clusterSlotID)
				{
					return std::make_pair(streamSlotID, clusterSlotID);
				});
			}
#if 0
			else if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
			{
				auto const& streamPortNode = controlledEntity->getStreamPortOutputNode(entityDescriptor->currentConfiguration, streamPortIndex);
				std::vector<la::avdecc::controller::model::StreamNode const*> streamNodes;
				// Build list of StreamOutput
				for (auto const& streamKV : configurationNode.streamOutputs)
				{
					auto const& streamNode = streamKV.second;
					if (!streamNode.isRedundant && isValidStream(&streamNode))
					{
						streamNodes.push_back(&streamNode);
					}
				}
				for (auto const& redundantStreamKV : configurationNode.redundantStreamOutputs)
				{
					auto const& redundantStreamNode = redundantStreamKV.second;
					auto const streamKV = redundantStreamNode.redundantStreams.begin(); // Get primary stream
					if (streamKV != redundantStreamNode.redundantStreams.end())
					{
						auto const* const streamNode = streamKV->second;
						if (isValidStream(streamNode))
						{
							streamNodes.push_back(streamNode);
						}
					}
				}
				// Build mappingMatrix vectors
				buildVectors(streamPortNode, streamNodes, outputs, inputs, [](int const streamPos, int const clusterPos)
				{
					return std::make_pair(clusterPos, streamPos);
				});
			}
#endif
			else
			{
				AVDECC_ASSERT(false, "Should not happen");
			}

			if (!outputs.empty() && !inputs.empty())
			{
				mappingMatrix::MappingMatrixDialog dialog(outputs, inputs, connections);

				QMessageBox::warning(nullptr, "", "Dynamic Mappings modification is partially bugged:<br>See https://github.com/christophe-calmejane/Hive/issues/12<br><br>Will be fixed in next release.");
				
				if (dialog.exec() == QDialog::Accepted)
				{
					if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
					{
						using HashType = std::uint64_t;
						auto const makeHash = [](mappingMatrix::Connection const& connection) -> HashType
						{
							return (static_cast<HashType>(connection.first.first & 0xFFFF) << 48) + (static_cast<HashType>(connection.first.second & 0xFFFF) << 32) + (static_cast<HashType>(connection.second.first & 0xFFFF) << 16) + static_cast<HashType>(connection.second.second & 0xFFFF);
						};
						auto const unmakeHash = [](HashType const& hash) -> la::avdecc::entity::model::AudioMapping
						{
							la::avdecc::entity::model::AudioMapping m;
							
							m.streamIndex = (hash >> 48) & 0xFFFF;
							m.streamChannel = (hash >> 32) & 0xFFFF;
							m.clusterOffset = (hash >> 16) & 0xFFFF;
							m.clusterChannel = hash & 0xFFFF;
							
							return m;
						};
						using ConvertedList = std::set<HashType>;
						auto const convertList = [&makeHash](mappingMatrix::Connections const& connections)
						{
							ConvertedList list;
							
							for (auto const& c : connections)
							{
								list.insert(makeHash(c));
							}
							
							return list;
						};
						auto const unconvertList = [&unmakeHash](ConvertedList const& list)
						{
							la::avdecc::entity::model::AudioMappings mappings;
							
							for (auto const& l : list)
							{
								mappings.push_back(unmakeHash(l));
							}
							
							return mappings;
						};
						auto const substractList = [](ConvertedList const& a, ConvertedList const& b)
						{
							ConvertedList sub{a};
							
							for (auto const& v : b)
							{
								sub.erase(v);
							}
							
							return sub;
						};

						// Build lists of mappings to add/remove
						auto const oldConnections = convertList(connections);
						auto const newConnections = convertList(dialog.connections());
						auto const toRemove = unconvertList(substractList(oldConnections, newConnections));
						auto const toAdd = unconvertList(substractList(newConnections, oldConnections));
						
						// Remove and Add the mappings
						auto& manager = avdecc::ControllerManager::getInstance();
						manager.removeStreamPortInputAudioMappings(_entityID, _streamPortIndex, toRemove);
						manager.addStreamPortInputAudioMappings(_entityID, _streamPortIndex, toAdd);
					}
				}
			}
		}
	}
	catch (...)
	{
	}
}

