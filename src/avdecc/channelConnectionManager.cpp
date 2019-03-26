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

#include "channelConnectionManager.hpp"

#include <set>
#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

#include "helper.hpp"
#include "controllerManager.hpp"

namespace avdecc
{
class ChannelConnectionManagerImpl final : public ChannelConnectionManager
{
private:
	// Private members
	std::set<la::avdecc::UniqueIdentifier> _entities{}; // No lock required, only read/write in the UI thread
	std::map<la::avdecc::UniqueIdentifier, std::shared_ptr<SourceChannelConnections>> _listenerChannelMappings;

public:
	/**
		* Constructor.
		*/
	ChannelConnectionManagerImpl() noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		connect(&manager, &ControllerManager::controllerOffline, this, &ChannelConnectionManagerImpl::onControllerOffline);
		connect(&manager, &ControllerManager::entityOnline, this, &ChannelConnectionManagerImpl::onEntityOnline);
		connect(&manager, &ControllerManager::entityOffline, this, &ChannelConnectionManagerImpl::onEntityOffline);

		connect(&manager, &ControllerManager::streamConnectionChanged, this, &ChannelConnectionManagerImpl::onStreamConnectionChanged);
		connect(&manager, &ControllerManager::streamPortAudioMappingsChanged, this, &ChannelConnectionManagerImpl::onStreamPortAudioMappingsChanged);
	}

	/**
	* Destructor.
	*/
	~ChannelConnectionManagerImpl() noexcept {}

	/**
	* Removes all entities from the internal list.
	*/
	Q_SLOT void onControllerOffline()
	{
		_entities.clear();
	}

	/**
	* Adds the entity to the internal list.
	*/
	Q_SLOT void onEntityOnline(la::avdecc::UniqueIdentifier const& entityId)
	{
		// add entity to the set
		_entities.insert(entityId);
	}

	/**
	* Removes the entity from the internal list.
	*/
	Q_SLOT void onEntityOffline(la::avdecc::UniqueIdentifier const& entityId)
	{
		// remove entity from the set
		_entities.erase(entityId);
		// also remove the cached connections for this entity
		_listenerChannelMappings.erase(entityId);
	}

