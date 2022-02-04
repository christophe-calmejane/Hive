/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "deviceDetailsChannelTableModel.hpp"
#include "ui_deviceDetailsDialog.h"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "avdecc/helper.hpp"
#include "connectionMatrix/paintHelper.hpp"
#include "avdecc/channelConnectionManager.hpp"

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>

#include <QCoreApplication>
#include <QLayout>
#include <QPainter>

/**
* Helper function for connectionStatus medthod. Checks if the stream is currently connected.
* @param talkerID		The id of the talking entity.
* @param talkerNode		The index of the output node to check.
* @param listenerNode	The index of the input node to check.
*/
bool isStreamConnected(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) noexcept
{
	if (listenerNode && listenerNode->dynamicModel && talkerNode)
	{
		return (listenerNode->dynamicModel->connectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected) && (listenerNode->dynamicModel->connectionInfo.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionInfo.talkerStream.streamIndex == talkerNode->descriptorIndex);
	}

	return false;
}

/**
* Helper function for connectionStatus medthod. Checks if the stream is in fast connect mode.
* @param talkerID		The id of the talking entity.
* @param talkerNode		The index of the output node to check.
* @param listenerNode	The index of the input node to check.
*/
bool isStreamFastConnecting(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) noexcept
{
	if (listenerNode && listenerNode->dynamicModel && talkerNode)
	{
		return (listenerNode->dynamicModel->connectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::FastConnecting) && (listenerNode->dynamicModel->connectionInfo.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionInfo.talkerStream.streamIndex == talkerNode->descriptorIndex);
	}

	return false;
}

// Returns true if the gptp domain number and grandmasterId are the same for talker and listener
bool isSameDomain(la::avdecc::entity::model::AvbInterfaceNodeDynamicModel const& talkerDynamicInterfaceNode, la::avdecc::entity::model::AvbInterfaceNodeDynamicModel const& listenerDynamicInterfaceNode) noexcept
{
	return talkerDynamicInterfaceNode.gptpGrandmasterID == listenerDynamicInterfaceNode.gptpGrandmasterID && talkerDynamicInterfaceNode.gptpDomainNumber == listenerDynamicInterfaceNode.gptpDomainNumber;
}

// Returns the connection status of the given talker-listener pair.
DeviceDetailsChannelTableModel::ConnectionStatus calculateConnectionStatus(la::avdecc::UniqueIdentifier talkerEntityId, la::avdecc::entity::model::StreamIndex talkerStreamIndex, la::avdecc::UniqueIdentifier listenerEntityId, la::avdecc::entity::model::StreamIndex listenerStreamIndex)
{
	auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto talkerEntity = manager.getControlledEntity(talkerEntityId);
	auto listenerEntity = manager.getControlledEntity(listenerEntityId);
	if (!talkerEntity || !listenerEntity)
	{
		return {};
	}
	auto const& talkerEntityNode = talkerEntity->getEntityNode();
	auto const& listenerEntityNode = listenerEntity->getEntityNode();

	auto const& talkerConfigurationNode = talkerEntity->getConfigurationNode(talkerEntityNode.dynamicModel->currentConfiguration);
	auto const& listenerConfigurationNode = listenerEntity->getConfigurationNode(listenerEntityNode.dynamicModel->currentConfiguration);
	auto status = DeviceDetailsChannelTableModel::ConnectionStatus{ connectionMatrix::Model::IntersectionData::Type::SingleStream_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected };
	auto const& talkerOutputStreamNode = talkerEntity->getStreamOutputNode(talkerConfigurationNode.descriptorIndex, talkerStreamIndex);
	auto const& listenerInputStreamNode = listenerEntity->getStreamInputNode(listenerConfigurationNode.descriptorIndex, listenerStreamIndex);

	auto const talkerAvbInterfaceIndex{ talkerOutputStreamNode.staticModel->avbInterfaceIndex };
	auto const& talkerAvbInterfaceNode = talkerEntity->getAvbInterfaceNode(talkerConfigurationNode.descriptorIndex, talkerAvbInterfaceIndex);
	auto talkerStreamFormat = talkerOutputStreamNode.dynamicModel->streamFormat;
	auto const talkerDynamicInterfaceNode = talkerAvbInterfaceNode.dynamicModel;
	auto talkerInterfaceLinkStatus = talkerEntity->getAvbInterfaceLinkStatus(talkerAvbInterfaceIndex);

	auto const listenerAvbInterfaceIndex{ listenerInputStreamNode.staticModel->avbInterfaceIndex };
	auto const& listenerAvbInterfaceNode = listenerEntity->getAvbInterfaceNode(listenerConfigurationNode.descriptorIndex, listenerAvbInterfaceIndex);
	auto listenerStreamFormat = listenerInputStreamNode.dynamicModel->streamFormat;
	auto const listenerDynamicInterfaceNode = listenerAvbInterfaceNode.dynamicModel;
	auto listenerInterfaceLinkStatus = listenerEntity->getAvbInterfaceLinkStatus(listenerAvbInterfaceIndex);

	if (la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerStreamFormat, talkerStreamFormat))
	{
		status.flags.reset(connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible);
		status.flags.reset(connectionMatrix::Model::IntersectionData::Flag::WrongFormatImpossible);
	}
	else
	{
		//la::avdecc::controller::Controller::chooseBestStreamFormat
		status.flags.set(connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible);
	}

	auto const interfaceDown = talkerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down || listenerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down;

	if (interfaceDown)
	{
		status.flags.set(connectionMatrix::Model::IntersectionData::Flag::InterfaceDown);
	}
	else
	{
		status.flags.reset(connectionMatrix::Model::IntersectionData::Flag::InterfaceDown);
	}

	if (isSameDomain(*talkerDynamicInterfaceNode, *listenerDynamicInterfaceNode))
	{
		status.flags.reset(connectionMatrix::Model::IntersectionData::Flag::WrongDomain);
	}
	else
	{
		status.flags.set(connectionMatrix::Model::IntersectionData::Flag::WrongDomain);
	}

	return status;
}


