/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "deviceDetailsChannelTableModel.hpp"

#include "avdecc/channelConnectionManager.hpp"

#include <QCoreApplication>
#include <QLayout>
#include <QPainter>

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

#include "ui_deviceDetailsDialog.h"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/controllerManager.hpp"
#include "connectionMatrix/paintHelper.hpp"


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
		return (listenerNode->dynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::Connected) && (listenerNode->dynamicModel->connectionState.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionState.talkerStream.streamIndex == talkerNode->descriptorIndex);
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
		return (listenerNode->dynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::FastConnecting) && (listenerNode->dynamicModel->connectionState.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionState.talkerStream.streamIndex == talkerNode->descriptorIndex);
	}

	return false;
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
DeviceDetailsChannelTableModel::ConnectionStatus connectionStatus(la::avdecc::UniqueIdentifier const& talkerEntityId, la::avdecc::entity::model::StreamIndex talkerStreamIndex, bool talkerStreamRedundant, la::avdecc::UniqueIdentifier const& listenerEntityId, la::avdecc::entity::model::StreamIndex listenerStreamIndex, bool listenerStreamRedundant) noexcept
{
	if (talkerEntityId == listenerEntityId)
		return DeviceDetailsChannelTableModel::ConnectionStatus::None;

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
				return la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerNode.dynamicModel->streamInfo.streamFormat, talkerNode.dynamicModel->streamInfo.streamFormat);
			};
			auto const computeDomainCompatible = [&talkerEntity, &listenerEntity, &talkerEntityNode, &listenerEntityNode](auto const talkerAvbInterfaceIndex, auto const listenerAvbInterfaceIndex)
			{
				try
				{
					// Get the AvbInterface associated to the streams
					auto const& talkerAvbInterfaceNode = talkerEntity->getAvbInterfaceNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerAvbInterfaceIndex);
					auto const& listenerAvbInterfaceNode = listenerEntity->getAvbInterfaceNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerAvbInterfaceIndex);

					// Check both have the same grandmaster
					return talkerAvbInterfaceNode.dynamicModel->avbInfo.gptpGrandmasterID == listenerAvbInterfaceNode.dynamicModel->avbInfo.gptpGrandmasterID;
				}
				catch (...)
				{
					return false;
				}
			};
			enum class ConnectState
			{
				NotConnected = 0,
				FastConnecting,
				Connected,
			};
			auto const computeCapabilities = [](ConnectState const connectState, bool const areAllConnected, bool const isFormatCompatible, bool const isDomainCompatible)
			{
				auto caps{ DeviceDetailsChannelTableModel::ConnectionStatus::Connectable };

				if (!isDomainCompatible)
					caps |= DeviceDetailsChannelTableModel::ConnectionStatus::WrongDomain;

				if (!isFormatCompatible)
					caps |= DeviceDetailsChannelTableModel::ConnectionStatus::WrongFormat;

				if (connectState != ConnectState::NotConnected)
				{
					if (areAllConnected)
						caps |= DeviceDetailsChannelTableModel::ConnectionStatus::Connected;
					else if (connectState == ConnectState::FastConnecting)
						caps |= DeviceDetailsChannelTableModel::ConnectionStatus::FastConnecting;
					else
						caps |= DeviceDetailsChannelTableModel::ConnectionStatus::PartiallyConnected;
				}

				return caps;
			};

			{
				la::avdecc::controller::model::StreamOutputNode const* talkerNode{ nullptr };
				la::avdecc::controller::model::StreamInputNode const* listenerNode{ nullptr };
				if (talkerEntityNode.dynamicModel && listenerEntityNode.dynamicModel)
				{
					talkerNode = &talkerEntity->getStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerStreamIndex);
					listenerNode = &listenerEntity->getStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerStreamIndex);

					// Get connected state
					auto const areConnected = isStreamConnected(talkerEntityId, talkerNode, listenerNode);
					auto const fastConnecting = isStreamFastConnecting(talkerEntityId, talkerNode, listenerNode);
					auto const connectState = areConnected ? ConnectState::Connected : (fastConnecting ? ConnectState::FastConnecting : ConnectState::NotConnected);

					// Get stream format compatibility
					auto const isFormatCompatible = computeFormatCompatible(*talkerNode, *listenerNode);

					// Get domain compatibility
					auto const isDomainCompatible = computeDomainCompatible(talkerNode->staticModel->avbInterfaceIndex, listenerNode->staticModel->avbInterfaceIndex);

					return computeCapabilities(connectState, areConnected, isFormatCompatible, isDomainCompatible);
				}
			}
		}
	}
	catch (...)
	{
	}

	return DeviceDetailsChannelTableModel::ConnectionStatus::None;
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

	void addNode(avdecc::ConnectionInformation const& connectionInformation);
	void removeAllNodes();
	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsChannelTableModelColumn, QVariant>*> getChanges() const;
	void resetChangedData();

	TableRowEntry const& tableDataAtRow(int row) const;

	void channelConnectionsUpdate(la::avdecc::UniqueIdentifier const& entityId);
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
void DeviceDetailsChannelTableModelPrivate::addNode(avdecc::ConnectionInformation const& connectionInformation)
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
	int row = 0;
	for (auto& node : _nodes)
	{
		auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
		if (node.connectionInformation.forward)
		{
			node.connectionInformation = channelConnectionManager.getChannelConnections(node.connectionInformation.sourceEntityId, *node.connectionInformation.sourceConfigurationIndex, *node.connectionInformation.sourceAudioUnitIndex, *node.connectionInformation.sourceStreamPortIndex, *node.connectionInformation.sourceClusterIndex, *node.connectionInformation.sourceBaseCluster, node.connectionInformation.sourceClusterChannel);
			auto begin = q->index(0, static_cast<int>(DeviceDetailsChannelTableModelColumn::Connection), QModelIndex());
			auto end = q->index(static_cast<int>(_nodes.size()), static_cast<int>(DeviceDetailsChannelTableModelColumn::ConnectionStatus), QModelIndex());
			q->dataChanged(begin, end, QVector<int>(Qt::DisplayRole));
		}
		else
		{
			if (node.connectionInformation.sourceEntityId == entityId)
			{
				node.connectionInformation = channelConnectionManager.getChannelConnectionsReverse(node.connectionInformation.sourceEntityId, *node.connectionInformation.sourceConfigurationIndex, *node.connectionInformation.sourceAudioUnitIndex, *node.connectionInformation.sourceStreamPortIndex, *node.connectionInformation.sourceClusterIndex, *node.connectionInformation.sourceBaseCluster, node.connectionInformation.sourceClusterChannel);
				auto indexConnection = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::Connection), QModelIndex());
				q->dataChanged(indexConnection, indexConnection, QVector<int>(Qt::DisplayRole));
				auto indexConnectionStatus = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::ConnectionStatus), QModelIndex());
				q->dataChanged(indexConnectionStatus, indexConnectionStatus, QVector<int>(Qt::DisplayRole));
			}
		}

		row++;
	}
}