	/**
	* Update the cached connection info if it's already in the map.
	*/
	Q_SLOT void onStreamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& streamConnectionState)
	{
		// check if it is a redundant (secondary) stream, if so stop processing. (redundancy is handled inside the determineChannelConnectionsReverse method)
		if (!isInputStreamPrimaryOrNonRedundant(streamConnectionState.listenerStream))
		{
			return;
		}

		if (_listenerChannelMappings.find(streamConnectionState.listenerStream.entityID) != _listenerChannelMappings.end())
		{
			std::set<std::pair<la::avdecc::UniqueIdentifier, SourceChannelIdentification>> listenerChannelsToUpdate;
			std::set<std::pair<la::avdecc::UniqueIdentifier, SourceChannelIdentification>> updatedListenerChannels;
			auto connectionInfo = _listenerChannelMappings.at(streamConnectionState.listenerStream.entityID);

			// if a stream was disconnected, only update the entries that have a connection currently
			// and if the stream was connected, only update the entries that have no connections yet.
			if (streamConnectionState.state == la::avdecc::controller::model::StreamConnectionState::State::NotConnected)
			{
				for (auto const& mappingKV : connectionInfo->channelMappings)
				{
					for (auto const& target : mappingKV.second->targets)
					{
						if (target->sourceStreamIndex == streamConnectionState.listenerStream.streamIndex && target->targetStreamIndex == streamConnectionState.talkerStream.streamIndex)
						{
							// this needs a refresh
							auto channel = std::make_pair(streamConnectionState.listenerStream.entityID, mappingKV.first);
							listenerChannelsToUpdate.insert(channel);
							break;
						}
					}
				}
			}
			else
			{
				// if the new connection overwrites an existing one (implicit disconnect), this has to be handled here too:
				for (auto const& mappingKV : connectionInfo->channelMappings)
				{
					for (auto const& target : mappingKV.second->targets)
					{
						if ((target->targetStreamIndex != streamConnectionState.talkerStream.streamIndex || target->targetEntityId != streamConnectionState.talkerStream.entityID) && target->sourceStreamIndex == streamConnectionState.listenerStream.streamIndex)
						{
							// this needs a refresh
							auto channel = std::make_pair(streamConnectionState.listenerStream.entityID, mappingKV.first);
							listenerChannelsToUpdate.insert(channel);
							break;
						}
					}
				}

				// handle changes from the new connetion
				for (auto const& mappingKV : connectionInfo->channelMappings)
				{
					if (mappingKV.second->targets.empty())
					{
						la::avdecc::entity::model::AudioMappings mappings;
						try
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto controlledEntity = manager.getControlledEntity(streamConnectionState.listenerStream.entityID);
							if (controlledEntity)
							{
								auto const& configNode = controlledEntity->getConfigurationNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex);
								auto const& audioUnitIndex = *mappingKV.second->sourceClusterChannelInfo.sourceAudioUnitIndex;
								auto const& streamPortIndex = *mappingKV.second->sourceClusterChannelInfo.sourceStreamPortIndex;
								if (configNode.audioUnits.size() > audioUnitIndex && configNode.audioUnits.at(audioUnitIndex).streamPortOutputs.size() > streamPortIndex)
								{
									auto streamPortInputDynamicModel = configNode.audioUnits.at(audioUnitIndex).streamPortInputs.at(streamPortIndex).dynamicModel;
									if (streamPortInputDynamicModel)
									{
										mappings = streamPortInputDynamicModel->dynamicAudioMap;
									}
								}
							}
						}
						catch (la::avdecc::controller::ControlledEntity::Exception const&)
						{
						}

						for (auto const& mapping : mappings)
						{
							if (*mappingKV.second->sourceClusterChannelInfo.sourceClusterIndex + *mappingKV.second->sourceClusterChannelInfo.sourceBaseCluster == mapping.clusterOffset && *mappingKV.second->sourceClusterChannelInfo.sourceClusterChannel == mapping.clusterChannel && mapping.streamIndex == streamConnectionState.listenerStream.streamIndex)
							{
								// this propably needs a refresh
								auto channel = std::make_pair(streamConnectionState.listenerStream.entityID, mappingKV.first);
								listenerChannelsToUpdate.insert(channel);
								break;
							}
						}
					}
				}
			}

			for (auto const& listenerChannelToUpdate : listenerChannelsToUpdate)
			{
				auto const& sourceInfo = listenerChannelToUpdate.second;
				auto newListenerChannelConnections = determineChannelConnectionsReverse(listenerChannelToUpdate.first, la::avdecc::UniqueIdentifier{*sourceInfo.sourceConfigurationIndex}, *sourceInfo.sourceAudioUnitIndex, *sourceInfo.sourceStreamPortIndex, *sourceInfo.sourceClusterIndex, *sourceInfo.sourceBaseCluster, *sourceInfo.sourceClusterChannel);
				auto oldListenerChannelConnections = connectionInfo->channelMappings.at(sourceInfo);
				if (newListenerChannelConnections != oldListenerChannelConnections)
				{
					connectionInfo->channelMappings[sourceInfo] = newListenerChannelConnections;
					updatedListenerChannels.insert(listenerChannelToUpdate);
				}
			}

			if (!updatedListenerChannels.empty())
			{
				emit listenerChannelConnectionsUpdate(updatedListenerChannels);
			}
		}
	}

	/**
	* Update the cached connection info if it's already in the map.
	*/
	Q_SLOT void onStreamPortAudioMappingsChanged(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::DescriptorType const& descriptorType, la::avdecc::entity::model::StreamPortIndex const& streamPortIndex)
	{
		std::set<std::pair<la::avdecc::UniqueIdentifier, SourceChannelIdentification>> listenerChannelsToUpdate;
		std::set<std::pair<la::avdecc::UniqueIdentifier, SourceChannelIdentification>> updatedListenerChannels;

		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
		{
			if (_listenerChannelMappings.find(entityId) != _listenerChannelMappings.end())
			{
				auto connectionInfo = _listenerChannelMappings.at(entityId);
				for (auto const& mappingKV : connectionInfo->channelMappings)
				{
					if (mappingKV.first.sourceStreamPortIndex == streamPortIndex)
					{
						// this needs a refresh
						auto channel = std::make_pair(entityId, mappingKV.first);
						listenerChannelsToUpdate.insert(channel);
					}
				}
			}
		}
		else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
		{
			try
			{
				auto& manager = avdecc::ControllerManager::getInstance();

				// search for talker changes that affect a listener in the cached map.
				for (auto const& deviceMappingsKV : _listenerChannelMappings)
				{
					auto controlledEntityListener = manager.getControlledEntity(deviceMappingsKV.first);
					if (controlledEntityListener)
					{
						if (!controlledEntityListener->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
						{
							continue;
						}
						auto const& configuration = controlledEntityListener->getCurrentConfigurationNode();
						std::unordered_set<la::avdecc::UniqueIdentifier, la::avdecc::UniqueIdentifier::hash> currentlyConnectedEntities;
						for (auto const& streamInput : configuration.streamInputs)
						{
							auto* streamInputDynamicModel = streamInput.second.dynamicModel;
							if (streamInputDynamicModel && streamInputDynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::Connected)
							{
								currentlyConnectedEntities.emplace(streamInputDynamicModel->connectionState.talkerStream.entityID);
							}
						}

						if (currentlyConnectedEntities.find(entityId) != currentlyConnectedEntities.end())
						{
							for (auto const& mappingKV : deviceMappingsKV.second->channelMappings)
							{
								auto channel = std::make_pair(deviceMappingsKV.first, mappingKV.first);
								listenerChannelsToUpdate.insert(channel);
							}
						}
					}
				}
			}
			catch (la::avdecc::Exception e)
			{
			}
		}

		for (auto const& listenerChannelToUpdateKV : listenerChannelsToUpdate)
		{
			auto const& sourceInfo = listenerChannelToUpdateKV.second;

			auto connectionInfo = _listenerChannelMappings.at(listenerChannelToUpdateKV.first);

			auto newListenerChannelConnections = determineChannelConnectionsReverse(listenerChannelToUpdateKV.first, la::avdecc::UniqueIdentifier{*sourceInfo.sourceConfigurationIndex}, *sourceInfo.sourceAudioUnitIndex, *sourceInfo.sourceStreamPortIndex, *sourceInfo.sourceClusterIndex, *sourceInfo.sourceBaseCluster, *sourceInfo.sourceClusterChannel);
			auto oldListenerChannelConnections = connectionInfo->channelMappings.at(sourceInfo);

			if (!newListenerChannelConnections->isEqualTo(*oldListenerChannelConnections))
			{
				connectionInfo->channelMappings[sourceInfo] = newListenerChannelConnections;
				updatedListenerChannels.insert(listenerChannelToUpdateKV);
			}
		}

		if (!updatedListenerChannels.empty())
		{
			emit listenerChannelConnectionsUpdate(updatedListenerChannels);
		}
	}


	/**
	* Checks if the given stream is the primary of a redundant stream pair or a non redundant stream.
	* Assumes the that the given StreamIdentification is valid.
	*/
	bool isInputStreamPrimaryOrNonRedundant(la::avdecc::entity::model::StreamIdentification streamIdentification)
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(streamIdentification.entityID);
		if (controlledEntity)
		{
			auto const& configNode = controlledEntity->getConfigurationNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex);
			auto const& streamInputNode = configNode.streamInputs.at(streamIdentification.streamIndex);
			if (!streamInputNode.isRedundant)
			{
				return true;
			}
			else
			{
				for (auto const& redundantStreamInput : configNode.redundantStreamInputs)
				{
					if (redundantStreamInput.second.primaryStream->descriptorIndex == streamIdentification.streamIndex)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	/**
	* Checks if the given stream is the primary of a redundant stream pair or a non redundant stream.
	* Assumes the that the given StreamIdentification is valid.
	*/
	bool isOutputStreamPrimaryOrNonRedundant(la::avdecc::entity::model::StreamIdentification streamIdentification)
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(streamIdentification.entityID);
		if (controlledEntity)
		{
			auto const& configNode = controlledEntity->getConfigurationNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex);
			auto const& streamInputNode = configNode.streamInputs.at(streamIdentification.streamIndex);
			if (!streamInputNode.isRedundant)
			{
				return true;
			}
			else
			{
				for (auto const& redundantStreamInput : configNode.redundantStreamInputs)
				{
					if (redundantStreamInput.second.primaryStream->descriptorIndex == streamIdentification.streamIndex)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	/**
	* Iterates over the list of known entities and returns all connections that originate from the given talker.
	*/
	std::vector<la::avdecc::controller::model::StreamConnectionState> getAllStreamOutputConnections(la::avdecc::UniqueIdentifier talkerEntityId)
	{
		std::vector<la::avdecc::controller::model::StreamConnectionState> disconnectedStreams;
		auto const& manager = avdecc::ControllerManager::getInstance();
		for (auto const& potentialListenerEntityId : _entities)
		{
			auto const controlledEntity = manager.getControlledEntity(potentialListenerEntityId);
			if (controlledEntity)
			{
				if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
				{
					continue;
				}
				auto const& configNode = controlledEntity->getCurrentConfigurationNode();
				for (auto const& streamInput : configNode.streamInputs)
				{
					auto* streamInputDynamicModel = streamInput.second.dynamicModel;
					if (streamInputDynamicModel)
					{
						if (streamInputDynamicModel->connectionState.talkerStream.entityID == talkerEntityId)
						{
							disconnectedStreams.push_back(streamInputDynamicModel->connectionState);
						}
					}
				}
			}
		}
		return disconnectedStreams;
	}

	/**
	* Gets all connections of an input.
	* @param entityId			The id of the entity to.
	* @param configurationIndex	The index of the configuration to use when tracing.
	* @param audioUnitIndex		The index of the audio unit to use when tracing.
	* @param streamPortIndex	The index of the stream port to use when tracing.
	* @param clusterIndex		The index of the cluster to use when tracing.
	* @param baseCluster		The offset of the cluster indexing. To be used in conjunction with clusterIndex.
	* @param clusterChannel		The channel offset inside the cluster.
	* @return The results stored in a struct.
	*/
	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnections(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, la::avdecc::entity::model::ClusterIndex const baseCluster, std::uint16_t const clusterChannel) const noexcept
	{
		SourceChannelIdentification sourceChannelIdentification;
		sourceChannelIdentification.sourceConfigurationIndex = configurationIndex;
		sourceChannelIdentification.sourceAudioUnitIndex = audioUnitIndex;
		sourceChannelIdentification.sourceStreamPortIndex = streamPortIndex;
		sourceChannelIdentification.sourceClusterIndex = clusterIndex;
		sourceChannelIdentification.sourceBaseCluster = baseCluster;
		sourceChannelIdentification.sourceClusterChannel = clusterChannel;
		sourceChannelIdentification.forward = true;

		auto result = std::make_shared<TargetConnectionInformations>();
		result->sourceClusterChannelInfo = sourceChannelIdentification;
		result->sourceEntityId = entityId;

		// find channel connections via connection matrix + stream connections.
		// an output channel can be connected to one or multiple input channels on different devices or to none.

		auto const& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityId);
		if (!controlledEntity)
		{
			return result;
		}
		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return result;
		}

		auto const& entityNode = controlledEntity->getEntityNode();
		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getConfigurationNode(configurationIndex);
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// one of the given parameters is invalid.
			return result;
		}

		la::avdecc::entity::model::AudioMappings mappings;
		try
		{
			if (configurationNode.audioUnits.size() > audioUnitIndex && configurationNode.audioUnits.at(audioUnitIndex).streamPortOutputs.size() > streamPortIndex)
			{
				if (configurationNode.audioUnits.at(audioUnitIndex).streamPortOutputs.at(streamPortIndex).dynamicModel)
				{
					mappings = configurationNode.audioUnits.at(audioUnitIndex).streamPortOutputs.at(streamPortIndex).dynamicModel->dynamicAudioMap;
				}
			}
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// one of the given parameters is invalid.
			return result;
		}

		std::vector<std::pair<la::avdecc::entity::model::StreamIndex, std::uint16_t>> sourceStreams;
		for (auto const& mapping : mappings)
		{
			if (mapping.clusterOffset == clusterIndex - baseCluster && clusterChannel == mapping.clusterChannel)
			{
				sourceStreams.push_back(std::make_pair(mapping.streamIndex, mapping.streamChannel));
			}
		}

		auto const& streamConnections = getAllStreamOutputConnections(entityId);

		// find out the connected streams:
		for (auto const& stream : sourceStreams)
		{
			auto sourceStreamChannel = stream.second;
			for (auto const& streamConnection : streamConnections)
			{
				if (streamConnection.talkerStream.streamIndex == stream.first)
				{
					// after getting the connected stream, resolve the underlying channels:
					auto targetControlledEntity = manager.getControlledEntity(streamConnection.listenerStream.entityID);
					if (!targetControlledEntity)
					{
						continue;
					}
					if (!targetControlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
					{
						continue;
					}
					auto const& targetEntityNode = targetControlledEntity->getEntityNode();
					if (targetEntityNode.dynamicModel)
					{
						auto const& targetConfigurationNode = targetControlledEntity->getConfigurationNode(targetEntityNode.dynamicModel->currentConfiguration);

						std::set<la::avdecc::entity::model::StreamIndex> relevantStreamIndexes;
						for (auto const& redundantStreamOutput : targetConfigurationNode.redundantStreamInputs)
						{
							relevantStreamIndexes.insert(redundantStreamOutput.second.primaryStream->descriptorIndex);
						}
						for (auto const& streamOutput : targetConfigurationNode.streamInputs)
						{
							if (!streamOutput.second.isRedundant)
							{
								relevantStreamIndexes.insert(streamOutput.first);
							}
						}

						// find correct index of audio unit and stream port index:
						for (auto const& audioUnitKV : targetConfigurationNode.audioUnits)
						{
							for (auto const& streamPortInputKV : audioUnitKV.second.streamPortInputs)
							{
								if (streamPortInputKV.second.dynamicModel)
								{
									auto targetMappings = streamPortInputKV.second.dynamicModel->dynamicAudioMap;
									for (auto const& mapping : targetMappings)
									{
										if (relevantStreamIndexes.find(mapping.streamIndex) == relevantStreamIndexes.end())
										{
											continue;
										}
										// the source stream channel is connected to the corresponding target stream channel.
										if (mapping.streamIndex == streamConnection.listenerStream.streamIndex && mapping.streamChannel == sourceStreamChannel)
										{
											auto connectionInformation = std::make_shared<TargetConnectionInformation>();

											connectionInformation->targetEntityId = streamConnection.listenerStream.entityID;
											connectionInformation->streamChannel = sourceStreamChannel;
											connectionInformation->sourceStreamIndex = streamConnection.talkerStream.streamIndex;
											connectionInformation->targetStreamIndex = streamConnection.listenerStream.streamIndex;
											connectionInformation->targetClusterChannels.push_back(std::make_pair(mapping.clusterOffset, mapping.clusterChannel));
											connectionInformation->targetAudioUnitIndex = audioUnitKV.first;
											if (streamPortInputKV.second.staticModel)
											{
												connectionInformation->targetBaseCluster = streamPortInputKV.second.staticModel->baseCluster;
											}
											connectionInformation->targetStreamPortIndex = streamPortInputKV.first;
											connectionInformation->isSourceRedundant = controlledEntity->getStreamOutputNode(configurationNode.descriptorIndex, stream.first).isRedundant;
											connectionInformation->isTargetRedundant = targetControlledEntity->getStreamInputNode(targetConfigurationNode.descriptorIndex, mapping.streamIndex).isRedundant;
											result->targets.push_back(connectionInformation);
										}
									}
								}
							}
						}
					}
				}
			}
		}

		return result;
	}

	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnectionsReverse(la::avdecc::UniqueIdentifier entityId, la::avdecc::UniqueIdentifier const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, la::avdecc::entity::model::ClusterIndex const baseCluster, std::uint16_t const clusterChannel) noexcept
	{
		bool entityAlreadyInMap = false;
		SourceChannelIdentification sourceChannelIdentification;
		sourceChannelIdentification.forward = false;
		sourceChannelIdentification.sourceConfigurationIndex = configurationIndex;
		sourceChannelIdentification.sourceAudioUnitIndex = audioUnitIndex;
		sourceChannelIdentification.sourceStreamPortIndex = streamPortIndex;
		sourceChannelIdentification.sourceClusterIndex = clusterIndex;
		sourceChannelIdentification.sourceBaseCluster = baseCluster;
		sourceChannelIdentification.sourceClusterChannel = clusterChannel;
		auto const& listenerChannelMappingsIt = _listenerChannelMappings.find(entityId);
		if (listenerChannelMappingsIt != _listenerChannelMappings.end())
		{
			entityAlreadyInMap = true;
			auto const& entityChannelMappings = listenerChannelMappingsIt->second->channelMappings;
			auto const& entityChannelMappingsIt = entityChannelMappings.find(sourceChannelIdentification);
			if (entityChannelMappingsIt != entityChannelMappings.end())
			{
				return entityChannelMappingsIt->second;
			}
		}

		auto targetConnectionInfo = determineChannelConnectionsReverse(entityId, configurationIndex, audioUnitIndex, streamPortIndex, clusterIndex, baseCluster, clusterChannel);

		// create the entity entry if not existant yet
		if (!entityAlreadyInMap)
		{
			_listenerChannelMappings.emplace(entityId, std::make_shared<SourceChannelConnections>());
		}

		_listenerChannelMappings.at(entityId)->channelMappings.emplace(sourceChannelIdentification, targetConnectionInfo);
		return targetConnectionInfo;
	}

	/**
	* Gets all connections of an output. (Tracing is done in the reverse direction from output to input)
	* @param entityId			The id of the entity to.
	* @param configurationIndex	The index of the configuration to use when tracing.
	* @param audioUnitIndex		The index of the audio unit to use when tracing.
	* @param streamPortIndex	The index of the stream port to use when tracing.
	* @param clusterIndex		The index of the cluster to use when tracing.
	* @param baseCluster		The offset of the cluster indexing. To be used in conjunction with clusterIndex.
	* @param clusterChannel		The channel offset inside the cluster.
	* @return The results stored in a struct.
	*/
	virtual std::shared_ptr<TargetConnectionInformations> determineChannelConnectionsReverse(la::avdecc::UniqueIdentifier const entityId, la::avdecc::UniqueIdentifier const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, la::avdecc::entity::model::ClusterIndex const baseCluster, std::uint16_t const clusterChannel) const noexcept
	{
		SourceChannelIdentification sourceChannelIdentification;
		sourceChannelIdentification.sourceConfigurationIndex = configurationIndex; // can be removed in the other functions as well
		sourceChannelIdentification.sourceAudioUnitIndex = audioUnitIndex; // can be removed in the other functions as well
		sourceChannelIdentification.sourceStreamPortIndex = streamPortIndex;
		sourceChannelIdentification.sourceClusterIndex = clusterIndex;
		sourceChannelIdentification.sourceBaseCluster = baseCluster;
		sourceChannelIdentification.sourceClusterChannel = clusterChannel;
		sourceChannelIdentification.forward = false;

		auto result = std::make_shared<TargetConnectionInformations>();
		result->sourceClusterChannelInfo = sourceChannelIdentification;
		result->sourceEntityId = entityId;

		// find channel connections via connection matrix + stream connections.
		// an output channel can be connected to one or multiple input channels on different devices or to none.

		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityId);

		if (!controlledEntity)
		{
			return result;
		}
		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return result;
		}

		auto const& entityNode = controlledEntity->getEntityNode();

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getConfigurationNode(configurationIndex);
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// one of the given parameters is invalid.
			return result;
		}

		la::avdecc::entity::model::AudioMappings mappings;
		try
		{
			if (configurationNode.audioUnits.size() > audioUnitIndex && configurationNode.audioUnits.at(audioUnitIndex).streamPortOutputs.size() > streamPortIndex)
			{
				if (configurationNode.audioUnits.at(audioUnitIndex).streamPortInputs.at(streamPortIndex).dynamicModel)
				{
					mappings = configurationNode.audioUnits.at(audioUnitIndex).streamPortInputs.at(streamPortIndex).dynamicModel->dynamicAudioMap;
					//mappings = configurationNode.audioUnits.at(audioUnitIndex).streamPortInputs.at(streamPortIndex).staticModel->has;
				}
			}
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// one of the given parameters is invalid.
			return result;
		}

		// find all streams this cluster is connected to. (Should only be 1, but can be multiple on redundant connections)
		std::vector<std::pair<la::avdecc::entity::model::StreamIndex, std::uint16_t>> sourceStreams;
		for (auto const& mapping : mappings)
		{
			if (mapping.clusterOffset == clusterIndex - baseCluster && clusterChannel == mapping.clusterChannel)
			{
				sourceStreams.push_back(std::make_pair(mapping.streamIndex, mapping.streamChannel));
			}
		}

		// find out the connected streams:
		for (auto const& stream : sourceStreams)
		{
			if (controlledEntity->getCurrentConfigurationNode().streamInputs.at(stream.first).dynamicModel)
			{
				auto connectedTalker = controlledEntity->getCurrentConfigurationNode().streamInputs.at(stream.first).dynamicModel->connectionState.talkerStream.entityID;
				auto connectedTalkerStreamIndex = controlledEntity->getCurrentConfigurationNode().streamInputs.at(stream.first).dynamicModel->connectionState.talkerStream.streamIndex;


				auto sourceStreamChannel = stream.second;

				// after getting the connected stream, resolve the underlying channels:
				auto targetControlledEntity = manager.getControlledEntity(connectedTalker);

				if (!targetControlledEntity)
				{
					continue;
				}

				if (!targetControlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
				{
					continue;
				}
				auto const& targetEntityNode = targetControlledEntity->getEntityNode();
				if (targetEntityNode.dynamicModel)
				{
					auto const& targetConfigurationNode = targetControlledEntity->getConfigurationNode(targetEntityNode.dynamicModel->currentConfiguration);

					std::set<la::avdecc::entity::model::StreamIndex> relevantStreamIndexes;
					for (auto const& redundantStreamOutput : targetConfigurationNode.redundantStreamOutputs)
					{
						relevantStreamIndexes.insert(redundantStreamOutput.second.primaryStream->descriptorIndex);
					}
					for (auto const& streamOutput : targetConfigurationNode.streamOutputs)
					{
						if (!streamOutput.second.isRedundant)
						{
							relevantStreamIndexes.insert(streamOutput.first);
						}
					}

					// find correct index of audio unit and stream port index:
					for (auto const& audioUnitKV : targetConfigurationNode.audioUnits)
					{
						for (auto const& streamPortOutputKV : audioUnitKV.second.streamPortOutputs)
						{
							if (streamPortOutputKV.second.dynamicModel)
							{
								auto targetMappings = streamPortOutputKV.second.dynamicModel->dynamicAudioMap;
								for (auto const& mapping : targetMappings)
								{
									if (relevantStreamIndexes.find(mapping.streamIndex) == relevantStreamIndexes.end())
									{
										continue;
									}

									// the source stream channel is connected to the corresponding target stream channel.
									if (mapping.streamIndex == connectedTalkerStreamIndex && mapping.streamChannel == sourceStreamChannel)
									{
										auto connectionInformation = std::make_shared<TargetConnectionInformation>();
										connectionInformation->isSourceRedundant = controlledEntity->getStreamInputNode(configurationNode.descriptorIndex, stream.first).isRedundant;
										connectionInformation->targetEntityId = connectedTalker;
										connectionInformation->sourceStreamIndex = stream.first;
										connectionInformation->targetStreamIndex = connectedTalkerStreamIndex;
										connectionInformation->streamChannel = sourceStreamChannel;
										connectionInformation->targetClusterChannels.push_back(std::make_pair(mapping.clusterOffset, mapping.clusterChannel));
										connectionInformation->targetAudioUnitIndex = audioUnitKV.first;
										if (streamPortOutputKV.second.staticModel)
										{
											connectionInformation->targetBaseCluster = streamPortOutputKV.second.staticModel->baseCluster;
										}
										connectionInformation->targetStreamPortIndex = streamPortOutputKV.first;
										connectionInformation->isTargetRedundant = targetControlledEntity->getStreamOutputNode(targetConfigurationNode.descriptorIndex, mapping.streamIndex).isRedundant;
										result->targets.push_back(connectionInformation);
									}
								}
							}
						}
					}
				}
			}
		}

		return result;
	}

	/**
	* Iterates over the list of known entities and returns all connections that originate from the given talker.
	*/
	std::vector<la::avdecc::controller::model::StreamConnectionState> getAllStreamOutputConnections(la::avdecc::UniqueIdentifier const& talkerEntityId) const noexcept
	{
		std::vector<la::avdecc::controller::model::StreamConnectionState> disconnectedStreams;
		auto const& manager = avdecc::ControllerManager::getInstance();
		for (auto const& potentialListenerEntityId : _entities)
		{
			auto const controlledEntity = manager.getControlledEntity(potentialListenerEntityId);
			if (controlledEntity)
			{
				if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
				{
					continue;
				}
				auto const& configNode = controlledEntity->getCurrentConfigurationNode();
				for (auto const& streamInput : configNode.streamInputs)
				{
					auto* streamInputDynamicModel = streamInput.second.dynamicModel;
					if (streamInputDynamicModel)
					{
						if (streamInputDynamicModel->connectionState.talkerStream.entityID == talkerEntityId)
						{
							disconnectedStreams.push_back(streamInputDynamicModel->connectionState);
						}
					}
				}
			}
		}
		return disconnectedStreams;
	}


	/**
	* Gets all redundant stream outputs of a primary stream output if there are any.
	*/
	virtual std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> getRedundantStreamOutputsForPrimary(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex primaryStreamIndex) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityId);

		if (!controlledEntity)
		{
			return std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*>();
		}
		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*>();
		}

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*>();
		}

		for (auto const& redundantStreamOutput : configurationNode.redundantStreamOutputs)
		{
			if (redundantStreamOutput.second.primaryStream->descriptorIndex == primaryStreamIndex)
			{
				return redundantStreamOutput.second.redundantStreams;
			}
		}
		return std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*>();
	}

	/**
	* Gets all redundant stream inputs of a primary stream input if there are any.
	*/
	virtual std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> getRedundantStreamInputsForPrimary(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex primaryStreamIndex) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityId);

		if (!controlledEntity)
		{
			return std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*>();
		}
		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*>();
		}

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*>();
		}

		for (auto const& redundantStreamInput : configurationNode.redundantStreamInputs)
		{
			if (redundantStreamInput.second.primaryStream->descriptorIndex == primaryStreamIndex)
			{
				return redundantStreamInput.second.redundantStreams;
			}
		}
		return std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*>();
	}

private:
};

/**
	* Singleton implementation. Gets the instance.
	* @return The channel connection manager instance.
	*/
ChannelConnectionManager& ChannelConnectionManager::getInstance() noexcept
{
	static ChannelConnectionManagerImpl s_manager{};

	return s_manager;
}

} // namespace avdecc
