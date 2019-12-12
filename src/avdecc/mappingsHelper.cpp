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

#include "mappingsHelper.hpp"

namespace avdecc
{
namespace mappingsHelper
{
std::pair<NodeMappings, mappingMatrix::Nodes> buildClusterMappings(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::StreamPortNode const& streamPortNode)
{
	NodeMappings clusterMappings;
	mappingMatrix::Nodes clusterMatrixNodes;

	// Build list of cluster mappings
	for (auto const& clusterKV : streamPortNode.audioClusters)
	{
		auto const clusterIndex = static_cast<la::avdecc::entity::model::ClusterIndex>(clusterKV.first - streamPortNode.staticModel->baseCluster); // Mappings use relative index (see IEEE1722.1 Table 7.33)
		AVDECC_ASSERT(clusterIndex < streamPortNode.staticModel->numberOfClusters, "ClusterIndex invalid");
		auto const& clusterNode = clusterKV.second;
		auto clusterName = avdecc::helper::objectName(controlledEntity, clusterNode).toStdString();
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

	return std::make_pair(clusterMappings, clusterMatrixNodes);
}

HashType makeHash(mappingMatrix::Connection const& connection)
{
	return (static_cast<HashType>(connection.first.first & 0xFFFF) << 48) + (static_cast<HashType>(connection.first.second & 0xFFFF) << 32) + (static_cast<HashType>(connection.second.first & 0xFFFF) << 16) + static_cast<HashType>(connection.second.second & 0xFFFF);
}

mappingMatrix::Connection unmakeHash(HashType const& hash)
{
	mappingMatrix::Connection connection;

	connection.first.first = (hash >> 48) & 0xFFFF;
	connection.first.second = (hash >> 32) & 0xFFFF;
	connection.second.first = (hash >> 16) & 0xFFFF;
	connection.second.second = hash & 0xFFFF;

	return connection;
}

HashedConnectionsList hashConnectionsList(mappingMatrix::Connections const& connections)
{
	HashedConnectionsList list;

	for (auto const& c : connections)
	{
		list.insert(makeHash(c));
	}

	return list;
}

HashedConnectionsList substractList(HashedConnectionsList const& a, HashedConnectionsList const& b)
{
	HashedConnectionsList sub{ a };

	for (auto const& v : b)
	{
		sub.erase(v);
	}

	return sub;
}

} // namespace mappingsHelper
} // namespace avdecc
