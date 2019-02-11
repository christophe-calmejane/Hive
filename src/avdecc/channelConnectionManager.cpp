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
	}

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
	}

	/**
	* Destructor.
	*/
	~ChannelConnectionManagerImpl() noexcept {}

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
	virtual const ConnectionInformation getChannelConnections(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, la::avdecc::entity::model::ClusterIndex const baseCluster, std::uint16_t const clusterChannel) noexcept
	{
		ConnectionInformation result;
		result.sourceEntityId = entityId;
		result.sourceConfigurationIndex = configurationIndex;
		result.sourceAudioUnitIndex = audioUnitIndex;
		result.sourceStreamPortIndex = streamPortIndex;
		result.sourceClusterIndex = clusterIndex;
		result.sourceBaseCluster = baseCluster;
		result.sourceClusterChannel = clusterChannel;
		result.forward = true;

		// find channel connections via connection matrix + stream connections.
		// an output channel can be connected to one or multiple input channels on different devices or to none.

		auto const& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityId);
		if (!controlledEntity)
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
			auto sourceStreamIndex = stream.first;
			auto sourceStreamChannel = stream.second;

			for (auto const& streamConnection : streamConnections)
			{
				if (streamConnection.talkerStream.streamIndex == sourceStreamIndex)
				{
					// after getting the connected stream, resolve the underlying channels:
					auto targetControlledEntity = manager.getControlledEntity(streamConnection.listenerStream.entityID);
					if (!targetControlledEntity)
					{
						continue;
					}
					auto const& targetEntityNode = targetControlledEntity->getEntityNode();
					if (targetEntityNode.dynamicModel)
					{
						auto const& targetConfigurationNode = targetControlledEntity->getConfigurationNode(targetEntityNode.dynamicModel->currentConfiguration);

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
										// the source stream channel is connected to the corresponding target stream channel.
										if (mapping.streamIndex == streamConnection.listenerStream.streamIndex && mapping.streamChannel == sourceStreamChannel)
										{
											if (!result.deviceConnections.count(streamConnection.listenerStream.entityID))
											{
												auto connections = std::make_shared<Connections>();
												connections->entityId = streamConnection.listenerStream.entityID;
												connections->targetStreams.emplace(streamConnection.listenerStream.streamIndex, std::make_shared<ConnectionDetails>());
												result.deviceConnections.emplace(streamConnection.listenerStream.entityID, connections);
											}
											else
											{
												if (!result.deviceConnections.at(streamConnection.listenerStream.entityID)->targetStreams.count(streamConnection.listenerStream.streamIndex))
												{
													result.deviceConnections.at(streamConnection.listenerStream.entityID)->targetStreams.emplace(streamConnection.listenerStream.streamIndex, std::make_shared<ConnectionDetails>());
												}
											}

											result.deviceConnections.at(streamConnection.listenerStream.entityID)->targetStreams.at(mapping.streamIndex)->streamChannel = sourceStreamChannel;
											result.deviceConnections.at(streamConnection.listenerStream.entityID)->targetStreams.at(mapping.streamIndex)->sourceStreamIndex = stream.first;
											result.deviceConnections.at(streamConnection.listenerStream.entityID)->targetStreams.at(mapping.streamIndex)->targetClusters.push_back(std::make_pair(mapping.clusterOffset, mapping.clusterChannel));
											result.deviceConnections.at(streamConnection.listenerStream.entityID)->targetStreams.at(mapping.streamIndex)->targetAudioUnitIndex = audioUnitKV.first;
											if (streamPortInputKV.second.staticModel)
											{
												result.deviceConnections.at(streamConnection.listenerStream.entityID)->targetStreams.at(mapping.streamIndex)->targetBaseCluster = streamPortInputKV.second.staticModel->baseCluster;
											}
											result.deviceConnections.at(streamConnection.listenerStream.entityID)->targetStreams.at(mapping.streamIndex)->targetStreamPortIndex = streamPortInputKV.first;
											result.deviceConnections.at(streamConnection.listenerStream.entityID)->targetStreams.at(mapping.streamIndex)->isTargetRedundant = targetControlledEntity->getStreamInputNode(targetConfigurationNode.descriptorIndex, mapping.streamIndex).isRedundant;
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
	virtual const ConnectionInformation getChannelConnectionsReverse(la::avdecc::UniqueIdentifier const entityId, la::avdecc::UniqueIdentifier const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, la::avdecc::entity::model::ClusterIndex const baseCluster, std::uint16_t const clusterChannel) noexcept
	{
		ConnectionInformation result;
		result.sourceEntityId = entityId;
		result.sourceConfigurationIndex = configurationIndex;
		result.sourceAudioUnitIndex = audioUnitIndex;
		result.sourceStreamPortIndex = streamPortIndex;
		result.sourceClusterIndex = clusterIndex;
		result.sourceBaseCluster = baseCluster;
		result.sourceClusterChannel = clusterChannel;
		result.forward = false;

		// find channel connections via connection matrix + stream connections.
		// an output channel can be connected to one or multiple input channels on different devices or to none.

		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityId);

		if (!controlledEntity)
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
				auto const& targetEntityNode = targetControlledEntity->getEntityNode();
				if (targetEntityNode.dynamicModel)
				{
					auto const& targetConfigurationNode = targetControlledEntity->getConfigurationNode(targetEntityNode.dynamicModel->currentConfiguration);

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
									// the source stream channel is connected to the corresponding target stream channel.
									if (mapping.streamIndex == connectedTalkerStreamIndex && mapping.streamChannel == sourceStreamChannel)
									{
										if (!result.deviceConnections.count(connectedTalker))
										{
											auto connections = std::make_shared<Connections>();
											connections->entityId = connectedTalker;
											connections->targetStreams.emplace(connectedTalkerStreamIndex, std::make_shared<ConnectionDetails>());
											connections->isSourceRedundant = controlledEntity->getStreamInputNode(configurationNode.descriptorIndex, stream.first).isRedundant;
											result.deviceConnections.emplace(connectedTalker, connections);
										}
										else
										{
											if (!result.deviceConnections.at(connectedTalker)->targetStreams.count(connectedTalkerStreamIndex))
											{
												result.deviceConnections.at(connectedTalker)->targetStreams.emplace(connectedTalkerStreamIndex, std::make_shared<ConnectionDetails>());
											}
										}

										result.deviceConnections.at(connectedTalker)->targetStreams.at(mapping.streamIndex)->streamChannel = sourceStreamChannel;
										result.deviceConnections.at(connectedTalker)->targetStreams.at(mapping.streamIndex)->sourceStreamIndex = stream.first;
										result.deviceConnections.at(connectedTalker)->targetStreams.at(mapping.streamIndex)->targetClusters.push_back(std::make_pair(mapping.clusterOffset, mapping.clusterChannel));
										result.deviceConnections.at(connectedTalker)->targetStreams.at(mapping.streamIndex)->targetAudioUnitIndex = audioUnitKV.first;
										if (streamPortOutputKV.second.staticModel)
										{
											result.deviceConnections.at(connectedTalker)->targetStreams.at(mapping.streamIndex)->targetBaseCluster = streamPortOutputKV.second.staticModel->baseCluster;
										}
										result.deviceConnections.at(connectedTalker)->targetStreams.at(mapping.streamIndex)->targetStreamPortIndex = streamPortOutputKV.first;
										result.deviceConnections.at(connectedTalker)->targetStreams.at(mapping.streamIndex)->isTargetRedundant = targetControlledEntity->getStreamOutputNode(targetConfigurationNode.descriptorIndex, mapping.streamIndex).isRedundant;
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
