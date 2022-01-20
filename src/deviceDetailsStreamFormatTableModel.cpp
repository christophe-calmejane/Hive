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

#include "deviceDetailsStreamFormatTableModel.hpp"
#include "ui_deviceDetailsDialog.h"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "avdecc/helper.hpp"

#include "nodeTreeDynamicWidgets/streamFormatComboBox.hpp"

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>

#include <QCoreApplication>
#include <QLayout>
#include <QPainter>


// **************************************************************
// class DeviceDetailsStreamFormatTableModelPrivate
// **************************************************************
/**
* @brief    Private implementation of the table model for displaying/modifying
* 			stream formats per device talker/listener stream.
* [@author  Christian Ahrens]
* [@date    2021-05-27]
*
* .
*/
class DeviceDetailsStreamFormatTableModelPrivate : public QObject
{
	Q_OBJECT
public:
	DeviceDetailsStreamFormatTableModelPrivate(DeviceDetailsStreamFormatTableModel* model);
	virtual ~DeviceDetailsStreamFormatTableModelPrivate();

	int rowCount() const;
	int columnCount() const;
	QVariant data(QModelIndex const& index, int role) const;
	bool setData(QModelIndex const& index, QVariant const& value, int role);
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	Qt::ItemFlags flags(QModelIndex const& index) const;

	void setControlledEntityID(la::avdecc::UniqueIdentifier const entityID);
	la::avdecc::UniqueIdentifier getControlledEntityID() const;
	void addNode(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::DescriptorType const streamType, la::avdecc::entity::model::StreamFormat const streamFormat);
	void removeAllNodes();
	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsStreamFormatTableModelColumn, QVariant>*> getChanges() const;
	void resetChangedData();

	StreamFormatTableRowEntry const& tableDataAtRow(int row) const;

private:
	DeviceDetailsStreamFormatTableModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(DeviceDetailsStreamFormatTableModel);

	QTableView* _ui;

	la::avdecc::UniqueIdentifier _entityID{ 0u };

	using StreamFormatNodes = std::vector<StreamFormatTableRowEntry>;
	StreamFormatNodes _nodes{};

	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsStreamFormatTableModelColumn, QVariant>*> _hasChangesMap;
};

//////////////////////////////////////

/**
* Constructor.
* @param model The public implementation class.
*/
DeviceDetailsStreamFormatTableModelPrivate::DeviceDetailsStreamFormatTableModelPrivate(DeviceDetailsStreamFormatTableModel* model)
	: q_ptr(model)
{
}

/**
* Destructor.
*/
DeviceDetailsStreamFormatTableModelPrivate::~DeviceDetailsStreamFormatTableModelPrivate() {}

/**
*
*/
void DeviceDetailsStreamFormatTableModelPrivate::setControlledEntityID(la::avdecc::UniqueIdentifier const entityID)
{
	_entityID = entityID;
}

/**
*
*/
la::avdecc::UniqueIdentifier DeviceDetailsStreamFormatTableModelPrivate::getControlledEntityID() const
{
	return _entityID;
}

/**
* Adds a node to the table model. Doesn't check for duplicates or correct order.
* @param audioClusterNode The node to add to this model.
*/
void DeviceDetailsStreamFormatTableModelPrivate::addNode(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::DescriptorType const streamType, la::avdecc::entity::model::StreamFormat const streamFormat)
{
	Q_Q(DeviceDetailsStreamFormatTableModel);

	q->beginInsertRows(QModelIndex(), static_cast<int>(_nodes.size()), static_cast<int>(_nodes.size()));
	_nodes.push_back(StreamFormatTableRowEntry(streamIndex, streamType, streamFormat));
	q->endInsertRows();
}

/**
* Removes all nodes from the table model.
*/
void DeviceDetailsStreamFormatTableModelPrivate::removeAllNodes()
{
	Q_Q(DeviceDetailsStreamFormatTableModel);
	q->beginResetModel();
	_nodes.clear();
	q->endResetModel();
}

/**
* Gets the model data at a specific row index.
*/
StreamFormatTableRowEntry const& DeviceDetailsStreamFormatTableModelPrivate::tableDataAtRow(int row) const
{
	return _nodes.at(row);
}

/**
* Gets all user edits.
* @return The edits in a QMap. The key indicates the node the edit has been made on, the key of the interal QMap is the type of data changed by the user.
*/
QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsStreamFormatTableModelColumn, QVariant>*> DeviceDetailsStreamFormatTableModelPrivate::getChanges() const
{
	return _hasChangesMap;
}

