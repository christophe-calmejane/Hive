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

#include <QCoreApplication>

#include <QComboBox>
#include <QPainter>
#include <QLabel>
#include <QRadioButton>
#include <QDebug>
#include <QStandardItemModel>

#include "mediaClock/domainTreeModel.hpp"
#include "avdecc/mcDomainManager.hpp"
#include "mediaClock/domainTreeDomainNameDelegate.hpp"
#include "mediaClock/domainTreeEntityNameDelegate.hpp"

#include "abstractTreeItem.hpp"
#include "domainTreeItem.hpp"
#include "entityTreeItem.hpp"

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
class DomainTreeModelPrivate : public QObject
{
	Q_OBJECT
public:
	DomainTreeModelPrivate(DomainTreeModel* model);
	virtual ~DomainTreeModelPrivate();

	int rowCount(QModelIndex const& parent) const;
	int columnCount(QModelIndex const& parent) const;
	QVariant data(QModelIndex const& index, int role) const;
	bool setData(QModelIndex const& index, QVariant const& value, int role);
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	Qt::ItemFlags flags(QModelIndex const& index) const;
	QModelIndex index(int row, int column, QModelIndex const& parent) const;
	QModelIndex parent(QModelIndex const& index) const;

	void setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping domains);
	avdecc::mediaClock::MCEntityDomainMapping createMediaClockMappings();

	bool addEntityToSelection(QModelIndex const& currentIndex, la::avdecc::UniqueIdentifier const& entityId);
	bool addEntityToDomain(avdecc::mediaClock::DomainIndex const& domainIndex, la::avdecc::UniqueIdentifier const& entityId);
	std::optional<avdecc::mediaClock::DomainIndex> getSelectedDomain(QModelIndex const& currentIndex) const;
	QPair<std::optional<avdecc::mediaClock::DomainIndex>, la::avdecc::UniqueIdentifier> getSelectedEntity(QModelIndex const& currentIndex) const;
	void removeEntity(avdecc::mediaClock::DomainIndex const& domainIndex, la::avdecc::UniqueIdentifier const& entityId);
	avdecc::mediaClock::DomainIndex addNewDomain();
	QList<la::avdecc::UniqueIdentifier> removeSelectedDomain(QModelIndex const& currentIndex);
	QList<la::avdecc::UniqueIdentifier> removeAllDomains();
	bool isEntityDoubled(la::avdecc::UniqueIdentifier const& entityId) const;

	void handleClick(QModelIndex const& current, QModelIndex const& previous);

	Q_SLOT void onGptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain);

private:
	RootTreeItem* _rootItem;

private:
	DomainTreeModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(DomainTreeModel);
};

//////////////////////////////////////

/**
* Constructor.
* @param model The public implementation class.
*/
DomainTreeModelPrivate::DomainTreeModelPrivate(DomainTreeModel* model)
	: q_ptr(model)
{
	_rootItem = new RootTreeItem();

	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::gptpChanged, this, &DomainTreeModelPrivate::onGptpChanged);
}

/**
* Destructor.
*/
DomainTreeModelPrivate::~DomainTreeModelPrivate()
{
	delete _rootItem;
}

/**
* Sets the data this model operates on.
* @param domains The model.
*/
void DomainTreeModelPrivate::setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping domains)
{
	Q_Q(DomainTreeModel);
	q->beginResetModel();
	delete _rootItem; // delete all items in the tree.
	_rootItem = new RootTreeItem();
	q->endResetModel();

	for (auto& domainKV : domains.getMediaClockDomains())
	{
		_rootItem->appendChild(new DomainTreeItem(domainKV.second, _rootItem));
	}

	for (auto& entityDomainKV : domains.getEntityMediaClockMasterMappings())
	{
		for (int i = 0; i < _rootItem->childCount(); i++)
		{
			auto* domainTreeItem = ((DomainTreeItem*)_rootItem->child(i));
			if (std::find(entityDomainKV.second.begin(), entityDomainKV.second.end(), domainTreeItem->domain().getDomainIndex()) != entityDomainKV.second.end())
			{
				domainTreeItem->appendChild(new EntityTreeItem(entityDomainKV.first, domainTreeItem));
			}
		}
	}
}

