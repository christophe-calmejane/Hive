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

#include "mediaClock/unassignedListModel.hpp"

#include <hive/modelsLibrary/helper.hpp>

#include <QMenu>
#include <QMimeData>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "avdecc/mcDomainManager.hpp"
#include "avdecc/helper.hpp"

// **************************************************************
// class UnassignedListModelPrivate
// **************************************************************
/**
* @brief    Private implemenation of the list model.
* [@author  Marius Erlen]
* [@date    2018-09-21]
*
* Holds a list of entity ids to display in a list view.
*/
class UnassignedListModelPrivate : public QObject
{
	Q_OBJECT
public:
	UnassignedListModelPrivate(UnassignedListModel* model);
	virtual ~UnassignedListModelPrivate();

	int rowCount(QModelIndex const& parent) const;
	QVariant data(QModelIndex const& index, int role) const;
	Qt::ItemFlags flags(QModelIndex const& index) const;
	bool removeRows(int row, int count, const QModelIndex& parent);
	Qt::DropActions supportedDropActions() const;
	bool canDropMimeData(QMimeData const* data, Qt::DropAction action, int row, int column, QModelIndex const& parent) const;
	bool dropMimeData(QMimeData const* data, Qt::DropAction action, int row, int column, QModelIndex const& parent);
	QStringList mimeTypes() const;
	QMimeData* mimeData(QModelIndexList const& indexes) const;

	void setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping const& domains);
	QList<la::avdecc::UniqueIdentifier> getSelectedItems(QItemSelection const& itemSelection) const;
	void removeEntity(la::avdecc::UniqueIdentifier const& entityId);
	void addEntity(la::avdecc::UniqueIdentifier const& entityId);

	QList<la::avdecc::UniqueIdentifier> getAllItems() const;

private:
	QList<la::avdecc::UniqueIdentifier> _entities;

private:
	UnassignedListModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(UnassignedListModel);
};

//////////////////////////////////////

/**
* Constructor.
* @param model The public implementation class.
*/
UnassignedListModelPrivate::UnassignedListModelPrivate(UnassignedListModel* model)
	: q_ptr(model)
{
}

/**
* Destructor.
*/
UnassignedListModelPrivate::~UnassignedListModelPrivate() {}

/**
* Sets the data this model operates on.
* @param domains The model.
*/
void UnassignedListModelPrivate::setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping const& domains)
{
	Q_Q(UnassignedListModel);
	q->beginResetModel();
	_entities.clear();

	auto domainsLocal = domains;

	// code for list model
	for (auto const& entityDomainKV : domainsLocal.getEntityMediaClockMasterMappings())
	{
		if (entityDomainKV.second.empty() && avdecc::mediaClock::MCDomainManager::getInstance().isMediaClockDomainManageable(entityDomainKV.first))
		{
			// empty means unassigned with the exception of entities that cannot be managed by MCMD in the first place
			_entities.append(entityDomainKV.first);
		}
	}
	q->endResetModel();
}

/**
* Gets the currently selected entities.
* @return Entity ids of the rows that are selected.
*/
QList<la::avdecc::UniqueIdentifier> UnassignedListModelPrivate::getSelectedItems(QItemSelection const& itemSelection) const
{
	QList<la::avdecc::UniqueIdentifier> result;
	for (auto const& selection : itemSelection)
	{
		for (const auto& index : selection.indexes())
		{
			result.append(_entities.at(index.row()));
		}
	}

	return result;
}

/**
* Removes an entity from the list and updates the view.
* @param entityId The entity to remove to this model.
*/
void UnassignedListModelPrivate::removeEntity(la::avdecc::UniqueIdentifier const& entityId)
{
	Q_Q(UnassignedListModel);
	int rowIndex = _entities.indexOf(entityId);

	if (rowIndex != -1)
	{
		q->beginRemoveRows(QModelIndex(), rowIndex, rowIndex);
		_entities.removeAt(rowIndex);
		q->endRemoveRows();
	}
}

/**
* Adds the given entity to the model.
* @param entityId The entity to add.
*/
void UnassignedListModelPrivate::addEntity(la::avdecc::UniqueIdentifier const& entityId)
{
	Q_Q(UnassignedListModel);
	if (_entities.contains(entityId))
	{ // doubled entities are not allowed in this list
		return;
	}
	q->beginInsertRows(QModelIndex(), _entities.size(), _entities.size());
	_entities.append(entityId);
	q->endInsertRows();
}