// **************************************************************
// class DeviceDetailsDialogImpl
// **************************************************************
/**
* @brief    Private implemenation of the table model.
* [@author  Marius Erlen]
* [@date    2018-09-21]
*
* Holds a list audio cluster nodes to display in a QTableView. Supports editing of the data.
* The data is not directly written to the controller, but stored in a map first.
* These changes can be gathered with the getChanges method.
*/
class DeviceDetailsChannelTableModelPrivate : public QObject
{
	Q_OBJECT
public:
	DeviceDetailsChannelTableModelPrivate(DeviceDetailsChannelTableModel* model);
	virtual ~DeviceDetailsChannelTableModelPrivate();

	int rowCount() const;
	int columnCount() const;
	QVariant data(QModelIndex const& index, int role) const;
	bool setData(QModelIndex const& index, QVariant const& value, int role);
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	Qt::ItemFlags flags(QModelIndex const& index) const;

	void addNode(std::shared_ptr<avdecc::TargetConnectionInformations> const& connectionInformation);
	void removeAllNodes();
	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsChannelTableModelColumn, QVariant>*> getChanges() const;
	void resetChangedData();

	TableRowEntry const& tableDataAtRow(int row) const;

	void channelConnectionsUpdate(la::avdecc::UniqueIdentifier const& entityId);
	void channelConnectionsUpdate(std::set<std::pair<la::avdecc::UniqueIdentifier, avdecc::ChannelIdentification>> channels);
	void updateAudioClusterName(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& audioClusterName);

private:
	DeviceDetailsChannelTableModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(DeviceDetailsChannelTableModel);

	QTableView* _ui;

	using AudioClusterNodes = std::vector<TableRowEntry>;
	AudioClusterNodes _nodes{};

	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsChannelTableModelColumn, QVariant>*> _hasChangesMap;
};

//////////////////////////////////////

/**
* Constructor.
* @param model The public implementation class.
*/
DeviceDetailsChannelTableModelPrivate::DeviceDetailsChannelTableModelPrivate(DeviceDetailsChannelTableModel* model)
	: q_ptr(model)
{
}

/**
* Destructor.
*/
DeviceDetailsChannelTableModelPrivate::~DeviceDetailsChannelTableModelPrivate() {}

/**
* Adds a node to the table model. Doesn't check for duplicates or correct order.
* @param audioClusterNode The node to add to this model.
*/
void DeviceDetailsChannelTableModelPrivate::addNode(std::shared_ptr<avdecc::TargetConnectionInformations> const& connectionInformation)
{
	Q_Q(DeviceDetailsChannelTableModel);

	q->beginInsertRows(QModelIndex(), static_cast<int>(_nodes.size()), static_cast<int>(_nodes.size()));
	_nodes.push_back(TableRowEntry(connectionInformation));
	q->endInsertRows();
}