/**
* Creates a avdecc::MediaClockDomains object from the current state of the model.
* @return Mapping of domains and entities.
*/
avdecc::mediaClock::MCEntityDomainMapping DomainTreeModelPrivate::createMediaClockMappings()
{
	avdecc::mediaClock::MCEntityDomainMapping mediaClockDomains;
	for (int i = 0; i < _rootItem->childCount(); i++)
	{
		auto* domainTreeItem = static_cast<DomainTreeItem*>(_rootItem->childAt(i));
		mediaClockDomains.getMediaClockDomains().emplace(domainTreeItem->domain().getDomainIndex(), avdecc::mediaClock::MCDomain(domainTreeItem->domain()));

		for (int j = 0; j < domainTreeItem->childCount(); j++)
		{
			auto* entityTreeItem = static_cast<EntityTreeItem*>(domainTreeItem->childAt(j));
			std::vector<avdecc::mediaClock::DomainIndex> domains; // multi domain support
			if (mediaClockDomains.getEntityMediaClockMasterMappings().count(entityTreeItem->entityId()) > 0)
			{
				for (auto const& domain : mediaClockDomains.getEntityMediaClockMasterMappings().find(entityTreeItem->entityId())->second)
				{
					domains.push_back(domain);
				}
			}
			domains.push_back(domainTreeItem->domain().getDomainIndex());
			mediaClockDomains.getEntityMediaClockMasterMappings().emplace(entityTreeItem->entityId(), domains);
		}
	}

	return mediaClockDomains;
}

/**
* Adds an entity id to the currently selected domain or to the parent domain if an entity is selected.
* @param	entityId The id of the entity to add.
* @return	If adding was successful.
*/
bool DomainTreeModelPrivate::addEntityToSelection(QModelIndex const& currentIndex, la::avdecc::UniqueIdentifier const& entityId)
{
	Q_Q(DomainTreeModel);
	auto* domainTreeItem = dynamic_cast<DomainTreeItem*>(static_cast<AbstractTreeItem*>(currentIndex.internalPointer()));

	if (domainTreeItem != nullptr)
	{
		// add to selected domain
		q->beginInsertRows(currentIndex, domainTreeItem->childCount(), domainTreeItem->childCount());
		domainTreeItem->appendChild(new EntityTreeItem(entityId, domainTreeItem));
		q->endInsertRows();

		// after adding the first entity to a domain it should be the mc master for that domain
		if (!domainTreeItem->domain().getMediaClockDomainMaster())
		{
			domainTreeItem->setDefaultMcMaster();
		}
		return true;
	}

	auto* entityTreeItem = dynamic_cast<EntityTreeItem*>(static_cast<AbstractTreeItem*>(currentIndex.internalPointer()));
	if (entityTreeItem != nullptr)
	{
		// add to parent domain
		auto* parentDomainTreeItem = dynamic_cast<DomainTreeItem*>(entityTreeItem->parentItem());
		q->beginInsertRows(currentIndex.parent(), parentDomainTreeItem->childCount(), parentDomainTreeItem->childCount());
		parentDomainTreeItem->appendChild(new EntityTreeItem(entityId, parentDomainTreeItem));
		q->endInsertRows();

		// after adding the first entity to a domain it should be the mc master for that domain
		if (!parentDomainTreeItem->domain().getMediaClockDomainMaster())
		{
			parentDomainTreeItem->setDefaultMcMaster();
		}
		return true;
	}

	return false; // no item selected.
}

/**
* Adds an entity id to the domain with the given index.
* @param entityId The id of the entity to add.
* @return		  If adding was successful.
*/
bool DomainTreeModelPrivate::addEntityToDomain(avdecc::mediaClock::DomainIndex const& domainIndex, la::avdecc::UniqueIdentifier const& entityId)
{
	Q_Q(DomainTreeModel);
	auto* domainTreeItem = _rootItem->findDomainWithIndex(domainIndex);
	if (domainTreeItem)
	{
		// add to selected domain
		q->beginInsertRows(index(_rootItem->indexOf(domainTreeItem), static_cast<int>(DomainTreeModelColumn::Domain), QModelIndex()), domainTreeItem->childCount(), domainTreeItem->childCount());
		domainTreeItem->appendChild(new EntityTreeItem(entityId, domainTreeItem));
		q->endInsertRows();

		if (!domainTreeItem->domain().getMediaClockDomainMaster())
		{
			domainTreeItem->setDefaultMcMaster();
		}
		else
		{
			domainTreeItem->reevaluateDomainSampleRate();
		}
		return true;
	}
	return false;
}

std::optional<avdecc::mediaClock::DomainIndex> DomainTreeModelPrivate::getSelectedDomain(QModelIndex const& currentIndex) const
{
	auto* domainTreeItem = dynamic_cast<DomainTreeItem*>(static_cast<AbstractTreeItem*>(currentIndex.internalPointer()));
	if (domainTreeItem)
	{
		return domainTreeItem->domain().getDomainIndex();
	}

	return std::nullopt;
}