/**
* Gets the entities from this model.
* @return List of all entities in the model.
*/
QList<la::avdecc::UniqueIdentifier> UnassignedListModelPrivate::getAllItems() const
{
	return _entities;
}

/**
* Gets the row count of the table.
*/
int UnassignedListModelPrivate::rowCount(QModelIndex const& /*parent*/) const
{
	return _entities.size();
}

/**
* Gets the data of a table cell.
*/
QVariant UnassignedListModelPrivate::data(QModelIndex const& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (role != Qt::DisplayRole)
		return QVariant();

	auto controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(_entities.at(index.row()));
	if (!controlledEntity)
		return QVariant();

	return hive::modelsLibrary::helper::smartEntityName(*controlledEntity);
}

/**
* Gets the flags of the table cells.
*/
Qt::ItemFlags UnassignedListModelPrivate::flags(QModelIndex const& index) const
{
	Q_Q(const UnassignedListModel);
	if (!index.isValid())
		return Qt::ItemIsDropEnabled; // enable drop into empty space
	return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | q->QAbstractItemModel::flags(index);
}

/**
* Removes rows from the model. (Used by drag&drop mechanisms)
*/
bool UnassignedListModelPrivate::removeRows(int row, int count, QModelIndex const& parent)
{
	Q_Q(UnassignedListModel);
	if (row < 0 || row + count > _entities.size())
	{
		return false;
	}
	q->beginRemoveRows(parent, row, row + count - 1);
	for (int i = row + count - 1; i >= row; i--)
	{
		_entities.removeAt(i);
	}
	q->endRemoveRows();
	return true;
}

/**
* Gets the supported drop actions of this model. We only want to move.
*/
Qt::DropActions UnassignedListModelPrivate::supportedDropActions() const
{
	return Qt::MoveAction;
}

/**
* Checks if the given mime data can be dropped into this model.
*/
bool UnassignedListModelPrivate::canDropMimeData(QMimeData const* data, Qt::DropAction, int /*row*/, int /*column*/, QModelIndex const& /*parent*/) const
{
	if (!data->hasFormat("application/json"))
		return false;

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(data->data("application/json"), &parseError);
	if (parseError.error != QJsonParseError::NoError)
	{
		return false;
	}
	auto jsonFormattedData = doc.object();
	if (jsonFormattedData.empty() || jsonFormattedData.value("dataType") != "la::avdecc::UniqueIdentifier" || jsonFormattedData.value("dataSource") == "UnassignedListModel")
	{
		return false;
	}

	return true;
}

/**
* Adds the given data (entity ids) to this model and returns true if successful.
*/
bool UnassignedListModelPrivate::dropMimeData(QMimeData const* data, Qt::DropAction /*action*/, int row, int, QModelIndex const& parent)
{
	Q_Q(UnassignedListModel);
	if (!data->hasFormat("application/json"))
		return false;

	int beginRow;

	if (row != -1)
		beginRow = row;
	else if (parent.isValid())
		beginRow = parent.row();
	else
		beginRow = rowCount(QModelIndex());

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(data->data("application/json"), &parseError);
	if (parseError.error != QJsonParseError::NoError)
	{
		return false;
	}
	auto jsonFormattedData = doc.object();
	if (jsonFormattedData.empty() || jsonFormattedData.value("dataType") != "la::avdecc::UniqueIdentifier")
	{
		return false;
	}
	auto jsonFormattedDataEntries = jsonFormattedData.value("data").toArray();

	for (auto const& entry : jsonFormattedDataEntries)
	{
		addEntity(la::avdecc::UniqueIdentifier(static_cast<qint64>(entry.toDouble()))); // ::toDouble is used, since QJsonValue(qint64) constructor internally creates a double value, which is what happens when mimeData is created when drag is started.
	}
	emit q->domainSetupChanged();
	return true;
}

/**
* Gets the supported mime types. (Json)
*/
QStringList UnassignedListModelPrivate::mimeTypes() const
{
	QStringList types;
	types << "application/json";
	return types;
}