/**
* Removes all nodes from the table model.
*/
void DeviceDetailsChannelTableModelPrivate::removeAllNodes()
{
	Q_Q(DeviceDetailsChannelTableModel);
	q->beginResetModel();
	_nodes.clear();
	q->endResetModel();
}

/**
* Gets the model data at a specific row index.
*/
TableRowEntry const& DeviceDetailsChannelTableModelPrivate::tableDataAtRow(int row) const
{
	return _nodes.at(row);
}

/**
* Gets all user edits.
* @return The edits in a QMap. The key indicates the node the edit has been made on, the key of the interal QMap is the type of data changed by the user.
*/
QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsChannelTableModelColumn, QVariant>*> DeviceDetailsChannelTableModelPrivate::getChanges() const
{
	return _hasChangesMap;
}

/**
* Resets all changes. That have been made by the user.
*/
void DeviceDetailsChannelTableModelPrivate::resetChangedData()
{
	Q_Q(DeviceDetailsChannelTableModel);
	q->beginResetModel();
	qDeleteAll(_hasChangesMap);
	_hasChangesMap.clear();
	q->endResetModel();
}

/**
* Updates the channel connection data of an entity and updates the view.
*/
void DeviceDetailsChannelTableModelPrivate::channelConnectionsUpdate(la::avdecc::UniqueIdentifier const& entityId)
{
	Q_Q(DeviceDetailsChannelTableModel);
	auto row = 0;
	auto updateModel = false;
	for (auto& node : _nodes)
	{
		auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
		if (node.connectionInformation->sourceClusterChannelInfo->direction == avdecc::ChannelConnectionDirection::OutputToInput)
		{
			// update the node connection information (forward)
			node.connectionInformation = channelConnectionManager.getChannelConnections(node.connectionInformation->sourceEntityId, *node.connectionInformation->sourceClusterChannelInfo);
			updateModel = true;
		}
		else
		{
			// if either the connectionInfo source or one of the targets is the entity that the call refers to...
			if (node.connectionInformation->sourceEntityId == entityId || std::find_if(node.connectionInformation->targets.begin(), node.connectionInformation->targets.end(), [entityId](std::shared_ptr<avdecc::TargetConnectionInformation>& targetInfo) { return (targetInfo->targetEntityId == entityId); }) != node.connectionInformation->targets.end())
			{
				// ...update the node connection information (reverse)
				node.connectionInformation = channelConnectionManager.getChannelConnectionsReverse(node.connectionInformation->sourceEntityId, *node.connectionInformation->sourceClusterChannelInfo);
				updateModel = true;
			}
		}

		if (updateModel)
		{
			// trigger updating the model
			auto indexConnection = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::Connection), QModelIndex());
			if (indexConnection.isValid())
				q->dataChanged(indexConnection, indexConnection, { Qt::DisplayRole });
			auto indexConnectionStatus = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::ConnectionStatus), QModelIndex());
			if (indexConnectionStatus.isValid())
				q->dataChanged(indexConnectionStatus, indexConnectionStatus, { Qt::DisplayRole });
		}

		row++;
	}
}

/**
* Updates the channel connection data of all changed channels and updates the view.
*/
void DeviceDetailsChannelTableModelPrivate::channelConnectionsUpdate(std::set<std::pair<la::avdecc::UniqueIdentifier, avdecc::ChannelIdentification>> channels)
{
	Q_Q(DeviceDetailsChannelTableModel);
	auto row = 0;
	auto updateModel = false;
	for (auto& node : _nodes)
	{
		auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
		if (node.connectionInformation->sourceClusterChannelInfo->direction == avdecc::ChannelConnectionDirection::OutputToInput)
		{
			// in the case of the talker we can't know which channels have to be updated without getting the changes from the ChannelConnectionManager
			// therefor all talkers are updated for now. To improve on this talker connections would need to be cached in the ChannelConnectionManager like the listenerChannelConnections are cached.
			node.connectionInformation = channelConnectionManager.getChannelConnections(node.connectionInformation->sourceEntityId, *node.connectionInformation->sourceClusterChannelInfo);
			updateModel = true;
		}
		else
		{
			if (channels.find(std::make_pair(node.connectionInformation->sourceEntityId, *node.connectionInformation->sourceClusterChannelInfo)) != channels.end())
			{
				// update the node connection information (reverse)
				node.connectionInformation = channelConnectionManager.getChannelConnectionsReverse(node.connectionInformation->sourceEntityId, *node.connectionInformation->sourceClusterChannelInfo);
				updateModel = true;
			}
		}

		if (updateModel)
		{
			// trigger updating the model
			auto indexConnection = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::Connection), QModelIndex());
			if (indexConnection.isValid())
				q->dataChanged(indexConnection, indexConnection, { Qt::DisplayRole });
			auto indexConnectionStatus = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::ConnectionStatus), QModelIndex());
			if (indexConnectionStatus.isValid())
				q->dataChanged(indexConnectionStatus, indexConnectionStatus, { Qt::DisplayRole });
		}

		row++;
	}
}