/**
* Gets the currently selected entity. If there is non a pair of domain index -1 and default entity id are returned.
* @return Pair of domain index and entity id. 
*/
QPair<std::optional<avdecc::mediaClock::DomainIndex>, la::avdecc::UniqueIdentifier> DomainTreeModelPrivate::getSelectedEntity(QModelIndex const& currentIndex) const
{
	auto* entityTreeItem = dynamic_cast<EntityTreeItem*>(static_cast<AbstractTreeItem*>(currentIndex.internalPointer()));
	if (entityTreeItem)
	{
		auto* parentDomainTreeItem = dynamic_cast<DomainTreeItem*>(entityTreeItem->parentItem());
		return qMakePair(parentDomainTreeItem->domain().getDomainIndex(), entityTreeItem->entityId());
	}
	return qMakePair(std::nullopt, la::avdecc::UniqueIdentifier());
}

/**
* Removes an entity in a specific domain.
* @param domainIndex Index of the domain to remove the entity from.
* @param entityId	 Id of the entity to remove.
*/
void DomainTreeModelPrivate::removeEntity(avdecc::mediaClock::DomainIndex const& domainIndex, la::avdecc::UniqueIdentifier const& entityId)
{
	Q_Q(DomainTreeModel);
	auto* domainItem = _rootItem->findDomainWithIndex(domainIndex);
	int domainRowIndex = _rootItem->indexOf(domainItem);
	auto* entityItem = domainItem->findEntityWithId(entityId);
	int entityRowIndex = domainItem->indexOf(entityItem);


	auto domainModelIndex = index(domainRowIndex, 0, QModelIndex());
	q->beginRemoveRows(domainModelIndex, entityRowIndex, entityRowIndex);
	domainItem->removeChildAt(entityRowIndex);
	q->endRemoveRows();

	// after removing the entity that was mc master, a new media clock master should be set.
	// just set first item for now.
	bool mcMasterEnabledEntitiesFound = false;
	for (int i = 0; i < domainItem->childCount(); i++)
	{
		if (!static_cast<EntityTreeItem*>(domainItem->childAt(i))->isEntitySingleAudioStreamListener())
		{
			auto* entityTreeItem = static_cast<EntityTreeItem*>(domainItem->child(0));
			domainItem->domain().setMediaClockDomainMaster(entityTreeItem->entityId());
			q->dataChanged(domainModelIndex.child(0, 1), domainModelIndex.child(0, 1));
			mcMasterEnabledEntitiesFound = true;
			break;
		}
	}
	if (!mcMasterEnabledEntitiesFound)
	{
		// if no items are left in this domain the mc id has to be set to invalid
		domainItem->domain().setMediaClockDomainMaster(la::avdecc::UniqueIdentifier());
	}
	domainItem->reevaluateDomainSampleRate();
}

/**
* Adds a new empty domain.
*/
avdecc::mediaClock::DomainIndex DomainTreeModelPrivate::addNewDomain()
{
	Q_Q(DomainTreeModel);
	avdecc::mediaClock::MCDomain newDomain(_rootItem->childCount());
	q->beginInsertRows(QModelIndex(), _rootItem->childCount(), _rootItem->childCount());
	_rootItem->appendChild(new DomainTreeItem(newDomain, _rootItem));
	q->endInsertRows();
	if (_rootItem->childCount() == 1)
	{
		// resize the columns once 1 item is added
		emit q->triggerResizeColumns();
	}
	return newDomain.getDomainIndex();
}

/**
* Removes the currently selected domain.
* @return List containing all entity ids that were assigned to the removed domain.
*/
QList<la::avdecc::UniqueIdentifier> DomainTreeModelPrivate::removeSelectedDomain(QModelIndex const& currentIndex)
{
	Q_Q(DomainTreeModel);
	auto* domainTreeItem = dynamic_cast<DomainTreeItem*>(static_cast<AbstractTreeItem*>(currentIndex.internalPointer()));
	int domainRowIndex = _rootItem->indexOf(domainTreeItem);

	// after deleting the domain all entities should be returned to the unassigned list.
	QList<la::avdecc::UniqueIdentifier> entities;
	for (int i = 0; i < domainTreeItem->childCount(); i++)
	{
		auto* entityTreeItem = static_cast<EntityTreeItem*>(domainTreeItem->childAt(i));
		if (!entityTreeItem->isEntitySingleAudioStreamListener()) // via audio stream connected entities are not shown in the unassigned list.
		{
			entities.append(entityTreeItem->entityId());
		}
	}

	q->beginRemoveRows(QModelIndex(), domainRowIndex, domainRowIndex);
	_rootItem->removeChildAt(domainRowIndex);
	q->endRemoveRows();

	// all following domain indices have to corrected.
	for (int i = _rootItem->childCount() - 1; i >= domainRowIndex; i--)
	{
		auto* domainTreeItem = static_cast<DomainTreeItem*>(_rootItem->childAt(i));
		domainTreeItem->domain().setDomainIndex(i);
	}

	return entities;
}

