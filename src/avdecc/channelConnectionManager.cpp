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

	mediaClock::SequentialAsyncCommandExecuter _sequentialAcmpCommandExecuter{};

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

		connect(&_sequentialAcmpCommandExecuter, &mediaClock::SequentialAsyncCommandExecuter::completed, this,
			[this](mediaClock::CommandExecutionErrors errors)
			{
				CreateConnectionsInfo info;
				info.connectionCreationErrors = errors;
				emit createChannelConnectionsFinished(info);
			});
	}

	/**
	* Destructor.
	*/
	~ChannelConnectionManagerImpl() noexcept {}

private:
	using StreamChannelConnection = std::tuple<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex, uint16_t>;
	using StreamChannelConnections = std::vector<StreamChannelConnection>;

	struct FindStreamConnectionResult
	{
		StreamChannelConnections connectionsToCreate;
		la::avdecc::entity::model::AudioMappings listenerDynamicMappingsToRemove;
		bool unallowedRemovalOfUnusedAudioMappingsNecessary{ false };
	};

	using StreamPortAudioMappings = std::map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings>;
	using StreamChannelMappings = std::map<la::avdecc::entity::model::StreamIndex, StreamPortAudioMappings>;
	using StreamConnections = std::vector<std::pair<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex>>;
	using StreamFormatChanges = std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamFormat>;
	struct CheckChannelCreationsPossibleResult
	{
		ChannelConnectResult connectionCheckResult;
		StreamChannelMappings overriddenMappingsListener;
		StreamChannelMappings newMappingsTalker;
		StreamChannelMappings newMappingsListener;

		StreamConnections newStreamConnections;
	};

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
	Q_SLOT void onStreamConnectionChanged(la::avdecc::entity::model::StreamConnectionState const& streamConnectionState)
	{
		// check if it is a redundant (secondary) stream, if so stop processing. (redundancy is handled inside the determineChannelConnectionsReverse method)
		/*if (!isInputStreamPrimaryOrNonRedundant(streamConnectionState.listenerStream))
		{
			return;
		}*/

		auto listenerChannelMappingIt = _listenerChannelMappings.find(streamConnectionState.listenerStream.entityID);

		if (listenerChannelMappingIt != _listenerChannelMappings.end())
		{
			auto virtualTalkerIndex = getRedundantVirtualIndexFromOutputStreamIndex(streamConnectionState.talkerStream);
			auto virtualListenerIndex = getRedundantVirtualIndexFromInputStreamIndex(streamConnectionState.listenerStream);

			std::set<std::pair<la::avdecc::UniqueIdentifier, ChannelIdentification>> listenerChannelsToUpdate;
			std::set<std::pair<la::avdecc::UniqueIdentifier, ChannelIdentification>> updatedListenerChannels;
			auto connectionInfo = listenerChannelMappingIt->second;

			// if a stream was disconnected, only update the entries that have a connection currently
			// and if the stream was connected, only update the entries that have no connections yet.
			if (streamConnectionState.state == la::avdecc::entity::model::StreamConnectionState::State::NotConnected)
			{
				for (auto const& mappingKV : connectionInfo->channelMappings)
				{
					for (auto const& target : mappingKV.second->targets)
					{
						// special handling for redundant connections, as the channel connection still exists if only one of the connections is active.
						if (virtualListenerIndex)
						{
							if (*virtualListenerIndex == *target->sourceVirtualIndex)
							{
								auto& manager = avdecc::ControllerManager::getInstance();
								auto controlledEntity = manager.getControlledEntity(streamConnectionState.listenerStream.entityID);
								auto const configIndex = controlledEntity->getCurrentConfigurationNode().descriptorIndex;
								auto const& redundantListenerStreamNode = controlledEntity->getRedundantStreamInputNode(configIndex, *virtualListenerIndex);
								bool atLeastOneConnected = false;
								for (auto const& [streamIndex, streamNode] : redundantListenerStreamNode.redundantStreams)
								{
									if (controlledEntity->getStreamInputNode(configIndex, streamIndex).dynamicModel->connectionState.state != la::avdecc::entity::model::StreamConnectionState::State::NotConnected)
									{
										atLeastOneConnected = true;
										break;
									}
								}
								// check if at least one is still connected
								// if not, insert into listenerChannelsToUpdate
								if (!atLeastOneConnected)
								{
									auto channel = std::make_pair(streamConnectionState.listenerStream.entityID, mappingKV.first);
									listenerChannelsToUpdate.insert(channel);
								}
							}
						}
						else if (target->sourceStreamIndex == streamConnectionState.listenerStream.streamIndex && target->targetStreamIndex == streamConnectionState.talkerStream.streamIndex)
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
						// check if the source (listener) is the same but the target (talker) changed
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
								auto const configurationIndex = controlledEntity->getCurrentConfigurationNode().descriptorIndex;
								auto const streamPortIndex = *mappingKV.second->sourceClusterChannelInfo->streamPortIndex;
								auto const& streamPortInputNode = controlledEntity->getStreamPortInputNode(configurationIndex, streamPortIndex);
								auto const* const streamPortInputDynamicModel = streamPortInputNode.dynamicModel;
								if (streamPortInputDynamicModel)
								{
									mappings = streamPortInputDynamicModel->dynamicAudioMap;
								}
							}
						}
						catch (la::avdecc::controller::ControlledEntity::Exception const&)
						{
						}

						for (auto const& mapping : mappings)
						{
							auto const clusterIndex = mappingKV.second->sourceClusterChannelInfo->clusterIndex;
							auto const baseCluster = *mappingKV.second->sourceClusterChannelInfo->baseCluster;
							auto const clusterChannel = mappingKV.second->sourceClusterChannelInfo->clusterChannel;
							auto const streamIndex = streamConnectionState.listenerStream.streamIndex;

							auto virtualStreamIndex = getRedundantVirtualIndexFromInputStreamIndex(streamConnectionState.listenerStream);
							if (virtualListenerIndex)
							{
								if (*virtualListenerIndex == *virtualStreamIndex)
								{
									auto& manager = avdecc::ControllerManager::getInstance();
									auto controlledEntity = manager.getControlledEntity(streamConnectionState.listenerStream.entityID);
									auto const configIndex = controlledEntity->getCurrentConfigurationNode().descriptorIndex;
									auto const& redundantListenerStreamNode = controlledEntity->getRedundantStreamInputNode(configIndex, *virtualListenerIndex);
									bool atLeastOneConnected = false;
									for (auto const& [redundantListenerStreamIndex, streamNode] : redundantListenerStreamNode.redundantStreams)
									{
										if (controlledEntity->getStreamInputNode(configIndex, redundantListenerStreamIndex).dynamicModel->connectionState.state != la::avdecc::entity::model::StreamConnectionState::State::NotConnected)
										{
											atLeastOneConnected = true;
											break;
										}
									}
									// check if at least one is connected
									// if so, insert into listenerChannelsToUpdate
									if (atLeastOneConnected)
									{
										auto channel = std::make_pair(streamConnectionState.listenerStream.entityID, mappingKV.first);
										listenerChannelsToUpdate.insert(channel);
									}
								}
							}
							else if (clusterIndex + baseCluster == mapping.clusterOffset && clusterChannel == mapping.clusterChannel && mapping.streamIndex == streamIndex)
							{
								// this propably needs a refresh
								auto const& channelIdentification = mappingKV.first;
								auto channel = std::make_pair(streamConnectionState.listenerStream.entityID, channelIdentification);
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
				auto newListenerChannelConnections = determineChannelConnectionsReverse(listenerChannelToUpdate.first, sourceInfo);
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
	Q_SLOT void onStreamPortAudioMappingsChanged(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex)
	{
		std::set<std::pair<la::avdecc::UniqueIdentifier, ChannelIdentification>> listenerChannelsToUpdate;
		std::set<std::pair<la::avdecc::UniqueIdentifier, ChannelIdentification>> updatedListenerChannels;

		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
		{
			if (_listenerChannelMappings.find(entityId) != _listenerChannelMappings.end())
			{
				auto const& listenerMappings = _listenerChannelMappings.at(entityId)->channelMappings;
				for (auto const& mappingKV : listenerMappings)
				{
					if (mappingKV.first.streamPortIndex == streamPortIndex)
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
							if (streamInputDynamicModel && streamInputDynamicModel->connectionState.state == la::avdecc::entity::model::StreamConnectionState::State::Connected)
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

			auto newListenerChannelConnections = determineChannelConnectionsReverse(listenerChannelToUpdateKV.first, sourceInfo);
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
	virtual bool isInputStreamPrimaryOrNonRedundant(la::avdecc::entity::model::StreamIdentification const& streamIdentification) const noexcept
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
	* Gets the virtual index of a input stream if it is redundant, otherwise std::nullopt is returned.
	*/
	std::optional<la::avdecc::controller::model::VirtualIndex> getRedundantVirtualIndexFromInputStreamIndex(la::avdecc::entity::model::StreamIdentification streamIdentification) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(streamIdentification.entityID);
		if (controlledEntity)
		{
			try
			{
				auto const& configNode = controlledEntity->getConfigurationNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex);
				auto const& streamInputNode = configNode.streamInputs.at(streamIdentification.streamIndex);
				if (!streamInputNode.isRedundant)
				{
					return std::nullopt;
				}
				else
				{
					for (auto const& [virtualIndex, redundantStreamNode] : configNode.redundantStreamInputs)
					{
						for (auto const& [streamIndex, streamNode] : redundantStreamNode.redundantStreams)
						{
							if (streamIndex == streamIdentification.streamIndex)
							{
								return virtualIndex;
							}
						}
					}
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
				return std::nullopt;
			}
		}

		return std::nullopt;
	}

	/**
	* Gets the virtual index of a output stream if it is redundant, otherwise std::nullopt is returned.
	*/
	std::optional<la::avdecc::controller::model::VirtualIndex> getRedundantVirtualIndexFromOutputStreamIndex(la::avdecc::entity::model::StreamIdentification streamIdentification) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(streamIdentification.entityID);
		if (controlledEntity)
		{
			try
			{
				auto const& configNode = controlledEntity->getConfigurationNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex);
				auto const& streamOutputNode = configNode.streamOutputs.at(streamIdentification.streamIndex);
				if (!streamOutputNode.isRedundant)
				{
					return std::nullopt;
				}
				else
				{
					for (auto const& [virtualIndex, redundantStreamNode] : configNode.redundantStreamOutputs)
					{
						for (auto const& [streamIndex, streamNode] : redundantStreamNode.redundantStreams)
						{
							if (streamIndex == streamIdentification.streamIndex)
							{
								return virtualIndex;
							}
						}
					}
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
				return std::nullopt;
			}
		}

		return std::nullopt;
	}

	std::vector<std::pair<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex>> getRedundantStreamIndexPairs(la::avdecc::UniqueIdentifier talkerEntityId, la::avdecc::controller::model::VirtualIndex talkerStreamVirtualIndex, la::avdecc::UniqueIdentifier listenerEntityId, la::avdecc::controller::model::VirtualIndex listenerStreamVirtualIndex) const noexcept
	{
		std::vector<std::pair<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex>> result;
		auto const& manager = avdecc::ControllerManager::getInstance();

		auto const controlledTalkerEntity = manager.getControlledEntity(talkerEntityId);
		auto const controlledListenerEntity = manager.getControlledEntity(listenerEntityId);

		if (controlledTalkerEntity && controlledListenerEntity)
		{
			try
			{
				auto redundantStreamOutputNode = controlledTalkerEntity->getRedundantStreamOutputNode(controlledTalkerEntity->getCurrentConfigurationNode().descriptorIndex, talkerStreamVirtualIndex);
				auto redundantStreamInputNode = controlledListenerEntity->getRedundantStreamInputNode(controlledTalkerEntity->getCurrentConfigurationNode().descriptorIndex, listenerStreamVirtualIndex);

				auto redundantStreamOutputsIt = redundantStreamOutputNode.redundantStreams.begin();
				auto redundantStreamInputsIt = redundantStreamInputNode.redundantStreams.begin();

				while (redundantStreamOutputsIt != redundantStreamOutputNode.redundantStreams.end() && redundantStreamInputsIt != redundantStreamInputNode.redundantStreams.end())
				{
					result.push_back(std::make_pair(redundantStreamOutputsIt->first, redundantStreamInputsIt->first));


					redundantStreamOutputsIt++;
					redundantStreamInputsIt++;
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
				return result;
			}
		}
		return result;
	}

	/**
	* Iterates over the list of known entities and returns all connections that originate from the given talker.
	*/
	std::vector<la::avdecc::entity::model::StreamConnectionState> getAllStreamOutputConnections(la::avdecc::UniqueIdentifier talkerEntityId)
	{
		std::vector<la::avdecc::entity::model::StreamConnectionState> disconnectedStreams;
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
	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnections(la::avdecc::UniqueIdentifier const& entityId, ChannelIdentification sourceChannelIdentification) const noexcept
	{
		// make sure forward is set to true
		sourceChannelIdentification.direction = ChannelConnectionDirection::OutputToInput;

		auto result = std::make_shared<TargetConnectionInformations>();
		result->sourceClusterChannelInfo = sourceChannelIdentification;
		result->sourceEntityId = entityId;
		if (!sourceChannelIdentification.streamPortIndex || !sourceChannelIdentification.audioUnitIndex || !sourceChannelIdentification.baseCluster)
		{
			return result; // incomplete arguments.
		}

		auto const configurationIndex = sourceChannelIdentification.configurationIndex;
		auto const audioUnitIndex = *sourceChannelIdentification.audioUnitIndex;
		auto const streamPortIndex = *sourceChannelIdentification.streamPortIndex;
		auto const clusterIndex = sourceChannelIdentification.clusterIndex;
		auto const baseCluster = *sourceChannelIdentification.baseCluster;
		auto const clusterChannel = sourceChannelIdentification.clusterChannel;

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
			auto const& streamPortOutputNode = controlledEntity->getStreamPortOutputNode(configurationIndex, streamPortIndex);
			auto const* const streamPortOutputDynamicModel = streamPortOutputNode.dynamicModel;
			if (streamPortOutputDynamicModel)
			{
				mappings = streamPortOutputDynamicModel->dynamicAudioMap;
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
									auto const& targetMappings = streamPortInputKV.second.dynamicModel->dynamicAudioMap;
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
											connectionInformation->sourceVirtualIndex = getRedundantVirtualIndexFromOutputStreamIndex(streamConnection.talkerStream);
											connectionInformation->targetVirtualIndex = getRedundantVirtualIndexFromInputStreamIndex(streamConnection.listenerStream);
											if (connectionInformation->sourceVirtualIndex && connectionInformation->targetVirtualIndex)
											{
												// both redundant
												connectionInformation->streamPairs = getRedundantStreamIndexPairs(streamConnection.listenerStream.entityID, *connectionInformation->sourceVirtualIndex, connectionInformation->targetEntityId, *connectionInformation->targetVirtualIndex);
											}
											else
											{
												connectionInformation->streamPairs = { std::make_pair(connectionInformation->sourceStreamIndex, connectionInformation->targetStreamIndex) };
											}

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

	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnectionsReverse(la::avdecc::UniqueIdentifier const& entityId, ChannelIdentification const& sourceChannelIdentification) noexcept
	{
		bool entityAlreadyInMap = false;

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

		// not cached yet, determine it:
		auto targetConnectionInfo = determineChannelConnectionsReverse(entityId, sourceChannelIdentification);

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
	virtual std::shared_ptr<TargetConnectionInformations> determineChannelConnectionsReverse(la::avdecc::UniqueIdentifier const& entityId, ChannelIdentification const& sourceChannelIdentification) const noexcept
	{
		auto result = std::make_shared<TargetConnectionInformations>();
		result->sourceClusterChannelInfo = sourceChannelIdentification;
		result->sourceEntityId = entityId;

		// make sure direction is correct
		result->sourceClusterChannelInfo->direction = ChannelConnectionDirection::InputToOutput;

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

		if (!sourceChannelIdentification.streamPortIndex || !sourceChannelIdentification.audioUnitIndex || !sourceChannelIdentification.baseCluster)
		{
			return result; // incomplete arguments.
		}

		auto const configurationIndex = sourceChannelIdentification.configurationIndex;
		auto const streamPortIndex = *sourceChannelIdentification.streamPortIndex;
		auto const audioUnitIndex = *sourceChannelIdentification.audioUnitIndex;
		auto const clusterIndex = sourceChannelIdentification.clusterIndex;
		auto const baseCluster = *sourceChannelIdentification.baseCluster;
		auto const clusterChannel = sourceChannelIdentification.clusterChannel;

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
			auto const& streamPortInputNode = controlledEntity->getStreamPortInputNode(configurationIndex, streamPortIndex);
			auto const* const streamPortInputDynamicModel = streamPortInputNode.dynamicModel;
			if (streamPortInputDynamicModel)
			{
				mappings = streamPortInputDynamicModel->dynamicAudioMap;
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
			auto const& streamInput = controlledEntity->getCurrentConfigurationNode().streamInputs.at(stream.first);
			auto const* streamInputDynamicModel = streamInput.dynamicModel;
			if (streamInputDynamicModel)
			{
				auto connectedTalker = streamInputDynamicModel->connectionState.talkerStream.entityID;
				auto connectedTalkerStreamIndex = streamInputDynamicModel->connectionState.talkerStream.streamIndex;

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

					std::map<la::avdecc::entity::model::StreamIndex, std::vector<la::avdecc::entity::model::StreamIndex>> relevantPrimaryStreamIndexes;
					std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex> relevantRedundantStreamIndexes;

					// if the primary is not connected but the secondary is, the channel connection is still returned
					for (auto const& redundantStreamOutput : targetConfigurationNode.redundantStreamOutputs)
					{
						auto primaryStreamIndex = redundantStreamOutput.second.primaryStream->descriptorIndex;
						std::vector<la::avdecc::entity::model::StreamIndex> redundantStreams;
						for (auto const& [redundantStreamIndex, redundantStream] : redundantStreamOutput.second.redundantStreams)
						{
							if (redundantStreamIndex != primaryStreamIndex)
							{
								redundantStreams.push_back(redundantStreamIndex);
								relevantRedundantStreamIndexes.emplace(redundantStreamIndex, primaryStreamIndex);
							}
						}
						relevantPrimaryStreamIndexes.emplace(primaryStreamIndex, redundantStreams);
					}
					for (auto const& streamOutput : targetConfigurationNode.streamOutputs)
					{
						if (!streamOutput.second.isRedundant)
						{
							relevantPrimaryStreamIndexes.emplace(streamOutput.first, std::vector<la::avdecc::entity::model::StreamIndex>{});
						}
					}

					// find correct index of audio unit and stream port index:
					for (auto const& audioUnitKV : targetConfigurationNode.audioUnits)
					{
						for (auto const& streamPortOutputKV : audioUnitKV.second.streamPortOutputs)
						{
							if (streamPortOutputKV.second.dynamicModel)
							{
								auto const& targetMappings = streamPortOutputKV.second.dynamicModel->dynamicAudioMap;
								for (auto const& mapping : targetMappings)
								{
									auto primaryStreamIndex = relevantPrimaryStreamIndexes.find(mapping.streamIndex);
									if (primaryStreamIndex != relevantPrimaryStreamIndexes.end())
									{
										// the source stream channel is connected to the corresponding target stream channel.
										if (mapping.streamIndex == connectedTalkerStreamIndex && mapping.streamChannel == sourceStreamChannel)
										{
											auto redundantIndices = primaryStreamIndex->second;
											for (auto const redundantIndex : redundantIndices)
											{
												relevantRedundantStreamIndexes.erase(redundantIndex);
											}

											la::avdecc::entity::model::StreamIdentification sourceStreamIdentification{ entityId, stream.first };
											la::avdecc::entity::model::StreamIdentification targetStreamIdentification{ connectedTalker, connectedTalkerStreamIndex };

											auto connectionInformation = std::make_shared<TargetConnectionInformation>();
											connectionInformation->targetEntityId = connectedTalker;
											connectionInformation->sourceStreamIndex = stream.first;
											connectionInformation->targetStreamIndex = connectedTalkerStreamIndex;
											connectionInformation->sourceVirtualIndex = getRedundantVirtualIndexFromInputStreamIndex(sourceStreamIdentification);
											connectionInformation->targetVirtualIndex = getRedundantVirtualIndexFromOutputStreamIndex(targetStreamIdentification);
											if (connectionInformation->sourceVirtualIndex && connectionInformation->targetVirtualIndex)
											{
												// both redundant
												connectionInformation->streamPairs = getRedundantStreamIndexPairs(connectionInformation->targetEntityId, *connectionInformation->targetVirtualIndex, entityId, *connectionInformation->sourceVirtualIndex);
											}
											else
											{
												connectionInformation->streamPairs = { std::make_pair(connectionInformation->targetStreamIndex, connectionInformation->sourceStreamIndex) };
											}
											connectionInformation->streamChannel = sourceStreamChannel;
											connectionInformation->targetClusterChannels.push_back(std::make_pair(mapping.clusterOffset, mapping.clusterChannel));
											connectionInformation->targetAudioUnitIndex = audioUnitKV.first;
											if (streamPortOutputKV.second.staticModel)
											{
												connectionInformation->targetBaseCluster = streamPortOutputKV.second.staticModel->baseCluster;
											}
											connectionInformation->targetStreamPortIndex = streamPortOutputKV.first;
											connectionInformation->isSourceRedundant = streamInput.isRedundant;
											connectionInformation->isTargetRedundant = connectionInformation->targetVirtualIndex != std::nullopt;

											result->targets.push_back(connectionInformation);
										}
									}
									else
									{
										auto streamIndex = relevantRedundantStreamIndexes.find(mapping.streamIndex);
										if (streamIndex != relevantRedundantStreamIndexes.end())
										{
											// if we land in here the primary is not connected, but a secundary is.
											auto primaryTalkerStreamIndex = streamIndex->second;
											if (mapping.streamIndex == connectedTalkerStreamIndex && mapping.streamChannel == sourceStreamChannel)
											{
												la::avdecc::entity::model::StreamIdentification sourceStreamIdentification{ entityId, stream.first };
												la::avdecc::entity::model::StreamIdentification targetStreamIdentification{ connectedTalker, connectedTalkerStreamIndex };

												auto connectionInformation = std::make_shared<TargetConnectionInformation>();
												connectionInformation->targetEntityId = connectedTalker;
												connectionInformation->sourceVirtualIndex = getRedundantVirtualIndexFromInputStreamIndex(sourceStreamIdentification);
												connectionInformation->targetVirtualIndex = getRedundantVirtualIndexFromOutputStreamIndex(targetStreamIdentification);
												connectionInformation->sourceStreamIndex = controlledEntity->getRedundantStreamInputNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex, *connectionInformation->sourceVirtualIndex).primaryStream->descriptorIndex;
												connectionInformation->targetStreamIndex = primaryTalkerStreamIndex;
												if (connectionInformation->sourceVirtualIndex && connectionInformation->targetVirtualIndex)
												{
													// both redundant
													connectionInformation->streamPairs = getRedundantStreamIndexPairs(connectionInformation->targetEntityId, *connectionInformation->targetVirtualIndex, entityId, *connectionInformation->sourceVirtualIndex);
												}
												else
												{
													connectionInformation->streamPairs = { std::make_pair(connectionInformation->targetStreamIndex, connectionInformation->sourceStreamIndex) };
												}
												connectionInformation->streamChannel = sourceStreamChannel;
												connectionInformation->targetClusterChannels.push_back(std::make_pair(mapping.clusterOffset, mapping.clusterChannel));
												connectionInformation->targetAudioUnitIndex = audioUnitKV.first;
												if (streamPortOutputKV.second.staticModel)
												{
													connectionInformation->targetBaseCluster = streamPortOutputKV.second.staticModel->baseCluster;
												}
												connectionInformation->targetStreamPortIndex = streamPortOutputKV.first;
												connectionInformation->isSourceRedundant = streamInput.isRedundant;
												connectionInformation->isTargetRedundant = connectionInformation->targetVirtualIndex != std::nullopt;

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
		}
		return result;
	}

	/**
	* Iterates over the list of known entities and returns all connections that originate from the given talker.
	*/
	std::vector<la::avdecc::entity::model::StreamConnectionState> getAllStreamOutputConnections(la::avdecc::UniqueIdentifier const& talkerEntityId) const noexcept
	{
		std::vector<la::avdecc::entity::model::StreamConnectionState> disconnectedStreams;
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


	/**
	* Gets all connections between two entities.
	* @param entityId			The id of the entity to.
	* @param configurationIndex	The index of the configuration to use when tracing.
	* @param audioUnitIndex		The index of the audio unit to use when tracing.
	* @param streamPortIndex	The index of the stream port to use when tracing.
	* @param clusterIndex		The index of the cluster to use when tracing.
	* @param baseCluster		The offset of the cluster indexing. To be used in conjunction with clusterIndex.
	* @param clusterChannel		The channel offset inside the cluster.
	* @return The results stored in a struct.
	*/
	virtual std::shared_ptr<TargetConnectionInformations> getAllChannelConnectionsBetweenDevices(la::avdecc::UniqueIdentifier const& sourceEntityId, la::avdecc::entity::model::StreamPortIndex const sourceStreamPortIndex, la::avdecc::UniqueIdentifier const& targetEntityId) const noexcept
	{
		auto result = std::make_shared<TargetConnectionInformations>();
		result->sourceEntityId = sourceEntityId;

		// find channel connections via connection matrix + stream connections.
		// an output channel can be connected to one or multiple input channels on different devices or to none.

		auto const& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(sourceEntityId);
		if (!controlledEntity)
		{
			return result;
		}

		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return result;
		}

		std::vector<std::pair<la::avdecc::entity::model::StreamIndex, std::uint16_t>> sourceStreams;

		auto const& streamPortOutputAudioMappings = controlledEntity->getStreamPortOutputAudioMappings(sourceStreamPortIndex);
		for (auto const& mapping : streamPortOutputAudioMappings)
		{
			sourceStreams.push_back(std::make_pair(mapping.streamIndex, mapping.streamChannel));
		}

		auto const& streamConnections = getAllStreamOutputConnections(sourceEntityId);

		try
		{
			// find out the connected streams:
			for (auto const& stream : sourceStreams)
			{
				auto sourceStreamChannel = stream.second;
				for (auto const& streamConnection : streamConnections)
				{
					if (targetEntityId == streamConnection.listenerStream.entityID && streamConnection.talkerStream.streamIndex == stream.first)
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
												connectionInformation->sourceStreamIndex = streamConnection.talkerStream.streamIndex;
												connectionInformation->targetStreamIndex = streamConnection.listenerStream.streamIndex;
												connectionInformation->isSourceRedundant = controlledEntity->getStreamOutputNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex, stream.first).isRedundant;
												connectionInformation->streamChannel = sourceStreamChannel;
												connectionInformation->targetClusterChannels.push_back(std::make_pair(mapping.clusterOffset, mapping.clusterChannel));
												connectionInformation->targetAudioUnitIndex = audioUnitKV.first;

												if (streamPortInputKV.second.staticModel)
												{
													connectionInformation->targetBaseCluster = streamPortInputKV.second.staticModel->baseCluster;
												}
												connectionInformation->targetStreamPortIndex = streamPortInputKV.first;
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
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// catch arbitrary exceptions (partially incompatible device, etc.)
			return result;
		}
		return result;
	}

	/**
	* Iterates over the list of known entities and returns all connections that originate from the given talker.
	*/
	std::vector<la::avdecc::entity::model::StreamConnectionState> getStreamOutputConnections(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::StreamIndex outputStreamIndex) const noexcept
	{
		std::vector<la::avdecc::entity::model::StreamConnectionState> disconnectedStreams;
		auto const& manager = avdecc::ControllerManager::getInstance();
		for (auto const& potentialListenerEntityId : _entities)
		{
			auto const controlledEntity = manager.getControlledEntity(potentialListenerEntityId);
			if (controlledEntity)
			{
				if (controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
				{
					auto const& configNode = controlledEntity->getCurrentConfigurationNode();
					for (auto const& streamInput : configNode.streamInputs)
					{
						auto* streamInputDynamicModel = streamInput.second.dynamicModel;
						if (streamInputDynamicModel)
						{
							if (streamInputDynamicModel->connectionState.talkerStream.entityID == talkerEntityId && streamInputDynamicModel->connectionState.talkerStream.streamIndex == outputStreamIndex)
							{
								disconnectedStreams.push_back(streamInputDynamicModel->connectionState);
							}
						}
					}
				}
			}
		}
		return disconnectedStreams;
	}

	/**
	* Checks if the given connections could be created on the current setup (allowing format changes)
	* could alse be used for normal channel connections in the matrix.
	* 
	* algorithm:
	* find all potiential connections that could be used.
	* first only streams are configured correctly already and connected to the listener
	* then correctly configured streams that are not connected
	* then unconnected wrongly configured
	*
	* @param talkerEntityId The id of the talker entity.
	* @param listenerEntityId The id of the listener entity.
	* @param	std::vector<std::pair<avdecc::ChannelIdentification>>
	* @param talkerToListenerChannelConnections The desired channel connection to be checked
	* @param allowRemovalOfUnusedAudioMappings Flag parameter to indicate if existing mappings can be overridden
	* @param channelUsageHint
	*/
	CheckChannelCreationsPossibleResult checkChannelCreationsPossible(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId, std::vector<std::pair<avdecc::ChannelIdentification, avdecc::ChannelIdentification>> const& talkerToListenerChannelConnections, bool allowRemovalOfUnusedAudioMappings, uint16_t channelUsageHint) const noexcept
	{
		auto insertAudioMapping = [](StreamChannelMappings& streamChannelMappings, la::avdecc::entity::model::AudioMapping const& audioMapping, la::avdecc::entity::model::StreamPortIndex const streamPortIndex)
		{
			auto const& streamChannelMappingsIt = streamChannelMappings.find(audioMapping.streamIndex);
			if (streamChannelMappingsIt != streamChannelMappings.end())
			{
				auto& streamPortAudioMappings = streamChannelMappingsIt->second;
				auto const& streamPortAudioMappingsIt = streamPortAudioMappings.find(streamPortIndex);
				if (streamPortAudioMappingsIt != streamPortAudioMappings.end())
				{
					auto& audioMappings = streamPortAudioMappingsIt->second;
					audioMappings.push_back(audioMapping);
				}
				else
				{
					la::avdecc::entity::model::AudioMappings mappings;
					mappings.push_back(audioMapping);
					streamPortAudioMappings.emplace(streamPortIndex, mappings);
				}
			}
			else
			{
				StreamPortAudioMappings streamPortAudioMappings;
				la::avdecc::entity::model::AudioMappings mappings;
				mappings.push_back(audioMapping);
				streamPortAudioMappings.emplace(streamPortIndex, mappings);
				streamChannelMappings.emplace(audioMapping.streamIndex, streamPortAudioMappings);
			}
		};

		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledTalkerEntity = manager.getControlledEntity(talkerEntityId);
		auto controlledListenerEntity = manager.getControlledEntity(listenerEntityId);

		if (!controlledTalkerEntity || !controlledListenerEntity)
		{
			return {};
		}
		if (!controlledTalkerEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return {};
		}

		// in here the new mappings for each stream are stored
		StreamChannelMappings overriddenMappingsListener;
		StreamChannelMappings newMappingsTalker;
		StreamChannelMappings newMappingsListener;
		StreamConnections newStreamConnections;
		StreamFormatChanges streamFormatChangesTalker;
		StreamFormatChanges streamFormatChangesListener;

		auto streamDeviceConnections = getStreamConnectionsBetweenDevices(talkerEntityId, listenerEntityId);

		// check if every connection is creatable
		// to do so, every new connection has to be stored into a temporary connection object (so that no new connection overwrites previous ones)

		for (auto const& channelPair : talkerToListenerChannelConnections)
		{
			auto const& talkerChannelIdentification = channelPair.first;
			auto const& listenerChannelIdentification = channelPair.second;
			try
			{
				auto const& streamPortInputAudioMappings = controlledListenerEntity->getStreamPortInputAudioMappings(*channelPair.second.streamPortIndex);
				for (auto const& mapping : streamPortInputAudioMappings)
				{
					if (mapping.clusterChannel == listenerChannelIdentification.clusterChannel && mapping.clusterOffset == listenerChannelIdentification.clusterIndex - *listenerChannelIdentification.baseCluster)
					{
						la::avdecc::entity::model::AudioMapping listenerMapping;
						listenerMapping.clusterChannel = listenerChannelIdentification.clusterChannel;
						listenerMapping.clusterOffset = listenerChannelIdentification.clusterIndex - *listenerChannelIdentification.baseCluster;
						listenerMapping.streamChannel = mapping.streamChannel;
						listenerMapping.streamIndex = mapping.streamIndex;

						insertAudioMapping(overriddenMappingsListener, listenerMapping, *channelPair.second.streamPortIndex);
					}
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}

			bool foundResuableExistingStreamConnection = false;
			for (auto const& deviceConnectionKV : streamDeviceConnections)
			{
				// try to reuse existing stream connection + channel mapping when only the output mapping is missing
				try
				{
					auto const& streamOutput = controlledTalkerEntity->getStreamOutputNode(controlledTalkerEntity->getCurrentConfigurationNode().descriptorIndex, deviceConnectionKV.first);
					auto const& streamInput = controlledListenerEntity->getStreamInputNode(controlledListenerEntity->getCurrentConfigurationNode().descriptorIndex, deviceConnectionKV.second);

					auto const& newConnectionIt = std::find(newStreamConnections.begin(), newStreamConnections.end(), deviceConnectionKV);
					std::optional<uint16_t> adjustedStreamChannelCount = std::nullopt;
					// not a new connection + streamformat is not equals.
					if (newConnectionIt == newStreamConnections.end() && streamOutput.dynamicModel->streamInfo.streamFormat != streamInput.dynamicModel->streamInfo.streamFormat)
					{
						continue;
					}
					else if (newConnectionIt != newStreamConnections.end())
					{
						auto streamFormatChangeTalker = streamFormatChangesTalker.find(newConnectionIt->first);
						auto streamFormatChangeListener = streamFormatChangesListener.find(newConnectionIt->second);
						if (streamFormatChangeTalker != streamFormatChangesTalker.end())
						{
							auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormatChangeTalker->second);
							adjustedStreamChannelCount = streamFormatInfo->getChannelsCount();
						}
						else if (streamFormatChangeListener != streamFormatChangesListener.end())
						{
							auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormatChangeListener->second);
							adjustedStreamChannelCount = streamFormatInfo->getChannelsCount();
						}
					}
					if (checkStreamFormatType(streamOutput.dynamicModel->streamInfo.streamFormat, la::avdecc::entity::model::StreamFormatInfo::Type::AAF))
					{
						auto freeOrReusableChannelConnection = findFreeOrReuseChannelOnExistingStreamConnection(talkerEntityId, deviceConnectionKV.first, talkerChannelIdentification.clusterIndex - *talkerChannelIdentification.baseCluster, talkerChannelIdentification.clusterChannel, newMappingsTalker, listenerEntityId, deviceConnectionKV.second, listenerChannelIdentification.clusterIndex - *listenerChannelIdentification.baseCluster, listenerChannelIdentification.clusterChannel, newMappingsListener, adjustedStreamChannelCount);

						// already connected and also got a free channel on a compatible stream
						if (freeOrReusableChannelConnection)
						{
							auto const connectionStreamChannel = std::get<0>(*freeOrReusableChannelConnection);
							auto const resuseOfTalkerStreamChannel = std::get<1>(*freeOrReusableChannelConnection);
							auto const reuseOfListenerStreamChannel = std::get<2>(*freeOrReusableChannelConnection);
							auto const connectionStreamSourcePrimaryIndex = deviceConnectionKV.first;
							auto const connectionStreamTargetPrimaryIndex = deviceConnectionKV.second;

							la::avdecc::entity::model::AudioMapping talkerMapping;
							talkerMapping.clusterChannel = talkerChannelIdentification.clusterChannel;
							talkerMapping.clusterOffset = talkerChannelIdentification.clusterIndex - *talkerChannelIdentification.baseCluster;
							talkerMapping.streamChannel = connectionStreamChannel;
							talkerMapping.streamIndex = connectionStreamSourcePrimaryIndex;
							insertAudioMapping(newMappingsTalker, talkerMapping, *channelPair.second.streamPortIndex);

							la::avdecc::entity::model::AudioMapping listenerMapping;
							listenerMapping.clusterChannel = listenerChannelIdentification.clusterChannel;
							listenerMapping.clusterOffset = listenerChannelIdentification.clusterIndex - *listenerChannelIdentification.baseCluster;
							listenerMapping.streamChannel = connectionStreamChannel;
							listenerMapping.streamIndex = connectionStreamTargetPrimaryIndex;

							insertAudioMapping(newMappingsListener, listenerMapping, *channelPair.second.streamPortIndex);

							foundResuableExistingStreamConnection = true;
							break;
						}
					}
				}
				catch (la::avdecc::controller::ControlledEntity::Exception const&)
				{
				}
			}

			if (!foundResuableExistingStreamConnection)
			{
				auto findStreamConnectionResult = findAvailableStreamConnection(talkerEntityId, listenerEntityId, talkerChannelIdentification.clusterIndex - *talkerChannelIdentification.baseCluster, talkerChannelIdentification.clusterChannel, listenerChannelIdentification.clusterIndex - *listenerChannelIdentification.baseCluster, listenerChannelIdentification.clusterChannel, newStreamConnections, allowRemovalOfUnusedAudioMappings);

				if (findStreamConnectionResult.unallowedRemovalOfUnusedAudioMappingsNecessary)
				{
					return CheckChannelCreationsPossibleResult{ ChannelConnectResult::RemovalOfListenerDynamicMappingsNecessary };
				}
				else if (!findStreamConnectionResult.connectionsToCreate.empty())
				{
					// first is the primary:
					auto const& primaryStreamConnection = findStreamConnectionResult.connectionsToCreate.at(0);
					auto const connectionStreamSourcePrimaryIndex = std::get<0>(primaryStreamConnection);
					auto const connectionStreamTargetPrimaryIndex = std::get<1>(primaryStreamConnection);
					auto const connectionStreamChannel = std::get<2>(primaryStreamConnection);

					la::avdecc::entity::model::AudioMapping talkerMapping;
					talkerMapping.clusterChannel = talkerChannelIdentification.clusterChannel;
					talkerMapping.clusterOffset = talkerChannelIdentification.clusterIndex - *talkerChannelIdentification.baseCluster;
					talkerMapping.streamChannel = connectionStreamChannel;
					talkerMapping.streamIndex = connectionStreamSourcePrimaryIndex;

					insertAudioMapping(newMappingsTalker, talkerMapping, *channelPair.second.streamPortIndex);

					la::avdecc::entity::model::AudioMapping listenerMapping;
					listenerMapping.clusterChannel = listenerChannelIdentification.clusterChannel;
					listenerMapping.clusterOffset = listenerChannelIdentification.clusterIndex - *listenerChannelIdentification.baseCluster;
					listenerMapping.streamChannel = connectionStreamChannel;
					listenerMapping.streamIndex = connectionStreamTargetPrimaryIndex;

					insertAudioMapping(newMappingsListener, listenerMapping, *channelPair.second.streamPortIndex);

					for (auto const& mappingToRemove : findStreamConnectionResult.listenerDynamicMappingsToRemove)
					{
						insertAudioMapping(overriddenMappingsListener, mappingToRemove, *channelPair.second.streamPortIndex);
					}

					newStreamConnections.push_back(std::make_pair(connectionStreamSourcePrimaryIndex, connectionStreamTargetPrimaryIndex));

					streamDeviceConnections.push_back(std::make_pair(connectionStreamSourcePrimaryIndex, connectionStreamTargetPrimaryIndex));

					auto const& compatibleFormats = findCompatibleStreamPairFormat(talkerEntityId, connectionStreamSourcePrimaryIndex, listenerEntityId, connectionStreamTargetPrimaryIndex, la::avdecc::entity::model::StreamFormatInfo::Type::AAF, channelUsageHint);
					if (compatibleFormats.first)
					{
						streamFormatChangesTalker.emplace(connectionStreamSourcePrimaryIndex, *compatibleFormats.first);
					}
					if (compatibleFormats.second)
					{
						streamFormatChangesTalker.emplace(connectionStreamTargetPrimaryIndex, *compatibleFormats.second);
					}
					// TODOs:
					// -take stream size adjustments into account when checking the connection possibilites.
					// this regards connections that are newly created, (resizable because, both talker and listener can have more channels)
					// + connections where one side has to be adjusted. (to be bigger)
					// -> findFreeOrReuseChannelOnExistingStreamConnection has to be changed to take into account a different stream format then currently active.
				}
				else
				{
					// if one connection can not be created, no connection shall be created.
					return CheckChannelCreationsPossibleResult{ ChannelConnectResult::Impossible };
				}
			}
		}

		return CheckChannelCreationsPossibleResult{ ChannelConnectResult::NoError, std::move(overriddenMappingsListener), std::move(newMappingsTalker), std::move(newMappingsListener), std::move(newStreamConnections) };
	}

	/**
	* Tries to establish a channel connection between two audio channels of different devices.
	*
	* @param talkerEntityId The id of the talker entity.
	* @param listenerEntityId The id of the listener entity.
	* @param talkerChannelIdentification The identification for the output channel.
	* @param listenerChannelIdentification The identification for the input channel.
	* @return ChannelConnectResult::NoError if it is theoretically possbile to create the connection. However errors can occur while executing the commands. The errors can be catched from the createChannelConnectionsFinished signal.
	*/
	virtual ChannelConnectResult createChannelConnection(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId, avdecc::ChannelIdentification const& talkerChannelIdentification, avdecc::ChannelIdentification const& listenerChannelIdentification, bool allowRemovalOfUnusedAudioMappings) noexcept
	{
		std::vector<std::pair<avdecc::ChannelIdentification, avdecc::ChannelIdentification>> channelsToConnect;
		channelsToConnect.push_back(std::make_pair(talkerChannelIdentification, listenerChannelIdentification));

		return createChannelConnections(talkerEntityId, listenerEntityId, channelsToConnect, allowRemovalOfUnusedAudioMappings);
	}

	/**
	* Tries to establish the channel connections between two audio channels of different devices.
	*
	* @param talkerEntityId The id of the talker entity.
	* @param listenerEntityId The id of the listener entity.
	* @param talkerChannelIdentification The identification for the output channel.
	* @param listenerChannelIdentification The identification for the input channel.
	* @return ChannelConnectResult::NoError if it is theoretically possbile to create the connection. However errors can occur while executing the commands. The errors can be catched from the createChannelConnectionsFinished signal.
	*/
	virtual ChannelConnectResult createChannelConnections(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId, std::vector<std::pair<avdecc::ChannelIdentification, avdecc::ChannelIdentification>> const& talkerToListenerChannelConnections, bool allowRemovalOfUnusedAudioMappings) noexcept
	{
		// count the number of channel connections needed (filter doubled talker connections)
		uint16_t channelUsage = 0;
		std::set<avdecc::ChannelIdentification> uniqueTalkers;
		for (auto const& connection : talkerToListenerChannelConnections)
		{
			auto const& processedTalkersIt = uniqueTalkers.find(connection.first);
			if (processedTalkersIt == uniqueTalkers.end())
			{
				channelUsage++;
				uniqueTalkers.insert(connection.first);
			}
		}

		auto const& result = checkChannelCreationsPossible(talkerEntityId, listenerEntityId, talkerToListenerChannelConnections, allowRemovalOfUnusedAudioMappings, channelUsage);
		if (result.connectionCheckResult == ChannelConnectResult::NoError)
		{
			std::vector<mediaClock::AsyncParallelCommandSet*> commands;

			std::vector<mediaClock::AsyncParallelCommandSet::AsyncCommand> commandsChangeStreamFormat;
			std::vector<mediaClock::AsyncParallelCommandSet::AsyncCommand> commandsCreateStreamConnections;
			for (auto const& newStreamConnection : result.newStreamConnections)
			{
				// change the stream format if necessary
				auto const& compatibleStreamFormats = findCompatibleStreamPairFormat(talkerEntityId, newStreamConnection.first, listenerEntityId, newStreamConnection.second, la::avdecc::entity::model::StreamFormatInfo::Type::AAF, channelUsage);

				auto const talkerStreamIndex = newStreamConnection.first;
				if (compatibleStreamFormats.first)
				{
					commandsChangeStreamFormat.push_back(
						[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = mediaClock::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != mediaClock::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::StopStream);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
							};
							manager.setStreamOutputFormat(talkerEntityId, talkerStreamIndex, *compatibleStreamFormats.first, responseHandler);
							return true;
						});
				}
				if (compatibleStreamFormats.second)
				{
					commandsChangeStreamFormat.push_back(
						[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = mediaClock::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != mediaClock::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::StopStream);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
							};
							manager.setStreamInputFormat(listenerEntityId, newStreamConnection.second, *compatibleStreamFormats.second, responseHandler);
							return true;
						});
				}

				// get the redundant connections and connect all
				auto const& redundantOutputStreams = getRedundantStreamOutputsForPrimary(talkerEntityId, newStreamConnection.first);
				auto const& redundantInputStreams = getRedundantStreamInputsForPrimary(listenerEntityId, newStreamConnection.second);
				auto redundantOutputStreamsIterator = redundantOutputStreams.begin();
				auto redundantInputStreamsIterator = redundantInputStreams.begin();
				auto talkerPrimStreamIndex{ newStreamConnection.first };
				auto listenerPrimStreamIndex{ newStreamConnection.second };

				if (!redundantOutputStreams.empty() && !redundantInputStreams.empty())
				{
					talkerPrimStreamIndex = redundantOutputStreamsIterator->first;
					listenerPrimStreamIndex = redundantInputStreamsIterator->first;
				}

				// connect primary
				commandsCreateStreamConnections.push_back(
					[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
					{
						auto& manager = avdecc::ControllerManager::getInstance();
						auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status)
						{
							// notify SequentialAsyncCommandExecuter that the command completed.
							auto error = mediaClock::AsyncParallelCommandSet::controlStatusToCommandError(status);
							if (error != mediaClock::CommandExecutionError::NoError)
							{
								switch (status)
								{
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerMisbehaving:
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerUnknownID:
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerDestMacFail:
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoBandwidth:
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoStreamIndex:
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerExclusive:
										parentCommandSet->addErrorInfo(talkerEntityID, error, avdecc::ControllerManager::AcmpCommandType::ConnectStream);
										break;
									case la::avdecc::entity::LocalEntity::ControlStatus::ListenerMisbehaving:
									case la::avdecc::entity::LocalEntity::ControlStatus::ListenerUnknownID:
									case la::avdecc::entity::LocalEntity::ControlStatus::ListenerExclusive:
										parentCommandSet->addErrorInfo(listenerEntityID, error, avdecc::ControllerManager::AcmpCommandType::ConnectStream);
										break;
									default:
										parentCommandSet->addErrorInfo(listenerEntityID, error, avdecc::ControllerManager::AcmpCommandType::ConnectStream);
										parentCommandSet->addErrorInfo(listenerEntityID, error, avdecc::ControllerManager::AcmpCommandType::ConnectStream);
								}
							}
							parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
						};
						manager.connectStream(talkerEntityId, talkerPrimStreamIndex, listenerEntityId, listenerPrimStreamIndex, responseHandler);
						return true;
					});

				// connect secundary
				while (redundantOutputStreamsIterator != redundantOutputStreams.end() && redundantInputStreamsIterator != redundantInputStreams.end())
				{
					if (newStreamConnection.first != redundantOutputStreamsIterator->first && newStreamConnection.second != redundantInputStreamsIterator->first)
					{
						auto const talkerSecStreamIndex = redundantOutputStreamsIterator->first;
						auto const listenerSecStreamIndex = redundantInputStreamsIterator->first;
						commandsCreateStreamConnections.push_back(
							[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
							{
								auto& manager = avdecc::ControllerManager::getInstance();
								auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status)
								{
									// notify SequentialAsyncCommandExecuter that the command completed.
									auto error = mediaClock::AsyncParallelCommandSet::controlStatusToCommandError(status);
									if (error != mediaClock::CommandExecutionError::NoError)
									{
										switch (status)
										{
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerMisbehaving:
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerUnknownID:
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerDestMacFail:
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoBandwidth:
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoStreamIndex:
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerExclusive:
												parentCommandSet->addErrorInfo(talkerEntityID, error, avdecc::ControllerManager::AcmpCommandType::ConnectStream);
												break;
											case la::avdecc::entity::LocalEntity::ControlStatus::ListenerMisbehaving:
											case la::avdecc::entity::LocalEntity::ControlStatus::ListenerUnknownID:
											case la::avdecc::entity::LocalEntity::ControlStatus::ListenerExclusive:
												parentCommandSet->addErrorInfo(listenerEntityID, error, avdecc::ControllerManager::AcmpCommandType::ConnectStream);
												break;
											default:
												parentCommandSet->addErrorInfo(listenerEntityID, error, avdecc::ControllerManager::AcmpCommandType::ConnectStream);
												parentCommandSet->addErrorInfo(listenerEntityID, error, avdecc::ControllerManager::AcmpCommandType::ConnectStream);
										}
									}
									parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
								};
								manager.connectStream(talkerEntityId, talkerSecStreamIndex, listenerEntityId, listenerSecStreamIndex, responseHandler);
								return true;
							});
					}
					redundantOutputStreamsIterator++;
					redundantInputStreamsIterator++;
				}
			}
			auto* commandSetChangeStreamFormat = new mediaClock::AsyncParallelCommandSet(commandsChangeStreamFormat);
			auto* commandSetCreateStreamConnections = new mediaClock::AsyncParallelCommandSet(commandsCreateStreamConnections);


			// create the set of streams to stop and later start again
			std::set<la::avdecc::entity::model::StreamIndex> talkerStreamsToStop;
			for (auto const& mappingsTalker : result.newMappingsTalker)
			{
				talkerStreamsToStop.insert(mappingsTalker.first);
			}

			// create the set of streams to stop and later start again
			std::set<la::avdecc::entity::model::StreamIndex> listenerStreamsToStop;
			for (auto const& mappingsListener : result.newMappingsListener)
			{
				listenerStreamsToStop.insert(mappingsListener.first);
			}

			// create commands to stop the streams
			std::vector<mediaClock::AsyncParallelCommandSet::AsyncCommand> commandsStopStreams;
			for (auto const streamIndex : talkerStreamsToStop)
			{
				commandsStopStreams.push_back(
					[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
					{
						auto& manager = avdecc::ControllerManager::getInstance();
						auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
						{
							// notify SequentialAsyncCommandExecuter that the command completed.
							auto error = mediaClock::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
							if (error != mediaClock::CommandExecutionError::NoError)
							{
								parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::StopStream);
							}
							parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
						};
						manager.stopStreamOutput(talkerEntityId, streamIndex, responseHandler);
						return true;
					});
			}
			for (auto const streamIndex : listenerStreamsToStop)
			{
				commandsStopStreams.push_back(
					[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
					{
						auto& manager = avdecc::ControllerManager::getInstance();
						auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
						{
							// notify SequentialAsyncCommandExecuter that the command completed.
							auto error = mediaClock::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
							if (error != mediaClock::CommandExecutionError::NoError)
							{
								parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::StopStream);
							}
							parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
						};
						manager.stopStreamOutput(listenerEntityId, streamIndex, responseHandler);
						return true;
					});
			}
			auto* commandSetStopStreams = new mediaClock::AsyncParallelCommandSet(commandsStopStreams);

			std::vector<mediaClock::AsyncParallelCommandSet::AsyncCommand> commandsStartStreams;
			for (auto const streamIndex : talkerStreamsToStop)
			{
				commandsStartStreams.push_back(
					[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
					{
						auto& manager = avdecc::ControllerManager::getInstance();
						auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
						{
							// notify SequentialAsyncCommandExecuter that the command completed.
							auto error = mediaClock::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
							if (error != mediaClock::CommandExecutionError::NoError)
							{
								parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::StartStream);
							}
							parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
						};
						manager.startStreamOutput(talkerEntityId, streamIndex, responseHandler);
						return true;
					});
			}

			for (auto const streamIndex : listenerStreamsToStop)
			{
				commandsStartStreams.push_back(
					[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
					{
						auto& manager = avdecc::ControllerManager::getInstance();
						auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
						{
							// notify SequentialAsyncCommandExecuter that the command completed.
							auto error = mediaClock::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
							if (error != mediaClock::CommandExecutionError::NoError)
							{
								parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::StartStream);
							}
							parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
						};
						manager.startStreamInput(listenerEntityId, streamIndex, responseHandler);
						return true;
					});
			}
			auto* commandSetStartStreams = new mediaClock::AsyncParallelCommandSet(commandsStartStreams);

			std::vector<mediaClock::AsyncParallelCommandSet::AsyncCommand> commandsRemoveMappings;
			for (auto const& mappingsListener : result.overriddenMappingsListener)
			{
				for (auto const& mapping : mappingsListener.second)
				{
					commandsRemoveMappings.push_back(
						[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = mediaClock::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != mediaClock::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::AddStreamPortAudioMappings);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
							};
							manager.removeStreamPortInputAudioMappings(listenerEntityId, mapping.first, mapping.second, responseHandler);
							return true;
						});
				}
			}
			auto* commandSetRemoveMappings = new mediaClock::AsyncParallelCommandSet(commandsRemoveMappings);

			std::vector<mediaClock::AsyncParallelCommandSet::AsyncCommand> commandsCreateMappings;
			for (auto const& mappingsTalker : result.newMappingsTalker)
			{
				for (auto const& mapping : mappingsTalker.second)
				{
					commandsCreateMappings.push_back(
						[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = mediaClock::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != mediaClock::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::AddStreamPortAudioMappings);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
							};
							manager.addStreamPortOutputAudioMappings(talkerEntityId, mapping.first, mapping.second, responseHandler);
							return true;
						});
				}
			}

			for (auto const& mappingsListener : result.newMappingsListener)
			{
				for (auto const& mapping : mappingsListener.second)
				{
					commandsCreateMappings.push_back(
						[=](mediaClock::AsyncParallelCommandSet* parentCommandSet, int commandIndex) -> bool
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = mediaClock::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != mediaClock::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::AddStreamPortAudioMappings);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != mediaClock::CommandExecutionError::NoError);
							};
							manager.addStreamPortInputAudioMappings(listenerEntityId, mapping.first, mapping.second, responseHandler);
							return true;
						});
				}
			}
			auto* commandSetCreateMappings = new mediaClock::AsyncParallelCommandSet(commandsCreateMappings);

			// create chain
			commands.push_back(commandSetChangeStreamFormat);
			commands.push_back(commandSetCreateStreamConnections);
			commands.push_back(commandSetStopStreams);
			commands.push_back(commandSetRemoveMappings);
			commands.push_back(commandSetCreateMappings);
			commands.push_back(commandSetStartStreams);

			// execute the command chain
			_sequentialAcmpCommandExecuter.setCommandChain(commands);
			_sequentialAcmpCommandExecuter.start();
		}

		return result.connectionCheckResult;
	}

	/**
	* Removes a channel connection if it exists.
	*
	* The stream connection is only removed if no other channel is mapped on this stream and used by the same listener.
	* The talker channel mapping is only removed if it's not in use by any listeners anymore.
	*
	* @param talkerEntityId The id of the talker entity.
	* @param talkerAudioUnitIndex The index of the talker audio unit to use.
	* @param talkerStreamPortIndex The index of the talker stream port to use.
	* @param talkerClusterIndex The index of the audio cluster to use.
	* @param talkerBaseCluster The base cluster of the talker.
	* @param talkerClusterChannel The cluster channel number.
	* @param listenerEntityId The id of the listener entity.
	* @param listenerAudioUnitIndex The index of the listener audio unit to use.
	* @param listenerStreamPortIndex The index of the listener stream port to use.
	* @param listenerClusterIndex The index of the audio cluster to use.
	* @param listenerBaseCluster The base cluster of the listener.
	* @param listenerClusterChannel The cluster channel number.
	* @return ChannelDisconnectResult::NoError if the connection and mappings could be created.
	*/
	virtual ChannelDisconnectResult removeChannelConnection(
		la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::AudioUnitIndex const talkerAudioUnitIndex, la::avdecc::entity::model::StreamPortIndex const talkerStreamPortIndex, la::avdecc::entity::model::ClusterIndex const talkerClusterIndex, la::avdecc::entity::model::ClusterIndex const talkerBaseCluster, std::uint16_t const talkerClusterChannel, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::AudioUnitIndex const listenerAudioUnitIndex, la::avdecc::entity::model::StreamPortIndex const listenerStreamPortIndex, la::avdecc::entity::model::ClusterIndex const listenerClusterIndex, la::avdecc::entity::model::ClusterIndex const listenerBaseCluster, std::uint16_t const listenerClusterChannel) noexcept
	{
		// check if the connection exists
		// and remove all parts of it
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledTalkerEntity = manager.getControlledEntity(talkerEntityId);
		auto controlledListenerEntity = manager.getControlledEntity(listenerEntityId);

		if (!controlledTalkerEntity || !controlledListenerEntity)
		{
			return ChannelDisconnectResult::Error;
		}

		if (!controlledTalkerEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return ChannelDisconnectResult::Unsupported;
		}
		if (!controlledListenerEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return ChannelDisconnectResult::Unsupported;
		}

		ChannelIdentification listenerChannelIdentification(controlledListenerEntity->getCurrentConfigurationNode().descriptorIndex, listenerClusterIndex, listenerClusterChannel, ChannelConnectionDirection::InputToOutput, listenerAudioUnitIndex, listenerStreamPortIndex, listenerBaseCluster);

		auto const channelConnectionOfListenerChannel = getChannelConnectionsReverse(listenerEntityId, listenerChannelIdentification);

		if (channelConnectionOfListenerChannel->targets.empty())
		{
			return ChannelDisconnectResult::NonExistent; // connection does not exist
		}

		std::optional<la::avdecc::entity::model::StreamIndex> connectionStreamSourceIndex = std::nullopt;
		std::optional<la::avdecc::entity::model::StreamIndex> connectionStreamTargetIndex = std::nullopt;
		std::optional<uint16_t> connectionStreamChannel = std::nullopt;

		for (auto deviceConnection : channelConnectionOfListenerChannel->targets)
		{
			if (deviceConnection->targetEntityId == talkerEntityId)
			{
				if (deviceConnection->targetAudioUnitIndex == talkerAudioUnitIndex && deviceConnection->targetStreamPortIndex == talkerStreamPortIndex)
				{
					for (auto const& clusterIdentificationPair : deviceConnection->targetClusterChannels)
					{
						if (clusterIdentificationPair.first == talkerClusterIndex - talkerBaseCluster && clusterIdentificationPair.second == talkerClusterChannel)
						{
							// flip because we are going into the opposite direction. (talker->listener)
							connectionStreamSourceIndex = deviceConnection->targetStreamIndex;
							connectionStreamTargetIndex = deviceConnection->sourceStreamIndex;
							connectionStreamChannel = deviceConnection->streamChannel;
							break;
						}
					}
				}
			}
		}

		if (connectionStreamSourceIndex)
		{
			// determine the amount of channels that are used on this stream connection.
			// If the connection to remove isn't the only one on the stream, streamConnectionStillNeeded is set to true and the stream will stay connected.
			auto unassignedChannels = getUnassignedChannelsOnTalkerStream(talkerEntityId, *connectionStreamSourceIndex);
			auto const channelConnectionsOfTalker = getAllChannelConnectionsBetweenDevices(talkerEntityId, talkerStreamPortIndex, listenerEntityId);
			bool streamConnectionStillNeeded = false;

			int streamConnectionUsages = 0;
			for (auto deviceConnection : channelConnectionsOfTalker->targets)
			{
				if (deviceConnection->targetEntityId == listenerEntityId && deviceConnection->targetStreamIndex == *connectionStreamTargetIndex && deviceConnection->sourceStreamIndex == *connectionStreamSourceIndex)
				{
					if (!deviceConnection->targetClusterChannels.empty())
					{
						streamConnectionUsages += static_cast<int>(deviceConnection->targetClusterChannels.size());
						if (streamConnectionUsages > 1)
						{
							streamConnectionStillNeeded = true;
							break;
						}
					}
				}
			}

			// determine the amount of channel receivers:
			ChannelIdentification talkerChannelIdentification(controlledTalkerEntity->getCurrentConfigurationNode().descriptorIndex, talkerClusterIndex, talkerClusterChannel, ChannelConnectionDirection::OutputToInput, talkerAudioUnitIndex, talkerStreamPortIndex, talkerBaseCluster);

			int talkerChannelReceivers = 0;
			auto const channelConnectionsOfTalkerChannel = getChannelConnections(talkerEntityId, talkerChannelIdentification);

			for (auto deviceConnection : channelConnectionsOfTalkerChannel->targets)
			{
				if (deviceConnection->sourceStreamIndex == *connectionStreamSourceIndex && deviceConnection->streamChannel == connectionStreamChannel)
				{
					talkerChannelReceivers += deviceConnection->targetClusterChannels.size();
				}
			}

			// disconnect the stream if it isn't used by that listener anymore
			if (!streamConnectionStillNeeded)
			{
				manager.disconnectStream(talkerEntityId, *connectionStreamSourceIndex, listenerEntityId, *connectionStreamTargetIndex);

				// disconnect redundant streams too
				auto redundantOutputStreams = getRedundantStreamOutputsForPrimary(talkerEntityId, *connectionStreamSourceIndex);
				auto redundantInputStreams = getRedundantStreamInputsForPrimary(listenerEntityId, *connectionStreamTargetIndex);
				auto redundantOutputStreamsIterator = redundantOutputStreams.begin();
				auto redundantInputStreamsIterator = redundantInputStreams.begin();

				while (redundantOutputStreamsIterator != redundantOutputStreams.end() && redundantInputStreamsIterator != redundantInputStreams.end())
				{
					if (*connectionStreamSourceIndex != redundantOutputStreamsIterator->first && *connectionStreamTargetIndex != redundantInputStreamsIterator->first)
					{
						manager.disconnectStream(talkerEntityId, redundantOutputStreamsIterator->first, listenerEntityId, redundantInputStreamsIterator->first);
					}
					redundantOutputStreamsIterator++;
					redundantInputStreamsIterator++;
				}
			}

			// only remove the talker channel mapping if it isn't in use by any other channel on any target anymore.
			if (talkerChannelReceivers <= 1)
			{
				la::avdecc::entity::model::AudioMapping mapping;
				mapping.clusterChannel = talkerClusterChannel;
				mapping.clusterOffset = talkerClusterIndex - talkerBaseCluster;
				mapping.streamChannel = *connectionStreamChannel;
				mapping.streamIndex = *connectionStreamSourceIndex;
				la::avdecc::entity::model::AudioMappings mappings;
				mappings.push_back(mapping);

				manager.removeStreamPortOutputAudioMappings(talkerEntityId, talkerStreamPortIndex, mappings);
			}

			// the listener channel has to be unmapped in any case.
			{
				la::avdecc::entity::model::AudioMapping mapping;
				mapping.clusterChannel = listenerClusterChannel;
				mapping.clusterOffset = listenerClusterIndex - listenerBaseCluster;
				mapping.streamChannel = *connectionStreamChannel;
				mapping.streamIndex = *connectionStreamTargetIndex;
				la::avdecc::entity::model::AudioMappings mappings;
				mappings.push_back(mapping);

				manager.removeStreamPortInputAudioMappings(listenerEntityId, listenerStreamPortIndex, mappings);
			}
		}
		else
		{
			return ChannelDisconnectResult::NonExistent;
		}
		return ChannelDisconnectResult::NoError;
	}

	/**
	* Finds the stream indexes of a channel connection if there are any.
	*/
	std::vector<StreamConnection> getStreamIndexPairUsedByAudioChannelConnection(la::avdecc::UniqueIdentifier const& talkerEntityId, avdecc::ChannelIdentification const& talkerChannelIdentification, la::avdecc::UniqueIdentifier const& listenerEntityId, avdecc::ChannelIdentification const& listenerChannelIdentification) noexcept
	{
		auto connections = getChannelConnectionsReverse(listenerEntityId, listenerChannelIdentification);
		for (auto const& deviceConnection : connections->targets)
		{
			if (deviceConnection->targetEntityId == talkerEntityId)
			{
				for (auto const& targetClusterKV : deviceConnection->targetClusterChannels)
				{
					if (deviceConnection->targetAudioUnitIndex == *listenerChannelIdentification.audioUnitIndex && deviceConnection->targetStreamPortIndex == *talkerChannelIdentification.streamPortIndex && targetClusterKV.first == talkerChannelIdentification.clusterIndex - *talkerChannelIdentification.baseCluster && targetClusterKV.second == talkerChannelIdentification.clusterChannel)
					{
						std::vector<StreamConnection> result;
						for (auto const [talkerStreamIndex, listenerStreamIndex] : deviceConnection->streamPairs)
						{
							la::avdecc::entity::model::StreamIdentification streamTalker{ talkerEntityId, talkerStreamIndex };
							la::avdecc::entity::model::StreamIdentification streamListener{ listenerEntityId, listenerStreamIndex };

							result.push_back(std::make_pair(streamTalker, streamListener));
						}

						return result;
					}
				}
			}
		}

		return {};
	}

	/**
	* Find all outgoing stream channels that are assigned to the given cluster channel.
	*/
	std::set<uint16_t> getAssignedChannelsOnTalkerStream(la::avdecc::UniqueIdentifier entityId, la::avdecc::entity::model::StreamIndex outputStreamIndex, std::optional<la::avdecc::entity::model::ClusterIndex> clusterOffset = std::nullopt, std::optional<uint16_t> clusterChannel = std::nullopt) const noexcept
	{
		std::set<uint16_t> result;

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

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return result;
		}

		for (auto const& audioUnit : configurationNode.audioUnits)
		{
			for (auto const& streamPortOutput : audioUnit.second.streamPortOutputs)
			{
				for (auto const& mapping : streamPortOutput.second.dynamicModel->dynamicAudioMap)
				{
					if (mapping.streamIndex == outputStreamIndex)
					{
						if (clusterOffset && clusterChannel)
						{
							// only search for the given cluster channel
							if (mapping.clusterOffset == *clusterOffset && mapping.clusterChannel == *clusterChannel)
							{
								result.emplace(mapping.streamChannel);
							}
						}
						else
						{
							result.emplace(mapping.streamChannel);
						}
					}
				}
			}
		}

		return result;
	}

	/**
	* Find all outgoing stream channels that are assigned to the given cluster channel.
	*/
	std::set<uint16_t> getAssignedChannelsOnListenerStream(la::avdecc::UniqueIdentifier entityId, la::avdecc::entity::model::StreamIndex inputStreamIndex, std::optional<la::avdecc::entity::model::ClusterIndex> clusterOffset = std::nullopt, std::optional<uint16_t> clusterChannel = std::nullopt) const noexcept
	{
		std::set<uint16_t> result;

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

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return result;
		}

		for (auto const& audioUnit : configurationNode.audioUnits)
		{
			for (auto const& streamPortInput : audioUnit.second.streamPortInputs)
			{
				for (auto const& mapping : streamPortInput.second.dynamicModel->dynamicAudioMap)
				{
					if (mapping.streamIndex == inputStreamIndex)
					{
						if (clusterOffset && clusterChannel)
						{
							// only search for the given cluster channel
							if (mapping.clusterOffset == *clusterOffset && mapping.clusterChannel == *clusterChannel)
							{
								result.emplace(mapping.streamChannel);
							}
						}
						else
						{
							result.emplace(mapping.streamChannel);
						}
					}
				}
			}
		}

		return result;
	}

	std::set<uint16_t> getAssignedChannelsOnConnectedListenerStreams(la::avdecc::UniqueIdentifier talkerEntityId, la::avdecc::UniqueIdentifier listenerEntityId, la::avdecc::entity::model::StreamIndex outputStreamIndex, std::optional<la::avdecc::entity::model::ClusterIndex> clusterOffset = std::nullopt, std::optional<uint16_t> clusterChannel = std::nullopt) const noexcept
	{
		std::set<uint16_t> result;

		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(listenerEntityId);

		if (!controlledEntity)
		{
			return result;
		}
		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return result;
		}

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return result;
		}

		auto streamConnections = getStreamConnectionsBetweenDevices(talkerEntityId, listenerEntityId);
		std::set<la::avdecc::entity::model::StreamIndex> connectedStreamListenerIndices;
		for (auto const& streamConnectionKV : streamConnections)
		{
			if (streamConnectionKV.first == outputStreamIndex)
			{
				connectedStreamListenerIndices.insert(streamConnectionKV.second);
			}
		}

		for (auto const& audioUnit : configurationNode.audioUnits)
		{
			for (auto const& streamPortInput : audioUnit.second.streamPortInputs)
			{
				for (auto const& mapping : streamPortInput.second.dynamicModel->dynamicAudioMap)
				{
					if (connectedStreamListenerIndices.find(mapping.streamIndex) != connectedStreamListenerIndices.end())
					{
						if (clusterOffset && clusterChannel)
						{
							// only search for the given cluster channel
							if (mapping.clusterOffset == *clusterOffset && mapping.clusterChannel == *clusterChannel)
							{
								result.emplace(mapping.streamChannel);
							}
						}
						else
						{
							result.emplace(mapping.streamChannel);
						}
					}
				}
			}
		}

		return result;
	}

	/**
	* Find all outgoing stream channels that are unassigned.
	*/
	std::set<uint16_t> getUnassignedChannelsOnTalkerStream(la::avdecc::UniqueIdentifier entityId, la::avdecc::entity::model::StreamIndex outputStreamIndex) const noexcept
	{
		std::set<uint16_t> result;

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

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return result;
		}

		std::set<uint16_t> occupiedStreamChannels;
		for (auto const& audioUnit : configurationNode.audioUnits)
		{
			for (auto const& streamPortOutput : audioUnit.second.streamPortOutputs)
			{
				for (auto const& mapping : streamPortOutput.second.dynamicModel->dynamicAudioMap)
				{
					if (mapping.streamIndex == outputStreamIndex)
					{
						occupiedStreamChannels.insert(mapping.streamChannel);
					}
				}
			}
		}

		// get the stream channel count:
		uint16_t channelCount = getStreamChannelCount(entityId, outputStreamIndex);
		for (int i = 0; i < channelCount; i++)
		{
			if (occupiedStreamChannels.find(i) == occupiedStreamChannels.end())
			{
				result.insert(i);
			}
		}

		return result;
	}

	/**
	* Find all incoming stream channels that are unassigned.
	*/
	std::set<uint16_t> getUnassignedChannelsOnListenerStream(la::avdecc::UniqueIdentifier entityId, la::avdecc::entity::model::StreamIndex inputStreamIndex) const noexcept
	{
		std::set<uint16_t> result;

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

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return result;
		}

		std::set<uint16_t> occupiedStreamChannels;
		for (auto const& audioUnit : configurationNode.audioUnits)
		{
			for (auto const& streamPortInput : audioUnit.second.streamPortInputs)
			{
				for (auto const& mapping : streamPortInput.second.dynamicModel->dynamicAudioMap)
				{
					if (mapping.streamIndex == inputStreamIndex)
					{
						occupiedStreamChannels.insert(mapping.streamChannel);
					}
				}
			}
		}

		// get the stream channel count:
		uint16_t channelCount = getStreamChannelCount(entityId, inputStreamIndex);
		for (int i = 0; i < channelCount; i++)
		{
			if (occupiedStreamChannels.find(i) == occupiedStreamChannels.end())
			{
				result.emplace(i);
			}
		}

		return result;
	}

	la::avdecc::entity::model::AudioMappings getMappingsFromStreamInputChannel(la::avdecc::UniqueIdentifier listenerEntityId, la::avdecc::entity::model::StreamIndex inputStreamIndex, uint16_t streamChannel) const noexcept
	{
		la::avdecc::entity::model::AudioMappings result;
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(listenerEntityId);

		if (!controlledEntity)
		{
			return result;
		}
		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return result;
		}

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return result;
		}

		for (auto const& audioUnit : configurationNode.audioUnits)
		{
			for (auto const& streamPortInput : audioUnit.second.streamPortInputs)
			{
				for (auto const& mapping : streamPortInput.second.dynamicModel->dynamicAudioMap)
				{
					if (mapping.streamIndex == inputStreamIndex && mapping.streamChannel == streamChannel)
					{
						result.push_back(mapping);
					}
				}
			}
		}
		return result;
	}

	/**
	* Creates a list of all connected streams between two entities.
	*/
	StreamConnections getStreamConnectionsBetweenDevices(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId) const noexcept
	{
		StreamConnections result;
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(talkerEntityId);

		if (!controlledEntity)
		{
			return result;
		}
		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return result;
		}

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return result;
		}

		auto outputConnections = getAllStreamOutputConnections(talkerEntityId);

		for (auto const& connection : outputConnections)
		{
			if (connection.listenerStream.entityID == listenerEntityId)
			{
				auto const sourceStreamIndex = connection.talkerStream.streamIndex;
				auto const targetStreamIndex = connection.listenerStream.streamIndex;
				result.push_back(std::make_pair(sourceStreamIndex, targetStreamIndex));
			}
		}
		return result;
	}

	/**
	* Finds a free or reusable stream connection to setup the channel connection with. (In the case both streams are redundant, multiple will be returned)
	* If a stream is connected to an other listener, the stream port may still be reused for a different device, if it contains the same cluster channel.
	*
	* Example:
	*
	*	Entity 1		Entity 2
	*	C1 - SO  -----> SI - C2
	*			 \
	*			  \		Entity 3
	*			   `--> SI - C4
	*
	* C  = Cluster
	* SO = Stream Output
	* SI = Stream Input
	* -  = Cluster to Stream Port Channel Mapping or vice versa
	* -> = Stream Connection
	*/
	FindStreamConnectionResult findAvailableStreamConnection(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::ClusterIndex talkerClusterOffset, uint16_t talkerClusterChannel, la::avdecc::entity::model::ClusterIndex listenerClusterOffset, uint16_t listenerClusterChannel, StreamConnections newStreamConnections, bool allowRemovalOfUnusedAudioMappings) const noexcept
	{
		FindStreamConnectionResult defaultResult;
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledTalkerEntity = manager.getControlledEntity(talkerEntityId);
			auto controlledListenerEntity = manager.getControlledEntity(listenerEntityId);
			if (!controlledTalkerEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
			{
				return defaultResult;
			}
			if (!controlledListenerEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
			{
				return defaultResult;
			}

			auto const buildResult = [this, talkerEntityId, listenerEntityId](la::avdecc::controller::model::StreamOutputNode talkerStreamOutput, la::avdecc::controller::model::StreamInputNode listenerStreamInput, uint16_t channel) -> FindStreamConnectionResult
			{
				FindStreamConnectionResult result;
				// already included in redundant list:
				result.connectionsToCreate.push_back(std::make_tuple(talkerStreamOutput.descriptorIndex, listenerStreamInput.descriptorIndex, channel));
				if (talkerStreamOutput.isRedundant && listenerStreamInput.isRedundant)
				{
					auto redundantOutputStreams = getRedundantStreamOutputsForPrimary(talkerEntityId, talkerStreamOutput.descriptorIndex);
					auto redundantInputStreams = getRedundantStreamInputsForPrimary(listenerEntityId, listenerStreamInput.descriptorIndex);

					auto redundantOutputStreamsIterator = redundantOutputStreams.begin();
					auto redundantInputStreamsIterator = redundantInputStreams.begin();
					while (redundantOutputStreamsIterator != redundantOutputStreams.end() && redundantInputStreamsIterator != redundantInputStreams.end())
					{
						if (redundantOutputStreamsIterator->second->descriptorIndex != talkerStreamOutput.descriptorIndex && redundantInputStreamsIterator->second->descriptorIndex != listenerStreamInput.descriptorIndex)
						{
							result.connectionsToCreate.push_back(std::make_tuple(redundantOutputStreamsIterator->second->descriptorIndex, redundantInputStreamsIterator->second->descriptorIndex, channel));
						}
						redundantOutputStreamsIterator++;
						redundantInputStreamsIterator++;
					}
				}
				return result;
			};

			if (controlledTalkerEntity && controlledListenerEntity)
			{
				auto const& configTalker = controlledTalkerEntity->getCurrentConfigurationNode();

				for (auto const& streamOutputKV : configTalker.streamOutputs)
				{
					if (streamOutputKV.second.dynamicModel)
					{
						auto const streamFormatTalker = streamOutputKV.second.dynamicModel->streamInfo.streamFormat;

						// check if the stream has the correct format
						if (supportsStreamFormat(streamOutputKV.second.staticModel->formats, la::avdecc::entity::model::StreamFormatInfo::Type::AAF))
						{
							auto const& configListener = controlledListenerEntity->getCurrentConfigurationNode();
							for (auto const& streamInputKV : configListener.streamInputs) // TODO skip redundant secondary...
							{
								if (streamInputKV.second.dynamicModel)
								{
									auto const streamFormatListener = streamInputKV.second.dynamicModel->streamInfo.streamFormat;
									auto streamConfigurationMatches = streamFormatTalker == streamFormatListener;
									auto compatiblePair = anyStreamFormatCompatible(streamOutputKV.second.staticModel->formats, streamInputKV.second.staticModel->formats, la::avdecc::entity::model::StreamFormatInfo::Type::AAF);

									// check if the stream has the correct format && the stream is not connected yet...
									// if not check if has compatible formats, so that it could be reconfigured. (commentet out, to wait for state machine implementation: compatiblePair)
									if ((streamConfigurationMatches || compatiblePair) && streamInputKV.second.dynamicModel->connectionState.state == la::avdecc::entity::model::StreamConnectionState::State::NotConnected)
									{
										// check if the newly created streams already contain this one.
										auto const& newStreamConnectionIt = std::find(newStreamConnections.begin(), newStreamConnections.end(), std::make_pair(streamOutputKV.first, streamInputKV.first));
										if (newStreamConnectionIt != newStreamConnections.end())
										{
											// this entity input stream is already connected
											continue;
										}

										// find a stream channel that contains the right audio channel already
										auto fittingChannelsTalker = getAssignedChannelsOnTalkerStream(talkerEntityId, streamOutputKV.first, talkerClusterOffset, talkerClusterChannel);
										auto fittingChannelsListener = getAssignedChannelsOnListenerStream(listenerEntityId, streamInputKV.first, listenerClusterOffset, listenerClusterChannel);

										// find all unassigned channels on the stream input/output
										auto freeChannelsTalker = getUnassignedChannelsOnTalkerStream(talkerEntityId, streamOutputKV.first);
										auto freeChannelsListener = getUnassignedChannelsOnListenerStream(listenerEntityId, streamInputKV.first);

										// also check if we would create an unwanted connection
										auto assignedChannelsTalker = getAssignedChannelsOnTalkerStream(talkerEntityId, streamOutputKV.first);
										auto assignedChannelsListener = getAssignedChannelsOnListenerStream(listenerEntityId, streamInputKV.first);

										std::vector<uint16_t> unwantedConnections;
										set_intersection(assignedChannelsTalker.begin(), assignedChannelsTalker.end(), assignedChannelsListener.begin(), assignedChannelsListener.end(), back_inserter(unwantedConnections));

										// check if there is a talker channel that can be used
										std::vector<uint16_t> intersectionTalkerReuse;
										set_intersection(fittingChannelsTalker.begin(), fittingChannelsTalker.end(), freeChannelsListener.begin(), freeChannelsListener.end(), back_inserter(intersectionTalkerReuse));

										// check if there is a listener channel that can be used
										std::vector<uint16_t> intersectionListenerReuse;
										set_intersection(fittingChannelsListener.begin(), fittingChannelsListener.end(), freeChannelsTalker.begin(), freeChannelsTalker.end(), back_inserter(intersectionListenerReuse));

										// check if there is a talker & listener channel pair that can be used
										std::vector<uint16_t> intersectionTalkerAndListenerReuse;
										set_intersection(fittingChannelsListener.begin(), fittingChannelsListener.end(), fittingChannelsTalker.begin(), fittingChannelsTalker.end(), back_inserter(intersectionTalkerAndListenerReuse));

										if (!intersectionTalkerAndListenerReuse.empty())
										{
											// both mappings exist already, only the stream connection is missing
											unwantedConnections.erase(std::remove(unwantedConnections.begin(), unwantedConnections.end(), intersectionTalkerAndListenerReuse.at(0)), unwantedConnections.end());

											if (unwantedConnections.empty())
											{
												if (streamConfigurationMatches)
												{
													return buildResult(streamOutputKV.second, streamInputKV.second, intersectionTalkerAndListenerReuse.at(0));
												}
												else if (defaultResult.connectionsToCreate.empty())
												{
													// iterate over the other options to see if a reconfiguration is really necessary and take this as second option:
													defaultResult = buildResult(streamOutputKV.second, streamInputKV.second, intersectionTalkerAndListenerReuse.at(0));
												}
											}
										}

										if (!intersectionTalkerReuse.empty() && unwantedConnections.empty())
										{
											if (streamConfigurationMatches)
											{
												// reuse the talker mapping
												return buildResult(streamOutputKV.second, streamInputKV.second, intersectionTalkerReuse.at(0));
											}
											else if (defaultResult.connectionsToCreate.empty())
											{
												// iterate over the other options to see if a reconfiguration is really necessary and take this as second option:
												defaultResult = buildResult(streamOutputKV.second, streamInputKV.second, intersectionTalkerReuse.at(0));
											}
										}
										else if (!intersectionListenerReuse.empty() && unwantedConnections.empty())
										{
											if (streamConfigurationMatches)
											{
												// reuse the listener mapping
												return buildResult(streamOutputKV.second, streamInputKV.second, intersectionListenerReuse.at(0));
											}
											else if (defaultResult.connectionsToCreate.empty())
											{
												// iterate over the other options to see if a reconfiguration is really necessary and take this as second option:
												defaultResult = buildResult(streamOutputKV.second, streamInputKV.second, intersectionListenerReuse.at(0));
											}
										}
										else
										{
											// no channel is already existant/can be reused, check if we can assign the channels accordingly.
											std::vector<uint16_t> intersectionFree;
											set_intersection(freeChannelsListener.begin(), freeChannelsListener.end(), freeChannelsTalker.begin(), freeChannelsTalker.end(), back_inserter(intersectionFree));

											// insert the talker channel connection that would be created:
											if (!intersectionFree.empty())
											{
												assignedChannelsTalker.insert(intersectionFree.at(0));
											}

											// append the currently existing stream connections to the same device (not in use currently)
											//if (!allowRemovalOfUnusedAudioMappings)
											{
												auto assignedChannelsListenerOnExistingStreamConnections = getAssignedChannelsOnConnectedListenerStreams(talkerEntityId, listenerEntityId, streamOutputKV.first);
												assignedChannelsListener.insert(assignedChannelsListenerOnExistingStreamConnections.begin(), assignedChannelsListenerOnExistingStreamConnections.end());
											}

											std::vector<uint16_t> unwantedConnectionsAfterAssignments;
											set_intersection(assignedChannelsTalker.begin(), assignedChannelsTalker.end(), assignedChannelsListener.begin(), assignedChannelsListener.end(), back_inserter(unwantedConnectionsAfterAssignments));

											if (!intersectionFree.empty() && unwantedConnectionsAfterAssignments.empty())
											{
												if (streamConfigurationMatches)
												{
													return buildResult(streamOutputKV.second, streamInputKV.second, intersectionFree.at(0));
												}
												else if (defaultResult.connectionsToCreate.empty())
												{
													defaultResult = buildResult(streamOutputKV.second, streamInputKV.second, intersectionFree.at(0));
												}
											}
											else if (!unwantedConnectionsAfterAssignments.empty())
											{
												// check if the unwanted connection - listener dynamic mappings can be removed safely
												// it can be removed if no other entity is connected to it:
												bool allListenerMappingsCanBeRemoved = true;
												if (streamInputKV.second.dynamicModel->connectionState.state == la::avdecc::entity::model::StreamConnectionState::State::NotConnected)
												{
													allListenerMappingsCanBeRemoved = true;
												}

												if (allListenerMappingsCanBeRemoved)
												{
													if (allowRemovalOfUnusedAudioMappings)
													{
														uint16_t streamChannelToUse = 0;
														if (!fittingChannelsTalker.empty())
														{
															streamChannelToUse = *fittingChannelsTalker.begin();
														}
														else if (!freeChannelsTalker.empty())
														{
															streamChannelToUse = *freeChannelsTalker.begin();
														}

														auto result = buildResult(streamOutputKV.second, streamInputKV.second, streamChannelToUse);
														//the mapping can be removed:
														for (auto unwantedConnectionChannel : unwantedConnectionsAfterAssignments)
														{
															auto unwantedMappingsMatch = getMappingsFromStreamInputChannel(listenerEntityId, streamInputKV.first, unwantedConnectionChannel);

															// append the found mappings
															result.listenerDynamicMappingsToRemove.insert(result.listenerDynamicMappingsToRemove.end(), unwantedMappingsMatch.begin(), unwantedMappingsMatch.end());
														}
														if (streamConfigurationMatches)
														{
															return result;
														}
														else if (defaultResult.connectionsToCreate.empty())
														{
															defaultResult = result;
														}
													}
													else if (defaultResult.connectionsToCreate.empty()) // check if we got a stream reconfiguration option
													{
														defaultResult.unallowedRemovalOfUnusedAudioMappingsNecessary = true;
													}
												}
											}
										}
									}
									else
									{
										// stream already occupied or incompatible (even after reconfiguring)
									}
								}
							}
						}
					}
				}
			}
		}
		catch (la::avdecc::Exception e)
		{
		}
		// no unconnected stream with an unoccupied channel could be found
		return defaultResult;
	}

	/**
	* Checks if the given list of stream formats contains a format with the given type.
	*/
	bool supportsStreamFormat(la::avdecc::entity::model::StreamFormats streamFormats, la::avdecc::entity::model::StreamFormatInfo::Type expectedStreamFormatType) const noexcept
	{
		for (auto streamFormat : streamFormats)
		{
			if (checkStreamFormatType(streamFormat, expectedStreamFormatType))
			{
				return true;
			}
		}
		return false;
	}

	/**
	* Checks if the given stream format is of the given type.
	*/
	bool checkStreamFormatType(la::avdecc::entity::model::StreamFormat streamFormat, la::avdecc::entity::model::StreamFormatInfo::Type expectedStreamFormatType) const noexcept
	{
		auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
		auto const streamFormatType = streamFormatInfo->getType();
		if (expectedStreamFormatType == streamFormatType)
		{
			return true;
		}
		return false;
	}

	std::optional<std::pair<la::avdecc::entity::model::StreamFormat, la::avdecc::entity::model::StreamFormat>> anyStreamFormatCompatible(la::avdecc::entity::model::StreamFormats const& streamFormatsTalker, la::avdecc::entity::model::StreamFormats const& streamFormatsListener, la::avdecc::entity::model::StreamFormatInfo::Type const streamFormatTypeFilter) const noexcept
	{
		for (auto const& streamFormatTalker : streamFormatsTalker)
		{
			auto streamFormatInfoTalker = la::avdecc::entity::model::StreamFormatInfo::create(streamFormatTalker);
			if (streamFormatInfoTalker->getType() == streamFormatTypeFilter)
			{
				for (auto const& streamFormatListener : streamFormatsListener)
				{
					auto streamFormatInfoListener = la::avdecc::entity::model::StreamFormatInfo::create(streamFormatListener);

					if (streamFormatTalker == streamFormatListener)
					{
						return std::make_pair(streamFormatTalker, streamFormatListener);
					}
					else if (streamFormatInfoListener->getType() == streamFormatTypeFilter)
					{
						// check if the streams could be resized to match each other:
						if ((streamFormatInfoTalker->isUpToChannelsCount() && streamFormatInfoListener->isUpToChannelsCount()) || (streamFormatInfoTalker->isUpToChannelsCount() && streamFormatInfoTalker->getChannelsCount() >= streamFormatInfoListener->getChannelsCount()) || (streamFormatInfoListener->isUpToChannelsCount() && streamFormatInfoListener->getChannelsCount() >= streamFormatInfoTalker->getChannelsCount()))
						{
							return std::make_pair(streamFormatTalker, streamFormatListener);
						}
					}
				}
			}
		}
		return std::nullopt;
	}

	std::pair<std::optional<la::avdecc::entity::model::StreamFormat>, std::optional<la::avdecc::entity::model::StreamFormat>> getCompatibleStreamFormatChannelCount(la::avdecc::entity::model::StreamFormat talkerStreamFormat, la::avdecc::entity::model::StreamFormat listenerStreamFormat, uint16_t channelMinSizeHint) const noexcept
	{
		std::optional<la::avdecc::entity::model::StreamFormat> resultingTalkerStreamFormat = std::nullopt;
		std::optional<la::avdecc::entity::model::StreamFormat> resultingListenerStreamFormat = std::nullopt;


		auto resultingTalkerStreamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(talkerStreamFormat);
		auto resultingListenerStreamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(listenerStreamFormat);

		auto resultingTalkerStreamChannelCount = resultingTalkerStreamFormatInfo->getChannelsCount();
		auto isTalkerStreamFormatResizable = resultingTalkerStreamFormatInfo->isUpToChannelsCount();
		auto resultingListenerStreamChannelCount = resultingListenerStreamFormatInfo->getChannelsCount();
		auto isListenerStreamFormatResizable = resultingListenerStreamFormatInfo->isUpToChannelsCount();

		if (resultingTalkerStreamChannelCount != resultingListenerStreamChannelCount || channelMinSizeHint > resultingTalkerStreamChannelCount)
		{
			if (isTalkerStreamFormatResizable && isListenerStreamFormatResizable)
			{
				// both resizable -> resize to hint
				if (channelMinSizeHint < resultingTalkerStreamChannelCount)
				{
				}

				// try to resize to multiples of 8 taking into account the hint and max size of both, if the max of one of the stream formats is less then 8, the max is used instead.
				channelMinSizeHint = std::min(std::min(resultingTalkerStreamChannelCount, channelMinSizeHint), resultingListenerStreamChannelCount);
				if (channelMinSizeHint > 8)
				{
					channelMinSizeHint = (channelMinSizeHint / 8 + channelMinSizeHint % 8 == 0 ? 0 : 1) * 8; // get multiples of 8.
				}

				resultingTalkerStreamFormat = (resultingTalkerStreamFormatInfo)->getAdaptedStreamFormat(channelMinSizeHint);

				resultingListenerStreamFormat = (resultingListenerStreamFormatInfo)->getAdaptedStreamFormat(channelMinSizeHint);
			}
			else if (isTalkerStreamFormatResizable && resultingTalkerStreamChannelCount != resultingListenerStreamChannelCount)
			{
				// only talker resizable, resize to listener size
				resultingTalkerStreamFormat = (resultingTalkerStreamFormatInfo)->getAdaptedStreamFormat(resultingListenerStreamChannelCount);
			}
			else if (isListenerStreamFormatResizable && resultingTalkerStreamChannelCount != resultingListenerStreamChannelCount)
			{
				// only listener resizable, resize to talker size
				resultingListenerStreamFormat = (resultingListenerStreamFormatInfo)->getAdaptedStreamFormat(resultingTalkerStreamChannelCount);
			}
		}
		return std::make_pair(resultingTalkerStreamFormat, resultingListenerStreamFormat);
	}

	/**
	* Changes the stream format if it is not already the given type and adjusts the number of channels if necessary.
	*/
	std::pair<std::optional<la::avdecc::entity::model::StreamFormat>, std::optional<la::avdecc::entity::model::StreamFormat>> findCompatibleStreamPairFormat(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::StreamIndex streamOutputIndex, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::StreamIndex streamInputIndex, la::avdecc::entity::model::StreamFormatInfo::Type expectedStreamFormatType, uint16_t channelMinSizeHint = 0) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledTalkerEntity = manager.getControlledEntity(talkerEntityId);
		auto controlledListenerEntity = manager.getControlledEntity(listenerEntityId);

		if (!controlledTalkerEntity || !controlledListenerEntity)
		{
			return std::make_pair(std::nullopt, std::nullopt);
		}
		if (!controlledTalkerEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return std::make_pair(std::nullopt, std::nullopt);
		}
		if (!controlledListenerEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return std::make_pair(std::nullopt, std::nullopt);
		}

		la::avdecc::controller::model::ConfigurationNode talkerConfigurationNode;
		try
		{
			talkerConfigurationNode = controlledTalkerEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return std::make_pair(std::nullopt, std::nullopt);
		}

		la::avdecc::controller::model::ConfigurationNode listenerConfigurationNode;
		try
		{
			listenerConfigurationNode = controlledListenerEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return std::make_pair(std::nullopt, std::nullopt);
		}

		std::optional<la::avdecc::entity::model::StreamFormat> resultingTalkerStreamFormat{ std::nullopt };
		std::optional<la::avdecc::entity::model::StreamFormatInfo::UniquePointer> resultingTalkerStreamFormatInfo{ std::nullopt };
		std::optional<la::avdecc::entity::model::StreamFormat> resultingListenerStreamFormat{ std::nullopt };
		std::optional<la::avdecc::entity::model::StreamFormatInfo::UniquePointer> resultingListenerStreamFormatInfo{ std::nullopt };

		la::avdecc::controller::model::StreamOutputNode streamOutputNode;
		try
		{
			streamOutputNode = controlledTalkerEntity->getStreamOutputNode(talkerConfigurationNode.descriptorIndex, streamOutputIndex);
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return std::make_pair(std::nullopt, std::nullopt);
		}

		auto const& currentStreamOutputFormat = streamOutputNode.dynamicModel->streamInfo.streamFormat;
		auto const currentStreamOutputFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(currentStreamOutputFormat);

		la::avdecc::controller::model::StreamInputNode streamInputNode;
		try
		{
			streamInputNode = controlledListenerEntity->getStreamInputNode(listenerConfigurationNode.descriptorIndex, streamInputIndex);
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return std::make_pair(std::nullopt, std::nullopt);
		}
		auto const& currentStreamInputFormat = streamInputNode.dynamicModel->streamInfo.streamFormat;

		std::vector<std::pair<la::avdecc::entity::model::StreamFormat, la::avdecc::entity::model::StreamFormat>> compatibleFormatOptions;

		auto const& streamOutputFormats = streamOutputNode.staticModel->formats;
		for (auto const& streamOutputFormat : streamOutputFormats)
		{
			auto const streamFormatInfoTalker = la::avdecc::entity::model::StreamFormatInfo::create(streamOutputFormat);
			auto const streamFormatTypeTalker = streamFormatInfoTalker->getType();
			auto const streamFormatSamplingRateTalker = streamFormatInfoTalker->getSamplingRate();
			auto const streamFormatSampleFormatTalker = streamFormatInfoTalker->getSampleFormat();
			auto const streamFormatSynchronousClockTalker = streamFormatInfoTalker->useSynchronousClock();
			if (expectedStreamFormatType == streamFormatTypeTalker)
			{
				// search for fitting listener configuration.
				auto const& streamInputFormats = streamInputNode.staticModel->formats;
				for (auto const& streamInputFormat : streamInputFormats)
				{
					auto const streamFormatInfoListener = la::avdecc::entity::model::StreamFormatInfo::create(streamInputFormat);
					auto const streamFormatTypeListener = streamFormatInfoListener->getType();
					auto const streamFormatSamplingRateListener = streamFormatInfoListener->getSamplingRate();
					auto const streamFormatSampleFormatListener = streamFormatInfoListener->getSampleFormat();
					auto const streamFormatSynchronousClockListener = streamFormatInfoListener->useSynchronousClock();

					// check if compatible (after size adaptations)
					if (expectedStreamFormatType == streamFormatTypeListener && streamFormatSamplingRateTalker == streamFormatSamplingRateListener && streamFormatSampleFormatTalker == streamFormatSampleFormatListener && (streamFormatSynchronousClockTalker || !streamFormatSynchronousClockListener) && (streamFormatInfoTalker->getChannelsCount() == streamFormatInfoListener->getChannelsCount() || (streamFormatInfoTalker->isUpToChannelsCount() && streamFormatInfoListener->isUpToChannelsCount()) || (streamFormatInfoTalker->isUpToChannelsCount() && streamFormatInfoTalker->getChannelsCount() >= streamFormatInfoListener->getChannelsCount()) || (streamFormatInfoListener->isUpToChannelsCount() && streamFormatInfoTalker->getChannelsCount() <= streamFormatInfoListener->getChannelsCount())))
					{
						compatibleFormatOptions.push_back(std::make_pair(streamOutputFormat, streamInputFormat));
					}
				}
			}
		}

		if (compatibleFormatOptions.empty())
		{
			// no compatible formats, abort
			return std::make_pair(std::nullopt, std::nullopt);
		}

		// prefer the format that is currently the talker format, if possible, then the listener format
		// and as last option change both.
		bool optionFound = false;
		for (auto const& compatibleFormatOption : compatibleFormatOptions)
		{
			if (compatibleFormatOption.first == currentStreamOutputFormat)
			{
				resultingListenerStreamFormat = compatibleFormatOption.second;
				resultingListenerStreamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(*resultingListenerStreamFormat);
				optionFound = true;
				break;
			}
		}
		if (!optionFound)
		{
			for (auto const& compatibleFormatOption : compatibleFormatOptions)
			{
				if (compatibleFormatOption.second == currentStreamInputFormat)
				{
					resultingTalkerStreamFormat = compatibleFormatOption.first;
					resultingTalkerStreamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(*resultingTalkerStreamFormat);
					optionFound = true;
					break;
				}
			}
			if (!optionFound)
			{
				resultingTalkerStreamFormat = compatibleFormatOptions.begin()->first;
				resultingTalkerStreamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(*resultingTalkerStreamFormat);
				resultingListenerStreamFormat = compatibleFormatOptions.begin()->second;
				resultingListenerStreamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(*resultingListenerStreamFormat);
			}
		}

		// Change the channel count of the stream format if necessary
		auto compatibleResizedStreamFormats = getCompatibleStreamFormatChannelCount(resultingTalkerStreamFormat ? *resultingTalkerStreamFormat : currentStreamOutputFormat, resultingListenerStreamFormat ? *resultingListenerStreamFormat : currentStreamInputFormat, channelMinSizeHint);
		if (compatibleResizedStreamFormats.first)
		{
			resultingTalkerStreamFormat = compatibleResizedStreamFormats.first;
		}
		if (compatibleResizedStreamFormats.second)
		{
			resultingListenerStreamFormat = compatibleResizedStreamFormats.second;
		}
		return std::make_pair(resultingTalkerStreamFormat, resultingListenerStreamFormat);
	}

	void adjustStreamPairFormats(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::StreamIndex streamOutputIndex, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::StreamIndex streamInputIndex, std::pair<std::optional<la::avdecc::entity::model::StreamFormat>, std::optional<la::avdecc::entity::model::StreamFormat>> streamFormats) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		if (streamFormats.first)
		{
			manager.setStreamOutputFormat(talkerEntityId, streamOutputIndex, *streamFormats.first);
		}
		if (streamFormats.second)
		{
			manager.setStreamInputFormat(listenerEntityId, streamInputIndex, *streamFormats.second);
		}
	}

	/**
	* Returns the count of channels a stream supports.
	*/
	uint16_t getStreamChannelCount(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex streamIndex) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityId);

		if (!controlledEntity)
		{
			return 0;
		}
		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return 0;
		}

		la::avdecc::controller::model::ConfigurationNode configurationNode;
		try
		{
			configurationNode = controlledEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return 0;
		}

		// get the stream channel count:
		auto const& streamNode = controlledEntity->getStreamInputNode(configurationNode.descriptorIndex, streamIndex);
		auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode.dynamicModel->streamInfo.streamFormat);
		return sfi->getChannelsCount();
	}

	/**
	* Finds a free or reusable stream connection to setup the channel connection with.
	* The stream is reused in the case the needed talker cluster is already in use in some stream channel.
	* @return Tuple with: <uint16_t: the channel to use; bool: the talker channel mapping already exists; bool: the listener channel mapping already exists>
	*/
	std::optional<std::tuple<uint16_t, bool, bool>> findFreeOrReuseChannelOnExistingStreamConnection(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::StreamIndex talkerStreamIndex, la::avdecc::entity::model::ClusterIndex talkerClusterOffset, uint16_t talkerClusterChannel, StreamChannelMappings const& newMappingsTalker, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::StreamIndex listenerStreamIndex, la::avdecc::entity::model::ClusterIndex listenerClusterOffset, uint16_t listenerClusterChannel, StreamChannelMappings const& newMappingsListener, std::optional<uint16_t> listenerStreamChannelCountAfterFormatChange = std::nullopt) const noexcept
	{
		auto existingFittingTalkerMappings = getAssignedChannelsOnTalkerStream(talkerEntityId, talkerStreamIndex, talkerClusterOffset, talkerClusterChannel);
		auto const& newMappingsTalkerIt = newMappingsTalker.find(talkerStreamIndex);
		auto const& newMappingsListenerIt = newMappingsListener.find(listenerStreamIndex);
		if (newMappingsTalkerIt != newMappingsTalker.end())
		{
			for (auto const& mappingWrapper : newMappingsTalkerIt->second)
			{
				for (auto const& mapping : mappingWrapper.second)
				{
					if (mapping.clusterOffset == talkerClusterOffset && mapping.clusterChannel == talkerClusterChannel)
					{
						existingFittingTalkerMappings.insert(mapping.streamChannel);
					}
				}
			}
		}

		if (!existingFittingTalkerMappings.empty())
		{
			// reuse existing channel
			return std::make_tuple(*existingFittingTalkerMappings.begin(), true, false);
		}
		else
		{
			// search for listener channel mapping that could be reused.
			{
				auto existingFittingListenerMappings = getAssignedChannelsOnListenerStream(listenerEntityId, listenerStreamIndex, listenerClusterOffset, listenerClusterChannel);

				if (newMappingsListenerIt != newMappingsListener.end())
				{
					for (auto const& mappingWrapper : newMappingsListenerIt->second)
					{
						for (auto const& mapping : mappingWrapper.second)
						{
							if (mapping.clusterOffset == listenerClusterOffset && mapping.clusterChannel == listenerClusterChannel)
							{
								existingFittingListenerMappings.insert(mapping.streamChannel);
							}
						}
					}
				}

				auto unassignedTalkerChannels = getUnassignedChannelsOnTalkerStream(talkerEntityId, talkerStreamIndex);
				if (!existingFittingListenerMappings.empty())
				{
					std::vector<uint16_t> intersectionReuseListenerMapping;
					set_intersection(existingFittingListenerMappings.begin(), existingFittingListenerMappings.end(), unassignedTalkerChannels.begin(), unassignedTalkerChannels.end(), back_inserter(intersectionReuseListenerMapping));
					if (!intersectionReuseListenerMapping.empty())
					{
						// remove the reusage if another listener is connected to the streamOutput and uses the same stream channel.
						auto streamOutputConnections = getStreamOutputConnections(talkerEntityId, talkerStreamIndex);
						for (auto const& streamOutputConnection : streamOutputConnections)
						{
							if (streamOutputConnection.listenerStream.entityID == listenerEntityId && streamOutputConnection.listenerStream.streamIndex == listenerStreamIndex)
							{
								// it's the connection we're trying to reuse
								continue;
							}

							auto listenerOccupiedChannels = getAssignedChannelsOnListenerStream(streamOutputConnection.listenerStream.entityID, streamOutputConnection.listenerStream.streamIndex);
							for (auto const& listenerOccupiedChannel : listenerOccupiedChannels)
							{
								auto it = std::find(intersectionReuseListenerMapping.begin(), intersectionReuseListenerMapping.end(), listenerOccupiedChannel);
								if (it != intersectionReuseListenerMapping.end() && listenerClusterOffset != *it)
								{
									intersectionReuseListenerMapping.erase(it);
								}
							}
						}
					}

					if (!intersectionReuseListenerMapping.empty())
					{
						return std::make_tuple(intersectionReuseListenerMapping.at(0), false, true);
					}
				}
			}

			// if we get here there is no fitting channel mapping on either side (listener or talker).
			{
				auto freeStreamSlotsSource = getUnassignedChannelsOnTalkerStream(talkerEntityId, talkerStreamIndex);

				// filter out mappings that will be created
				if (newMappingsTalkerIt != newMappingsTalker.end())
				{
					for (auto const& mappingWrapper : newMappingsTalkerIt->second)
					{
						for (auto const& mapping : mappingWrapper.second)
						{
							auto const& slotIt = freeStreamSlotsSource.find(mapping.streamChannel);
							if (slotIt != freeStreamSlotsSource.end())
							{
								freeStreamSlotsSource.erase(mapping.streamChannel);
							}
						}
					}
				}

				if (!freeStreamSlotsSource.empty())
				{
					// find new channel
					// the following has to be adjusted to reflect stream format changes:
					std::set<uint16_t> freeStreamSlotsTarget;

					if (listenerStreamChannelCountAfterFormatChange) // check optional param
					{
						for (int i = 0; i < *listenerStreamChannelCountAfterFormatChange; i++)
						{
							freeStreamSlotsTarget.insert(i);
						}
					}
					else
					{
						freeStreamSlotsTarget = getUnassignedChannelsOnListenerStream(listenerEntityId, listenerStreamIndex);
					}

					// filter out mappings that will be created
					if (newMappingsListenerIt != newMappingsListener.end())
					{
						for (auto const& mappingWrapper : newMappingsListenerIt->second)
						{
							for (auto const& mapping : mappingWrapper.second)
							{
								auto const& slotIt = freeStreamSlotsTarget.find(mapping.streamChannel);
								if (slotIt != freeStreamSlotsTarget.end())
								{
									freeStreamSlotsTarget.erase(mapping.streamChannel);
								}
							}
						}
					}

					std::vector<uint16_t> intersection;
					set_intersection(freeStreamSlotsSource.begin(), freeStreamSlotsSource.end(), freeStreamSlotsTarget.begin(), freeStreamSlotsTarget.end(), back_inserter(intersection));

					auto streamOutputConnections = getStreamOutputConnections(talkerEntityId, talkerStreamIndex);
					for (auto const& streamOutputConnection : streamOutputConnections)
					{
						if (intersection.empty())
						{
							break;
						}
						auto listenerOccupiedChannels = getAssignedChannelsOnListenerStream(streamOutputConnection.listenerStream.entityID, streamOutputConnection.listenerStream.streamIndex);
						for (auto const& listenerOccupiedChannel : listenerOccupiedChannels)
						{
							auto it = std::find(intersection.begin(), intersection.end(), listenerOccupiedChannel);
							if (it != intersection.end())
							{
								intersection.erase(it);
							}
						}
					}

					if (!intersection.empty())
					{
						return std::make_tuple(intersection.at(0), false, false);
					}
				}
			}
		}

		return std::nullopt;
	}
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