/**
* Update an audio cluster name.
*/
void DeviceDetailsChannelTableModelPrivate::updateAudioClusterName(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& /*audioClusterName*/)
{
	Q_Q(DeviceDetailsChannelTableModel);
	int row = 0;
	for (auto& node : _nodes)
	{
		if (!_hasChangesMap.contains(node.connectionInformation->sourceClusterChannelInfo->clusterIndex) || !_hasChangesMap.value(node.connectionInformation->sourceClusterChannelInfo->clusterIndex)->contains(DeviceDetailsChannelTableModelColumn::ChannelName))
		{
			if (entityID == node.connectionInformation->sourceEntityId && configurationIndex == node.connectionInformation->sourceClusterChannelInfo->configurationIndex && audioClusterIndex == node.connectionInformation->sourceClusterChannelInfo->clusterIndex)
			{
				auto indexConnectionStatus = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::ChannelName), QModelIndex());
				if (indexConnectionStatus.isValid())
					q->dataChanged(indexConnectionStatus, indexConnectionStatus, QVector<int>(Qt::DisplayRole));
			}
		}
		row++;
	}
}

/**
* Gets the row count of the table.
*/
int DeviceDetailsChannelTableModelPrivate::rowCount() const
{
	return static_cast<int>(_nodes.size());
}

/**
* Gets the column count of the table.
*/
int DeviceDetailsChannelTableModelPrivate::columnCount() const
{
	return 3;
}

