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

#include "mediaClock/unassignedListModel.hpp"

#include <QMenu>

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

	void setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping domains);
	QList<la::avdecc::UniqueIdentifier> getSelectedItems(QItemSelection const& itemSelection) const;
	void removeEntity(const la::avdecc::UniqueIdentifier& entityId);
	void addEntity(const la::avdecc::UniqueIdentifier& entityId);

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
void UnassignedListModelPrivate::setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping domains)
{
	Q_Q(UnassignedListModel);
	q->beginResetModel();
	_entities.clear();

	// code for list model
	for (auto const& entityDomainKV : domains.getEntityMediaClockMasterMappings())
	{
		if (entityDomainKV.second.empty() && !avdecc::mediaClock::MCDomainManager::getInstance().isSingleAudioListener(entityDomainKV.first))
		{
			// empty means unassigned 
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
int UnassignedListModelPrivate::rowCount(QModelIndex const& parent) const
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

	auto controlledEntity = avdecc::ControllerManager::getInstance().getControlledEntity(_entities.at(index.row()));
	return avdecc::helper::entityName(*controlledEntity);
}

/**
* Gets the flags of the table cells.
*/
Qt::ItemFlags UnassignedListModelPrivate::flags(QModelIndex const& index) const
{
	Q_Q(const UnassignedListModel);
	if (!index.isValid())
		return 0;
	return q->QAbstractItemModel::flags(index);
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
void UnassignedListModel::setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping domains)
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