/**
* Removes all domains in the model.
* @return List containing all entity ids that were assigned to the removed domains.
*/
QList<la::avdecc::UniqueIdentifier> DomainTreeModelPrivate::removeAllDomains()
{
	Q_Q(DomainTreeModel);

	QList<la::avdecc::UniqueIdentifier> entities;
	for (int i = _rootItem->childCount() - 1; i >= 0; i--)
	{
		// after deleting the domain all entities should be returned to the unassigned list.
		for (int j = 0; j < _rootItem->childAt(i)->childCount(); j++)
		{
			auto* entityTreeItem = static_cast<EntityTreeItem*>(_rootItem->childAt(i)->childAt(j));
			if (!entityTreeItem->isEntitySingleAudioStreamListener()) // via audio stream connected entities are not shown in the unassigned list.
			{
				entities.append(entityTreeItem->entityId());
			}
		}

		q->beginRemoveRows(QModelIndex(), i, i);
		_rootItem->removeChildAt(i);
		q->endRemoveRows();
	}

	return entities;
}

/**
* Checks if an entity occurs multiple times in the tree.
* @param entityId	The id of the entity to check.
* @return			True if multiple occurences.
*/
bool DomainTreeModelPrivate::isEntityDoubled(la::avdecc::UniqueIdentifier const& entityId) const
{
	int count = 0;
	for (int i = _rootItem->childCount() - 1; i >= 0; i--)
	{
		// after deleting the domain all entities should be returned to the unassigned list.
		for (int j = 0; j < _rootItem->childAt(i)->childCount(); j++)
		{
			if (entityId == static_cast<EntityTreeItem*>(_rootItem->childAt(i)->childAt(j))->entityId())
			{
				count++;
			}
		}
	}
	return count > 1;
}

/**
* Handle cell selection change events to change the current media clock master.
* @param current The currently selected cell index.
* @param previous The previously selected cell index.
*/
void DomainTreeModelPrivate::handleClick(QModelIndex const& current, QModelIndex const& previous)
{
	Q_Q(DomainTreeModel);

	if (current.column() == static_cast<int>(DomainTreeModelColumn::MediaClockMaster))
	{
		auto* entityTreeItem = dynamic_cast<EntityTreeItem*>(static_cast<AbstractTreeItem*>(current.internalPointer()));

		if (entityTreeItem != nullptr)
		{
			auto* parentDomainTreeItem = dynamic_cast<DomainTreeItem*>(entityTreeItem->parentItem());
			bool isEntitySingleAudioStreamListener = entityTreeItem->isEntitySingleAudioStreamListener();
			bool entityIsMCMaster = entityTreeItem->entityId() == parentDomainTreeItem->domain().getMediaClockDomainMaster();
			if (!entityIsMCMaster && !isEntitySingleAudioStreamListener)
			{
				parentDomainTreeItem->domain().setMediaClockDomainMaster(entityTreeItem->entityId());
				auto parentIndex = index(parentDomainTreeItem->row(), static_cast<int>(DomainTreeModelColumn::MediaClockMaster), QModelIndex());
				auto beginIndex = index(0, static_cast<int>(DomainTreeModelColumn::MediaClockMaster), parentIndex);
				auto endIndex = index(parentDomainTreeItem->childCount(), static_cast<int>(DomainTreeModelColumn::MediaClockMaster), parentIndex);
				q->dataChanged(beginIndex, endIndex);
				emit q->mcMasterSelectionChanged();
			}
		}
	}
}

/**
* Handle gptp changes from the Controller Manager to update the according cell.
*/
Q_SLOT void DomainTreeModelPrivate::onGptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
{
	Q_Q(DomainTreeModel);
	for (int i = _rootItem->childCount() - 1; i >= 0; i--)
	{
		for (int j = 0; j < _rootItem->childAt(i)->childCount(); j++)
		{
			if (static_cast<EntityTreeItem*>(_rootItem->childAt(i)->childAt(j))->entityId() == entityID)
			{
				auto parentIndex = index(i, 0, QModelIndex());
				auto entityIndex = index(j, 0, parentIndex);
				q->dataChanged(entityIndex, entityIndex);
			}
		}
	}
}

/**
* Gets the row count of the table.
*/
int DomainTreeModelPrivate::rowCount(QModelIndex const& parent) const
{
	AbstractTreeItem* parentItem;
	if (parent.column() > 0)
		return 0;

	if (!parent.isValid())
		parentItem = _rootItem;
	else
		parentItem = static_cast<AbstractTreeItem*>(parent.internalPointer());

	return parentItem->childCount();
}