/**
* Gets the data of a table cell.
*/
QVariant DeviceDetailsChannelTableModelPrivate::data(QModelIndex const& index, int role) const
{
	auto const column = static_cast<DeviceDetailsChannelTableModelColumn>(index.column());
	if (role == Qt::TextAlignmentRole)
	{
		return Qt::AlignAbsolute;
	}
	switch (column)
	{
		case DeviceDetailsChannelTableModelColumn::ChannelName:
		{
			auto const& connectionInfo = _nodes.at(index.row()).connectionInformation;
			if (role == Qt::DisplayRole || role == Qt::EditRole)
			{
				if (_hasChangesMap.contains(connectionInfo->sourceClusterChannelInfo->clusterIndex) && _hasChangesMap.value(connectionInfo->sourceClusterChannelInfo->clusterIndex)->contains(DeviceDetailsChannelTableModelColumn::ChannelName))
				{
					return _hasChangesMap.value(connectionInfo->sourceClusterChannelInfo->clusterIndex)->value(DeviceDetailsChannelTableModelColumn::ChannelName);
				}
				else
				{
					try
					{
						auto const& controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(connectionInfo->sourceEntityId);
						if (controlledEntity)
						{
							auto const configurationIndex = connectionInfo->sourceClusterChannelInfo->configurationIndex;
							auto const streamPortIndex = connectionInfo->sourceClusterChannelInfo->streamPortIndex;
							auto const clusterIndex = connectionInfo->sourceClusterChannelInfo->clusterIndex;
							auto const baseCluster = connectionInfo->sourceClusterChannelInfo->baseCluster;
							if (connectionInfo->sourceClusterChannelInfo->direction == avdecc::ChannelConnectionDirection::InputToOutput)
							{
								if (streamPortIndex)
								{
									auto const& streamPortInput = controlledEntity->getStreamPortInputNode(configurationIndex, *streamPortIndex);
									auto const& audioClusters = streamPortInput.audioClusters;
									auto audioClusterIt = audioClusters.find(clusterIndex);
									if (audioClusterIt != audioClusters.end())
									{
										return hive::modelsLibrary::helper::objectName(controlledEntity.get(), audioClusterIt->second);
									}
								}
							}
							else
							{
								if (streamPortIndex)
								{
									auto const& streamPortOutput = controlledEntity->getStreamPortOutputNode(configurationIndex, *streamPortIndex);
									auto const& audioClusters = streamPortOutput.audioClusters;
									auto audioClusterIt = audioClusters.find(clusterIndex);
									if (audioClusterIt != audioClusters.end())
									{
										return hive::modelsLibrary::helper::objectName(controlledEntity.get(), audioClusterIt->second);
									}
								}
							}
						}
					}
					catch (...)
					{
						// in case something went wrong/was invalid we cannot provide a valid name string
						break;
					}
				}
			}
			break;
		}
		case DeviceDetailsChannelTableModelColumn::Connection:
		{
			if (role == Qt::DisplayRole)
			{
				try
				{
					QVariantList connectionLines;
					auto const& connectionInfo = _nodes.at(index.row()).connectionInformation;
					for (auto const& connection : connectionInfo->targets)
					{
						for (auto const& clusterKV : connection->targetClusterChannels)
						{
							auto const& targetEntityId = connection->targetEntityId;
							auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
							auto controlledEntity = manager.getControlledEntity(targetEntityId);
							if (!controlledEntity)
							{
								continue;
							}
							auto const& entityNode = controlledEntity->getEntityNode();
							if (entityNode.dynamicModel)
							{
								auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);
								QString clusterName;
								if (connectionInfo->sourceClusterChannelInfo->direction == avdecc::ChannelConnectionDirection::OutputToInput)
								{
									clusterName = hive::modelsLibrary::helper::objectName(controlledEntity.get(), (configurationNode.audioUnits.at(connection->targetAudioUnitIndex).streamPortInputs.at(connection->targetStreamPortIndex).audioClusters.at(clusterKV.first + connection->targetBaseCluster)));
								}
								else
								{
									clusterName = hive::modelsLibrary::helper::objectName(controlledEntity.get(), (configurationNode.audioUnits.at(connection->targetAudioUnitIndex).streamPortOutputs.at(connection->targetStreamPortIndex).audioClusters.at(clusterKV.first + connection->targetBaseCluster)));
								}

								if (connection->isSourceRedundant && connection->isTargetRedundant)
								{
									connectionLines.append(QString(clusterName).append(": ").append(hive::modelsLibrary::helper::smartEntityName(*controlledEntity.get())).append(" (Prim)"));

									auto const& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
									std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> redundantOutputs;
									std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> redundantInputs;
									if (connectionInfo->sourceClusterChannelInfo->direction == avdecc::ChannelConnectionDirection::OutputToInput)
									{
										redundantOutputs = channelConnectionManager.getRedundantStreamOutputsForPrimary(connectionInfo->sourceEntityId, connection->sourceStreamIndex);
										redundantInputs = channelConnectionManager.getRedundantStreamInputsForPrimary(connection->targetEntityId, connection->targetStreamIndex);
									}
									else
									{
										redundantOutputs = channelConnectionManager.getRedundantStreamInputsForPrimary(connectionInfo->sourceEntityId, connection->sourceStreamIndex);
										redundantInputs = channelConnectionManager.getRedundantStreamOutputsForPrimary(connection->targetEntityId, connection->targetStreamIndex);
									}
									auto itOutputs = redundantOutputs.begin();
									auto itInputs = redundantInputs.begin();

									if (itOutputs != redundantOutputs.end() && itInputs != redundantInputs.end())
									{
										// skip primary
										itOutputs++;
										itInputs++;
									}

									while (itOutputs != redundantOutputs.end() && itInputs != redundantInputs.end())
									{
										connectionLines.append(QString(clusterName).append(": ").append(hive::modelsLibrary::helper::smartEntityName(*controlledEntity.get())).append(" (Sec)"));

										itOutputs++;
										itInputs++;
									}
								}
								else
								{
									connectionLines.append(QString(clusterName).append(": ").append(hive::modelsLibrary::helper::smartEntityName(*controlledEntity.get())));
								}
							}
						}
					}
					return connectionLines;
				}
				catch (...)
				{
					return {};
				}
			}
			break;
		}
		case DeviceDetailsChannelTableModelColumn::ConnectionStatus:
		{
			if (role == Qt::DisplayRole)
			{
				try
				{
					QVariantList connectionStates;
					auto const& connectionInfo = _nodes.at(index.row()).connectionInformation;
					for (auto const& connection : connectionInfo->targets)
					{
						QVariantList connectionStatesTmp;
						auto talkerEntityId = la::avdecc::UniqueIdentifier::getNullUniqueIdentifier();
						auto listenerEntityId = la::avdecc::UniqueIdentifier::getNullUniqueIdentifier();
						auto talkerStreamIndex = la::avdecc::entity::model::StreamIndex{};
						auto listenerStreamIndex = la::avdecc::entity::model::StreamIndex{};

						if (connectionInfo->sourceClusterChannelInfo->direction == avdecc::ChannelConnectionDirection::OutputToInput)
						{
							talkerEntityId = connectionInfo->sourceEntityId;
							listenerEntityId = connection->targetEntityId;
							talkerStreamIndex = connection->sourceStreamIndex;
							listenerStreamIndex = connection->targetStreamIndex;
						}
						else
						{
							talkerEntityId = connection->targetEntityId;
							listenerEntityId = connectionInfo->sourceEntityId;
							talkerStreamIndex = connection->targetStreamIndex;
							listenerStreamIndex = connection->sourceStreamIndex;
						}

						auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
						auto talkerEntity = manager.getControlledEntity(talkerEntityId);
						auto listenerEntity = manager.getControlledEntity(listenerEntityId);

						if (!talkerEntity || !listenerEntity)
						{
							continue;
						}
						auto const& talkerEntityNode = talkerEntity->getEntityNode();
						auto const& listenerEntityNode = listenerEntity->getEntityNode();

						if (talkerEntityNode.dynamicModel && listenerEntityNode.dynamicModel)
						{
							{
								auto status = calculateConnectionStatus(talkerEntityId, talkerStreamIndex, listenerEntityId, listenerStreamIndex);

								connectionStatesTmp.append(QVariant::fromValue(status));
							}

							if (connection->isSourceRedundant && connection->isTargetRedundant)
							{
								auto const& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();

								auto redundantOutputs = channelConnectionManager.getRedundantStreamOutputsForPrimary(talkerEntityId, talkerStreamIndex);
								auto redundantInputs = channelConnectionManager.getRedundantStreamInputsForPrimary(listenerEntityId, listenerStreamIndex);

								auto itOutputs = redundantOutputs.begin();
								auto itInputs = redundantInputs.begin();

								if (itOutputs != redundantOutputs.end() && itInputs != redundantInputs.end())
								{
									// skip primary
									itOutputs++;
									itInputs++;
								}
								while (itOutputs != redundantOutputs.end() && itInputs != redundantInputs.end())
								{
									auto status = calculateConnectionStatus(talkerEntityId, itOutputs->first, listenerEntityId, itInputs->first);
									connectionStatesTmp.append(QVariant::fromValue(status));

									itOutputs++;
									itInputs++;
								}
							}

							// add the states for each cluster channel
							for (uint32_t i = 0; i < connection->targetClusterChannels.size(); i++)
							{
								connectionStates << connectionStatesTmp;
							}
						}
					}
					return connectionStates;
				}
				catch (...)
				{
					return {};
				}
			}
			break;
		}
		default:
			break;
	}

	return QVariant();
}