/**
* Resets all changes. That have been made by the user.
*/
void DeviceDetailsStreamFormatTableModelPrivate::resetChangedData()
{
	Q_Q(DeviceDetailsStreamFormatTableModel);
	q->beginResetModel();
	qDeleteAll(_hasChangesMap);
	_hasChangesMap.clear();
	q->endResetModel();
}

/**
* Gets the row count of the table.
*/
int DeviceDetailsStreamFormatTableModelPrivate::rowCount() const
{
	return static_cast<int>(_nodes.size());
}

/**
* Gets the column count of the table.
*/
int DeviceDetailsStreamFormatTableModelPrivate::columnCount() const
{
	return 2;
}

/**
* Gets the data of a table cell.
*/
QVariant DeviceDetailsStreamFormatTableModelPrivate::data(QModelIndex const& index, int role) const
{
	auto const column = static_cast<DeviceDetailsStreamFormatTableModelColumn>(index.column());
	if (role == Qt::TextAlignmentRole)
	{
		return Qt::AlignAbsolute;
	}
	switch (column)
	{
		case DeviceDetailsStreamFormatTableModelColumn::StreamName:
		{
			auto const& streamFormatData = _nodes.at(index.row());
			if (role == Qt::DisplayRole || role == Qt::EditRole)
			{
				if (_hasChangesMap.contains(streamFormatData.streamIndex) && _hasChangesMap.value(streamFormatData.streamIndex)->contains(DeviceDetailsStreamFormatTableModelColumn::StreamName))
				{
					return _hasChangesMap.value(streamFormatData.streamIndex)->value(DeviceDetailsStreamFormatTableModelColumn::StreamName);
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
							if (entityNode.dynamicModel)
							{
								auto const configurationIndex = entityNode.dynamicModel->currentConfiguration;

								auto streamName = QString();

								if (streamFormatData.streamType == la::avdecc::entity::model::DescriptorType::StreamOutput)
								{
									auto const& streamOutput = controlledEntity->getStreamOutputNode(configurationIndex, streamFormatData.streamIndex);
									streamName = hive::modelsLibrary::helper::localizedString(*controlledEntity, streamOutput.staticModel->localizedDescription);

									if (configurationIndex == controlledEntity->getEntityNode().dynamicModel->currentConfiguration && !streamOutput.dynamicModel->objectName.empty())
										streamName = streamOutput.dynamicModel->objectName.data();
								}
								else if (streamFormatData.streamType == la::avdecc::entity::model::DescriptorType::StreamInput)
								{
									auto const& streamInput = controlledEntity->getStreamInputNode(configurationIndex, streamFormatData.streamIndex);
									streamName = hive::modelsLibrary::helper::localizedString(*controlledEntity, streamInput.staticModel->localizedDescription);

									if (configurationIndex == controlledEntity->getEntityNode().dynamicModel->currentConfiguration && !streamInput.dynamicModel->objectName.empty())
										streamName = streamInput.dynamicModel->objectName.data();
								}

								return streamName;
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
		case DeviceDetailsStreamFormatTableModelColumn::StreamFormat:
		{
			if (role == Qt::DisplayRole || role == Qt::EditRole)
			{
				try
				{
					QVariant streamFormatVariantData;
					streamFormatVariantData.setValue(_nodes.at(index.row()));
					return streamFormatVariantData;
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
bool DeviceDetailsStreamFormatTableModelPrivate::setData(QModelIndex const& index, QVariant const& value, int role)
{
	Q_Q(DeviceDetailsStreamFormatTableModel);
	if (index.isValid() && role == Qt::EditRole)
	{
		auto const column = static_cast<DeviceDetailsStreamFormatTableModelColumn>(index.column());
		switch (column)
		{
			case DeviceDetailsStreamFormatTableModelColumn::StreamName:
			{
				if (value.toString() != data(index, role))
				{
					try
					{
						auto const& streamIndex = _nodes.at(index.row()).streamIndex;
						if (!_hasChangesMap.contains(streamIndex))
						{
							_hasChangesMap.insert(streamIndex, new QMap<DeviceDetailsStreamFormatTableModelColumn, QVariant>());
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
			case DeviceDetailsStreamFormatTableModelColumn::StreamFormat:
			{
				auto currentDataValue = data(index, role).value<StreamFormatTableRowEntry>();
				if (value.canConvert<StreamFormatTableRowEntry>() && (value.value<StreamFormatTableRowEntry>() != currentDataValue))
				{
					try
					{
						auto const& streamIndex = currentDataValue.streamIndex;
						if (!_hasChangesMap.contains(streamIndex))
						{
							_hasChangesMap.insert(streamIndex, new QMap<DeviceDetailsStreamFormatTableModelColumn, QVariant>());
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
QVariant DeviceDetailsStreamFormatTableModelPrivate::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		if (role == Qt::DisplayRole)
		{
			switch (static_cast<DeviceDetailsStreamFormatTableModelColumn>(section))
			{
				case DeviceDetailsStreamFormatTableModelColumn::StreamName:
					if (static_cast<size_t>(section) >= _nodes.size())
					{
						return {};
					}
					return QString("%1 Name").arg(_nodes.at(section).streamType == la::avdecc::entity::model::DescriptorType::StreamInput ? "Stream Input" : "Stream Output");
				case DeviceDetailsStreamFormatTableModelColumn::StreamFormat:
					return "Format";
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
Qt::ItemFlags DeviceDetailsStreamFormatTableModelPrivate::flags(QModelIndex const& index) const
{
	if (index.column() == static_cast<int>(DeviceDetailsStreamFormatTableModelColumn::StreamFormat))
	{
		return Qt::ItemIsEnabled | Qt::ItemIsEditable;
	}
	return Qt::ItemIsEnabled;
}


/* ************************************************************ */
/* DeviceDetailsStreamFormatTableModel                               */
/* ************************************************************ */
/**
* Constructor.
* @param parent The parent object.
*/
DeviceDetailsStreamFormatTableModel::DeviceDetailsStreamFormatTableModel(QObject* parent)
	: QAbstractTableModel(parent)
	, d_ptr(new DeviceDetailsStreamFormatTableModelPrivate(this))
{
}

/**
* Destructor. Deletes the private implementation pointer.
*/
DeviceDetailsStreamFormatTableModel::~DeviceDetailsStreamFormatTableModel()
{
	delete d_ptr;
}

/**
* Gets the row count.
*/
int DeviceDetailsStreamFormatTableModel::rowCount(QModelIndex const& /*parent*/) const
{
	Q_D(const DeviceDetailsStreamFormatTableModel);
	return d->rowCount();
}

/**
* Gets the column count.
*/
int DeviceDetailsStreamFormatTableModel::columnCount(QModelIndex const& /*parent*/) const
{
	Q_D(const DeviceDetailsStreamFormatTableModel);
	return d->columnCount();
}

/**
* Gets the data of a cell.
*/
QVariant DeviceDetailsStreamFormatTableModel::data(QModelIndex const& index, int role) const
{
	Q_D(const DeviceDetailsStreamFormatTableModel);
	return d->data(index, role);
}

/**
* Sets the data of a cell.
*/
bool DeviceDetailsStreamFormatTableModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	Q_D(DeviceDetailsStreamFormatTableModel);
	return d->setData(index, value, role);
}

/**
* Gets the header data for the table.
*/
QVariant DeviceDetailsStreamFormatTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_D(const DeviceDetailsStreamFormatTableModel);
	return d->headerData(section, orientation, role);
}

/**
* Gets the flags of a cell.
*/
Qt::ItemFlags DeviceDetailsStreamFormatTableModel::flags(QModelIndex const& index) const
{
	Q_D(const DeviceDetailsStreamFormatTableModel);
	return d->flags(index);
}

/**
*
*/
void DeviceDetailsStreamFormatTableModel::setControlledEntityID(la::avdecc::UniqueIdentifier const entityID)
{
	Q_D(DeviceDetailsStreamFormatTableModel);
	return d->setControlledEntityID(entityID);
}

/**
*
*/
la::avdecc::UniqueIdentifier DeviceDetailsStreamFormatTableModel::getControlledEntityID() const
{
	Q_D(const DeviceDetailsStreamFormatTableModel);
	return d->getControlledEntityID();
}

/**
* Adds a node to the table.
* @param audioClusterNode: The audio cluster node to display.
*/
void DeviceDetailsStreamFormatTableModel::addNode(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::DescriptorType const streamType, la::avdecc::entity::model::StreamFormat const streamFormat)
{
	Q_D(DeviceDetailsStreamFormatTableModel);
	return d->addNode(streamIndex, streamType, streamFormat);
}

/**
* Gets the changes made by the user.
* @return The edits in a QMap. The key indicates the node the edit has been made on, the key of the interal QMap is the type of data changed by the user.
*/
QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsStreamFormatTableModelColumn, QVariant>*> DeviceDetailsStreamFormatTableModel::getChanges() const
{
	Q_D(const DeviceDetailsStreamFormatTableModel);
	return d->getChanges();
}

/**
* Resets the changes that the user made.
*/
void DeviceDetailsStreamFormatTableModel::resetChangedData()
{
	Q_D(DeviceDetailsStreamFormatTableModel);
	return d->resetChangedData();
}

/**
* Clears the table model.
*/
void DeviceDetailsStreamFormatTableModel::removeAllNodes()
{
	Q_D(DeviceDetailsStreamFormatTableModel);
	return d->removeAllNodes();
}

/**
* Gets the data for a specific row.
* @param  The row index of the data to get.
* @return The audio cluster node object at the given row index.
*/
StreamFormatTableRowEntry const& DeviceDetailsStreamFormatTableModel::tableDataAtRow(int row) const
{
	Q_D(const DeviceDetailsStreamFormatTableModel);
	return d->tableDataAtRow(row);
}

/* ************************************************************ */
/* StreamFormatItemDelegate										*/
/* ************************************************************ */
/**
*
*/
QWidget* StreamFormatItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	auto model = static_cast<const DeviceDetailsStreamFormatTableModel*>(index.model());
	auto const delegateEntityID = model ? model->getControlledEntityID() : la::avdecc::UniqueIdentifier::getNullUniqueIdentifier();
	auto streamFormatData = qvariant_cast<StreamFormatTableRowEntry>(index.data());

	la::avdecc::entity::model::StreamNodeStaticModel const* staticModel = nullptr;
	la::avdecc::entity::model::StreamNodeDynamicModel const* dynamicModel = nullptr;

	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(delegateEntityID);
	if (controlledEntity)
	{
		auto const& entityNode = controlledEntity->getEntityNode();
		if (entityNode.dynamicModel)
		{
			auto const configurationIndex = entityNode.dynamicModel->currentConfiguration;

			if (streamFormatData.streamType == la::avdecc::entity::model::DescriptorType::StreamOutput)
			{
				auto const& streamOutput = controlledEntity->getStreamOutputNode(configurationIndex, streamFormatData.streamIndex);
				staticModel = streamOutput.staticModel;
				dynamicModel = static_cast<decltype(dynamicModel)>(streamOutput.dynamicModel);
			}
			else if (streamFormatData.streamType == la::avdecc::entity::model::DescriptorType::StreamInput)
			{
				auto const& streamInput = controlledEntity->getStreamInputNode(configurationIndex, streamFormatData.streamIndex);
				staticModel = streamInput.staticModel;
				dynamicModel = static_cast<decltype(dynamicModel)>(streamInput.dynamicModel);
			}
		}
	}

	auto* formatComboBox = new StreamFormatComboBox(delegateEntityID, streamFormatData.streamIndex, parent);
	if (staticModel)
		formatComboBox->setStreamFormats(staticModel->formats);
	if (dynamicModel)
		formatComboBox->setCurrentStreamFormat(dynamicModel->streamFormat);

	// Send changes
	connect(formatComboBox, &StreamFormatComboBox::currentFormatChanged, this,
		[this, formatComboBox]([[maybe_unused]] la::avdecc::entity::model::StreamFormat const previousStreamFormat, [[maybe_unused]] la::avdecc::entity::model::StreamFormat const newStreamFormat)
		{
			auto* p = const_cast<StreamFormatItemDelegate*>(this);
			emit p->commitData(formatComboBox);
		});

	// Listen for changes
	connect(&manager, &hive::modelsLibrary::ControllerManager::streamFormatChanged, formatComboBox,
		[this, formatComboBox, delegateEntityID, streamFormatData](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
		{
			if (entityID == delegateEntityID && descriptorType == streamFormatData.streamType && streamIndex == streamFormatData.streamIndex)
				formatComboBox->setCurrentStreamFormat(streamFormat);

			auto* p = const_cast<StreamFormatItemDelegate*>(this);
			emit p->commitData(formatComboBox);
		});

	return formatComboBox;
}

void StreamFormatItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	Q_UNUSED(editor);
	Q_UNUSED(index);
}

/**
* This is used to set changed stream format dropdown value.
*/
void StreamFormatItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, QModelIndex const& index) const
{
	auto* edit = static_cast<StreamFormatComboBox*>(editor);
	if (edit != nullptr && edit->getCurrentStreamFormat().isValid())
	{
		auto newStreamFormatData = qvariant_cast<StreamFormatTableRowEntry>(index.data());
		newStreamFormatData.streamFormat = edit->getCurrentStreamFormat();
		model->setData(index, QVariant::fromValue<StreamFormatTableRowEntry>(newStreamFormatData), Qt::EditRole);
	}
}

#include "deviceDetailsStreamFormatTableModel.moc"
