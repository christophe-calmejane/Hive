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

#include "channelConnectionManager.hpp"

#include <set>
#include <algorithm>
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

		connect(&manager, &ControllerManager::streamInputConnectionChanged, this, &ChannelConnectionManagerImpl::onStreamInputConnectionChanged);
		connect(&manager, &ControllerManager::streamPortAudioMappingsChanged, this, &ChannelConnectionManagerImpl::onStreamPortAudioMappingsChanged);
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
		StreamChannelConnections connectionsToCreate{};
		la::avdecc::entity::model::AudioMappings listenerDynamicMappingsToRemove{};
		bool unallowedRemovalOfUnusedAudioMappingsNecessary{ false };
	};

	using StreamPortAudioMappings = std::map<la::avdecc::entity::model::StreamPortIndex, la::avdecc::entity::model::AudioMappings>;
	using StreamChannelMappings = std::map<la::avdecc::entity::model::StreamIndex, StreamPortAudioMappings>;
	using StreamConnection = std::pair<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex>;
	using StreamConnections = std::vector<StreamConnection>;
	using StreamFormatChanges = std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamFormat>;
	struct CheckChannelCreationsPossibleResult
	{
		ChannelConnectResult connectionCheckResult{ ChannelConnectResult::NoError };
		StreamChannelMappings overriddenMappingsListener{};
		StreamChannelMappings newMappingsTalker{};
		StreamChannelMappings newMappingsListener{};
		StreamConnections newStreamConnections{};
	};

	struct StreamChannelInfo
	{
		StreamChannelInfo(la::avdecc::entity::model::StreamIndex const talkerPrimaryStreamIndex, la::avdecc::entity::model::StreamIndex const listenerPrimaryStreamIndex, uint16_t const streamChannel, bool const streamAlreadyConnected, bool const reusesTalkerMapping, bool const reusesListenerMapping, bool const isTalkerDefaultMapped, la::avdecc::entity::model::StreamFormat const talkerStreamFormat, la::avdecc::entity::model::StreamFormat const listenerStreamFormat)
			: talkerPrimaryStreamIndex(talkerPrimaryStreamIndex)
			, listenerPrimaryStreamIndex(listenerPrimaryStreamIndex)
			, streamChannel(streamChannel)
			, streamAlreadyConnected(streamAlreadyConnected)
			, reusesTalkerMapping(reusesTalkerMapping)
			, reusesListenerMapping(reusesListenerMapping)
			, isTalkerDefaultMapped(isTalkerDefaultMapped)
			, talkerStreamFormat(talkerStreamFormat)
			, listenerStreamFormat(listenerStreamFormat)
		{
		}

		StreamChannelInfo() {}

		la::avdecc::entity::model::StreamIndex talkerPrimaryStreamIndex{ 0 };
		la::avdecc::entity::model::StreamIndex listenerPrimaryStreamIndex{ 0 };
		uint16_t streamChannel{ 0 };
		bool streamAlreadyConnected{ false };
		bool reusesTalkerMapping{ false };
		bool reusesListenerMapping{ false };
		bool isTalkerDefaultMapped{ false }; // n:n mapping, meaning the cluster at index n (with channel 0) is mapped to the stream channel n.
		la::avdecc::entity::model::StreamFormat talkerStreamFormat{ 0 };
		la::avdecc::entity::model::StreamFormat listenerStreamFormat{ 0 };
	};

	struct StreamChannelInfoPriority
	{
		inline bool operator()(StreamChannelInfo const& streamChannelInfo1, StreamChannelInfo const& streamChannelInfo2)
		{
			if (streamChannelInfo1.reusesTalkerMapping == streamChannelInfo2.reusesTalkerMapping)
			{
				if (streamChannelInfo1.isTalkerDefaultMapped == streamChannelInfo2.isTalkerDefaultMapped)
				{
					if (streamChannelInfo1.streamAlreadyConnected == streamChannelInfo2.streamAlreadyConnected)
					{
						if (la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(streamChannelInfo1.listenerStreamFormat, streamChannelInfo1.talkerStreamFormat) == la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(streamChannelInfo2.listenerStreamFormat, streamChannelInfo2.talkerStreamFormat))
						{
							if (streamChannelInfo1.reusesListenerMapping == streamChannelInfo2.reusesListenerMapping)
							{
								// couldn't find any criteria to sort by prio, use the listenerStreamIndex or stream channel as sorting criteria
								if (streamChannelInfo1.listenerPrimaryStreamIndex == streamChannelInfo2.listenerPrimaryStreamIndex)
								{
									return streamChannelInfo1.streamChannel < streamChannelInfo2.streamChannel;
								}
								return streamChannelInfo1.listenerPrimaryStreamIndex < streamChannelInfo2.listenerPrimaryStreamIndex;
							}
							else if (streamChannelInfo1.reusesListenerMapping == streamChannelInfo2.reusesListenerMapping)
							{
								return true;
							}
							else
							{
								return false;
							}
						}
						else if (la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(streamChannelInfo1.listenerStreamFormat, streamChannelInfo1.talkerStreamFormat) && !la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(streamChannelInfo2.listenerStreamFormat, streamChannelInfo2.talkerStreamFormat))
						{
							return true;
						}
						else
						{
							return false;
						}
					}
					else if (streamChannelInfo1.streamAlreadyConnected && !streamChannelInfo2.streamAlreadyConnected)
					{
						return true;
					}
					else
					{
						return false;
					}
				}
				else if (streamChannelInfo1.isTalkerDefaultMapped && !streamChannelInfo2.isTalkerDefaultMapped)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			else if (streamChannelInfo1.reusesTalkerMapping && !streamChannelInfo2.reusesTalkerMapping)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	};

	/**
	* Checks if the given stream is the primary of a redundant stream pair or a non redundant stream.
	* Assumes the that the given StreamIdentification is valid.
	*/
	virtual bool isOutputStreamPrimaryOrNonRedundant(la::avdecc::entity::model::StreamIdentification const& streamIdentification) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(streamIdentification.entityID);
		if (controlledEntity)
		{
			auto const& configNode = controlledEntity->getConfigurationNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex);
			auto const& streamOutputNode = configNode.streamOutputs.at(streamIdentification.streamIndex);
			if (!streamOutputNode.isRedundant)
			{
				return true;
			}
			else
			{
				for (auto const& redundantStreamOutput : configNode.redundantStreamOutputs)
				{
					if (redundantStreamOutput.second.primaryStream->descriptorIndex == streamIdentification.streamIndex)
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
	std::optional<la::avdecc::controller::model::VirtualIndex> getRedundantVirtualIndexFromInputStreamIndex(la::avdecc::entity::model::StreamIdentification const& streamIdentification) const noexcept
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
	std::optional<la::avdecc::controller::model::VirtualIndex> getRedundantVirtualIndexFromOutputStreamIndex(la::avdecc::entity::model::StreamIdentification const& streamIdentification) const noexcept
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

	std::optional<la::avdecc::entity::model::StreamIndex> getPrimaryOutputStreamIndexFromVirtualIndex(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::VirtualIndex const virtualIndex) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);
		if (controlledEntity)
		{
			try
			{
				return controlledEntity->getRedundantStreamOutputNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex, virtualIndex).primaryStream->descriptorIndex;
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}
		}
		return std::nullopt;
	}

	std::optional<la::avdecc::entity::model::StreamIndex> getPrimaryInputStreamIndexFromVirtualIndex(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::VirtualIndex const virtualIndex) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);
		if (controlledEntity)
		{
			try
			{
				return controlledEntity->getRedundantStreamInputNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex, virtualIndex).primaryStream->descriptorIndex;
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}
		}
		return std::nullopt;
	}

	std::vector<std::pair<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex>> getRedundantStreamIndexPairs(la::avdecc::UniqueIdentifier const talkerEntityId, la::avdecc::controller::model::VirtualIndex const talkerStreamVirtualIndex, la::avdecc::UniqueIdentifier const listenerEntityId, la::avdecc::controller::model::VirtualIndex const listenerStreamVirtualIndex) const noexcept
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
	std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> getAllStreamOutputConnections(la::avdecc::UniqueIdentifier const talkerEntityId)
	{
		auto disconnectedStreams = std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>>{};
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
				for (auto const& [streamIndex, streamInputNode] : configNode.streamInputs)
				{
					auto* streamInputDynamicModel = streamInputNode.dynamicModel;
					if (streamInputDynamicModel)
					{
						if (streamInputDynamicModel->connectionInfo.talkerStream.entityID == talkerEntityId)
						{
							disconnectedStreams.push_back({ { potentialListenerEntityId, streamIndex }, streamInputDynamicModel->connectionInfo });
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
	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnections(la::avdecc::UniqueIdentifier const& entityId, ChannelIdentification const sourceChannelIdentification) const noexcept
	{
		auto result = std::make_shared<TargetConnectionInformations>();
		result->sourceClusterChannelInfo = sourceChannelIdentification;
		result->sourceEntityId = entityId;
		if (!sourceChannelIdentification.streamPortIndex || !sourceChannelIdentification.audioUnitIndex || !sourceChannelIdentification.baseCluster)
		{
			return result; // incomplete arguments. -> throw exception?
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

			// add the static mappings
			for (auto const& audioMap : streamPortOutputNode.audioMaps)
			{
				mappings.insert(mappings.end(), audioMap.second.staticModel->mappings.begin(), audioMap.second.staticModel->mappings.end());
			}
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// one of the given parameters is invalid.
			return result;
		}
		auto sourceStreams = std::vector<std::pair<la::avdecc::entity::model::StreamIndex, std::uint16_t>>{};
		for (auto const& mapping : mappings)
		{
			if (mapping.clusterOffset == clusterIndex - baseCluster && clusterChannel == mapping.clusterChannel)
			{
				sourceStreams.push_back(std::make_pair(mapping.streamIndex, mapping.streamChannel));
			}
		}


		std::set<std::tuple<la::avdecc::UniqueIdentifier, la::avdecc::entity::model::ClusterIndex, uint16_t>> processedListenerChannels;

		auto const& streamConnections = getAllStreamOutputConnections(entityId);

		// find out the connected streams:
		for (auto const& stream : sourceStreams)
		{
			auto sourceStreamChannel = stream.second;
			for (auto const& [listenerStream, streamConnectionInfo] : streamConnections)
			{
				if (streamConnectionInfo.talkerStream.streamIndex == stream.first)
				{
					// after getting the connected stream, resolve the underlying channels:
					auto targetControlledEntity = manager.getControlledEntity(listenerStream.entityID);
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

						auto relevantPrimaryStreamIndexes = std::map<la::avdecc::entity::model::StreamIndex, std::vector<la::avdecc::entity::model::StreamIndex>>{};
						auto relevantRedundantStreamIndexes = std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex>{};

						// if the primary is not connected but the secondary is, the channel connection is still returned
						for (auto const& redundantStreamInput : targetConfigurationNode.redundantStreamInputs)
						{
							auto primaryStreamIndex = redundantStreamInput.second.primaryStream->descriptorIndex;
							auto redundantStreams = std::vector<la::avdecc::entity::model::StreamIndex>{};
							for (auto const& [redundantStreamIndex, redundantStream] : redundantStreamInput.second.redundantStreams)
							{
								if (redundantStreamIndex != primaryStreamIndex)
								{
									redundantStreams.push_back(redundantStreamIndex);
									relevantRedundantStreamIndexes.emplace(redundantStreamIndex, primaryStreamIndex);
								}
							}
							relevantPrimaryStreamIndexes.emplace(primaryStreamIndex, redundantStreams);
						}
						for (auto const& streamInput : targetConfigurationNode.streamInputs)
						{
							if (!streamInput.second.isRedundant)
							{
								relevantPrimaryStreamIndexes.emplace(streamInput.first, std::vector<la::avdecc::entity::model::StreamIndex>{});
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
										auto listenerClusterChannel = std::make_tuple(listenerStream.entityID, mapping.clusterOffset, mapping.clusterChannel);
										if (processedListenerChannels.find(listenerClusterChannel) != processedListenerChannels.end())
										{
											continue;
										}

										// the source stream channel is connected to the corresponding target stream channel.
										if (mapping.streamIndex == listenerStream.streamIndex && mapping.streamChannel == sourceStreamChannel)
										{
											auto connectionInformation = std::make_shared<TargetConnectionInformation>();

											connectionInformation->sourceVirtualIndex = getRedundantVirtualIndexFromOutputStreamIndex(streamConnectionInfo.talkerStream);
											connectionInformation->targetVirtualIndex = getRedundantVirtualIndexFromInputStreamIndex(listenerStream);

											auto primaryListenerStreamIndex{ 0u };
											auto primaryTalkerStreamIndex{ 0u };
											auto primaryStreamIndexIt = relevantPrimaryStreamIndexes.find(mapping.streamIndex);
											if (primaryStreamIndexIt != relevantPrimaryStreamIndexes.end())
											{
												primaryListenerStreamIndex = primaryStreamIndexIt->first;
												primaryTalkerStreamIndex = streamConnectionInfo.talkerStream.streamIndex;
											}
											else
											{
												auto streamIndex = relevantRedundantStreamIndexes.find(mapping.streamIndex);
												if (streamIndex != relevantRedundantStreamIndexes.end())
												{
													// if we land in here the primary is not connected, but a secundary is.
													primaryListenerStreamIndex = streamIndex->second;
													primaryTalkerStreamIndex = controlledEntity->getRedundantStreamOutputNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex, *connectionInformation->sourceVirtualIndex).primaryStream->descriptorIndex;
												}
											}

											connectionInformation->targetEntityId = listenerStream.entityID;
											connectionInformation->streamChannel = sourceStreamChannel;
											connectionInformation->sourceStreamIndex = primaryTalkerStreamIndex;
											connectionInformation->targetStreamIndex = primaryListenerStreamIndex;
											if (connectionInformation->sourceVirtualIndex && connectionInformation->targetVirtualIndex)
											{
												// both redundant
												connectionInformation->streamPairs = getRedundantStreamIndexPairs(listenerStream.entityID, *connectionInformation->sourceVirtualIndex, connectionInformation->targetEntityId, *connectionInformation->targetVirtualIndex);
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

											// prevent doubled entries for redundant connected streams
											processedListenerChannels.insert(listenerClusterChannel);

											// add connection to the result data
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
				auto connectedTalker = streamInputDynamicModel->connectionInfo.talkerStream.entityID;
				auto connectedTalkerStreamIndex = streamInputDynamicModel->connectionInfo.talkerStream.streamIndex;

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

					auto relevantPrimaryStreamIndexes = std::map<la::avdecc::entity::model::StreamIndex, std::vector<la::avdecc::entity::model::StreamIndex>>{};
					auto relevantRedundantStreamIndexes = std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex>{};

					// if the primary is not connected but the secondary is, the channel connection is still returned
					for (auto const& redundantStreamOutput : targetConfigurationNode.redundantStreamOutputs)
					{
						auto primaryStreamIndex = redundantStreamOutput.second.primaryStream->descriptorIndex;
						auto redundantStreams = std::vector<la::avdecc::entity::model::StreamIndex>{};
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
								// get dynamic mappings
								auto targetMappings = streamPortOutputKV.second.dynamicModel->dynamicAudioMap;

								// add the static mappings
								for (auto const& audioMap : streamPortOutputKV.second.audioMaps)
								{
									targetMappings.insert(targetMappings.end(), audioMap.second.staticModel->mappings.begin(), audioMap.second.staticModel->mappings.end());
								}

								// iterate over mappings to find the channel connections
								for (auto const& mapping : targetMappings)
								{ // the source stream channel is connected to the corresponding target stream channel.
									if (mapping.streamIndex == connectedTalkerStreamIndex && mapping.streamChannel == sourceStreamChannel)
									{
										la::avdecc::entity::model::StreamIdentification sourceStreamIdentification{ entityId, stream.first };
										la::avdecc::entity::model::StreamIdentification targetStreamIdentification{ connectedTalker, connectedTalkerStreamIndex };

										auto connectionInformation = std::make_shared<TargetConnectionInformation>();
										connectionInformation->sourceVirtualIndex = getRedundantVirtualIndexFromInputStreamIndex(sourceStreamIdentification);
										connectionInformation->targetVirtualIndex = getRedundantVirtualIndexFromOutputStreamIndex(targetStreamIdentification);

										auto primaryListenerStreamIndex{ 0u };
										auto primaryTalkerStreamIndex{ 0u };
										auto primaryStreamIndexIt = relevantPrimaryStreamIndexes.find(mapping.streamIndex);
										if (primaryStreamIndexIt != relevantPrimaryStreamIndexes.end())
										{
											primaryTalkerStreamIndex = primaryStreamIndexIt->first;
											primaryListenerStreamIndex = stream.first;
										}
										else
										{
											auto streamIndex = relevantRedundantStreamIndexes.find(mapping.streamIndex);
											if (streamIndex != relevantRedundantStreamIndexes.end())
											{
												// if we land in here the primary is not connected, but a secundary is.
												primaryTalkerStreamIndex = streamIndex->second;
												primaryListenerStreamIndex = controlledEntity->getRedundantStreamInputNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex, *connectionInformation->sourceVirtualIndex).primaryStream->descriptorIndex;
											}
										}

										connectionInformation->targetEntityId = connectedTalker;
										connectionInformation->sourceStreamIndex = primaryListenerStreamIndex;
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
										return result; // there can only ever be one channel connected on the listener side
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
	std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> getAllStreamOutputConnections(la::avdecc::UniqueIdentifier const& talkerEntityId) const noexcept
	{
		auto disconnectedStreams = std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>>{};
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
				for (auto const& [streamIndex, streamInputNode] : configNode.streamInputs)
				{
					auto* streamInputDynamicModel = streamInputNode.dynamicModel;
					if (streamInputDynamicModel)
					{
						if (streamInputDynamicModel->connectionInfo.talkerStream.entityID == talkerEntityId)
						{
							disconnectedStreams.push_back({ { potentialListenerEntityId, streamIndex }, streamInputDynamicModel->connectionInfo });
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
	virtual std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> getRedundantStreamOutputsForPrimary(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex const primaryStreamIndex) const noexcept
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
	virtual std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> getRedundantStreamInputsForPrimary(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex const primaryStreamIndex) const noexcept
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
		// TODO refactor to utilze getChannelConnectionsReverse?

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


		try
		{
			auto const& streamPortOutput = controlledEntity->getStreamPortOutputNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex, sourceStreamPortIndex);
			auto streamPortOutputAudioMappings = la::avdecc::entity::model::AudioMappings{};
			if (streamPortOutput.staticModel->hasDynamicAudioMap)
			{
				streamPortOutputAudioMappings = controlledEntity->getStreamPortOutputAudioMappings(sourceStreamPortIndex);
			}
			else
			{
				for (auto const& audioMap : streamPortOutput.audioMaps)
				{
					auto const& mappings = audioMap.second.staticModel->mappings;
					streamPortOutputAudioMappings.insert(streamPortOutputAudioMappings.end(), mappings.begin(), mappings.end());
				}
			}

			// add the static mappings
			for (auto const& audioUnit : controlledEntity->getCurrentConfigurationNode().audioUnits)
			{
				for (auto const& streamPortOutput : audioUnit.second.streamPortOutputs)
				{
					for (auto const& audioMap : streamPortOutput.second.audioMaps)
					{
						streamPortOutputAudioMappings.insert(streamPortOutputAudioMappings.end(), audioMap.second.staticModel->mappings.begin(), audioMap.second.staticModel->mappings.end());
					}
				}
			}

			for (auto const& mapping : streamPortOutputAudioMappings)
			{
				sourceStreams.push_back(std::make_pair(mapping.streamIndex, mapping.streamChannel));
			}

			auto const& streamConnections = getAllStreamOutputConnections(sourceEntityId);


			// find out the connected streams:
			for (auto const& stream : sourceStreams)
			{
				auto sourceStreamChannel = stream.second;
				for (auto const& [listenerStream, streamConnectionInfo] : streamConnections)
				{
					if (targetEntityId == listenerStream.entityID && streamConnectionInfo.talkerStream.streamIndex == stream.first)
					{
						// after getting the connected stream, resolve the underlying channels:
						auto targetControlledEntity = manager.getControlledEntity(listenerStream.entityID);
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
												//continue;
											}

											// the source stream channel is connected to the corresponding target stream channel.
											if (mapping.streamIndex == listenerStream.streamIndex && mapping.streamChannel == sourceStreamChannel)
											{
												auto connectionInformation = std::make_shared<TargetConnectionInformation>();

												connectionInformation->targetEntityId = listenerStream.entityID;
												connectionInformation->sourceStreamIndex = streamConnectionInfo.talkerStream.streamIndex;
												connectionInformation->targetStreamIndex = listenerStream.streamIndex;
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
	std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> getStreamOutputConnections(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::StreamIndex const outputStreamIndex) const noexcept
	{
		auto disconnectedStreams = std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>>{};
		auto const& manager = avdecc::ControllerManager::getInstance();
		for (auto const& potentialListenerEntityId : _entities)
		{
			auto const controlledEntity = manager.getControlledEntity(potentialListenerEntityId);
			if (controlledEntity)
			{
				if (controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
				{
					auto const& configNode = controlledEntity->getCurrentConfigurationNode();
					for (auto const& [streamIndex, streamInputNode] : configNode.streamInputs)
					{
						auto* streamInputDynamicModel = streamInputNode.dynamicModel;
						if (streamInputDynamicModel)
						{
							if (streamInputDynamicModel->connectionInfo.talkerStream.entityID == talkerEntityId && streamInputDynamicModel->connectionInfo.talkerStream.streamIndex == outputStreamIndex)
							{
								disconnectedStreams.push_back({ { potentialListenerEntityId, streamIndex }, streamInputDynamicModel->connectionInfo });
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
	* FOR EVERY CHANNEL CONNECTION PAIR GIVEN:
	* find all potiential connections and mappings that could be used.
	* sort by best fit (less changes are preferred)
	* use the first (best) entry and store needed stream and mapping adjustments
	*
	* If any mapping changes on the talker is needed, ChannelConnectResult::NeedsTalkerMappingAdjustment is returned.
	* If any mappings on the listener need to be removed, ChannelConnectResult::RemovalOfListenerDynamicMappingsNecessary is returned.
	* The algorithm stops in both cases. However there won't be an error if a talker that currently has no stream connections needs mapping changes.
	*
	* To enable removal of mappings the 'allowTalkerMappingChanges' and 'allowRemovalOfUnusedAudioMappings' have to be set to true for the respective side.
	*
	* @param talkerEntityId The id of the talker entity.
	* @param listenerEntityId The id of the listener entity.
	* @param std::vector<std::pair<avdecc::ChannelIdentification>>
	* @param talkerToListenerChannelConnections The desired channel connection to be checked
	* @param allowTalkerMappingChanges Flag parameter to indicate if talker mappings can be added or removed. (Talker mapping changes require stream disconnection which might lead to audio dropouts)
	* @param allowRemovalOfUnusedAudioMappings Flag parameter to indicate if existing mappings can be overridden
	* @param channelUsageHint
	*/
	CheckChannelCreationsPossibleResult checkChannelCreationsPossible(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId, std::vector<std::pair<avdecc::ChannelIdentification, avdecc::ChannelIdentification>> const& talkerToListenerChannelConnections, bool const allowTalkerMappingChanges, bool const allowRemovalOfUnusedAudioMappings, uint16_t const channelUsageHint) const noexcept
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


		// store all connection, format and mapping changes, that will be applied as batch command chain
		StreamChannelMappings overriddenMappingsListener;
		StreamChannelMappings newMappingsTalker;
		StreamChannelMappings newMappingsListener;
		StreamConnections newStreamConnections;
		StreamFormatChanges streamFormatChangesTalker;
		StreamFormatChanges streamFormatChangesListener;

		for (auto const& channelPair : talkerToListenerChannelConnections)
		{
			auto const& talkerChannelIdentification = channelPair.first;
			auto const& listenerChannelIdentification = channelPair.second;

			try
			{
				std::vector<StreamChannelInfo> streamChannelInfos = findAllUsableStreamChannels(talkerEntityId, listenerEntityId, talkerChannelIdentification, listenerChannelIdentification, newStreamConnections, newMappingsTalker, newMappingsListener);
				if (streamChannelInfos.empty())
				{
					return CheckChannelCreationsPossibleResult{ ChannelConnectResult::Impossible };
				}
				std::sort(streamChannelInfos.begin(), streamChannelInfos.end(), StreamChannelInfoPriority());

				// take the first entry of the streamChannelInfos after they were sorted as the channel to use
				auto streamChannelInfoToUse = streamChannelInfos.begin();

				// the user has to agree that talker mappings are changed (can lead to audio interruptions)
				if (!allowTalkerMappingChanges && !streamChannelInfoToUse->reusesTalkerMapping)
				{
					// check if the talker stream is used anywhere yet, only then it makes sense to show a warning
					auto connections = getStreamOutputConnections(talkerEntityId, streamChannelInfoToUse->talkerPrimaryStreamIndex);
					if (!connections.empty())
					{
						return CheckChannelCreationsPossibleResult{ ChannelConnectResult::NeedsTalkerMappingAdjustment };
					}
				}

				// IF A NEW STREAM CONNECTION WILL BE CREATED: remove listener mappings that have to be removed before the new connection can be created, but only after user confirmation
				if (!streamChannelInfoToUse->streamAlreadyConnected)
				{
					auto assignedChannelsTalker = getAssignedChannelsOnTalkerStream(talkerEntityId, streamChannelInfoToUse->talkerPrimaryStreamIndex);
					auto assignedChannelsListener = getAssignedChannelsOnListenerStream(listenerEntityId, streamChannelInfoToUse->listenerPrimaryStreamIndex);

					std::vector<uint16_t> unwantedConnectionsAfterStreamConnect;
					set_intersection(assignedChannelsTalker.begin(), assignedChannelsTalker.end(), assignedChannelsListener.begin(), assignedChannelsListener.end(), back_inserter(unwantedConnectionsAfterStreamConnect));

					if (!unwantedConnectionsAfterStreamConnect.empty() && !allowRemovalOfUnusedAudioMappings)
					{
						return CheckChannelCreationsPossibleResult{ ChannelConnectResult::RemovalOfListenerDynamicMappingsNecessary };
					}

					// remove all listener mappings that would be created by the new stream connection
					for (auto const unwantedStreamConnectionChannel : unwantedConnectionsAfterStreamConnect)
					{
						auto unwantedMappings = getMappingsFromStreamInputChannel(listenerEntityId, streamChannelInfoToUse->listenerPrimaryStreamIndex, unwantedStreamConnectionChannel);
						for (auto const& unwantedMapping : unwantedMappings)
						{
							if (unwantedMapping.clusterOffset == listenerChannelIdentification.clusterIndex + *listenerChannelIdentification.baseCluster)
							{
								continue;
							}
							insertAudioMapping(overriddenMappingsListener, unwantedMapping, *listenerChannelIdentification.streamPortIndex);
						}
					}

					newStreamConnections.push_back(std::make_pair(streamChannelInfoToUse->talkerPrimaryStreamIndex, streamChannelInfoToUse->listenerPrimaryStreamIndex));
				}

				// IF NEW TALKER MAPPINGS ARE CREATED: remove listener mappings that would be created, except for the one that we actually want if it is reused, but only after user confirmation
				if (!streamChannelInfoToUse->reusesTalkerMapping)
				{
					auto unwantedMappings = getMappingsFromStreamInputChannel(listenerEntityId, streamChannelInfoToUse->listenerPrimaryStreamIndex, streamChannelInfoToUse->streamChannel);
					if (!unwantedMappings.empty() && !allowRemovalOfUnusedAudioMappings)
					{
						return CheckChannelCreationsPossibleResult{ ChannelConnectResult::RemovalOfListenerDynamicMappingsNecessary };
					}

					for (auto const& unwantedMapping : unwantedMappings)
					{
						if (streamChannelInfoToUse->reusesListenerMapping && unwantedMapping.clusterOffset == listenerChannelIdentification.clusterIndex - *listenerChannelIdentification.baseCluster)
						{
							continue;
						}
						// remove the mapping
						insertAudioMapping(overriddenMappingsListener, unwantedMapping, *listenerChannelIdentification.streamPortIndex);
					}
				}

				// talker mapping
				if (!streamChannelInfoToUse->reusesTalkerMapping)
				{
					if (!streamChannelInfoToUse->isTalkerDefaultMapped)
					{
						// create the talker mapping, because it is not created together with the default mappings
						la::avdecc::entity::model::AudioMapping talkerMapping;
						talkerMapping.clusterChannel = talkerChannelIdentification.clusterChannel;
						talkerMapping.clusterOffset = talkerChannelIdentification.clusterIndex - *talkerChannelIdentification.baseCluster;
						talkerMapping.streamChannel = streamChannelInfoToUse->streamChannel;
						talkerMapping.streamIndex = streamChannelInfoToUse->talkerPrimaryStreamIndex;
						insertAudioMapping(newMappingsTalker, talkerMapping, *talkerChannelIdentification.streamPortIndex);
					}

					// get the default mappings that can be created on the talker side without side effects of creating unwanted channel connections
					auto const mappings = getPossibleDefaultMappings(talkerEntityId, streamChannelInfoToUse->talkerPrimaryStreamIndex, listenerEntityId, streamChannelInfoToUse->listenerPrimaryStreamIndex, newMappingsTalker, overriddenMappingsListener);

					// create the talker default mappings if a new talker mapping has to be made
					for (auto const& talkerMapping : mappings)
					{
						insertAudioMapping(newMappingsTalker, talkerMapping, *talkerChannelIdentification.streamPortIndex);
					}
				}

				if (!streamChannelInfoToUse->reusesListenerMapping)
				{
					// remove the mapping to the listener channel before creating a new one
					auto const streamPortInputAudioMappings = controlledListenerEntity->getStreamPortInputAudioMappings(*channelPair.second.streamPortIndex);
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

					// create the listener mapping
					la::avdecc::entity::model::AudioMapping listenerMapping;
					listenerMapping.clusterChannel = listenerChannelIdentification.clusterChannel;
					listenerMapping.clusterOffset = listenerChannelIdentification.clusterIndex - *listenerChannelIdentification.baseCluster;
					listenerMapping.streamChannel = streamChannelInfoToUse->streamChannel;
					listenerMapping.streamIndex = streamChannelInfoToUse->listenerPrimaryStreamIndex;
					insertAudioMapping(newMappingsListener, listenerMapping, *listenerChannelIdentification.streamPortIndex);
				}

				auto const& compatibleFormats = findCompatibleStreamPairFormat(talkerEntityId, streamChannelInfoToUse->talkerPrimaryStreamIndex, listenerEntityId, streamChannelInfoToUse->listenerPrimaryStreamIndex, la::avdecc::entity::model::StreamFormatInfo::Type::AAF, channelUsageHint);
				if (compatibleFormats.first)
				{
					streamFormatChangesTalker.emplace(streamChannelInfoToUse->talkerPrimaryStreamIndex, *compatibleFormats.first);
				}
				if (compatibleFormats.second)
				{
					streamFormatChangesTalker.emplace(streamChannelInfoToUse->listenerPrimaryStreamIndex, *compatibleFormats.second);
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}
		}

		return CheckChannelCreationsPossibleResult{ ChannelConnectResult::NoError, std::move(overriddenMappingsListener), std::move(newMappingsTalker), std::move(newMappingsListener), std::move(newStreamConnections) };
	}

	/**
	* Finds all possible stream & channel combinations that allow to connect the two cluster channels.
	*/
	std::vector<StreamChannelInfo> findAllUsableStreamChannels(la::avdecc::UniqueIdentifier const talkerEntityId, la::avdecc::UniqueIdentifier const listenerEntityId, avdecc::ChannelIdentification const& talkerChannelIdentification, avdecc::ChannelIdentification const& listenerChannelIdentification, StreamConnections const& newStreamConnections, StreamChannelMappings const& newMappingsTalker, StreamChannelMappings const& newMappingsListener) const noexcept
	{
		std::vector<StreamChannelInfo> result;

		// find all (existing and possible) stream connections between the devices
		// check all listenerChannelConnections on it
		auto streamConnections = getStreamConnectionsBetweenDevices(talkerEntityId, listenerEntityId);

		// filter out streamConnections that are redundant
		auto primaryStreamConnections = StreamConnections{};
		for (auto const& streamConnection : streamConnections)
		{
			la::avdecc::entity::model::StreamIdentification talkerStreamIdentification{ talkerEntityId, streamConnection.first };
			la::avdecc::entity::model::StreamIdentification listenerStreamIdentification{ listenerEntityId, streamConnection.second };

			auto virtualTalkerIndex = getRedundantVirtualIndexFromOutputStreamIndex(talkerStreamIdentification);
			auto virtualListenerIndex = getRedundantVirtualIndexFromInputStreamIndex(listenerStreamIdentification);

			if (virtualTalkerIndex && virtualListenerIndex)
			{
				// convert secundary connected streams to primary connections:
				auto talkerPimaryStreamIndex = getPrimaryOutputStreamIndexFromVirtualIndex(talkerEntityId, *virtualTalkerIndex);
				auto listenerPimaryStreamIndex = getPrimaryInputStreamIndexFromVirtualIndex(listenerEntityId, *virtualListenerIndex);

				if (talkerPimaryStreamIndex && listenerPimaryStreamIndex)
				{
					auto const connection = std::make_pair(*talkerPimaryStreamIndex, *listenerPimaryStreamIndex);
					if (std::find(primaryStreamConnections.begin(), primaryStreamConnections.end(), connection) == primaryStreamConnections.end())
					{
						primaryStreamConnections.push_back(connection);
					}
				}
			}
			else
			{
				// non redundant connection
				primaryStreamConnections.push_back(streamConnection);
			}
		}

		// also take into account stream connections, that will be batch created with this one:
		primaryStreamConnections.insert(primaryStreamConnections.end(), newStreamConnections.begin(), newStreamConnections.end());

		// iterate over existing stream connections
		for (auto const& streamConnection : primaryStreamConnections)
		{
			// get all stream channels that could be used for the connection
			auto const usableChannels = findAllUsableStreamChannelsOnStreamConnection(talkerEntityId, listenerEntityId, streamConnection, true, talkerChannelIdentification.clusterIndex - *talkerChannelIdentification.baseCluster, talkerChannelIdentification.clusterChannel, listenerChannelIdentification.clusterIndex - *listenerChannelIdentification.baseCluster, listenerChannelIdentification.clusterChannel, newMappingsTalker, newMappingsListener);
			result.insert(result.end(), usableChannels.begin(), usableChannels.end());
		}

		// check the stream connections that have not been created yet
		auto possibleStreamConnections = getPossibleAudioStreamConnectionsBetweenDevices(talkerEntityId, listenerEntityId);

		// filter out stream connections that are already being created
		for (auto const& newStreamConnection : newStreamConnections)
		{
			auto connectionToRemoveIt = std::find(possibleStreamConnections.begin(), possibleStreamConnections.end(), newStreamConnection);
			possibleStreamConnections.erase(connectionToRemoveIt);
		}

		for (auto const& streamConnection : possibleStreamConnections)
		{
			// get all stream channels that could be used for the connection
			auto const usableChannels = findAllUsableStreamChannelsOnStreamConnection(talkerEntityId, listenerEntityId, streamConnection, false, talkerChannelIdentification.clusterIndex - *talkerChannelIdentification.baseCluster, talkerChannelIdentification.clusterChannel, listenerChannelIdentification.clusterIndex - *listenerChannelIdentification.baseCluster, listenerChannelIdentification.clusterChannel, newMappingsTalker, newMappingsListener);
			result.insert(result.end(), usableChannels.begin(), usableChannels.end());
		}

		return result;
	}

	/**
	* @param newMappingsTalker Contains mappings that will be created with createChannelConnections method, but are not created yet.
	* @param newMappingsListener Contains mappings that will be created with createChannelConnections method, but are not created yet.
	* @return Gets all connection
	*/
	std::vector<StreamChannelInfo> findAllUsableStreamChannelsOnStreamConnection(la::avdecc::UniqueIdentifier const talkerEntityId, la::avdecc::UniqueIdentifier const listenerEntityId, StreamConnection const streamConnection, bool const isStreamAlreadyConnected, la::avdecc::entity::model::ClusterIndex const talkerClusterOffset, uint16_t const talkerClusterChannel, la::avdecc::entity::model::ClusterIndex const listenerClusterOffset, uint16_t const listenerClusterChannel, StreamChannelMappings const& newMappingsTalker, StreamChannelMappings const& newMappingsListener) const noexcept
	{
		// convenience function to create StreamChannelInfo
		auto buildStreamChannelInfo = [talkerEntityId, listenerEntityId, streamConnection, isStreamAlreadyConnected, talkerClusterOffset, listenerClusterOffset](uint16_t streamChannel, bool reusesTalkerMapping, bool reusesListenerMapping) -> std::optional<StreamChannelInfo>
		{
			auto const& manager = avdecc::ControllerManager::getInstance();
			auto const talkerEntity = manager.getControlledEntity(talkerEntityId);
			auto const listenerEntity = manager.getControlledEntity(listenerEntityId);
			if (!talkerEntity || !listenerEntity)
			{
				return std::nullopt;
			}

			try
			{
				auto talkerStreamIndex = streamConnection.first;
				auto listenerStreamIndex = streamConnection.second;
				auto const* const streamOutputDynamicModel = talkerEntity->getStreamOutputNode(talkerEntity->getCurrentConfigurationNode().descriptorIndex, talkerStreamIndex).dynamicModel;
				if (!streamOutputDynamicModel)
				{
					return std::nullopt;
				}
				auto const* const streamInputDynamicModel = listenerEntity->getStreamInputNode(listenerEntity->getCurrentConfigurationNode().descriptorIndex, listenerStreamIndex).dynamicModel;
				if (!streamInputDynamicModel)
				{
					return std::nullopt;
				}
				auto talkerStreamFormat = streamOutputDynamicModel->streamFormat;
				auto listenerStreamFormat = streamInputDynamicModel->streamFormat;

				return StreamChannelInfo{ talkerStreamIndex, listenerStreamIndex, streamChannel, isStreamAlreadyConnected, reusesTalkerMapping, reusesListenerMapping, streamChannel == talkerClusterOffset, talkerStreamFormat, listenerStreamFormat };
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}
			return std::nullopt;
		};

		std::vector<StreamChannelInfo> result;
		auto const talkerStreamIndex = streamConnection.first;
		auto const listenerStreamIndex = streamConnection.second;

		// get the mappings for the cluster channel that are currently existant on the talker
		auto existingFittingTalkerMappings = getAssignedChannelsOnTalkerStream(talkerEntityId, talkerStreamIndex, talkerClusterOffset, talkerClusterChannel);

		// add the mappings that will be created
		auto const& newMappingsTalkerIt = newMappingsTalker.find(talkerStreamIndex);
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

		// get the mappings for the cluster channel that are currently existant on the listener
		auto existingFittingListenerMappings = getAssignedChannelsOnListenerStream(listenerEntityId, listenerStreamIndex, listenerClusterOffset, listenerClusterChannel);

		// add the mappings that will be created
		auto const& newMappingsListenerIt = newMappingsListener.find(listenerStreamIndex);
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

		for (auto const existantTalkerMapping : existingFittingTalkerMappings)
		{
			if (isStreamAlreadyConnected)
			{
				auto streamChannelInfo = buildStreamChannelInfo(existantTalkerMapping, true, existingFittingListenerMappings.find(existantTalkerMapping) != existingFittingListenerMappings.end());
				if (streamChannelInfo)
				{
					result.push_back(*streamChannelInfo);
				}
			}
			else
			{
				// if the stream is not connected yet, it could be that the listener has mappings on this stream that we need to remove before it can be used.
				// TODO: !! question is if the mappings that need to be removed should be stored in the StreamChannelInfo as well. !!
				auto streamChannelInfo = buildStreamChannelInfo(existantTalkerMapping, true, existingFittingListenerMappings.find(existantTalkerMapping) != existingFittingListenerMappings.end());
				if (streamChannelInfo)
				{
					result.push_back(*streamChannelInfo);
				}
			}
		}

		// get all stream channels that are unassigned on the talker side
		auto freeStreamSlotsSource = getUnassignedChannelsOnTalkerStream(talkerEntityId, talkerStreamIndex);
		// remove the channels that will be created
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

		for (auto const& unassignedTalkerStreamChannel : freeStreamSlotsSource)
		{
			// TODO: !! question is if the mappings that need to be removed should be stored in the StreamChannelInfo as well. !!
			auto streamChannelInfo = buildStreamChannelInfo(unassignedTalkerStreamChannel, false, existingFittingListenerMappings.find(unassignedTalkerStreamChannel) != existingFittingListenerMappings.end());
			if (streamChannelInfo)
			{
				result.push_back(*streamChannelInfo);
			}
		}

		return result;
	}

	la::avdecc::entity::model::AudioMappings getPossibleDefaultMappings(la::avdecc::UniqueIdentifier const talkerEntityId, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityId, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, StreamChannelMappings const& newMappingsTalker, StreamChannelMappings const& overriddenMappingsListener) const
	{
		la::avdecc::entity::model::AudioMappings result;
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledTalkerEntity = manager.getControlledEntity(talkerEntityId);
		if (!controlledTalkerEntity)
		{
			return result;
		}

		// find all unmappable streeam channels (occupied) and count the talker clusters
		std::set<uint16_t> unmappableStreamChannels;
		auto talkerStreamChannelCount = getStreamOutputChannelCount(talkerEntityId, talkerStreamIndex);
		auto talkerClusterCount = 0u;
		auto const& talkerAudioUnits = controlledTalkerEntity->getCurrentConfigurationNode().audioUnits;
		for (auto const& audioUnitKV : talkerAudioUnits)
		{
			for (auto const& streamPortOutputKV : audioUnitKV.second.streamPortOutputs)
			{
				talkerClusterCount += streamPortOutputKV.second.audioClusters.size(); // mapping cluster channels > 0 unsupported

				for (auto const& audioMapKV : streamPortOutputKV.second.audioMaps)
				{
					for (auto const& mapping : audioMapKV.second.staticModel->mappings)
					{
						if (mapping.streamIndex == talkerStreamIndex)
						{
							unmappableStreamChannels.insert(mapping.streamChannel);
							if (mapping.streamIndex != mapping.clusterOffset)
							{
								unmappableStreamChannels.insert(mapping.clusterOffset);
							}
						}
					}
				}
			}
		}

		auto const& newMappingsTalkerIt = newMappingsTalker.find(talkerStreamIndex);

		if (newMappingsTalkerIt != newMappingsTalker.end())
		{
			for (auto const& mappingWrapper : newMappingsTalkerIt->second)
			{
				for (auto const& mapping : mappingWrapper.second)
				{
					unmappableStreamChannels.insert(mapping.streamChannel);
				}
			}
		}

		// filter out channels that would create connections on any listener currently connected to the talker
		auto streamOutputConnections = getStreamOutputConnections(talkerEntityId, talkerStreamIndex);

		auto const& overriddenMappingsListenerIt = overriddenMappingsListener.find(listenerStreamIndex);

		for (auto const& [listenerStream, streamOutputInfo] : streamOutputConnections)
		{
			auto listenerOccupiedChannels = getAssignedChannelsOnListenerStream(listenerStream.entityID, listenerStream.streamIndex);

			// remove the mappings that will be removed by new stream connections
			if (listenerStream.entityID == listenerEntityId && listenerStream.streamIndex == listenerStreamIndex)
			{
				if (overriddenMappingsListenerIt != overriddenMappingsListener.end())
				{
					for (auto const& mappingWrapper : overriddenMappingsListenerIt->second)
					{
						for (auto const& mapping : mappingWrapper.second)
						{
							listenerOccupiedChannels.erase(mapping.streamChannel);
						}
					}
				}
			}

			for (auto const& listenerOccupiedChannel : listenerOccupiedChannels)
			{
				unmappableStreamChannels.insert(listenerOccupiedChannel);
			}
		}


		// get the stream channel count and the talker cluster count and use the lower value as maximum
		auto assignableChannels = qMin(talkerClusterCount, static_cast<uint32_t>(talkerStreamChannelCount));

		// create the i:i mappings where possible
		for (uint32_t i = 0; i < assignableChannels; i++)
		{
			if (unmappableStreamChannels.find(i) != unmappableStreamChannels.end())
			{
				continue;
			}
			la::avdecc::entity::model::AudioMapping talkerMapping;
			talkerMapping.clusterChannel = 0;
			talkerMapping.clusterOffset = i;
			talkerMapping.streamChannel = i;
			talkerMapping.streamIndex = talkerStreamIndex;

			result.push_back(talkerMapping);
		}
		return result;
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
	virtual ChannelConnectResult createChannelConnection(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId, avdecc::ChannelIdentification const& talkerChannelIdentification, avdecc::ChannelIdentification const& listenerChannelIdentification, bool const allowTalkerMappingChanges, bool const allowRemovalOfUnusedAudioMappings) noexcept
	{
		std::vector<std::pair<avdecc::ChannelIdentification, avdecc::ChannelIdentification>> channelsToConnect;
		channelsToConnect.push_back(std::make_pair(talkerChannelIdentification, listenerChannelIdentification));

		return createChannelConnections(talkerEntityId, listenerEntityId, channelsToConnect, allowTalkerMappingChanges, allowRemovalOfUnusedAudioMappings);
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
	virtual ChannelConnectResult createChannelConnections(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId, std::vector<std::pair<avdecc::ChannelIdentification, avdecc::ChannelIdentification>> const& talkerToListenerChannelConnections, bool const allowTalkerMappingChanges, bool const allowRemovalOfUnusedAudioMappings) noexcept
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

		auto const& result = checkChannelCreationsPossible(talkerEntityId, listenerEntityId, talkerToListenerChannelConnections, allowTalkerMappingChanges, allowRemovalOfUnusedAudioMappings, channelUsage);
		if (result.connectionCheckResult == ChannelConnectResult::NoError)
		{
			std::vector<commandChain::AsyncParallelCommandSet*> commands;

			std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commandsChangeStreamFormat;
			std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commandsCreateStreamConnections;
			for (auto const& newStreamConnection : result.newStreamConnections)
			{
				// change the stream format if necessary
				auto const& compatibleStreamFormats = findCompatibleStreamPairFormat(talkerEntityId, newStreamConnection.first, listenerEntityId, newStreamConnection.second, la::avdecc::entity::model::StreamFormatInfo::Type::AAF, channelUsage);

				auto const talkerStreamIndex = newStreamConnection.first;
				if (compatibleStreamFormats.first)
				{
					commandsChangeStreamFormat.push_back(
						[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::SetStreamFormat);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							manager.setStreamOutputFormat(talkerEntityId, talkerStreamIndex, *compatibleStreamFormats.first, responseHandler);
							return true;
						});
				}
				if (compatibleStreamFormats.second)
				{
					commandsChangeStreamFormat.push_back(
						[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::SetStreamFormat);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
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

					redundantOutputStreamsIterator++;
					redundantInputStreamsIterator++;
				}

				// connect primary
				commandsCreateStreamConnections.push_back(
					[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
					{
						auto& manager = avdecc::ControllerManager::getInstance();
						auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const /*talkerStreamIndex*/, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const /*listenerStreamIndex*/, la::avdecc::entity::ControllerEntity::ControlStatus const status)
						{
							// notify SequentialAsyncCommandExecuter that the command completed.
							auto error = commandChain::AsyncParallelCommandSet::controlStatusToCommandError(status);
							if (error != commandChain::CommandExecutionError::NoError)
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
							parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
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
							[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
							{
								auto& manager = avdecc::ControllerManager::getInstance();
								auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const /*talkerStreamIndex*/, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const /*listenerStreamIndex*/, la::avdecc::entity::ControllerEntity::ControlStatus const status)
								{
									// notify SequentialAsyncCommandExecuter that the command completed.
									auto error = commandChain::AsyncParallelCommandSet::controlStatusToCommandError(status);
									if (error != commandChain::CommandExecutionError::NoError)
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
									parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
								};
								manager.connectStream(talkerEntityId, talkerSecStreamIndex, listenerEntityId, listenerSecStreamIndex, responseHandler);
								return true;
							});
					}
					redundantOutputStreamsIterator++;
					redundantInputStreamsIterator++;
				}
			}

			auto* commandSetChangeStreamFormat = new commandChain::AsyncParallelCommandSet(commandsChangeStreamFormat);
			auto* commandSetCreateStreamConnections = new commandChain::AsyncParallelCommandSet(commandsCreateStreamConnections);


			// create the set of streams to disconnect and later connect again
			// find all stream connections by looking through the connections of each talker stream
			std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> streamsToDisconnect;
			for (auto const& mappingsTalker : result.newMappingsTalker)
			{
				// get the redundant connections and connect all
				auto const& redundantOutputStreams = getRedundantStreamOutputsForPrimary(talkerEntityId, mappingsTalker.first);
				auto redundantOutputStreamsIterator = redundantOutputStreams.begin();
				auto talkerPrimStreamIndex{ mappingsTalker.first };

				if (!redundantOutputStreams.empty())
				{
					talkerPrimStreamIndex = redundantOutputStreamsIterator->first;
					redundantOutputStreamsIterator++;
				}

				auto talkerStreamConnections = getAllStreamOutputConnections(talkerEntityId, talkerPrimStreamIndex);
				streamsToDisconnect.insert(streamsToDisconnect.end(), talkerStreamConnections.begin(), talkerStreamConnections.end());

				while (redundantOutputStreamsIterator != redundantOutputStreams.end())
				{
					auto redundantTalkerStreamConnections = getAllStreamOutputConnections(talkerEntityId, redundantOutputStreamsIterator->first);
					streamsToDisconnect.insert(streamsToDisconnect.end(), redundantTalkerStreamConnections.begin(), redundantTalkerStreamConnections.end());

					redundantOutputStreamsIterator++;
				}
			}

			// create commands to stop the streams
			std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commandsTempDisconnectStreams;
			for (auto const& [listenerStream, streamConnectionInfo] : streamsToDisconnect)
			{
				commandsTempDisconnectStreams.push_back(
					[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
					{
						auto& manager = avdecc::ControllerManager::getInstance();
						auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const /*talkerStreamIndex*/, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const /*listenerStreamIndex*/, la::avdecc::entity::ControllerEntity::ControlStatus const status)
						{
							// notify SequentialAsyncCommandExecuter that the command completed.
							auto error = commandChain::AsyncParallelCommandSet::controlStatusToCommandError(status);
							if (error != commandChain::CommandExecutionError::NoError)
							{
								switch (status)
								{
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerMisbehaving:
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerUnknownID:
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerDestMacFail:
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoBandwidth:
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoStreamIndex:
									case la::avdecc::entity::LocalEntity::ControlStatus::TalkerExclusive:
										parentCommandSet->addErrorInfo(talkerEntityID, error, avdecc::ControllerManager::AcmpCommandType::DisconnectStream);
										break;
									case la::avdecc::entity::LocalEntity::ControlStatus::ListenerMisbehaving:
									case la::avdecc::entity::LocalEntity::ControlStatus::ListenerUnknownID:
									case la::avdecc::entity::LocalEntity::ControlStatus::ListenerExclusive:
										parentCommandSet->addErrorInfo(listenerEntityID, error, avdecc::ControllerManager::AcmpCommandType::DisconnectStream);
										break;
									default:
										parentCommandSet->addErrorInfo(talkerEntityID, error, avdecc::ControllerManager::AcmpCommandType::DisconnectStream);
										parentCommandSet->addErrorInfo(listenerEntityID, error, avdecc::ControllerManager::AcmpCommandType::DisconnectStream);
										break;
								}
							}
							parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
						};
						manager.disconnectStream(streamConnectionInfo.talkerStream.entityID, streamConnectionInfo.talkerStream.streamIndex, listenerStream.entityID, listenerStream.streamIndex, responseHandler);
						return true;
					});
			}
			auto* commandSetTempDisconnectStreams = new commandChain::AsyncParallelCommandSet(commandsTempDisconnectStreams);

			std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commandsReconnectStreams;
			for (auto const& [listenerStream, streamConnectionInfo] : streamsToDisconnect)
			{
				commandsReconnectStreams.push_back(
					[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
					{
						auto& manager = avdecc::ControllerManager::getInstance();
						auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const /*talkerStreamIndex*/, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const /*listenerStreamIndex*/, la::avdecc::entity::ControllerEntity::ControlStatus const status)
						{
							// notify SequentialAsyncCommandExecuter that the command completed.
							auto error = commandChain::AsyncParallelCommandSet::controlStatusToCommandError(status);
							if (error != commandChain::CommandExecutionError::NoError)
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
										parentCommandSet->addErrorInfo(talkerEntityID, error, avdecc::ControllerManager::AcmpCommandType::ConnectStream);
										parentCommandSet->addErrorInfo(listenerEntityID, error, avdecc::ControllerManager::AcmpCommandType::ConnectStream);
										break;
								}
							}
							parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
						};
						manager.connectStream(streamConnectionInfo.talkerStream.entityID, streamConnectionInfo.talkerStream.streamIndex, listenerStream.entityID, listenerStream.streamIndex, responseHandler);
						return true;
					});
			}
			auto* commandSetReconnectStreams = new commandChain::AsyncParallelCommandSet(commandsReconnectStreams);

			std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commandsRemoveMappings;
			for (auto const& mappingsListener : result.overriddenMappingsListener)
			{
				for (auto const& mapping : mappingsListener.second)
				{
					commandsRemoveMappings.push_back(
						[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::AddStreamPortAudioMappings);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							manager.removeStreamPortInputAudioMappings(listenerEntityId, mapping.first, mapping.second, responseHandler);
							return true;
						});
				}
			}
			auto* commandSetRemoveMappings = new commandChain::AsyncParallelCommandSet(commandsRemoveMappings);

			std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commandsCreateMappings;
			for (auto const& mappingsTalker : result.newMappingsTalker)
			{
				for (auto const& mapping : mappingsTalker.second)
				{
					commandsCreateMappings.push_back(
						[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::AddStreamPortAudioMappings);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
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
						[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
						{
							auto& manager = avdecc::ControllerManager::getInstance();
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, avdecc::ControllerManager::AecpCommandType::AddStreamPortAudioMappings);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							manager.addStreamPortInputAudioMappings(listenerEntityId, mapping.first, mapping.second, responseHandler);
							return true;
						});
				}
			}
			auto* commandSetCreateMappings = new commandChain::AsyncParallelCommandSet(commandsCreateMappings);

			// create chain
			commands.push_back(commandSetTempDisconnectStreams);
			commands.push_back(commandSetRemoveMappings);
			commands.push_back(commandSetCreateMappings);
			commands.push_back(commandSetChangeStreamFormat);
			commands.push_back(commandSetCreateStreamConnections);
			commands.push_back(commandSetReconnectStreams);

			// execute the command chain
			auto* sequentialAcmpCommandExecuter = new commandChain::SequentialAsyncCommandExecuter(this);
			connect(sequentialAcmpCommandExecuter, &commandChain::SequentialAsyncCommandExecuter::completed, this,
				[this](commandChain::CommandExecutionErrors const errors)
				{
					CreateConnectionsInfo info;
					info.connectionCreationErrors = errors;
					emit createChannelConnectionsFinished(info);
				});
			connect(sequentialAcmpCommandExecuter, &commandChain::SequentialAsyncCommandExecuter::completed, sequentialAcmpCommandExecuter, &commandChain::SequentialAsyncCommandExecuter::deleteLater);
			sequentialAcmpCommandExecuter->setCommandChain(commands);
			sequentialAcmpCommandExecuter->start();
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

		auto connectionStreamSourceIndex = std::optional<la::avdecc::entity::model::StreamIndex>{ std::nullopt };
		auto connectionStreamTargetIndex = std::optional<la::avdecc::entity::model::StreamIndex>{ std::nullopt };
		auto connectionStreamChannel = std::optional<uint16_t>{ std::nullopt };

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

		if (connectionStreamSourceIndex.has_value())
		{
			// determine the amount of channels that are used on this stream connection.
			// If the connection to remove isn't the only one on the stream, streamConnectionStillNeeded is set to true and the stream will stay connected.
			auto unassignedChannels = getUnassignedChannelsOnTalkerStream(talkerEntityId, *connectionStreamSourceIndex);
			auto const channelConnectionsOfTalker = getAllChannelConnectionsBetweenDevices(talkerEntityId, talkerStreamPortIndex, listenerEntityId);
			bool streamConnectionStillNeeded = false;
			std::set<std::tuple<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::ClusterIndex, uint16_t>> listenerClusterChannels;

			auto streamConnectionUsages = uint32_t{ 0 };
			for (auto const& deviceConnection : channelConnectionsOfTalker->targets)
			{
				// if this is a redundant connection, we convert the index to the primary:
				la::avdecc::entity::model::StreamIdentification talkerStreamIdentification{ talkerEntityId, deviceConnection->sourceStreamIndex };
				la::avdecc::entity::model::StreamIdentification listenerStreamIdentification{ listenerEntityId, deviceConnection->targetStreamIndex };

				auto virtualTalkerIndex = getRedundantVirtualIndexFromOutputStreamIndex(talkerStreamIdentification);
				auto virtualListenerIndex = getRedundantVirtualIndexFromInputStreamIndex(listenerStreamIdentification);

				if (virtualTalkerIndex)
				{
					auto talkerPimaryStreamIndex = getPrimaryOutputStreamIndexFromVirtualIndex(talkerEntityId, *virtualTalkerIndex);

					if (talkerPimaryStreamIndex)
					{
						talkerStreamIdentification.streamIndex = *talkerPimaryStreamIndex;
					}
				}

				if (virtualListenerIndex)
				{
					auto listenerPimaryStreamIndex = getPrimaryInputStreamIndexFromVirtualIndex(listenerEntityId, *virtualListenerIndex);
					if (listenerPimaryStreamIndex)
					{
						bool alreadyHandledConnection = false;
						for (auto const& [clusterIndex, channel] : deviceConnection->targetClusterChannels)
						{
							auto const listenerClusterChannel = std::make_tuple(*listenerPimaryStreamIndex, clusterIndex, channel);
							if (listenerClusterChannels.find(listenerClusterChannel) == listenerClusterChannels.end())
							{
								listenerClusterChannels.insert(listenerClusterChannel);
							}
							else
							{
								alreadyHandledConnection = true;
							}
						}

						if (alreadyHandledConnection)
						{
							// skip secondary if primary was alread handled
							continue;
						}
						listenerStreamIdentification.streamIndex = *listenerPimaryStreamIndex;
					}
				}


				if (deviceConnection->targetEntityId == listenerEntityId && listenerStreamIdentification.streamIndex == *connectionStreamTargetIndex && talkerStreamIdentification.streamIndex == *connectionStreamSourceIndex)
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

			auto talkerChannelReceivers = uint32_t{ 0 };
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
			// also static mappings can't be removed
			auto const& streamPortOutput = controlledTalkerEntity->getStreamPortOutputNode(controlledTalkerEntity->getCurrentConfigurationNode().descriptorIndex, talkerStreamPortIndex);
			if (talkerChannelReceivers <= 1 && streamPortOutput.staticModel->hasDynamicAudioMap)
			{
				// never remove talker mappings, that are default mappings:
				if (talkerClusterIndex - talkerBaseCluster != *connectionStreamChannel)
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

	std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> getAllStreamOutputConnections(la::avdecc::UniqueIdentifier const talkerEntityId, la::avdecc::entity::model::StreamIndex const streamIndex)
	{
		auto disconnectedStreams = std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>>{};
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
				try
				{
					auto const& configNode = controlledEntity->getCurrentConfigurationNode();
					for (auto const& [streamIndex, streamInputNode] : configNode.streamInputs)
					{
						auto* streamInputDynamicModel = streamInputNode.dynamicModel;
						if (streamInputDynamicModel)
						{
							auto const& talkerStream = streamInputDynamicModel->connectionInfo.talkerStream;
							if (talkerStream.entityID == talkerEntityId && talkerStream.streamIndex == streamIndex)
							{
								disconnectedStreams.push_back({ { potentialListenerEntityId, streamIndex }, streamInputDynamicModel->connectionInfo });
							}
						}
					}
				}
				catch (la::avdecc::controller::ControlledEntity::Exception const&)
				{
				}
			}
		}
		return disconnectedStreams;
	}

	/**
	* Finds the stream indexes of a channel connection if there are any.
	*/
	std::vector<StreamIdentificationPair> getStreamIndexPairUsedByAudioChannelConnection(la::avdecc::UniqueIdentifier const& talkerEntityId, avdecc::ChannelIdentification const& talkerChannelIdentification, la::avdecc::UniqueIdentifier const& listenerEntityId, avdecc::ChannelIdentification const& listenerChannelIdentification) noexcept
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
						std::vector<StreamIdentificationPair> result;
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
	std::set<uint16_t> getAssignedChannelsOnTalkerStream(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::StreamIndex const outputStreamIndex, std::optional<la::avdecc::entity::model::ClusterIndex> const clusterOffset = std::nullopt, std::optional<uint16_t> const clusterChannel = std::nullopt) const noexcept
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
				// add dynamic mappings
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

				// add static mappings
				for (auto const& audioMap : streamPortOutput.second.audioMaps)
				{
					for (auto const& mapping : audioMap.second.staticModel->mappings)
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
		}

		return result;
	}

	/**
	* Find all outgoing stream channels that are assigned to the given cluster channel.
	*/
	std::set<uint16_t> getAssignedChannelsOnListenerStream(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::StreamIndex const inputStreamIndex, std::optional<la::avdecc::entity::model::ClusterIndex> const clusterOffset = std::nullopt, std::optional<uint16_t> const clusterChannel = std::nullopt) const noexcept
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

	std::set<uint16_t> getAssignedChannelsOnConnectedListenerStreams(la::avdecc::UniqueIdentifier const talkerEntityId, la::avdecc::UniqueIdentifier const listenerEntityId, la::avdecc::entity::model::StreamIndex const outputStreamIndex, std::optional<la::avdecc::entity::model::ClusterIndex> const clusterOffset = std::nullopt, std::optional<uint16_t> const clusterChannel = std::nullopt) const noexcept
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
		auto connectedStreamListenerIndices = std::set<la::avdecc::entity::model::StreamIndex>{};
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
	std::set<uint16_t> getUnassignedChannelsOnTalkerStream(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::StreamIndex const outputStreamIndex) const noexcept
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

		auto occupiedStreamChannels = std::set<uint16_t>{};
		for (auto const& audioUnit : configurationNode.audioUnits)
		{
			for (auto const& streamPortOutput : audioUnit.second.streamPortOutputs)
			{
				// dynamic mappings
				for (auto const& mapping : streamPortOutput.second.dynamicModel->dynamicAudioMap)
				{
					if (mapping.streamIndex == outputStreamIndex)
					{
						occupiedStreamChannels.insert(mapping.streamChannel);
					}
				}

				// add static mappings
				for (auto const& audioMap : streamPortOutput.second.audioMaps)
				{
					for (auto const& mapping : audioMap.second.staticModel->mappings)
					{
						if (mapping.streamIndex == outputStreamIndex)
						{
							occupiedStreamChannels.insert(mapping.streamChannel);
						}
					}
				}
			}
		}

		// get the stream channel count:
		auto channelCount = getStreamOutputChannelCount(entityId, outputStreamIndex);
		for (auto i = uint16_t{ 0 }; i < channelCount; i++)
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
	std::set<uint16_t> getUnassignedChannelsOnListenerStream(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::StreamIndex const inputStreamIndex) const noexcept
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

		auto occupiedStreamChannels = std::set<uint16_t>{};
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
		auto channelCount = getStreamInputChannelCount(entityId, inputStreamIndex);
		for (auto i = uint16_t{ 0 }; i < channelCount; i++)
		{
			if (occupiedStreamChannels.find(i) == occupiedStreamChannels.end())
			{
				result.emplace(i);
			}
		}

		return result;
	}

	la::avdecc::entity::model::AudioMappings getMappingsFromStreamInputChannel(la::avdecc::UniqueIdentifier const listenerEntityId, la::avdecc::entity::model::StreamIndex const inputStreamIndex, uint16_t const streamChannel) const noexcept
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

		auto outputConnections = getAllStreamOutputConnections(talkerEntityId);

		for (auto const& [listenerStream, connectionInfo] : outputConnections)
		{
			if (listenerStream.entityID == listenerEntityId)
			{
				auto const sourceStreamIndex = connectionInfo.talkerStream.streamIndex;
				auto const targetStreamIndex = listenerStream.streamIndex;
				result.push_back(std::make_pair(sourceStreamIndex, targetStreamIndex));
			}
		}
		return result;
	}

	StreamConnections getPossibleAudioStreamConnectionsBetweenDevices(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId) const noexcept
	{
		StreamConnections result;
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledTalkerEntity = manager.getControlledEntity(talkerEntityId);
		auto controlledListenerEntity = manager.getControlledEntity(listenerEntityId);

		if (!controlledTalkerEntity || !controlledListenerEntity)
		{
			return result;
		}
		if (!controlledTalkerEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledListenerEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return result;
		}

		la::avdecc::controller::model::ConfigurationNode talkerConfigurationNode;
		try
		{
			talkerConfigurationNode = controlledTalkerEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return result;
		}

		la::avdecc::controller::model::ConfigurationNode listenerConfigurationNode;
		try
		{
			listenerConfigurationNode = controlledListenerEntity->getCurrentConfigurationNode();
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			return result;
		}

		// Find all stream output & input combinations that are stream format compatible or can be made compatible (support AAF)
		// and are not in use already
		for (auto const& streamOutputKV : talkerConfigurationNode.streamOutputs)
		{
			if (!supportsStreamFormat(streamOutputKV.second.staticModel->formats, la::avdecc::entity::model::StreamFormatInfo::Type::AAF))
			{
				continue;
			}
			if (!isOutputStreamPrimaryOrNonRedundant(la::avdecc::entity::model::StreamIdentification{ talkerEntityId, streamOutputKV.first }))
			{
				// skip secundary streams
				continue;
			}
			for (auto const& streamInputKV : listenerConfigurationNode.streamInputs)
			{
				if (!isInputStreamPrimaryOrNonRedundant(la::avdecc::entity::model::StreamIdentification{ listenerEntityId, streamInputKV.first }))
				{
					// skip secundary streams
					continue;
				}
				auto const* const streamInputDymaicModel = streamInputKV.second.dynamicModel;
				if (streamInputDymaicModel)
				{
					if (streamInputDymaicModel->connectionInfo.state != la::avdecc::entity::model::StreamInputConnectionInfo::State::NotConnected)
					{
						continue; // skip if connected or fast connecting
					}
					if (!supportsStreamFormat(streamInputKV.second.staticModel->formats, la::avdecc::entity::model::StreamFormatInfo::Type::AAF))
					{
						continue;
					}

					result.push_back(std::make_pair(streamOutputKV.first, streamInputKV.first));
				}
			}
		}

		return result;
	}

	/**
	* Checks if the given list of stream formats contains a format with the given type.
	*/
	bool supportsStreamFormat(la::avdecc::entity::model::StreamFormats const streamFormats, la::avdecc::entity::model::StreamFormatInfo::Type const expectedStreamFormatType) const noexcept
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
	bool checkStreamFormatType(la::avdecc::entity::model::StreamFormat const streamFormat, la::avdecc::entity::model::StreamFormatInfo::Type const expectedStreamFormatType) const noexcept
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

	std::pair<std::optional<la::avdecc::entity::model::StreamFormat>, std::optional<la::avdecc::entity::model::StreamFormat>> getCompatibleStreamFormatChannelCount(la::avdecc::entity::model::StreamFormat const talkerStreamFormat, la::avdecc::entity::model::StreamFormat const listenerStreamFormat, uint16_t channelMinSizeHint) const noexcept
	{
		auto resultingTalkerStreamFormat = std::optional<la::avdecc::entity::model::StreamFormat>{ std::nullopt };
		auto resultingListenerStreamFormat = std::optional<la::avdecc::entity::model::StreamFormat>{ std::nullopt };


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
	std::pair<std::optional<la::avdecc::entity::model::StreamFormat>, std::optional<la::avdecc::entity::model::StreamFormat>> findCompatibleStreamPairFormat(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::StreamIndex const streamOutputIndex, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::StreamIndex const streamInputIndex, la::avdecc::entity::model::StreamFormatInfo::Type const expectedStreamFormatType, uint16_t const channelMinSizeHint = 0) const noexcept
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

		auto const& currentStreamOutputFormat = streamOutputNode.dynamicModel->streamFormat;
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
		auto const& currentStreamInputFormat = streamInputNode.dynamicModel->streamFormat;

		if (la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(currentStreamInputFormat, currentStreamOutputFormat))
		{
			// formats match already, no action necessary.
			return std::make_pair(std::nullopt, std::nullopt);
		}

		auto compatibleFormatOptions = std::vector<std::pair<la::avdecc::entity::model::StreamFormat, la::avdecc::entity::model::StreamFormat>>{};

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
						compatibleFormatOptions.push_back(la::avdecc::entity::model::StreamFormatInfo::getAdaptedCompatibleFormats(streamInputFormat, streamOutputFormat));
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
			if (la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(compatibleFormatOption.first, currentStreamOutputFormat))
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
				if (la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(currentStreamInputFormat, compatibleFormatOption.second))
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

	/**
	* Adjusts the stream formats of the given stream pair.
	*/
	void adjustStreamPairFormats(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::StreamIndex const streamOutputIndex, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::StreamIndex const streamInputIndex, std::pair<std::optional<la::avdecc::entity::model::StreamFormat>, std::optional<la::avdecc::entity::model::StreamFormat>> const streamFormats) const noexcept
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
	uint16_t getStreamInputChannelCount(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept
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
		auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode.dynamicModel->streamFormat);
		return sfi->getChannelsCount();
	}

	/**
	* Returns the count of channels a stream supports.
	*/
	uint16_t getStreamOutputChannelCount(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept
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
		auto const& streamNode = controlledEntity->getStreamOutputNode(configurationNode.descriptorIndex, streamIndex);
		auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode.dynamicModel->streamFormat);
		return sfi->getChannelsCount();
	}


	// Slots
	/**
	* Removes all entities from the internal list.
	*/
	void onControllerOffline()
	{
		_entities.clear();
	}

	/**
	* Adds the entity to the internal list.
	*/
	void onEntityOnline(la::avdecc::UniqueIdentifier const& entityId)
	{
		// add entity to the set
		_entities.insert(entityId);
	}

	/**
	* Removes the entity from the internal list.
	*/
	void onEntityOffline(la::avdecc::UniqueIdentifier const& entityId)
	{
		// remove entity from the set
		_entities.erase(entityId);
		// also remove the cached connections for this entity
		_listenerChannelMappings.erase(entityId);
	}

	/**
	* Update the cached connection info if it's already in the map.
	*/
	void onStreamInputConnectionChanged(la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamInputConnectionInfo const& info)
	{
		auto listenerChannelMappingIt = _listenerChannelMappings.find(stream.entityID);

		if (listenerChannelMappingIt != _listenerChannelMappings.end())
		{
			auto virtualTalkerIndex = getRedundantVirtualIndexFromOutputStreamIndex(info.talkerStream);
			auto virtualListenerIndex = getRedundantVirtualIndexFromInputStreamIndex(stream);

			auto listenerChannelsToUpdate = std::set<std::pair<la::avdecc::UniqueIdentifier, ChannelIdentification>>{};
			auto updatedListenerChannels = std::set<std::pair<la::avdecc::UniqueIdentifier, ChannelIdentification>>{};
			auto connectionInfo = listenerChannelMappingIt->second;

			// if a stream was disconnected, only update the entries that have a connection currently
			// and if the stream was connected, only update the entries that have no connections yet.
			if (info.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::NotConnected)
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
								auto controlledEntity = manager.getControlledEntity(stream.entityID);
								auto const configIndex = controlledEntity->getCurrentConfigurationNode().descriptorIndex;
								auto const& redundantListenerStreamNode = controlledEntity->getRedundantStreamInputNode(configIndex, *virtualListenerIndex);
								bool atLeastOneConnected = false;
								for (auto const& [streamIndex, streamNode] : redundantListenerStreamNode.redundantStreams)
								{
									if (controlledEntity->getStreamInputNode(configIndex, streamIndex).dynamicModel->connectionInfo.state != la::avdecc::entity::model::StreamInputConnectionInfo::State::NotConnected)
									{
										atLeastOneConnected = true;
										break;
									}
								}
								// check if at least one is still connected
								// if not, insert into listenerChannelsToUpdate
								if (!atLeastOneConnected)
								{
									auto channel = std::make_pair(stream.entityID, mappingKV.first);
									listenerChannelsToUpdate.insert(channel);
								}
							}
						}
						else if (target->sourceStreamIndex == stream.streamIndex && target->targetStreamIndex == info.talkerStream.streamIndex)
						{
							// this needs a refresh
							auto channel = std::make_pair(stream.entityID, mappingKV.first);
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
						if ((target->targetStreamIndex != info.talkerStream.streamIndex || target->targetEntityId != info.talkerStream.entityID) && target->sourceStreamIndex == stream.streamIndex)
						{
							// this needs a refresh
							auto channel = std::make_pair(stream.entityID, mappingKV.first);
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
							auto controlledEntity = manager.getControlledEntity(stream.entityID);
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
							auto const streamIndex = stream.streamIndex;

							auto virtualStreamIndex = getRedundantVirtualIndexFromInputStreamIndex(stream);
							if (virtualListenerIndex)
							{
								if (*virtualListenerIndex == *virtualStreamIndex)
								{
									auto& manager = avdecc::ControllerManager::getInstance();
									auto controlledEntity = manager.getControlledEntity(stream.entityID);
									auto const configIndex = controlledEntity->getCurrentConfigurationNode().descriptorIndex;
									auto const& redundantListenerStreamNode = controlledEntity->getRedundantStreamInputNode(configIndex, *virtualListenerIndex);
									bool atLeastOneConnected = false;
									for (auto const& [redundantListenerStreamIndex, streamNode] : redundantListenerStreamNode.redundantStreams)
									{
										if (controlledEntity->getStreamInputNode(configIndex, redundantListenerStreamIndex).dynamicModel->connectionInfo.state != la::avdecc::entity::model::StreamInputConnectionInfo::State::NotConnected)
										{
											atLeastOneConnected = true;
											break;
										}
									}
									// check if at least one is connected
									// if so, insert into listenerChannelsToUpdate
									if (atLeastOneConnected)
									{
										auto channel = std::make_pair(stream.entityID, mappingKV.first);
										listenerChannelsToUpdate.insert(channel);
									}
								}
							}
							else if (clusterIndex + baseCluster == mapping.clusterOffset && clusterChannel == mapping.clusterChannel && mapping.streamIndex == streamIndex)
							{
								// this propably needs a refresh
								auto const& channelIdentification = mappingKV.first;
								auto channel = std::make_pair(stream.entityID, channelIdentification);
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
	void onStreamPortAudioMappingsChanged(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex)
	{
		auto listenerChannelsToUpdate = std::set<std::pair<la::avdecc::UniqueIdentifier, ChannelIdentification>>{};
		auto updatedListenerChannels = std::set<std::pair<la::avdecc::UniqueIdentifier, ChannelIdentification>>{};

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
						auto currentlyConnectedEntities = std::unordered_set<la::avdecc::UniqueIdentifier, la::avdecc::UniqueIdentifier::hash>{};
						for (auto const& streamInput : configuration.streamInputs)
						{
							auto* streamInputDynamicModel = streamInput.second.dynamicModel;
							if (streamInputDynamicModel && streamInputDynamicModel->connectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected)
							{
								currentlyConnectedEntities.emplace(streamInputDynamicModel->connectionInfo.talkerStream.entityID);
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
			catch (la::avdecc::Exception const&)
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