/**
* Sets the data in the internal map to store the changes. The map data has the higher prio in the data() method.
*/
bool DeviceDetailsChannelTableModelPrivate::setData(QModelIndex const& index, QVariant const& value, int role)
{
	Q_Q(DeviceDetailsChannelTableModel);
	if (index.isValid() && role == Qt::EditRole)
	{
		auto const column = static_cast<DeviceDetailsChannelTableModelColumn>(index.column());
		switch (column)
		{
			case DeviceDetailsChannelTableModelColumn::ChannelName:
			{
				if (value.toString() != data(index, role))
				{
					auto const sourceClusterIndex = _nodes.at(index.row()).connectionInformation->sourceClusterChannelInfo->clusterIndex;
					if (!_hasChangesMap.contains(sourceClusterIndex))
					{
						_hasChangesMap.insert(sourceClusterIndex, new QMap<DeviceDetailsChannelTableModelColumn, QVariant>());
					}
					_hasChangesMap.value(sourceClusterIndex)->insert(DeviceDetailsChannelTableModelColumn::ChannelName, value.toString());
					emit q->dataEdited();
				}
				break;
			}
			default:
				break;
		}
		emit q->dataChanged(index, index);
		return true;
	}
	return false;
}

/**
* Gets the header data of the table.
*/
QVariant DeviceDetailsChannelTableModelPrivate::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		if (role == Qt::DisplayRole)
		{
			switch (static_cast<DeviceDetailsChannelTableModelColumn>(section))
			{
				case DeviceDetailsChannelTableModelColumn::ChannelName:
					return "Channel Name";
				case DeviceDetailsChannelTableModelColumn::Connection:
					return "Connection";
				case DeviceDetailsChannelTableModelColumn::ConnectionStatus:
					return "";
				default:
					break;
			}
		}
	}
	else if (orientation == Qt::Vertical)
	{
		if (role == Qt::DisplayRole)
		{
			if (static_cast<size_t>(section) >= _nodes.size())
			{
				return {};
			}
			return _nodes.at(section).connectionInformation->sourceClusterChannelInfo->clusterIndex - *_nodes.at(section).connectionInformation->sourceClusterChannelInfo->baseCluster + 1; // +1 to make the row count start at 1 instead of 0.
		}
	}

	return {};
}

