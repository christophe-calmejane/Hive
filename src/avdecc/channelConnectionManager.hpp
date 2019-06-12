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
* along with Hive. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <la/avdecc/controller/avdeccController.hpp>
#include <memory>
#include <unordered_set>
#include <optional>
#include <QObject>

namespace avdecc
{
using StreamConnection = std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamIdentification>;

struct ChannelIdentification
{
	/*Identifying data*/
	std::optional<la::avdecc::entity::model::ConfigurationIndex> configurationIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::ClusterIndex> clusterIndex = std::nullopt;
	std::optional<std::uint16_t> clusterChannel = std::nullopt;

	/*Additional data*/
	std::optional<la::avdecc::entity::model::AudioUnitIndex> audioUnitIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::StreamPortIndex> streamPortIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::ClusterIndex> baseCluster = std::nullopt;
	bool forward = true; /** This flag indicates the direction of the connections. */
};

constexpr bool operator<(ChannelIdentification const lhs, ChannelIdentification const rhs)
{
	return lhs.configurationIndex < rhs.configurationIndex || lhs.configurationIndex == rhs.configurationIndex && lhs.audioUnitIndex < rhs.audioUnitIndex || lhs.configurationIndex == rhs.configurationIndex && lhs.audioUnitIndex == rhs.audioUnitIndex && lhs.streamPortIndex < rhs.streamPortIndex || lhs.configurationIndex == rhs.configurationIndex && lhs.audioUnitIndex == rhs.audioUnitIndex && lhs.streamPortIndex == rhs.streamPortIndex && lhs.clusterIndex < rhs.clusterIndex || lhs.configurationIndex == rhs.configurationIndex && lhs.audioUnitIndex == rhs.audioUnitIndex && lhs.streamPortIndex == rhs.streamPortIndex && lhs.clusterIndex == rhs.clusterIndex && lhs.clusterChannel < rhs.clusterChannel;
}

constexpr bool operator==(ChannelIdentification const lhs, ChannelIdentification const rhs)
{
	return lhs.configurationIndex == rhs.configurationIndex && lhs.audioUnitIndex == rhs.audioUnitIndex && lhs.streamPortIndex == rhs.streamPortIndex && lhs.clusterIndex == rhs.clusterIndex && lhs.clusterChannel == rhs.clusterChannel;
}

struct TargetConnectionInformation
{
	la::avdecc::UniqueIdentifier targetEntityId;
	la::avdecc::entity::model::StreamIndex sourceStreamIndex;
	la::avdecc::entity::model::StreamIndex targetStreamIndex;
	std::optional<la::avdecc::controller::model::VirtualIndex> sourceVirtualIndex;
	std::optional<la::avdecc::controller::model::VirtualIndex> targetVirtualIndex;
	std::vector<std::pair<la::avdecc::entity::model::StreamIndex, la::avdecc::entity::model::StreamIndex>> streamPairs;
	uint16_t streamChannel; // same between talker and listener
	std::vector<std::pair<la::avdecc::entity::model::ClusterIndex, std::uint16_t>> targetClusterChannels;
	std::optional<la::avdecc::entity::model::AudioUnitIndex> targetAudioUnitIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::StreamPortIndex> targetStreamPortIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::ClusterIndex> targetBaseCluster = std::nullopt;
	bool isTargetRedundant = false; // could be removed and only use virtual index != nullopt instead.
	bool isSourceRedundant = false;

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
	la::avdecc::UniqueIdentifier sourceEntityId;
	avdecc::ChannelIdentification sourceClusterChannelInfo;
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

	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnections(la::avdecc::UniqueIdentifier const& entityId, ChannelIdentification sourceChannelIdentification) const noexcept = 0;

	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnectionsReverse(la::avdecc::UniqueIdentifier const& entityId, ChannelIdentification const& sourceChannelIdentification) noexcept = 0;

	virtual std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> getRedundantStreamOutputsForPrimary(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex primaryStreamIndex) const noexcept = 0;

	virtual std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> getRedundantStreamInputsForPrimary(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex primaryStreamIndex) const noexcept = 0;

	virtual ChannelConnectResult createChannelConnection(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::AudioUnitIndex const talkerAudioUnitIndex, la::avdecc::entity::model::StreamPortIndex const talkerStreamPortIndex, la::avdecc::entity::model::ClusterIndex const talkerClusterIndex, la::avdecc::entity::model::ClusterIndex const talkerBaseCluster, std::uint16_t const talkerClusterChannel, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::AudioUnitIndex const listenerAudioUnitIndex, la::avdecc::entity::model::StreamPortIndex const listenerStreamPortIndex, la::avdecc::entity::model::ClusterIndex const listenerClusterIndex, la::avdecc::entity::model::ClusterIndex const listenerBaseCluster, std::uint16_t const listenerClusterChannel,
		bool allowRemovalOfUnusedAudioMappings = false, uint16_t streamChannelCountHint = 8) const noexcept = 0;

	virtual ChannelConnectResult createChannelConnections(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::UniqueIdentifier const& listenerEntityId, std::vector<std::pair<avdecc::ChannelIdentification, avdecc::ChannelIdentification>> const& talkerToListenerChannelConnections, bool allowRemovalOfUnusedAudioMappings = false) const noexcept = 0;

	virtual ChannelDisconnectResult removeChannelConnection(
		la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::AudioUnitIndex const talkerAudioUnitIndex, la::avdecc::entity::model::StreamPortIndex const talkerStreamPortIndex, la::avdecc::entity::model::ClusterIndex const talkerClusterIndex, la::avdecc::entity::model::ClusterIndex const talkerBaseCluster, std::uint16_t const talkerClusterChannel, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::AudioUnitIndex const listenerAudioUnitIndex, la::avdecc::entity::model::StreamPortIndex const listenerStreamPortIndex, la::avdecc::entity::model::ClusterIndex const listenerClusterIndex, la::avdecc::entity::model::ClusterIndex const listenerBaseCluster, std::uint16_t const listenerClusterChannel) noexcept = 0;

	virtual std::vector<StreamConnection> getStreamIndexPairUsedByAudioChannelConnection(la::avdecc::UniqueIdentifier const& talkerEntityId, ChannelIdentification const& talkerChannelIdentification, la::avdecc::UniqueIdentifier const& listenerEntityId, ChannelIdentification const& listenerChannelIdentification) noexcept = 0;

	virtual bool isInputStreamPrimaryOrNonRedundant(la::avdecc::entity::model::StreamIdentification const& streamIdentification) const noexcept = 0;

	Q_SIGNAL void listenerChannelConnectionsUpdate(std::set<std::pair<la::avdecc::UniqueIdentifier, ChannelIdentification>> channels);
};


constexpr bool operator!(ChannelConnectionManager::ChannelConnectResult const error)
{
	return error == ChannelConnectionManager::ChannelConnectResult::NoError;
}

} // namespace avdecc
