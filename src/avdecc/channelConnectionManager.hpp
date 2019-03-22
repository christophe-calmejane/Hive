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

struct SourceChannelIdentification
{
	bool forward = true; /** This flag indicates the direction of the connections. */
	std::optional<la::avdecc::entity::model::ConfigurationIndex> sourceConfigurationIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::AudioUnitIndex> sourceAudioUnitIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::StreamPortIndex> sourceStreamPortIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::ClusterIndex> sourceClusterIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::ClusterIndex> sourceBaseCluster = std::nullopt;
	std::optional<std::uint16_t> sourceClusterChannel = 0;
};

constexpr bool operator<(SourceChannelIdentification const lhs, SourceChannelIdentification const rhs)
{
	return lhs.sourceConfigurationIndex < rhs.sourceConfigurationIndex || lhs.sourceConfigurationIndex == rhs.sourceConfigurationIndex && lhs.sourceAudioUnitIndex < rhs.sourceAudioUnitIndex || lhs.sourceConfigurationIndex == rhs.sourceConfigurationIndex && lhs.sourceAudioUnitIndex == rhs.sourceAudioUnitIndex && lhs.sourceStreamPortIndex < rhs.sourceStreamPortIndex || lhs.sourceConfigurationIndex == rhs.sourceConfigurationIndex && lhs.sourceAudioUnitIndex == rhs.sourceAudioUnitIndex && lhs.sourceStreamPortIndex == rhs.sourceStreamPortIndex && lhs.sourceClusterIndex < rhs.sourceClusterIndex
				 || lhs.sourceConfigurationIndex == rhs.sourceConfigurationIndex && lhs.sourceAudioUnitIndex == rhs.sourceAudioUnitIndex && lhs.sourceStreamPortIndex == rhs.sourceStreamPortIndex && lhs.sourceClusterIndex == rhs.sourceClusterIndex && lhs.sourceClusterChannel < rhs.sourceClusterChannel;
}

constexpr bool operator==(SourceChannelIdentification const lhs, SourceChannelIdentification const rhs)
{
	return lhs.sourceConfigurationIndex == rhs.sourceConfigurationIndex && lhs.sourceAudioUnitIndex == rhs.sourceAudioUnitIndex && lhs.sourceStreamPortIndex == rhs.sourceStreamPortIndex && lhs.sourceClusterIndex == rhs.sourceClusterIndex && lhs.sourceClusterChannel == rhs.sourceClusterChannel;
}

struct TargetConnectionInformation
{
	la::avdecc::UniqueIdentifier targetEntityId;
	la::avdecc::entity::model::StreamIndex sourceStreamIndex;
	la::avdecc::entity::model::StreamIndex targetStreamIndex;
	uint16_t streamChannel; // same between talker and listener
	std::vector<std::pair<la::avdecc::entity::model::ClusterIndex, std::uint16_t>> targetClusterChannels;
	std::optional<la::avdecc::entity::model::AudioUnitIndex> targetAudioUnitIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::StreamPortIndex> targetStreamPortIndex = std::nullopt;
	std::optional<la::avdecc::entity::model::ClusterIndex> targetBaseCluster = std::nullopt;
	bool isTargetRedundant = false;
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
	avdecc::SourceChannelIdentification sourceClusterChannelInfo;
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
	std::map<SourceChannelIdentification, std::shared_ptr<TargetConnectionInformations>> channelMappings;
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

	/* channel connection management helper functions */
	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnections(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, la::avdecc::entity::model::ClusterIndex const baseCluster, std::uint16_t const clusterChannel) const noexcept = 0;

	virtual std::shared_ptr<TargetConnectionInformations> getChannelConnectionsReverse(la::avdecc::UniqueIdentifier const entityId, la::avdecc::UniqueIdentifier const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, la::avdecc::entity::model::ClusterIndex const baseCluster, std::uint16_t const clusterChannel) noexcept = 0;

	virtual std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> getRedundantStreamOutputsForPrimary(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex primaryStreamIndex) const noexcept = 0;

	virtual std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> getRedundantStreamInputsForPrimary(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::StreamIndex primaryStreamIndex) const noexcept = 0;

	Q_SIGNAL void listenerChannelConnectionsUpdate(std::set<std::pair<la::avdecc::UniqueIdentifier, SourceChannelIdentification>> channels);
};

} // namespace avdecc