/**
* Gets the flags of the table cells.
*/
Qt::ItemFlags DeviceDetailsChannelTableModelPrivate::flags(QModelIndex const& index) const
{
	if (index.column() == static_cast<int>(DeviceDetailsChannelTableModelColumn::ChannelName))
	{
		return Qt::ItemIsEnabled | Qt::ItemIsEditable;
	}
	return Qt::ItemIsEnabled;
}


/* ************************************************************ */
/* DeviceDetailsChannelTableModel                               */
/* ************************************************************ */
/**
* Constructor.
* @param parent The parent object.
*/
DeviceDetailsChannelTableModel::DeviceDetailsChannelTableModel(QObject* parent)
	: QAbstractTableModel(parent)
	, d_ptr(new DeviceDetailsChannelTableModelPrivate(this))
{
}

/**
* Destructor. Deletes the private implementation pointer.
*/
DeviceDetailsChannelTableModel::~DeviceDetailsChannelTableModel()
{
	delete d_ptr;
}

/**
* Gets the row count.
*/
int DeviceDetailsChannelTableModel::rowCount(QModelIndex const& /*parent*/) const
{
	Q_D(const DeviceDetailsChannelTableModel);
	return d->rowCount();
}

/**
* Gets the column count.
*/
int DeviceDetailsChannelTableModel::columnCount(QModelIndex const& /*parent*/) const
{
	Q_D(const DeviceDetailsChannelTableModel);
	return d->columnCount();
}

/**
* Gets the data of a cell.
*/
QVariant DeviceDetailsChannelTableModel::data(QModelIndex const& index, int role) const
{
	Q_D(const DeviceDetailsChannelTableModel);
	return d->data(index, role);
}

/**
* Sets the data of a cell.
*/
bool DeviceDetailsChannelTableModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	Q_D(DeviceDetailsChannelTableModel);
	return d->setData(index, value, role);
}

/**
* Gets the header data for the table.
*/
QVariant DeviceDetailsChannelTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_D(const DeviceDetailsChannelTableModel);
	return d->headerData(section, orientation, role);
}

/**
* Gets the flags of a cell.
*/
Qt::ItemFlags DeviceDetailsChannelTableModel::flags(QModelIndex const& index) const
{
	Q_D(const DeviceDetailsChannelTableModel);
	return d->flags(index);
}

/**
* Adds a node to the table.
* @param audioClusterNode: The audio cluster node to display.
*/
void DeviceDetailsChannelTableModel::addNode(std::shared_ptr<avdecc::TargetConnectionInformations> const& connectionInformation)
{
	Q_D(DeviceDetailsChannelTableModel);
	return d->addNode(connectionInformation);
}

/**
* Gets the changes made by the user.
* @return The edits in a QMap. The key indicates the node the edit has been made on, the key of the interal QMap is the type of data changed by the user.
*/
QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsChannelTableModelColumn, QVariant>*> DeviceDetailsChannelTableModel::getChanges() const
{
	Q_D(const DeviceDetailsChannelTableModel);
	return d->getChanges();
}

/**
* Resets the changes that the user made.
*/
void DeviceDetailsChannelTableModel::resetChangedData()
{
	Q_D(DeviceDetailsChannelTableModel);
	return d->resetChangedData();
}

/**
* Update the displayed data for the channel connection columns.
*/
void DeviceDetailsChannelTableModel::channelConnectionsUpdate(la::avdecc::UniqueIdentifier const& entityId)
{
	Q_D(DeviceDetailsChannelTableModel);
	d->channelConnectionsUpdate(entityId);
}

/**
* Update the displayed data for the channel connection columns.
*/
void DeviceDetailsChannelTableModel::channelConnectionsUpdate(std::set<std::pair<la::avdecc::UniqueIdentifier, avdecc::ChannelIdentification>> channels)
{
	Q_D(DeviceDetailsChannelTableModel);
	d->channelConnectionsUpdate(channels);
}

/**
* Update an audio cluster name.
*/
void DeviceDetailsChannelTableModel::updateAudioClusterName(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& audioClusterName)
{
	Q_D(DeviceDetailsChannelTableModel);
	d->updateAudioClusterName(entityID, configurationIndex, audioClusterIndex, audioClusterName);
}