/**
* Gets the column count of the table.
*/
int DomainTreeModelPrivate::columnCount(QModelIndex const& parent) const
{
	return 2;
}

/**
* Gets the data of a cell.
*/
QVariant DomainTreeModelPrivate::data(QModelIndex const& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (role != Qt::DisplayRole)
		return QVariant();

	return QVariant::fromValue(index.internalPointer()); // return the pointer, handling of the data is done in the delegates
}

/**
* Sets the data on a cell when supported.
*/
bool DomainTreeModelPrivate::setData(QModelIndex const& index, QVariant const& value, int role)
{
	Q_Q(DomainTreeModel);
	if (index.column() == static_cast<int>(DomainTreeModelColumn::Domain))
	{
		auto* domainTreeItem = dynamic_cast<DomainTreeItem*>(static_cast<AbstractTreeItem*>(index.internalPointer()));

		if (domainTreeItem != nullptr)
		{
			if (domainTreeItem->domainSamplingRate().first != value.toInt())
			{
				domainTreeItem->setDomainSamplingRate(value.toInt());
				emit q->sampleRateSettingChanged();
				return true;
			}
		}
	}

	return false;
}

/**
* Gets the header data of the table.
*/
QVariant DomainTreeModelPrivate::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		if (role == Qt::DisplayRole)
		{
			switch (static_cast<DomainTreeModelColumn>(section))
			{
				case DomainTreeModelColumn::Domain:
					return "";
				case DomainTreeModelColumn::MediaClockMaster:
					return "MC Master";
				default:
					break;
			}
		}
	}

	return {};
}

/**
* Gets the flags of the table cells.
*/
Qt::ItemFlags DomainTreeModelPrivate::flags(QModelIndex const& index) const
{
	Q_Q(const DomainTreeModel);
	if (!index.isValid())
		return 0;
	if (dynamic_cast<DomainTreeItem*>(static_cast<AbstractTreeItem*>(index.internalPointer())) != nullptr && index.column() == static_cast<int>(DomainTreeModelColumn::Domain))
	{
		return q->QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
	}

	return q->QAbstractItemModel::flags(index);
}

/**
* Creates a model index from the given row, column and parent index.
*/
QModelIndex DomainTreeModelPrivate::index(int row, int column, QModelIndex const& parent) const
{
	Q_Q(const DomainTreeModel);
	if (!q->hasIndex(row, column, parent))
		return QModelIndex();

	AbstractTreeItem* parentItem;

	if (!parent.isValid())
		parentItem = _rootItem;
	else
		parentItem = static_cast<AbstractTreeItem*>(parent.internalPointer());

	auto* childItem = parentItem->child(row);
	if (childItem)
		return q->createIndex(row, column, childItem);
	else
		return QModelIndex();
}

/**
* Creates a model index from the given child index.
*/
QModelIndex DomainTreeModelPrivate::parent(QModelIndex const& index) const
{
	Q_Q(const DomainTreeModel);
	if (!index.isValid())
		return QModelIndex();

	auto* childItem = static_cast<AbstractTreeItem*>(index.internalPointer());
	auto* parentItem = childItem->parentItem();

	if (parentItem == _rootItem)
		return QModelIndex();

	return q->createIndex(parentItem->row(), 0, parentItem);
}


/* ************************************************************ */
/* DeviceDetailsChannelTableModel                               */
/* ************************************************************ */
/**
* Constructor.
* @param parent The parent object.
*/
DomainTreeModel::DomainTreeModel(QObject* parent)
	: QAbstractItemModel(parent)
	, d_ptr(new DomainTreeModelPrivate(this))
{
}

/**
* Destructor. Deletes the private implementation pointer.
*/
DomainTreeModel::~DomainTreeModel()
{
	delete d_ptr;
}

/**
* Gets the row count.
*/
int DomainTreeModel::rowCount(QModelIndex const& parent) const
{
	Q_D(const DomainTreeModel);
	return d->rowCount(parent);
}

/**
* Gets the column count.
*/
int DomainTreeModel::columnCount(QModelIndex const& parent) const
{
	Q_D(const DomainTreeModel);
	return d->columnCount(parent);
}

/**
* Gets the data of a cell.
*/
QVariant DomainTreeModel::data(QModelIndex const& index, int role) const
{
	Q_D(const DomainTreeModel);
	return d->data(index, role);
}

bool DomainTreeModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	Q_D(DomainTreeModel);
	return d->setData(index, value, role);
}

