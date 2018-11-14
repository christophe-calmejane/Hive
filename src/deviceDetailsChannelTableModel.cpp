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

	void registerUiWidget(QTableView* ui);
	QTableView* getUiWidget() const;

	void addNode(la::avdecc::controller::model::AudioClusterNode const& streamNode, uint16_t audioClusterNodeChannel, avdecc::ConnectionInformation const& connectionInformation);
	void removeAllNodes();
	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsChannelTableModelColumn, QVariant>*> getChanges() const;
	void resetChangedData();

	TableRowEntry const& DeviceDetailsChannelTableModelPrivate::tableDataAtRow(int row) const;

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
QTableView* DeviceDetailsChannelTableModelPrivate::getUiWidget() const
{
	return _ui;
}

/**
* Adds a node to the table model. Doesn't check for duplicates or correct order.
* @param audioClusterNode The node to add to this model.
*/
void DeviceDetailsChannelTableModelPrivate::registerUiWidget(QTableView* ui)
{
	_ui = ui;
	_ui->setItemDelegateForColumn(static_cast<int>(DeviceDetailsChannelTableModelColumn::ConnectionStatus), new ConnectionStateItemDelegate());
}

/**
* Adds a node to the table model. Doesn't check for duplicates or correct order.
* @param audioClusterNode The node to add to this model.
*/
void DeviceDetailsChannelTableModelPrivate::addNode(la::avdecc::controller::model::AudioClusterNode const& audioClusterNode, uint16_t audioClusterNodeChannel, avdecc::ConnectionInformation const& connectionInformation)
{
	Q_Q(DeviceDetailsChannelTableModel);

	q->beginInsertRows(QModelIndex(), _nodes.size(), _nodes.size());
	_nodes.push_back(TableRowEntry(audioClusterNode, audioClusterNodeChannel, connectionInformation));
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
			node.connectionInformation = channelConnectionManager.getChannelConnections(node.connectionInformation.sourceEntityId, node.connectionInformation.sourceConfigurationIndex.value(), node.connectionInformation.sourceAudioUnitIndex.value(), node.connectionInformation.sourceStreamPortIndex.value(), node.connectionInformation.sourceClusterIndex.value(), node.connectionInformation.sourceBaseCluster.value(), node.connectionInformation.sourceClusterChannel);
			QModelIndex begin = q->index(0, static_cast<int>(DeviceDetailsChannelTableModelColumn::Connection), QModelIndex());
			QModelIndex end = q->index(_nodes.size(), static_cast<int>(DeviceDetailsChannelTableModelColumn::ConnectionStatus), QModelIndex());
			q->dataChanged(begin, end, QVector<int>(Qt::DisplayRole));
			break;
		}
		else
		{
			if (node.connectionInformation.sourceEntityId == entityId)
			{
				node.connectionInformation = channelConnectionManager.getChannelConnectionsReverse(node.connectionInformation.sourceEntityId, node.connectionInformation.sourceConfigurationIndex.value(), node.connectionInformation.sourceAudioUnitIndex.value(), node.connectionInformation.sourceStreamPortIndex.value(), node.connectionInformation.sourceClusterIndex.value(), node.connectionInformation.sourceBaseCluster.value(), node.connectionInformation.sourceClusterChannel);
				QModelIndex indexConnection = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::Connection), QModelIndex());
				q->dataChanged(indexConnection, indexConnection, QVector<int>(Qt::DisplayRole));
				QModelIndex indexConnectionStatus = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::ConnectionStatus), QModelIndex());
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
		if (!_hasChangesMap.contains(node.audioClusterNode.descriptorIndex) || !_hasChangesMap.value(node.audioClusterNode.descriptorIndex)->contains(DeviceDetailsChannelTableModelColumn::ChannelName))
		{
			if (node.audioClusterNode.dynamicModel &&
				entityID == node.connectionInformation.sourceEntityId && configurationIndex == node.connectionInformation.sourceConfigurationIndex && audioClusterIndex == node.audioClusterNode.descriptorIndex)
			{
				node.audioClusterNode.dynamicModel->objectName = la::avdecc::entity::model::AvdeccFixedString(audioClusterName.toStdString());

				QModelIndex indexConnectionStatus = q->index(row, static_cast<int>(DeviceDetailsChannelTableModelColumn::ChannelName), QModelIndex());
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
		return Qt::AlignTop;
	}
	switch (column)
	{
		/*case DeviceDetailsChannelTableModelColumn::ChannelNumber:
		if (role == Qt::FontRole) {
			QFont font;
			return font;
		}
		else if (role == Qt::DisplayRole) {
			return _nodes.at(index.row()).first.descriptorIndex;
		}*/
		case DeviceDetailsChannelTableModelColumn::ChannelName:
		{
			bool hasChanges = false;
			if (_hasChangesMap.contains(_nodes.at(index.row()).audioClusterNode.descriptorIndex) && _hasChangesMap.value(_nodes.at(index.row()).audioClusterNode.descriptorIndex)->contains(DeviceDetailsChannelTableModelColumn::ChannelName))
			{
				hasChanges = true;
			}
			if (role == Qt::FontRole && hasChanges)
			{
				/*QFont font;
				font.setItalic(true);
				return font;*/
			}
			else if (role == Qt::DisplayRole || role == Qt::EditRole)
			{
				if (hasChanges)
				{
					return _hasChangesMap.value(_nodes.at(index.row()).audioClusterNode.descriptorIndex)->value(DeviceDetailsChannelTableModelColumn::ChannelName);
				}
				else
				{
					if(_nodes.at(index.row()).audioClusterNode.dynamicModel)
					{
						return _nodes.at(index.row()).audioClusterNode.dynamicModel->objectName.data();
					}
				}
			}
		}
		case DeviceDetailsChannelTableModelColumn::Connection:
		{
			if (role == Qt::DisplayRole)
			{
				int innerRow = 0;
				QString connectionsFormatted;
				avdecc::ConnectionInformation info = _nodes.at(index.row()).connectionInformation;
				for (auto const& connectionKV : info.deviceConnections)
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
							if (entityNode.dynamicModel) {
								auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);
								QString clusterName;
								if (info.forward)
								{
									clusterName = avdecc::helper::objectName(controlledEntity.get(), (configurationNode.audioUnits.at(streamKV.second->targetAudioUnitIndex.value()).streamPortInputs.at(streamKV.second->targetStreamPortIndex.value()).audioClusters.at(clusterKV.first + streamKV.second->targetBaseCluster.value())));
								}
								else
								{
									clusterName = avdecc::helper::objectName(controlledEntity.get(), (configurationNode.audioUnits.at(streamKV.second->targetAudioUnitIndex.value()).streamPortOutputs.at(streamKV.second->targetStreamPortIndex.value()).audioClusters.at(clusterKV.first + streamKV.second->targetBaseCluster.value())));
								}

								// add new line for every further entry.
								if (innerRow > 0)
								{
									connectionsFormatted.append("\n\n");
								}

								// format the cluster name + controller name.
								connectionsFormatted.append(clusterName.append(": ").append(avdecc::helper::entityName(*controlledEntity.get())));
							}
							innerRow++;
						}
					}
				}
				return connectionsFormatted;
			}
		}
		case DeviceDetailsChannelTableModelColumn::ConnectionStatus:
		{
			if (role == Qt::DisplayRole)
			{
				//return _nodes.at(index.row()).;
				return QVariant();
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
				if (_nodes.at(index.row()).audioClusterNode.dynamicModel && value.toString() != _nodes.at(index.row()).audioClusterNode.dynamicModel->objectName.data())
				{
					if (!_hasChangesMap.contains(_nodes.at(index.row()).audioClusterNode.descriptorIndex))
					{
						_hasChangesMap.insert(_nodes.at(index.row()).audioClusterNode.descriptorIndex, new QMap<DeviceDetailsChannelTableModelColumn, QVariant>());
					}
					_hasChangesMap.value(_nodes.at(index.row()).audioClusterNode.descriptorIndex)->insert(DeviceDetailsChannelTableModelColumn::ChannelName, value.toString());
					emit q->dataEdited();
				}
				else
				{
					if (_hasChangesMap.contains(_nodes.at(index.row()).audioClusterNode.descriptorIndex))
					{
						_hasChangesMap.remove(_nodes.at(index.row()).audioClusterNode.descriptorIndex);
					}
				}
				break;
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
			return _nodes.at(section).audioClusterNode.descriptorIndex - _nodes.at(section).connectionInformation.sourceBaseCluster.value() + 1; // +1 to make the row count start at 1 instead of 0.
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
* Gets the flags of a cell.
*/
void DeviceDetailsChannelTableModel::registerUiWidget(QTableView* view)
{
	Q_D(DeviceDetailsChannelTableModel);
	d->registerUiWidget(view);
}

/**
* Adds a node to the table.
* @param audioClusterNode: The audio cluster node to display.
*/
QTableView* DeviceDetailsChannelTableModel::getUiWidget() const
{
	Q_D(const DeviceDetailsChannelTableModel);
	return d->getUiWidget();
}

/**
* Adds a node to the table.
* @param audioClusterNode: The audio cluster node to display.
*/
void DeviceDetailsChannelTableModel::addNode(la::avdecc::controller::model::AudioClusterNode const& audioClusterNode, uint16_t audioClusterNodeChannel, avdecc::ConnectionInformation const& connectionInformation)
{
	Q_D(DeviceDetailsChannelTableModel);
	return d->addNode(audioClusterNode, audioClusterNodeChannel, connectionInformation);
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
	auto const& tableData = model->tableDataAtRow(index.row());

	QFontMetrics fm(model->getUiWidget()->fontMetrics());
	int fontPixelHeight = fm.height();

	int margin = 2;
	int circleDiameter = fontPixelHeight;

	auto const backgroundColor = index.data(Qt::BackgroundRole);
	if (!backgroundColor.isNull())
	{
		painter->fillRect(option.rect, backgroundColor.value<QColor>());
	}

	int innerRow = 0;
	QString connectionsFormatted;
	for (auto const& connectionKV : tableData.connectionInformation.deviceConnections)
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
					
					QRect iconDrawRect(option.rect.left() + (option.rect.width() - circleDiameter) / 2.0f, option.rect.top() + margin + innerRow * (fontPixelHeight * 2), circleDiameter, circleDiameter);

					auto const status = connectionKV.second->streamConnectionStatus;
					bool isRedundant = false; // TODO !((talkerNodeType == Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInput) || (talkerNodeType == Model::NodeType::OutputStream && listenerNodeType == Model::NodeType::InputStream));

					if (la::avdecc::hasFlag(status, avdecc::ConnectionStatus::Connected))
					{
						if (la::avdecc::hasFlag(status, avdecc::ConnectionStatus::WrongDomain))
						{
							connectionMatrix::drawWrongDomainConnectedStream(painter, iconDrawRect, isRedundant);
						}
						else if (la::avdecc::hasFlag(status, avdecc::ConnectionStatus::WrongFormat))
						{
							connectionMatrix::drawWrongFormatConnectedStream(painter, iconDrawRect, isRedundant);
						}
						else
						{
							connectionMatrix::drawConnectedStream(painter, iconDrawRect, isRedundant);
						}
					}
					else if (la::avdecc::hasFlag(status, avdecc::ConnectionStatus::FastConnecting))
					{
						if (la::avdecc::hasFlag(status, avdecc::ConnectionStatus::WrongDomain))
						{
							connectionMatrix::drawWrongDomainFastConnectingStream(painter, iconDrawRect, isRedundant);
						}
						else if (la::avdecc::hasFlag(status, avdecc::ConnectionStatus::WrongFormat))
						{
							connectionMatrix::drawWrongFormatFastConnectingStream(painter, iconDrawRect, isRedundant);
						}
						else
						{
							connectionMatrix::drawFastConnectingStream(painter, iconDrawRect, isRedundant);
						}
					}
					else if (la::avdecc::hasFlag(status, avdecc::ConnectionStatus::PartiallyConnected))
					{
						connectionMatrix::drawPartiallyConnectedRedundantNode(painter, iconDrawRect);
					}
					else
					{
						if (la::avdecc::hasFlag(status, avdecc::ConnectionStatus::WrongDomain))
						{
							connectionMatrix::drawWrongDomainNotConnectedStream(painter, iconDrawRect, isRedundant);
						}
						else if (la::avdecc::hasFlag(status, avdecc::ConnectionStatus::WrongFormat))
						{
							connectionMatrix::drawWrongFormatNotConnectedStream(painter, iconDrawRect, isRedundant);
						}
						else
						{
							connectionMatrix::drawNotConnectedStream(painter, iconDrawRect, isRedundant);
						}
					}

					innerRow++;
				}
			}
		}
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

#include "deviceDetailsChannelTableModel.moc"