/**
* Update an audio cluster name.
*/
void DeviceDetailsChannelTableModelPrivate::updateAudioClusterName(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& audioClusterName)
{
	Q_Q(DeviceDetailsChannelTableModel);
	int row = 0;
	for (auto& node : _nodes)
	{
		if (!_hasChangesMap.contains(*node.connectionInformation.sourceClusterIndex) || !_hasChangesMap.value(*node.connectionInformation.sourceClusterIndex)->contains(DeviceDetailsChannelTableModelColumn::ChannelName))
		{
			if (entityID == node.connectionInformation.sourceEntityId && configurationIndex == node.connectionInformation.sourceConfigurationIndex && audioClusterIndex == *node.connectionInformation.sourceClusterIndex)
			{
				auto indexConnectionStatus = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::ChannelName), QModelIndex());
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
				if (_hasChangesMap.contains(*connectionInfo.sourceClusterIndex) && _hasChangesMap.value(*connectionInfo.sourceClusterIndex)->contains(DeviceDetailsChannelTableModelColumn::ChannelName))
				{
					return _hasChangesMap.value(*connectionInfo.sourceClusterIndex)->value(DeviceDetailsChannelTableModelColumn::ChannelName);
				}
				else
				{
					try
					{
						auto const& controlledEntity = avdecc::ControllerManager::getInstance().getControlledEntity(connectionInfo.sourceEntityId);
						if (controlledEntity)
						{
							auto const& audioUnit = controlledEntity->getAudioUnitNode(*connectionInfo.sourceConfigurationIndex, *connectionInfo.sourceAudioUnitIndex);
							if (!connectionInfo.forward)
							{
								if (connectionInfo.sourceStreamPortIndex && connectionInfo.sourceClusterIndex && connectionInfo.sourceStreamPortIndex < audioUnit.streamPortInputs.size() && *connectionInfo.sourceClusterIndex - *connectionInfo.sourceBaseCluster < audioUnit.streamPortInputs.at(*connectionInfo.sourceStreamPortIndex).audioClusters.size())
								{
									auto const& audioCluster = audioUnit.streamPortInputs.at(*connectionInfo.sourceStreamPortIndex).audioClusters.at(*connectionInfo.sourceClusterIndex);
									if (audioCluster.dynamicModel)
									{
										return audioCluster.dynamicModel->objectName.data();
									}
								}
							}
							else
							{
								if (connectionInfo.sourceStreamPortIndex && connectionInfo.sourceClusterIndex && connectionInfo.sourceStreamPortIndex < audioUnit.streamPortOutputs.size() && *connectionInfo.sourceClusterIndex - *connectionInfo.sourceBaseCluster < audioUnit.streamPortOutputs.at(*connectionInfo.sourceStreamPortIndex).audioClusters.size())
								{
									auto const& audioCluster = audioUnit.streamPortOutputs.at(*connectionInfo.sourceStreamPortIndex).audioClusters.at(*connectionInfo.sourceClusterIndex);
									if (audioCluster.dynamicModel)
									{
										return audioCluster.dynamicModel->objectName.data();
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
		}
		case DeviceDetailsChannelTableModelColumn::Connection:
		{
			if (role == Qt::DisplayRole)
			{
				QVariantList connectionLines;
				int innerRow = 0;
				auto const& connectionInfo = _nodes.at(index.row()).connectionInformation;
				for (auto const& connectionKV : connectionInfo.deviceConnections)
				{
					for (auto const& streamKV : connectionKV.second->targetStreams)
					{
						for (auto const& clusterKV : streamKV.second->targetClusters)
						{
							auto controlledEntity = avdecc::ControllerManager::getInstance().getControlledEntity(connectionKV.second->entityId);
							if (!controlledEntity)
							{
								continue;
							}
							auto const& entityNode = controlledEntity->getEntityNode();
							if (entityNode.dynamicModel)
							{
								auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);
								QString clusterName;
								if (connectionInfo.forward)
								{
									clusterName = avdecc::helper::objectName(controlledEntity.get(), (configurationNode.audioUnits.at(*streamKV.second->targetAudioUnitIndex).streamPortInputs.at(*streamKV.second->targetStreamPortIndex).audioClusters.at(clusterKV.first + *streamKV.second->targetBaseCluster)));
								}
								else
								{
									clusterName = avdecc::helper::objectName(controlledEntity.get(), (configurationNode.audioUnits.at(*streamKV.second->targetAudioUnitIndex).streamPortOutputs.at(*streamKV.second->targetStreamPortIndex).audioClusters.at(clusterKV.first + *streamKV.second->targetBaseCluster)));
								}

								if (connectionKV.second->isSourceRedundant && streamKV.second->isTargetRedundant)
								{
									connectionLines.append(QString(clusterName).append(": ").append(avdecc::helper::entityName(*controlledEntity.get())).append(" (Prim)"));

									auto const& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
									std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> redundantConnections;
									if (connectionInfo.forward)
									{
										redundantConnections = channelConnectionManager.getRedundantStreamOutputsForPrimary(connectionKV.second->entityId, streamKV.second->sourceStreamIndex);
									}
									else
									{
										redundantConnections = channelConnectionManager.getRedundantStreamInputsForPrimary(connectionKV.second->entityId, streamKV.second->sourceStreamIndex);
									}
									for (auto const& redundantConnectionKV : redundantConnections)
									{
										if (redundantConnectionKV.second->descriptorIndex == streamKV.second->sourceStreamIndex)
										{
											continue; // skip primary
										}
										innerRow++;
										connectionLines.append(QString(clusterName).append(": ").append(avdecc::helper::entityName(*controlledEntity.get())).append(" (Sec)"));
									}
								}
								else
								{
									connectionLines.append(QString(clusterName).append(": ").append(avdecc::helper::entityName(*controlledEntity.get())));
								}
							}
							innerRow++;
						}
					}
				}
				return connectionLines;
			}
		}
		case DeviceDetailsChannelTableModelColumn::ConnectionStatus:
		{
			if (role == Qt::DisplayRole)
			{
				QVariantList connectionStates;
				auto const& connectionInfo = _nodes.at(index.row()).connectionInformation;
				for (auto const& connectionKV : connectionInfo.deviceConnections)
				{
					for (auto const& streamKV : connectionKV.second->targetStreams)
					{
						for (auto const& clusterKV : streamKV.second->targetClusters)
						{
							auto controlledEntity = avdecc::ControllerManager::getInstance().getControlledEntity(connectionKV.second->entityId);
							if (!controlledEntity)
							{
								continue;
							}
							auto const& entityNode = controlledEntity->getEntityNode();
							if (entityNode.dynamicModel)
							{
								auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);

								DeviceDetailsChannelTableModel::ConnectionStatus status;
								if (connectionInfo.forward)
								{
									status = connectionStatus(connectionInfo.sourceEntityId, streamKV.second->sourceStreamIndex, connectionKV.second->isSourceRedundant, connectionKV.second->entityId, streamKV.first, streamKV.second->isTargetRedundant);
								}
								else
								{
									status = connectionStatus(connectionKV.second->entityId, streamKV.first, streamKV.second->isTargetRedundant, connectionInfo.sourceEntityId, streamKV.second->sourceStreamIndex, connectionKV.second->isSourceRedundant);
								}

								connectionStates.append(QVariant::fromValue(status));
								if (connectionKV.second->isSourceRedundant && streamKV.second->isTargetRedundant)
								{
									auto const& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
									std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> redundantOutputs;
									std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> redundantInputs;
									if (connectionInfo.forward)
									{
										redundantOutputs = channelConnectionManager.getRedundantStreamOutputsForPrimary(connectionInfo.sourceEntityId, streamKV.second->sourceStreamIndex);
										redundantInputs = channelConnectionManager.getRedundantStreamInputsForPrimary(connectionKV.second->entityId, streamKV.first);
									}
									else
									{
										redundantOutputs = channelConnectionManager.getRedundantStreamInputsForPrimary(connectionInfo.sourceEntityId, streamKV.second->sourceStreamIndex);
										redundantInputs = channelConnectionManager.getRedundantStreamOutputsForPrimary(connectionKV.second->entityId, streamKV.first);
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
										if (connectionInfo.forward)
										{
											status = connectionStatus(connectionInfo.sourceEntityId, itOutputs->first, connectionKV.second->isSourceRedundant, connectionKV.second->entityId, itInputs->first, streamKV.second->isTargetRedundant);
										}
										else
										{
											status = connectionStatus(connectionKV.second->entityId, itInputs->first, streamKV.second->isTargetRedundant, connectionInfo.sourceEntityId, itOutputs->first, connectionKV.second->isSourceRedundant);
										}
										connectionStates.append(QVariant::fromValue(status));

										itOutputs++;
										itInputs++;
									}
								}
							}
						}
					}
				}
				return connectionStates;
			}
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
				auto const& sourceClusterIndex = _nodes.at(index.row()).connectionInformation.sourceClusterIndex;
				if (!_hasChangesMap.contains(*sourceClusterIndex))
				{
					_hasChangesMap.insert(*sourceClusterIndex, new QMap<DeviceDetailsChannelTableModelColumn, QVariant>());
				}
				_hasChangesMap.value(*sourceClusterIndex)->insert(DeviceDetailsChannelTableModelColumn::ChannelName, value.toString());
				emit q->dataEdited();
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
			return *_nodes.at(section).connectionInformation.sourceClusterIndex - *_nodes.at(section).connectionInformation.sourceBaseCluster + 1; // +1 to make the row count start at 1 instead of 0.
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
int DeviceDetailsChannelTableModel::rowCount(QModelIndex const& parent) const
{
	Q_D(const DeviceDetailsChannelTableModel);
	return d->rowCount();
}

/**
* Gets the column count.
*/
int DeviceDetailsChannelTableModel::columnCount(QModelIndex const& parent) const
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
void DeviceDetailsChannelTableModel::addNode(avdecc::ConnectionInformation const& connectionInformation)
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


void drawConnectionState(DeviceDetailsChannelTableModel::ConnectionStatus status, QPainter* painter, QRect iconDrawRect)
{
	if (la::avdecc::utils::hasFlag(status, DeviceDetailsChannelTableModel::ConnectionStatus::Connected))
	{
		if (la::avdecc::utils::hasFlag(status, DeviceDetailsChannelTableModel::ConnectionStatus::WrongDomain))
		{
			connectionMatrix::drawWrongDomainConnectedStream(painter, iconDrawRect, false);
		}
		else if (la::avdecc::utils::hasFlag(status, DeviceDetailsChannelTableModel::ConnectionStatus::WrongFormat))
		{
			connectionMatrix::drawWrongFormatConnectedStream(painter, iconDrawRect, false);
		}
		else
		{
			connectionMatrix::drawConnectedStream(painter, iconDrawRect, false);
		}
	}
	else if (la::avdecc::utils::hasFlag(status, DeviceDetailsChannelTableModel::ConnectionStatus::FastConnecting))
	{
		if (la::avdecc::utils::hasFlag(status, DeviceDetailsChannelTableModel::ConnectionStatus::WrongDomain))
		{
			connectionMatrix::drawWrongDomainFastConnectingStream(painter, iconDrawRect, false);
		}
		else if (la::avdecc::utils::hasFlag(status, DeviceDetailsChannelTableModel::ConnectionStatus::WrongFormat))
		{
			connectionMatrix::drawWrongFormatFastConnectingStream(painter, iconDrawRect, false);
		}
		else
		{
			connectionMatrix::drawFastConnectingStream(painter, iconDrawRect, false);
		}
	}
	else if (la::avdecc::utils::hasFlag(status, DeviceDetailsChannelTableModel::ConnectionStatus::PartiallyConnected))
	{
		connectionMatrix::drawPartiallyConnectedRedundantNode(painter, iconDrawRect);
	}
	else
	{
		if (la::avdecc::utils::hasFlag(status, DeviceDetailsChannelTableModel::ConnectionStatus::WrongDomain))
		{
			connectionMatrix::drawWrongDomainNotConnectedStream(painter, iconDrawRect, false);
		}
		else if (la::avdecc::utils::hasFlag(status, DeviceDetailsChannelTableModel::ConnectionStatus::WrongFormat))
		{
			connectionMatrix::drawWrongFormatNotConnectedStream(painter, iconDrawRect, false);
		}
		else
		{
			connectionMatrix::drawNotConnectedStream(painter, iconDrawRect, false);
		}
	}
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
	auto modelData = model->data(index).toList(); // data returns a string list to display for connection info and a DeviceDetailsChannelTableModel::ConnectionStatus list for connection state column

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
		drawConnectionState(status, painter, iconDrawRect);

		innerRow++;
	}
}

/**
* QAbstractItemDelegate sizeHint implentation.
* @return Default size.
*/
QSize ConnectionStateItemDelegate::sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	return QSize();
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
	auto modelData = model->data(index).toList(); // data returns a string list to display for connection info and a DeviceDetailsChannelTableModel::ConnectionStatus list for connection state column

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
	auto modelData = model->data(index).toList(); // data returns a string list to display for connection info and a DeviceDetailsChannelTableModel::ConnectionStatus list for connection state column

	int margin = 6;
	int totalHeight = margin;
	auto const& tableData = model->tableDataAtRow(index.row());
	for (auto const& line : modelData)
	{
		totalHeight += fontPixelHeight + margin;
	}
	return QSize(350, totalHeight);
}


#include "deviceDetailsChannelTableModel.moc"
