/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#pragma once

#include <la/avdecc/controller/avdeccController.hpp>
#include <memory>
#include <unordered_set>
#include <optional>
#include <QObject>

#include "mcDomainManager.hpp"

namespace avdecc
{
using StreamIdentificationPair = std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamIdentification>;

enum class ChannelConnectionDirection
{
	OutputToInput,
	InputToOutput,
};

struct ChannelIdentification
{
	ChannelIdentification(la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, std::uint16_t const clusterChannel, ChannelConnectionDirection const channelConnectionDirection, std::optional<la::avdecc::entity::model::AudioUnitIndex> const audioUnitIndex = std::nullopt, std::optional<la::avdecc::entity::model::StreamPortIndex> streamPortIndex = std::nullopt, std::optional<la::avdecc::entity::model::ClusterIndex> baseCluster = std::nullopt)
	{
		this->configurationIndex = configurationIndex;
		this->clusterIndex = clusterIndex;
		this->clusterChannel = clusterChannel;
		this->direction = channelConnectionDirection;
		this->audioUnitIndex = audioUnitIndex;
		this->streamPortIndex = streamPortIndex;
		this->baseCluster = baseCluster;
	}

	/**
	* Checks if the identifying data (configurationIndex, clusterIndex and clusterChannel) match.
	*/
	bool IsSameIdentification(ChannelIdentification const& other)
	{
		return this->configurationIndex == other.configurationIndex && this->clusterIndex == other.clusterIndex && this->clusterChannel == other.clusterChannel;
	}

	/*Identifying data*/
	la::avdecc::entity::model::ConfigurationIndex configurationIndex{ 0 };
	la::avdecc::entity::model::ClusterIndex clusterIndex{ 0 };
	std::uint16_t clusterChannel{ 0 };

	/*Additional data*/
	ChannelConnectionDirection direction{ ChannelConnectionDirection::OutputToInput };
	std::optional<la::avdecc::entity::model::AudioUnitIndex> audioUnitIndex{ 0 };
	std::optional<la::avdecc::entity::model::StreamPortIndex> streamPortIndex{ 0 };
	std::optional<la::avdecc::entity::model::ClusterIndex> baseCluster{ 0 };
};

constexpr bool operator<(ChannelIdentification const& lhs, ChannelIdentification const& rhs)
{
	if (lhs.configurationIndex < rhs.configurationIndex)
	{
		return true;
	}
	else if (lhs.configurationIndex == rhs.configurationIndex && lhs.clusterIndex < rhs.clusterIndex)
	{
		return true;
	}
	else if (lhs.configurationIndex == rhs.configurationIndex && lhs.clusterIndex == rhs.clusterIndex && lhs.clusterChannel < rhs.clusterChannel)
	{
		return true;
	}
	return false;
}

constexpr bool operator==(ChannelIdentification const& lhs, ChannelIdentification const& rhs)
{
	return lhs.configurationIndex == rhs.configurationIndex && lhs.clusterIndex == rhs.clusterIndex && lhs.clusterChannel == rhs.clusterChannel && lhs.direction == rhs.direction && *lhs.audioUnitIndex == *rhs.audioUnitIndex && *lhs.streamPortIndex == *rhs.streamPortIndex && *lhs.baseCluster == *rhs.baseCluster;
}

struct TargetConnectionInformation
{
	la::avdecc::UniqueIdentifier targetEntityId{ la::avdecc::UniqueIdentifier::getUninitializedUniqueIdentifier() };
	la::avdecc::entity::model::StreamIndex sourceStreamIndex{ 0 };
	la::avdecc::entity::model::StreamIndex targetStreamIndex{ 0 };
	std::optional<la::avdecc::controller::model::VirtualIndex> sourceVirtualIndex{ std::nullopt }; // only set if the stream is redundant
	std::optional<la::avdecc::controller::model::VirtualIndex> targetVirtualIndex{ std::nullopt }; // only set if the stream is redundant
	std::vector<std::pair<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex>> streamPairs;
	uint16_t streamChannel{ 0 }; // same between talker and listener
	std::vector<std::pair<la::avdecc::entity::model::ClusterIndex, std::uint16_t>> targetClusterChannels{ 0 };
	la::avdecc::entity::model::AudioUnitIndex targetAudioUnitIndex{ 0 };
	la::avdecc::entity::model::StreamPortIndex targetStreamPortIndex{ 0 };
	la::avdecc::entity::model::ClusterIndex targetBaseCluster{ 0 };
	bool isTargetRedundant{ false }; // could be removed and only use virtual index != nullopt instead.
	bool isSourceRedundant{ false };

	inline bool isEqualTo(TargetConnectionInformation const& other)
	{
		if (isSourceRedundant == other.isSourceRedundant && isTargetRedundant == other.isTargetRedundant && sourceStreamIndex == other.sourceStreamIndex && streamChannel == other.streamChannel && targetAudioUnitIndex == other.targetAudioUnitIndex && targetBaseCluster == other.targetBaseCluster && targetEntityId == other.targetEntityId && targetStreamIndex == other.targetStreamIndex && targetStreamPortIndex == other.targetStreamPortIndex && targetClusterChannels == other.targetClusterChannels)
		{
			return true;
		}
		return false;
	}
};

struct TargetConnectionInformations
{
	la::avdecc::UniqueIdentifier sourceEntityId{ la::avdecc::UniqueIdentifier::getUninitializedUniqueIdentifier() };
	std::optional<avdecc::ChannelIdentification> sourceClusterChannelInfo{ std::nullopt };
	std::vector<std::shared_ptr<TargetConnectionInformation>> targets;