/**
* Gets the header data for the table.
*/
QVariant DomainTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_D(const DomainTreeModel);
	return d->headerData(section, orientation, role);
}

/**
* Creates a model index from the given row, column and parent index.
*/
QModelIndex DomainTreeModel::index(int row, int column, QModelIndex const& parent) const
{
	Q_D(const DomainTreeModel);
	return d->index(row, column, parent);
}

/**
* Creates a model index from the given child index.
*/
QModelIndex DomainTreeModel::parent(QModelIndex const& index) const
{
	Q_D(const DomainTreeModel);
	return d->parent(index);
}

/**
* Gets the flags of a cell.
*/
Qt::ItemFlags DomainTreeModel::flags(QModelIndex const& index) const
{
	Q_D(const DomainTreeModel);
	return d->flags(index);
}

/**
* Sets the data this model operates on.
* @param domains The model.
*/
void DomainTreeModel::setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping const& domains)
{
	Q_D(DomainTreeModel);
	d->setMediaClockDomainModel(domains);
}

/**
* Handles the click of a cell from the ui widget.
*/
void DomainTreeModel::handleClick(QModelIndex const& current, QModelIndex const& previous)
{
	Q_D(DomainTreeModel);
	d->handleClick(current, previous);
}

/**
* Creates a avdecc::MediaClockDomains object from the current state of the model.
* @return Mapping of domains and entities.
*/
avdecc::mediaClock::MCEntityDomainMapping DomainTreeModel::createMediaClockMappings()
{
	Q_D(DomainTreeModel);
	return d->createMediaClockMappings();
}

/**
* Adds an entity id to the currently selected domain or to the parent domain if an entity is selected.
* @param entityId The id of the entity to add.
* @return		  If adding was successful.
*/
bool DomainTreeModel::addEntityToSelection(QModelIndex const& currentIndex, la::avdecc::UniqueIdentifier const& entityId)
{
	Q_D(DomainTreeModel);
	return d->addEntityToSelection(currentIndex, entityId);
}

/**
* Adds an entity id to a specific domain.
* @param domainIndex The domain to add the entity to.
* @param entityId The id of the entity to add.
* @return If adding was successful.
*/
bool DomainTreeModel::addEntityToDomain(avdecc::mediaClock::DomainIndex const& domainIndex, la::avdecc::UniqueIdentifier const& entityId)
{
	Q_D(DomainTreeModel);
	return d->addEntityToDomain(domainIndex, entityId);
}

/**
* Gets the currently selected domain.
* @return std::nullopt if not available.
*/
std::optional<avdecc::mediaClock::DomainIndex> DomainTreeModel::getSelectedDomain(QModelIndex const& currentIndex) const
{
	Q_D(const DomainTreeModel);
	return d->getSelectedDomain(currentIndex);
}

/**
* Gets the currently selected entity. If there is non a pair of domain index -1 and default entity id are returned.
* @return Pair of domain index and entity id.
*/
QPair<std::optional<avdecc::mediaClock::DomainIndex>, la::avdecc::UniqueIdentifier> DomainTreeModel::getSelectedEntity(QModelIndex const& currentIndex) const
{
	Q_D(const DomainTreeModel);
	return d->getSelectedEntity(currentIndex);
}

/**
* Removes an entity in a specific domain.
* @param domainIndex Index of the domain to remove the entity from.
* @param entityId	 Id of the entity to remove.
*/
void DomainTreeModel::removeEntity(avdecc::mediaClock::DomainIndex const& domainIndex, la::avdecc::UniqueIdentifier const& entityId)
{
	Q_D(DomainTreeModel);
	d->removeEntity(domainIndex, entityId);
}

/**
* Adds a new empty domain.
*/
avdecc::mediaClock::DomainIndex DomainTreeModel::addNewDomain()
{
	Q_D(DomainTreeModel);
	return d->addNewDomain();
}

/**
* Removes the currently selected domain.
* @return List containing all entity ids that were assigned to the removed domain.
*/
QList<la::avdecc::UniqueIdentifier> DomainTreeModel::removeSelectedDomain(QModelIndex const& currentIndex)
{
	Q_D(DomainTreeModel);
	return d->removeSelectedDomain(currentIndex);
}

/**
* Removes all domains in the model.
* @return List containing all entity ids that were assigned to the removed domains.
*/
QList<la::avdecc::UniqueIdentifier> DomainTreeModel::removeAllDomains()
{
	Q_D(DomainTreeModel);
	return d->removeAllDomains();
}

