/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "deviceDetailsLatencyTableModel.hpp"
#include "ui_deviceDetailsDialog.h"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "avdecc/helper.hpp"

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include <la/avdecc/internals/streamFormatInfo.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>

#include <QCoreApplication>
#include <QLayout>
#include <QPainter>

#include <sstream>

// **************************************************************
// class DeviceDetailsLatencyTableModelPrivate
// **************************************************************
/**
* @brief    Private implementation of the table model for displaying/modifying
* 			stream latency per device talker stream.
*/
class DeviceDetailsLatencyTableModelPrivate : public QObject
{
	Q_OBJECT
public:
	DeviceDetailsLatencyTableModelPrivate(DeviceDetailsLatencyTableModel* model);
	virtual ~DeviceDetailsLatencyTableModelPrivate();

	int rowCount() const;
	int columnCount() const;
	QVariant data(QModelIndex const& index, int role) const;
	bool setData(QModelIndex const& index, QVariant const& value, int role);
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	Qt::ItemFlags flags(QModelIndex const& index) const;

	void setControlledEntityID(la::avdecc::UniqueIdentifier const entityID);
	la::avdecc::UniqueIdentifier getControlledEntityID() const;
	void addNode(la::avdecc::entity::model::StreamIndex const streamIndex, std::chrono::nanoseconds const latency);
	void removeAllNodes();
	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsLatencyTableModelColumn, QVariant>*> getChanges() const;
	void resetChangedData();

	LatencyTableRowEntry const& tableDataAtRow(int row) const;

private:
	DeviceDetailsLatencyTableModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(DeviceDetailsLatencyTableModel)

	QTableView* _ui;

	la::avdecc::UniqueIdentifier _entityID{ 0u };

	using LatencyNodes = std::vector<LatencyTableRowEntry>;
	LatencyNodes _nodes{};

	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsLatencyTableModelColumn, QVariant>*> _hasChangesMap;
};

//////////////////////////////////////

/**
* Constructor.
* @param model The public implementation class.
*/
DeviceDetailsLatencyTableModelPrivate::DeviceDetailsLatencyTableModelPrivate(DeviceDetailsLatencyTableModel* model)
	: q_ptr(model)
{
}

/**
* Destructor.
*/
DeviceDetailsLatencyTableModelPrivate::~DeviceDetailsLatencyTableModelPrivate() {}

/**
*
*/
void DeviceDetailsLatencyTableModelPrivate::setControlledEntityID(la::avdecc::UniqueIdentifier const entityID)
{
	_entityID = entityID;
}

/**
*
*/
la::avdecc::UniqueIdentifier DeviceDetailsLatencyTableModelPrivate::getControlledEntityID() const
{
	return _entityID;
}

/**
* Adds a node to the table model. Doesn't check for duplicates or correct order.
* @param audioClusterNode The node to add to this model.
*/
void DeviceDetailsLatencyTableModelPrivate::addNode(la::avdecc::entity::model::StreamIndex const streamIndex, std::chrono::nanoseconds const latency)
{
	Q_Q(DeviceDetailsLatencyTableModel);

	q->beginInsertRows(QModelIndex(), static_cast<int>(_nodes.size()), static_cast<int>(_nodes.size()));
	_nodes.push_back(LatencyTableRowEntry(streamIndex, latency));
	q->endInsertRows();
}

/**
* Removes all nodes from the table model.
*/
void DeviceDetailsLatencyTableModelPrivate::removeAllNodes()
{
	Q_Q(DeviceDetailsLatencyTableModel);
	q->beginResetModel();
	_nodes.clear();
	q->endResetModel();
}

/**
* Gets the model data at a specific row index.
*/
LatencyTableRowEntry const& DeviceDetailsLatencyTableModelPrivate::tableDataAtRow(int row) const
{
	return _nodes.at(row);
}

/**
* Gets all user edits.
* @return The edits in a QMap. The key indicates the node the edit has been made on, the key of the interal QMap is the type of data changed by the user.
*/
QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsLatencyTableModelColumn, QVariant>*> DeviceDetailsLatencyTableModelPrivate::getChanges() const
{
	return _hasChangesMap;
}

/**
* Resets all changes. That have been made by the user.
*/
void DeviceDetailsLatencyTableModelPrivate::resetChangedData()
{
	Q_Q(DeviceDetailsLatencyTableModel);
	q->beginResetModel();
	qDeleteAll(_hasChangesMap);
	_hasChangesMap.clear();
	q->endResetModel();
}

