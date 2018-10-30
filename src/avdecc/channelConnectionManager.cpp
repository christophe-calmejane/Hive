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
public:
	/**
		* Constructor.
		*/
	ChannelConnectionManagerImpl() noexcept
	{
		auto& controllerManager = avdecc::ControllerManager::getInstance();
		connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &ChannelConnectionManagerImpl::controllerOffline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &ChannelConnectionManagerImpl::entityOnline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &ChannelConnectionManagerImpl::entityOffline);
		connect(&controllerManager, &avdecc::ControllerManager::streamRunningChanged, this, &ChannelConnectionManagerImpl::streamRunningChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamConnectionChanged, this, &ChannelConnectionManagerImpl::streamConnectionChanged);
	}

	/**
		* Destructor.
		*/
	~ChannelConnectionManagerImpl() noexcept {}

	/**
		* Slot to handle controller going offline.
		*/
	void controllerOffline() {}

	/**
		* Slot to handle an entity going online. Adds to the internal lists of all talkers and listener entity ids.
		* @param entityId The id of the entity that was detected online.
		*/
	void entityOnline(la::avdecc::UniqueIdentifier const entityId) {}

	/**
		* Slot to handle an entity going offline. Removes from the internal lists of all talkers and listener entity ids.
		* @param entityId The id of the entity that was detected offline.
		*/
	void entityOffline(la::avdecc::UniqueIdentifier const entityID) {}

	/**
		* TODO
		*/
	void streamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning) {}

	/**
		* TODO
		*/
	void streamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& state) {}

	/**
		* Helper function for connectionStatus medthod. Checks if the stream is currently connected.
		* @param talkerID		The id of the talking entity. 
		* @param talkerNode		The index of the output node to check.
		* @param listenerNode	The index of the input node to check.
		*/
	bool isStreamConnected(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) const noexcept
	{
		return (listenerNode->dynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::Connected) && (listenerNode->dynamicModel->connectionState.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionState.talkerStream.streamIndex == talkerNode->descriptorIndex);
	}

	/**
		* Helper function for connectionStatus medthod. Checks if the stream is in fast connect mode.
		* @param talkerID		The id of the talking entity.
		* @param talkerNode		The index of the output node to check.
		* @param listenerNode	The index of the input node to check.
		*/
	bool isStreamFastConnecting(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) const noexcept
	{
		return (listenerNode->dynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::FastConnecting) && (listenerNode->dynamicModel->connectionState.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionState.talkerStream.streamIndex == talkerNode->descriptorIndex);
	}

	/**
		* Detect the current connection status to be displayed.
		* @param talkerEntityId				The id of the talking entity.
		* @param talkerStreamIndex			The index of the output stream to check.
		* @param talkerStreamRedundant		Flag indicating if the talking stream is redundant.
		* @param listenerEntityId			The id of the listening entity.
		* @param listenerStreamIndex		The index of the inout stream to check.
		* @param listenerStreamRedundant	Flag indicating if the listening stream is redundant.
		*/
	ConnectionStatus connectionStatus(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::StreamIndex talkerStreamIndex, bool talkerStreamRedundant, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::StreamIndex listenerStreamIndex, bool listenerStreamRedundant) const noexcept
	{
		if (talkerEntityId == listenerEntityId)
			return ConnectionStatus::None;

		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto talkerEntity = manager.getControlledEntity(talkerEntityId);
			auto listenerEntity = manager.getControlledEntity(listenerEntityId);
			if (talkerEntity && listenerEntity)
			{
				auto const& talkerEntityNode = talkerEntity->getEntityNode();
				auto const& talkerEntityInfo = talkerEntity->getEntity();
				auto const& listenerEntityNode = listenerEntity->getEntityNode();
				auto const& listenerEntityInfo = listenerEntity->getEntity();

				auto const computeFormatCompatible = [](la::avdecc::controller::model::StreamOutputNode const& talkerNode, la::avdecc::controller::model::StreamInputNode const& listenerNode)
				{
					return la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerNode.dynamicModel->currentFormat, talkerNode.dynamicModel->currentFormat);
				};
				auto const computeDomainCompatible = [&talkerEntityInfo, &listenerEntityInfo]()
				{
					// TODO: Incorrect computation, must be based on the AVBInterface for the stream
					return listenerEntityInfo.getGptpGrandmasterID() == talkerEntityInfo.getGptpGrandmasterID();
				};
				enum class ConnectState
				{
					NotConnected = 0,
					FastConnecting,
					Connected,
				};
				auto const computeCapabilities = [](ConnectState const connectState, bool const areAllConnected, bool const isFormatCompatible, bool const isDomainCompatible)
				{
					auto caps{ ConnectionStatus::Connectable };

					if (!isDomainCompatible)
						caps |= ConnectionStatus::WrongDomain;

					if (!isFormatCompatible)
						caps |= ConnectionStatus::WrongFormat;

					if (connectState != ConnectState::NotConnected)
					{
						if (areAllConnected)
							caps |= ConnectionStatus::Connected;
						else if (connectState == ConnectState::FastConnecting)
							caps |= ConnectionStatus::FastConnecting;
						else
							caps |= ConnectionStatus::PartiallyConnected;
					}

					return caps;
				};

				{
					la::avdecc::controller::model::StreamOutputNode const* talkerNode{ nullptr };
					la::avdecc::controller::model::StreamInputNode const* listenerNode{ nullptr };
					talkerNode = &talkerEntity->getStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerStreamIndex);
					listenerNode = &listenerEntity->getStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerStreamIndex);

					// Get connected state
					auto const areConnected = isStreamConnected(talkerEntityId, talkerNode, listenerNode);
					auto const fastConnecting = isStreamFastConnecting(talkerEntityId, talkerNode, listenerNode);
					auto const connectState = areConnected ? ConnectState::Connected : (fastConnecting ? ConnectState::FastConnecting : ConnectState::NotConnected);

					// Get stream format compatibility
					auto const isFormatCompatible = computeFormatCompatible(*talkerNode, *listenerNode);

					// Get domain compatibility
					auto const isDomainCompatible = computeDomainCompatible();

					return computeCapabilities(connectState, areConnected, isFormatCompatible, isDomainCompatible);
				}
			}
		}
		catch (...)
		{
		}

		return ConnectionStatus::None;
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
	virtual const ConnectionInformation getChannelConnections(la::avdecc::UniqueIdentifier const entityId, la::avdecc::UniqueIdentifier const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, la::avdecc::entity::model::ClusterIndex const baseCluster, std::uint16_t const clusterChannel) noexcept
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

		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityId);

		auto const* const entity = controlledEntity.get();
		if (entity == nullptr)
		{
			return result;
		}
		auto const& entityNode = entity->getEntityNode();
		auto const& configurationNode = entity->getConfigurationNode(configurationIndex);

		la::avdecc::entity::model::AudioMappings mappings;
		mappings = configurationNode.audioUnits.at(audioUnitIndex).streamPortOutputs.at(streamPortIndex).dynamicModel->dynamicAudioMap;

		for (auto const& mapping : mappings)
		{
			if (mapping.clusterOffset == clusterIndex - baseCluster && clusterChannel == mapping.clusterChannel)
			{
				result.sourceStreams.append(std::make_pair(mapping.streamIndex, mapping.streamChannel));
			}
		}

		// find out the connected streams:
		for (auto const& stream : result.sourceStreams)
		{
			std::uint16_t sourceStreamChannel = stream.second;
			auto const& streamConnections = entity->getStreamOutputConnections(stream.first);
			for (auto const& streamConnection : streamConnections)
			{
				if (!result.deviceConnections.contains(streamConnection.entityID))
				{
					QSharedPointer<Connections> connections = QSharedPointer<Connections>::create();
					connections->entityId = streamConnection.entityID;
					connections->targetStreams.insert(streamConnection.streamIndex, QSharedPointer<ConnectionDetails>::create());
					result.deviceConnections.insert(streamConnection.entityID, connections);
					result.deviceConnections.value(streamConnection.entityID)->streamConnectionStatus = connectionStatus(entityId, stream.first, false, streamConnection.entityID, streamConnection.streamIndex, false);
				}
				else
				{
					if (!result.deviceConnections.value(streamConnection.entityID)->targetStreams.contains(streamConnection.streamIndex))
					{
						result.deviceConnections.value(streamConnection.entityID)->targetStreams.insertMulti(streamConnection.streamIndex, QSharedPointer<ConnectionDetails>::create());
					}
				}

				// after getting the connected stream, resolve the underlying channels:
				auto targetControlledEntity = manager.getControlledEntity(streamConnection.entityID);

				auto const* const targetEntity = targetControlledEntity.get();
				if (targetEntity == nullptr)
				{
					continue;
				}
				auto const& targetEntityNode = targetEntity->getEntityNode();
				auto const& targetConfigurationNode = targetEntity->getConfigurationNode(targetEntityNode.dynamicModel->currentConfiguration);


				// TODO find correct index of audio unit and stream port index:
				for (auto const& audioUnit : targetConfigurationNode.audioUnits)
				{
					for (auto const& streamPortInput : audioUnit.second.streamPortInputs)
					{
						la::avdecc::entity::model::AudioMappings targetMappings = streamPortInput.second.dynamicModel->dynamicAudioMap;
						for (auto const& mapping : targetMappings)
						{
							// the source stream channel is connected to the corresponding target stream channel.
							if (mapping.streamIndex == streamConnection.streamIndex && mapping.streamChannel == sourceStreamChannel)
							{
								result.deviceConnections.value(streamConnection.entityID)->targetStreams.value(mapping.streamIndex)->targetClusters.append(std::make_pair(mapping.clusterOffset, mapping.clusterChannel));
								result.deviceConnections.value(streamConnection.entityID)->targetStreams.value(mapping.streamIndex)->targetAudioUnitIndex = audioUnit.first;
								result.deviceConnections.value(streamConnection.entityID)->targetStreams.value(mapping.streamIndex)->targetBaseCluster = streamPortInput.second.staticModel->baseCluster;
								result.deviceConnections.value(streamConnection.entityID)->targetStreams.value(mapping.streamIndex)->targetStreamPortIndex = streamPortInput.first;
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

		auto const* const entity = controlledEntity.get();
		if (entity == nullptr)
		{
			return result;
		}
		auto const& entityNode = entity->getEntityNode();
		auto const& configurationNode = entity->getConfigurationNode(configurationIndex);

		la::avdecc::entity::model::AudioMappings mappings;
		mappings = configurationNode.audioUnits.at(audioUnitIndex).streamPortInputs.at(streamPortIndex).dynamicModel->dynamicAudioMap;

		// find all streams this cluster is connected to. (Should only be 1)
		for (auto const& mapping : mappings)
		{
			if (mapping.clusterOffset == clusterIndex - baseCluster && clusterChannel == mapping.clusterChannel)
			{
				result.sourceStreams.append(std::make_pair(mapping.streamIndex, mapping.streamChannel));
			}
		}

		// find out the connected streams:
		for (auto const& stream : result.sourceStreams)
		{
			la::avdecc::UniqueIdentifier connectedTalker = entity->getCurrentConfigurationNode().streamInputs.at(stream.first).dynamicModel->connectionState.talkerStream.entityID;
			la::avdecc::entity::model::StreamIndex connectedTalkerStreamIndex = entity->getCurrentConfigurationNode().streamInputs.at(stream.first).dynamicModel->connectionState.talkerStream.streamIndex;

			QSharedPointer<Connections> connections = QSharedPointer<Connections>::create();
			connections->entityId = connectedTalker;
			connections->targetStreams.insert(connectedTalkerStreamIndex, QSharedPointer<ConnectionDetails>::create());
			result.deviceConnections.insert(connectedTalker, connections);
			result.deviceConnections.value(connectedTalker)->streamConnectionStatus = connectionStatus(connectedTalker, connectedTalkerStreamIndex, false, entityId, stream.first, false);

			std::uint16_t sourceStreamChannel = stream.second;

			// after getting the connected stream, resolve the underlying channels:
			auto targetControlledEntity = manager.getControlledEntity(connectedTalker);

			auto const* const targetEntity = targetControlledEntity.get();
			if (targetEntity == nullptr)
			{
				continue;
			}
			auto const& targetEntityNode = targetEntity->getEntityNode();
			auto const& targetConfigurationNode = targetEntity->getConfigurationNode(targetEntityNode.dynamicModel->currentConfiguration);

			// TODO find correct index of audio unit and stream port index:
			for (auto const& audioUnit : targetConfigurationNode.audioUnits)
			{
				for (auto const& streamPortOutput : audioUnit.second.streamPortOutputs)
				{
					la::avdecc::entity::model::AudioMappings targetMappings = streamPortOutput.second.dynamicModel->dynamicAudioMap;
					for (auto const& mapping : targetMappings)
					{
						// the source stream channel is connected to the corresponding target stream channel.
						if (mapping.streamIndex == connectedTalkerStreamIndex && mapping.streamChannel == sourceStreamChannel)
						{
							result.deviceConnections.value(connectedTalker)->targetStreams.value(mapping.streamIndex)->targetClusters.append(std::make_pair(mapping.clusterOffset, mapping.clusterChannel));
							result.deviceConnections.value(connectedTalker)->targetStreams.value(mapping.streamIndex)->targetAudioUnitIndex = audioUnit.first;
							result.deviceConnections.value(connectedTalker)->targetStreams.value(mapping.streamIndex)->targetBaseCluster = streamPortOutput.second.staticModel->baseCluster;
							result.deviceConnections.value(connectedTalker)->targetStreams.value(mapping.streamIndex)->targetStreamPortIndex = streamPortOutput.first;
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