/**
* Gets the entity ids as mime data.
*/
QMimeData* UnassignedListModelPrivate::mimeData(QModelIndexList const& indexes) const
{
	QMimeData* mimeData = new QMimeData();

	QJsonDocument doc;
	QJsonObject jsonFormattedData;
	QJsonArray jsonFormattedDataEntries;

	jsonFormattedData.insert("dataType", "la::avdecc::UniqueIdentifier");
	jsonFormattedData.insert("dataSource", "UnassignedListModel");

	for (QModelIndex const& index : indexes)
	{
		if (index.isValid())
		{
			jsonFormattedDataEntries.append(QJsonValue((qint64)_entities.at(index.row()).getValue()));
		}
	}
	jsonFormattedData.insert("data", jsonFormattedDataEntries);
	doc.setObject(jsonFormattedData);

	mimeData->setData("application/json", doc.toJson());
	return mimeData;
}

/* ************************************************************ */
/* DeviceDetailsChannelTableModel                               */
/* ************************************************************ */
/**
* Constructor.
* @param parent The parent object.
*/
UnassignedListModel::UnassignedListModel(QObject* parent)
	: QAbstractListModel(parent)
	, d_ptr(new UnassignedListModelPrivate(this))
{
}

/**
* Destructor. Deletes the private implementation pointer.
*/
UnassignedListModel::~UnassignedListModel()
{
	delete d_ptr;
}

/**
* Gets the row count.
*/
int UnassignedListModel::rowCount(QModelIndex const& parent) const
{
	Q_D(const UnassignedListModel);
	return d->rowCount(parent);
}

Qt::DropActions UnassignedListModel::supportedDropActions() const
{
	Q_D(const UnassignedListModel);
	return d->supportedDropActions();
}

bool UnassignedListModel::canDropMimeData(QMimeData const* data, Qt::DropAction action, int row, int column, QModelIndex const& parent) const
{
	Q_D(const UnassignedListModel);
	return d->canDropMimeData(data, action, row, column, parent);
}

bool UnassignedListModel::dropMimeData(QMimeData const* data, Qt::DropAction action, int row, int column, QModelIndex const& parent)
{
	Q_D(UnassignedListModel);
	return d->dropMimeData(data, action, row, column, parent);
}

bool UnassignedListModel::removeRows(int row, int count, const QModelIndex& parent)
{
	Q_D(UnassignedListModel);
	return d->removeRows(row, count, parent);
}

QStringList UnassignedListModel::mimeTypes() const
{
	Q_D(const UnassignedListModel);
	return d->mimeTypes();
}

QMimeData* UnassignedListModel::mimeData(const QModelIndexList& indexes) const
{
	Q_D(const UnassignedListModel);
	return d->mimeData(indexes);
}

/**
* Gets the data of a cell.
*/
QVariant UnassignedListModel::data(QModelIndex const& index, int role) const
{
	Q_D(const UnassignedListModel);
	return d->data(index, role);
}

/**
* Gets the flags of a cell.
*/
Qt::ItemFlags UnassignedListModel::flags(QModelIndex const& index) const
{
	Q_D(const UnassignedListModel);
	return d->flags(index);
}

/**
* Sets the data this model operates on.
* @param domains The model.
*/
void UnassignedListModel::setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping const& domains)
{
	Q_D(UnassignedListModel);
	d->setMediaClockDomainModel(domains);
}

/**
* Removes an entity from the list and updates the view.
* @param entityId The entity to remove to this model.
*/
void UnassignedListModel::removeEntity(la::avdecc::UniqueIdentifier const& entityId)
{
	Q_D(UnassignedListModel);
	d->removeEntity(entityId);
}

/**
* Adds the given entity to the model.
* @param entityId The entity to add.
*/
void UnassignedListModel::addEntity(la::avdecc::UniqueIdentifier const& entityId)
{
	Q_D(UnassignedListModel);
	return d->addEntity(entityId);
}

/**
* Gets the currently selected entities.
* @return Entity ids of the rows that are selected.
*/
QList<la::avdecc::UniqueIdentifier> UnassignedListModel::getSelectedItems(QItemSelection const& itemSelection) const
{
	Q_D(const UnassignedListModel);
	return d->getSelectedItems(itemSelection);
}

/**
* Gets the entities from this model.
* @return List of all entities in the model.
*/
QList<la::avdecc::UniqueIdentifier> UnassignedListModel::getAllItems() const
{
	Q_D(const UnassignedListModel);
	return d->getAllItems();
}

#include "unassignedListModel.moc"