/**
* Gets the row count of the table.
*/
int DeviceDetailsLatencyTableModelPrivate::rowCount() const
{
	return static_cast<int>(_nodes.size());
}

/**
* Gets the column count of the table.
*/
int DeviceDetailsLatencyTableModelPrivate::columnCount() const
{
	return 2;
}

/**
* Gets the data of a table cell.
*/
QVariant DeviceDetailsLatencyTableModelPrivate::data(QModelIndex const& index, int role) const
{
	auto const column = static_cast<DeviceDetailsLatencyTableModelColumn>(index.column());
	if (role == Qt::TextAlignmentRole)
	{
		return Qt::AlignAbsolute;
	}
	switch (column)
	{
		case DeviceDetailsLatencyTableModelColumn::StreamName:
		{
			auto const& latencyData = _nodes.at(index.row());
			if (role == Qt::DisplayRole || role == Qt::EditRole)
			{
				if (_hasChangesMap.contains(latencyData.streamIndex) && _hasChangesMap.value(latencyData.streamIndex)->contains(DeviceDetailsLatencyTableModelColumn::StreamName))
				{
					return _hasChangesMap.value(latencyData.streamIndex)->value(DeviceDetailsLatencyTableModelColumn::StreamName);
				}
				else
				{
					try
					{
						auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
						auto const& controlledEntity = manager.getControlledEntity(_entityID);

						if (controlledEntity)
						{
							auto const& entityNode = controlledEntity->getEntityNode();
							auto const configurationIndex = entityNode.dynamicModel.currentConfiguration;

							auto const& streamOutput = controlledEntity->getStreamOutputNode(configurationIndex, latencyData.streamIndex);
#if 1
							{
								if (streamOutput.dynamicModel.objectName.empty())
								{
									return hive::modelsLibrary::helper::localizedString(*controlledEntity, configurationIndex, streamOutput.staticModel.localizedDescription);
								}

								return streamOutput.dynamicModel.objectName.data();
							}
#else
							// Don't know why this doesn't compile!
							return hive::modelsLibrary::helper::objectName(*controlledEntity, configurationIndex, streamOutput);
#endif
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
		case DeviceDetailsLatencyTableModelColumn::Latency:
		{
			if (role == Qt::DisplayRole || role == Qt::EditRole)
			{
				try
				{
					QVariant latencyVariantData;
					latencyVariantData.setValue(_nodes.at(index.row()));
					return latencyVariantData;
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
bool DeviceDetailsLatencyTableModelPrivate::setData(QModelIndex const& index, QVariant const& value, int role)
{
	Q_Q(DeviceDetailsLatencyTableModel);
	if (index.isValid() && role == Qt::EditRole)
	{
		auto const column = static_cast<DeviceDetailsLatencyTableModelColumn>(index.column());
		switch (column)
		{
			case DeviceDetailsLatencyTableModelColumn::StreamName:
			{
				if (value.toString() != data(index, role))
				{
					try
					{
						auto const& streamIndex = _nodes.at(index.row()).streamIndex;
						if (!_hasChangesMap.contains(streamIndex))
						{
							_hasChangesMap.insert(streamIndex, new QMap<DeviceDetailsLatencyTableModelColumn, QVariant>());
						}
						_hasChangesMap.value(streamIndex)->insert(column, value.toString());
					}
					catch (...)
					{
						return false;
					}
					emit q->dataEdited();
				}
				break;
			}
			case DeviceDetailsLatencyTableModelColumn::Latency:
			{
				auto currentDataValue = data(index, role).value<LatencyTableRowEntry>();
				if (value.canConvert<LatencyTableRowEntry>() && (value.value<LatencyTableRowEntry>() != currentDataValue))
				{
					try
					{
						auto const& streamIndex = currentDataValue.streamIndex;
						if (!_hasChangesMap.contains(streamIndex))
						{
							_hasChangesMap.insert(streamIndex, new QMap<DeviceDetailsLatencyTableModelColumn, QVariant>());
						}
						_hasChangesMap.value(streamIndex)->insert(column, value);
					}
					catch (...)
					{
						return false;
					}
					emit q->dataEdited();
				}
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
QVariant DeviceDetailsLatencyTableModelPrivate::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		if (role == Qt::DisplayRole)
		{
			switch (static_cast<DeviceDetailsLatencyTableModelColumn>(section))
			{
				case DeviceDetailsLatencyTableModelColumn::StreamName:
					if (static_cast<size_t>(section) >= _nodes.size())
					{
						return {};
					}
					return "Stream Output Name";
				case DeviceDetailsLatencyTableModelColumn::Latency:
					return "Latency";
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
			return _nodes.at(section).streamIndex;
		}
	}

	return {};
}

/**
* Gets the flags of the table cells.
*/
Qt::ItemFlags DeviceDetailsLatencyTableModelPrivate::flags(QModelIndex const& index) const
{
	if (index.column() == static_cast<int>(DeviceDetailsLatencyTableModelColumn::Latency))
	{
		return Qt::ItemIsEnabled | Qt::ItemIsEditable;
	}
	return Qt::ItemIsEnabled;
}


/* ************************************************************ */
/* DeviceDetailsLatencyTableModel                               */
/* ************************************************************ */
/**
* Constructor.
* @param parent The parent object.
*/
DeviceDetailsLatencyTableModel::DeviceDetailsLatencyTableModel(QObject* parent)
	: QAbstractTableModel(parent)
	, d_ptr(new DeviceDetailsLatencyTableModelPrivate(this))
{
}

/**
* Destructor. Deletes the private implementation pointer.
*/
DeviceDetailsLatencyTableModel::~DeviceDetailsLatencyTableModel()
{
	delete d_ptr;
}

/**
* Gets the row count.
*/
int DeviceDetailsLatencyTableModel::rowCount(QModelIndex const& /*parent*/) const
{
	Q_D(const DeviceDetailsLatencyTableModel);
	return d->rowCount();
}

/**
* Gets the column count.
*/
int DeviceDetailsLatencyTableModel::columnCount(QModelIndex const& /*parent*/) const
{
	Q_D(const DeviceDetailsLatencyTableModel);
	return d->columnCount();
}

/**
* Gets the data of a cell.
*/
QVariant DeviceDetailsLatencyTableModel::data(QModelIndex const& index, int role) const
{
	Q_D(const DeviceDetailsLatencyTableModel);
	return d->data(index, role);
}

/**
* Sets the data of a cell.
*/
bool DeviceDetailsLatencyTableModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	Q_D(DeviceDetailsLatencyTableModel);
	return d->setData(index, value, role);
}

/**
* Gets the header data for the table.
*/
QVariant DeviceDetailsLatencyTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_D(const DeviceDetailsLatencyTableModel);
	return d->headerData(section, orientation, role);
}

/**
* Gets the flags of a cell.
*/
Qt::ItemFlags DeviceDetailsLatencyTableModel::flags(QModelIndex const& index) const
{
	Q_D(const DeviceDetailsLatencyTableModel);
	return d->flags(index);
}

/**
*
*/
void DeviceDetailsLatencyTableModel::setControlledEntityID(la::avdecc::UniqueIdentifier const entityID)
{
	Q_D(DeviceDetailsLatencyTableModel);
	return d->setControlledEntityID(entityID);
}

/**
*
*/
la::avdecc::UniqueIdentifier DeviceDetailsLatencyTableModel::getControlledEntityID() const
{
	Q_D(const DeviceDetailsLatencyTableModel);
	return d->getControlledEntityID();
}

/**
* Adds a node to the table.
* @param audioClusterNode: The audio cluster node to display.
*/
void DeviceDetailsLatencyTableModel::addNode(la::avdecc::entity::model::StreamIndex const streamIndex, std::chrono::nanoseconds const latency)
{
	Q_D(DeviceDetailsLatencyTableModel);
	return d->addNode(streamIndex, latency);
}

/**
* Gets the changes made by the user.
* @return The edits in a QMap. The key indicates the node the edit has been made on, the key of the interal QMap is the type of data changed by the user.
*/
QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsLatencyTableModelColumn, QVariant>*> DeviceDetailsLatencyTableModel::getChanges() const
{
	Q_D(const DeviceDetailsLatencyTableModel);
	return d->getChanges();
}

/**
* Resets the changes that the user made.
*/
void DeviceDetailsLatencyTableModel::resetChangedData()
{
	Q_D(DeviceDetailsLatencyTableModel);
	return d->resetChangedData();
}

/**
* Clears the table model.
*/
void DeviceDetailsLatencyTableModel::removeAllNodes()
{
	Q_D(DeviceDetailsLatencyTableModel);
	return d->removeAllNodes();
}

/**
* Gets the data for a specific row.
* @param  The row index of the data to get.
* @return The audio cluster node object at the given row index.
*/
LatencyTableRowEntry const& DeviceDetailsLatencyTableModel::tableDataAtRow(int row) const
{
	Q_D(const DeviceDetailsLatencyTableModel);
	return d->tableDataAtRow(row);
}

#include "deviceDetailsLatencyTableModel.moc"