/**
* Checks if an entity occurs multiple times in the tree.
* @param entityId	The id of the entity to check.
* @return			True if multiple occurences.
*/
bool DomainTreeModel::isEntityDoubled(la::avdecc::UniqueIdentifier const& entityId) const
{
	Q_D(const DomainTreeModel);
	return d->isEntityDoubled(entityId);
}


//////////////////////////////////////////////////////////

/**
* Constructor.
* @param parent: The DomainTreeModel, used for a workaround in this delegate.
*/
SampleRateDomainDelegate::SampleRateDomainDelegate(QTreeView* parent)
	: QStyledItemDelegate(parent)
{
	_treeView = parent;
}

/**
* Used to create the widget to edit. Can be used to change the sample rate setting.
*/
QWidget* SampleRateDomainDelegate::createEditor(QWidget* parent, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	auto* domainTreeItem = dynamic_cast<DomainTreeItem*>(static_cast<AbstractTreeItem*>(index.internalPointer()));
	auto* entityTreeItem = dynamic_cast<EntityTreeItem*>(static_cast<AbstractTreeItem*>(index.internalPointer()));

	if (domainTreeItem != nullptr)
	{
		auto const& sampleRates = domainTreeItem->sampleRates();
		auto const& selectedSampleRate = domainTreeItem->domainSamplingRate();
		if (sampleRates.size() < 2)
		{
			return new QWidget();
		}
		auto* editor = new DomainTreeDomainEditDelegate(parent);
		for (const auto& sampleRate : sampleRates)
		{
			if (sampleRate.first)
			{
				editor->getComboBox()->addItem(sampleRate.second, *sampleRate.first);
			}
			else
			{
				editor->getComboBox()->addItem(sampleRate.second, 0);
			}
		}
		if (selectedSampleRate.first)
		{
			editor->getComboBox()->setCurrentIndex(editor->getComboBox()->findData(*selectedSampleRate.first));
		}

		if (!sampleRates.at(0).first)
		{
			// disable the "-" if it is in the list.
			auto* model = qobject_cast<QStandardItemModel*>(editor->getComboBox()->model());
			if (model)
			{
				QStandardItem* item = model->item(0);
				if (item)
				{
					item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
				}
			}
		}

		//editor->getComboBox()->setCurrentText(selectedSampleRate.second);
		QFontMetrics metrics(editor->getLabel()->font());
		auto elidedText = metrics.elidedText(domainTreeItem->domain().getDisplayName(), Qt::ElideRight, option.rect.width() - editor->getComboBox()->width());
		editor->getLabel()->setText(elidedText);

		void (QComboBox::*indexChangedSignal)(int) = &QComboBox::currentIndexChanged;
		editor->getComboBox()->connect(editor->getComboBox(), indexChangedSignal, this,
			[this, editor](int newIndex)
			{
				// allow to directly change the data from the change signal
				auto* p = const_cast<SampleRateDomainDelegate*>(this);
				emit p->commitData(editor);
			});

		return editor;
	}

	if (entityTreeItem != nullptr)
	{
		return new QWidget(); // entities shall not be editable
	}
	return nullptr;
}

/**
* This is used to set the sample rate in a domain.
*/
void SampleRateDomainDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, QModelIndex const& index) const
{
	auto* edit = dynamic_cast<DomainTreeDomainEditDelegate*>(editor);
	if (edit != nullptr && !edit->getComboBox()->currentData().isNull() && edit->getComboBox()->currentData().isValid())
	{
		if (edit->getComboBox()->currentData().toUInt() != 0)
		{
			model->setData(index, edit->getComboBox()->currentData().toUInt());
		}
	}
}

/**
* Updates the geometry of the item.
*/
void SampleRateDomainDelegate::updateEditorGeometry(QWidget* editor, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	editor->setGeometry(option.rect);
}