/**
* Clears the table model.
*/
void DeviceDetailsChannelTableModel::removeAllNodes()
{
	Q_D(DeviceDetailsChannelTableModel);
	return d->removeAllNodes();
}

/**
* Gets the data for a specific row.
* @param  The row index of the data to get.
* @return The audio cluster node object at the given row index.
*/
TableRowEntry const& DeviceDetailsChannelTableModel::tableDataAtRow(int row) const
{
	Q_D(const DeviceDetailsChannelTableModel);
	return d->tableDataAtRow(row);
}

/* ************************************************************ */
/* ConnectionStateItemDelegate                                  */
/* ************************************************************ */

/**
* QAbstractItemDelegate paint implentation.
* Paints the connection status icons into the corresponding column.
*/
void ConnectionStateItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	auto const* const model = static_cast<DeviceDetailsChannelTableModel const*>(index.model());
	auto modelData = model->data(index).toList(); // data returns a list of DeviceDetailsChannelTableModel::ConnectionStatus to decide which connection icon to render.

	QFontMetrics fm(option.fontMetrics);
	int fontPixelHeight = fm.height();
	int innerRow = 0;

	int circleDiameter = fontPixelHeight;

	auto const backgroundColor = index.data(Qt::BackgroundRole);
	if (!backgroundColor.isNull())
	{
		painter->fillRect(option.rect, backgroundColor.value<QColor>());
	}

	int margin = ConnectionStateItemDelegate::Margin;
	for (auto const& connection : modelData)
	{
		DeviceDetailsChannelTableModel::ConnectionStatus status = connection.value<DeviceDetailsChannelTableModel::ConnectionStatus>();

		QRect iconDrawRect(option.rect.left() + (option.rect.width() - circleDiameter) / 2.0f, option.rect.top() + margin + innerRow * (fontPixelHeight + margin), circleDiameter, circleDiameter);

		connectionMatrix::paintHelper::drawCapabilities(painter, iconDrawRect, status.type, status.state, status.flags, true);

		innerRow++;
	}
}

/**
* QAbstractItemDelegate sizeHint implentation.
* @return Default size.
*/
QSize ConnectionStateItemDelegate::sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	QFontMetrics fm(option.fontMetrics);
	int fontPixelHeight = fm.height();
	auto const* const model = static_cast<DeviceDetailsChannelTableModel const*>(index.model());
	auto modelData = model->data(index).toList();

	int margin = ConnectionStateItemDelegate::Margin;
	int totalHeight = margin;
	totalHeight += (fontPixelHeight + margin) * modelData.size();
	return QSize(40, totalHeight);
}

/* ************************************************************ */
/* ConnectionInfoItemDelegate                                  */
/* ************************************************************ */

/**
* QAbstractItemDelegate paint implentation.
* Paints the connection names with certain spacing.
*/
void ConnectionInfoItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	auto const* const model = static_cast<DeviceDetailsChannelTableModel const*>(index.model());
	auto modelData = model->data(index).toList(); // data returns a string list to display for connection info

	QFontMetrics fm(option.fontMetrics);
	int fontPixelHeight = fm.height();
	int innerRow = 0;

	auto const backgroundColor = index.data(Qt::BackgroundRole);
	if (!backgroundColor.isNull())
	{
		painter->fillRect(option.rect, backgroundColor.value<QColor>());
	}

	int margin = ConnectionStateItemDelegate::Margin;
	for (auto const& connection : modelData)
	{
		QString line = connection.toString();

		QRect textDrawRect(option.rect.left() + margin / 2, option.rect.top() + margin + innerRow * (fontPixelHeight + margin), option.rect.width(), fontPixelHeight);
		painter->drawText(textDrawRect, line);

		innerRow++;
	}
}

/**
* QAbstractItemDelegate sizeHint implentation.
* @return Default size.
*/
QSize ConnectionInfoItemDelegate::sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	QFontMetrics fm(option.fontMetrics);
	int fontPixelHeight = fm.height();
	auto const* const model = static_cast<DeviceDetailsChannelTableModel const*>(index.model());
	auto modelData = model->data(index).toList();

	int margin = ConnectionStateItemDelegate::Margin;
	int totalHeight = margin;
	totalHeight += (fontPixelHeight + margin) * modelData.size();
	return QSize(350, totalHeight);
}


#include "deviceDetailsChannelTableModel.moc"