	inline bool isEqualTo(TargetConnectionInformations const& other)
	{
		if (sourceEntityId == other.sourceEntityId && sourceClusterChannelInfo == other.sourceClusterChannelInfo && targets.size() == other.targets.size())
		{
			// compare targets:
			auto lhsIterator = targets.begin();
			auto rhsIterator = other.targets.begin();
			while (lhsIterator != targets.end() && rhsIterator != other.targets.end())
			{
				if (!(*lhsIterator)->isEqualTo(**rhsIterator))
				{
					return false;
				}
				lhsIterator++;
				rhsIterator++;
			}
			return true;
		}
		return false;
	}
};

struct SourceChannelConnections
{
	std::map<ChannelIdentification, std::shared_ptr<TargetConnectionInformations>> channelMappings;
};

struct CreateConnectionsInfo
{
	commandChain::CommandExecutionErrors connectionCreationErrors;
};

// **************************************************************
// class ChannelConnectionManager
// **************************************************************
/**
	* @brief    This class provides helper methods to determine channel connections
	*			between devices and their current state.
	* [@author  Marius Erlen]
	* [@date    2018-10-04]
	*/
class ChannelConnectionManager : public QObject
{
	Q_OBJECT
public:
	static ChannelConnectionManager& getInstance() noexcept;

	enum class ChannelConnectResult
	{
		NoError,
		RemovalOfListenerDynamicMappingsNecessary,
		Impossible,
		Error,
		Unsupported,
		NeedsTalkerMappingAdjustment,
	};

	enum class ChannelDisconnectResult
	{
		NoError,
		NonExistent,
		Error,
		Unsupported,
	};

	/* channel connection management helper functions */
	virtual std::shared_ptr<TargetConnectionInformations> getAllChannelConnectionsBetweenDevices(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::UniqueIdentifier const& targetEntityId) const noexcept = 0;

	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnections(la::avdecc::UniqueIdentifier const& entityId, ChannelIdentification const sourceChannelIdentification) const noexcept = 0;

	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnectionsReverse(la::avdecc::UniqueIdentifier const& entityId, ChannelIdentification const& sourceChannelIdentification) noexcept = 0;

	virtual std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> getRedundantStreamOutputsForPrimary(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex const primaryStreamIndex) const noexcept = 0;

	virtual std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> getRedundantStreamInputsForPrimary(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex const primaryStreamIndex) const noexcept = 0;

	virtual ChannelConnectResult createChannelConnection(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId, avdecc::ChannelIdentification const& talkerChannelIdentification, avdecc::ChannelIdentification const& listenerChannelIdentification, bool const allowTalkerMappingChanges = false, bool const allowRemovalOfUnusedAudioMappings = false) noexcept = 0;

	virtual ChannelConnectResult createChannelConnections(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId, std::vector<std::pair<avdecc::ChannelIdentification, avdecc::ChannelIdentification>> const& talkerToListenerChannelConnections, bool const allowTalkerMappingChanges = false, bool const allowRemovalOfUnusedAudioMappings = false) noexcept = 0;

	virtual ChannelDisconnectResult removeChannelConnection(
		la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::AudioUnitIndex const talkerAudioUnitIndex, la::avdecc::entity::model::StreamPortIndex const talkerStreamPortIndex, la::avdecc::entity::model::ClusterIndex const talkerClusterIndex, la::avdecc::entity::model::ClusterIndex const talkerBaseCluster, std::uint16_t const talkerClusterChannel, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::AudioUnitIndex const listenerAudioUnitIndex, la::avdecc::entity::model::StreamPortIndex const listenerStreamPortIndex, la::avdecc::entity::model::ClusterIndex const listenerClusterIndex, la::avdecc::entity::model::ClusterIndex const listenerBaseCluster, std::uint16_t const listenerClusterChannel) noexcept = 0;

	virtual std::vector<StreamIdentificationPair> getStreamIndexPairUsedByAudioChannelConnection(la::avdecc::UniqueIdentifier const& talkerEntityId, ChannelIdentification const& talkerChannelIdentification, la::avdecc::UniqueIdentifier const& listenerEntityId, ChannelIdentification const& listenerChannelIdentification) noexcept = 0;

	virtual bool isInputStreamPrimaryOrNonRedundant(la::avdecc::entity::model::StreamIdentification const& streamIdentification) const noexcept = 0;

	// SIGNALS:
	Q_SIGNAL void listenerChannelConnectionsUpdate(std::set<std::pair<la::avdecc::UniqueIdentifier, ChannelIdentification>> const& channels);

	/* Invoked after createChannelConnection or createChannelConnections are completed */
	Q_SIGNAL void createChannelConnectionsFinished(CreateConnectionsInfo const& info);
};


constexpr bool operator!(ChannelConnectionManager::ChannelConnectResult const error)
{
	return error == ChannelConnectionManager::ChannelConnectResult::NoError;
}

} // namespace avdecc