/**
* Paints the item. This is used to paint the domain name with the sample rate combo box or the entity name.
*/
void SampleRateDomainDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	QStyledItemDelegate::paint(painter, option, index);

	auto* domainTreeItem = dynamic_cast<DomainTreeItem*>(static_cast<AbstractTreeItem*>(index.internalPointer()));
	auto* entityTreeItem = dynamic_cast<EntityTreeItem*>(static_cast<AbstractTreeItem*>(index.internalPointer()));

	if (domainTreeItem != nullptr)
	{
		if (option.state.testFlag(QStyle::State_Editing)) // not beeing set: QTBUG-68947
		{
			return;
		}
		if (_treeView->isPersistentEditorOpen(index)) // workaround for that bug, can be removed once fixed.
		{
			return;
		}

		auto const& sampleRates = domainTreeItem->sampleRates();
		auto const& selectedSampleRate = domainTreeItem->domainSamplingRate();

		DomainTreeDomainEditDelegate editor(_treeView);
		QFontMetrics const& metrics = option.fontMetrics;
		auto elidedText = metrics.elidedText(domainTreeItem->domain().getDisplayName(), Qt::ElideRight, option.rect.width() - editor.getComboBox()->width());
		editor.getLabel()->setText(elidedText);
		for (auto const& sampleRate : sampleRates)
		{
			editor.getComboBox()->addItem(sampleRate.second, *sampleRate.first);
		}
		if (selectedSampleRate.first)
		{
			editor.getComboBox()->setCurrentIndex(editor.getComboBox()->findData(*selectedSampleRate.first));
		}
		if (sampleRates.size() < 2)
		{
			editor.getComboBox()->setEnabled(false);
		}

		editor.resize(option.rect.width(), option.rect.height());
		painter->save();
		painter->translate(option.rect.topLeft());
		editor.render(painter, QPoint(), QRegion(0, 0, option.rect.width(), option.rect.height()));
		painter->restore();
	}

	if (entityTreeItem != nullptr)
	{
		auto const& entityName = entityTreeItem->entityName();
		auto const& entitySampleRate = entityTreeItem->sampleRate();
		auto selectedSampleRate = entitySampleRate ? (*entitySampleRate).second : "";
		bool isDoubledEntity = static_cast<DomainTreeModel const*>(index.model())->isEntityDoubled(entityTreeItem->entityId());
		bool gptpInSync = entityTreeItem->isGPTPInSync();

		auto* parentDomainTreeItem = static_cast<DomainTreeItem*>(entityTreeItem->parentItem());


		DomainTreeEntityNameDelegate editor(_treeView);
		QFontMetrics const& metrics = option.fontMetrics;
		auto elidedTextEntityName = metrics.elidedText(entityName, Qt::ElideRight, option.rect.width() - editor.getLabelRight()->width());
		editor.getLabelLeft()->setText(elidedTextEntityName);
		if (isDoubledEntity)
		{
			editor.getLabelLeft()->setStyleSheet("QLabel { color : orange; }");
		}
		if (!gptpInSync)
		{
			editor.getLabelLeft()->setStyleSheet("QLabel { color : red; }");
		}

		if (!parentDomainTreeItem->domainSamplingRate().first)
		{
			auto elidedTextSampleRate = metrics.elidedText(selectedSampleRate, Qt::ElideRight, editor.getLabelRight()->width());
			editor.getLabelRight()->setText(elidedTextSampleRate);
		}

		editor.resize(option.rect.width(), option.rect.height());
		painter->save();
		painter->translate(option.rect.topLeft());
		editor.render(painter, QPoint(), QRegion(0, 0, option.rect.width(), option.rect.height()));
		painter->restore();
	}
}

/**
* Gets a size hint for the column.
*/
QSize SampleRateDomainDelegate::sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	return QSize(340, 22);
}

////////////////////////////////////////////////////////////////

/**
* Constructor.
* @param parent: The DomainTreeModel, used for a workaround in this delegate.
*/
MCMasterSelectionDelegate::MCMasterSelectionDelegate(QTreeView* parent)
	: QStyledItemDelegate(parent)
{
	_treeView = parent;
}

/**
* Sets the geometry of the editor.
*/
void MCMasterSelectionDelegate::updateEditorGeometry(QWidget* editor, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	editor->setGeometry(option.rect);
}

/**
* Paints the item. This is used to paint the radio buttons in their correct state to show the media clock master selection.
*/
void MCMasterSelectionDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	QStyledItemDelegate::paint(painter, option, index);

	auto* entityTreeItem = dynamic_cast<EntityTreeItem*>(static_cast<AbstractTreeItem*>(index.internalPointer()));

	if (entityTreeItem != nullptr)
	{
		auto* parentDomainTreeItem = dynamic_cast<DomainTreeItem*>(entityTreeItem->parentItem());

		bool entityIsMCMaster = entityTreeItem->entityId() == parentDomainTreeItem->domain().getMediaClockDomainMaster();
		bool isEntitySingleAudioStreamListener = entityTreeItem->isEntitySingleAudioStreamListener();

		QRadioButton editor;
		editor.setChecked(entityIsMCMaster);
		editor.setEnabled(!isEntitySingleAudioStreamListener);
		QPalette pal;
		pal.setColor(QPalette::Background, Qt::transparent);
		editor.setPalette(pal);

		painter->save();
		painter->translate(option.rect.topLeft());
		editor.setGeometry(option.rect);
		editor.render(painter);
		painter->restore();
	}
}

/**
* Gets a size hint for the column.
*/
QSize MCMasterSelectionDelegate::sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	return QSize(55, 22);
}

#include "domainTreeModel.moc"
