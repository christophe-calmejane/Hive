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
#include <QObject>
#include <QMap>
#include <QList>
#include <QSharedPointer>

namespace avdecc
{
enum class ConnectionStatus
{
	None = 0,
	WrongDomain = 1u << 0,
	WrongFormat = 1u << 1,
	Connectable = 1u << 2, /**< Stream connectable (might be connected, or not) */
	Connected = 1u << 3, /**< Stream is connected (Mutually exclusive with FastConnecting and PartiallyConnected) */
	FastConnecting = 1u << 4, /**< Stream is fast connecting (Mutually exclusive with Connected and PartiallyConnected) */
	PartiallyConnected = 1u << 5, /**< Some, but not all of a redundant streams tuple, are connected (Mutually exclusive with Connected and FastConnecting) */
};

// **************************************************************
// struct ConnectionDetails
// **************************************************************
/**
	* @brief    Represents a single connection. Holds all data to define
	*			the connection end point.
	* [@author  Marius Erlen]
	* [@date    2018-10-04]
	*/
struct ConnectionDetails : QObject
{
	QList<std::pair<la::avdecc::entity::model::ClusterIndex, std::uint16_t>> targetClusters;
	la::avdecc::entity::model::AudioUnitIndex targetAudioUnitIndex = -1;
	la::avdecc::entity::model::StreamPortIndex targetStreamPortIndex = -1;
	la::avdecc::entity::model::ClusterIndex targetBaseCluster = -1;
};

// **************************************************************
// struct Connections
// **************************************************************
/**
	* @brief    Represents a collection of connections to a single entity.
	* [@author  Marius Erlen]
	* [@date    2018-10-04]
	*/
struct Connections : QObject
{
	la::avdecc::UniqueIdentifier entityId;
	QMap<la::avdecc::entity::model::StreamIndex, QSharedPointer<ConnectionDetails>> targetStreams;
	ConnectionStatus streamConnectionStatus = ConnectionStatus::PartiallyConnected;
};

// **************************************************************
// struct ConnectionInformation
// **************************************************************
/**
	* @brief    Resulting data of a call to getChannelConnections
	*			or ChannelConnectionManager::getChannelConnectionsBackwards.			
	* [@author  Marius Erlen]
	* [@date    2018-10-04]
	*/
struct ConnectionInformation
{
	// store the information of the source
	la::avdecc::UniqueIdentifier sourceEntityId = 0;
	la::avdecc::UniqueIdentifier sourceConfigurationIndex = 0;
	la::avdecc::entity::model::AudioUnitIndex sourceAudioUnitIndex = 0;
	la::avdecc::entity::model::StreamPortIndex sourceStreamPortIndex = 0;
	la::avdecc::entity::model::ClusterIndex sourceClusterIndex = 0;
	la::avdecc::entity::model::ClusterIndex sourceBaseCluster = 0;
	std::uint16_t sourceClusterChannel = 0;
	bool forward = true; /** This flag indicates the direction of the connections. */

	QList<std::pair<la::avdecc::entity::model::StreamIndex, std::uint16_t>> sourceStreams;

	QMap<la::avdecc::UniqueIdentifier, QSharedPointer<Connections>> deviceConnections;
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
	virtual const ConnectionInformation getChannelConnections(la::avdecc::UniqueIdentifier const entityId, la::avdecc::UniqueIdentifier const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, la::avdecc::entity::model::ClusterIndex const baseCluster, std::uint16_t const clusterChannel) noexcept = 0;

	virtual const ConnectionInformation getChannelConnectionsReverse(la::avdecc::UniqueIdentifier const entityId, la::avdecc::UniqueIdentifier const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, la::avdecc::entity::model::ClusterIndex const baseCluster, std::uint16_t const clusterChannel) noexcept = 0;
};

} // namespace avdecc

// Define bitfield enum traits for ConnectionStatus
template<>
struct la::avdecc::enum_traits<avdecc::ConnectionStatus>
{
	static constexpr bool is_bitfield = true;
};
