/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "abstractTreeItem.hpp"
#include "domainTreeItem.hpp"
#include "entityTreeItem.hpp"

/**
* Constructor.
*/
AbstractTreeItem::AbstractTreeItem()
{
	m_parentItem = nullptr;
}

/**
* Destructor.
*/
AbstractTreeItem::~AbstractTreeItem()
{
	qDeleteAll(m_childItems);
}

/**
* Adds an item to this tree item.
*/
void AbstractTreeItem::appendChild(AbstractTreeItem* item)
{
	m_childItems.append(item);
}

/**
* Gets the child at a specific row.
*/
AbstractTreeItem* AbstractTreeItem::child(int row)
{
	return (m_childItems.value(row));
}

/**
* Gets the child count of this item.
*/
int AbstractTreeItem::childCount() const
{
	return m_childItems.count();
}

/**
* Gets the row of this item.
*/
int AbstractTreeItem::row() const
{
	if (m_parentItem)
		return m_parentItem->indexOf(const_cast<AbstractTreeItem*>(this));

	return 0;
}

/**
* Gets the parent item.
*/
AbstractTreeItem* AbstractTreeItem::parentItem()
{
	return m_parentItem;
}

/**
* Gets the index of the given child tree item.
* @return The index of the item.
*/
int AbstractTreeItem::indexOf(AbstractTreeItem* child) const
{
	return m_childItems.indexOf(child);
}

/**
* Removes the child at the row.
* @return True if the item was removed.
*/
bool AbstractTreeItem::removeChildAt(int row)
{
	if (m_childItems.size() > row)
	{
		auto* i = m_childItems.at(row);
		m_childItems.removeAt(row);
		delete i;
		return true;
	}

	return false;
}

/**
* Gets the child item model at a row.
* @return Null or the AbstractTreeItem pointer.
*/
AbstractTreeItem* AbstractTreeItem::childAt(int row)
{
	if (m_childItems.size() <= row)
	{
		return nullptr;
	}
	return m_childItems.at(row);
}

/////////////////////////////////////////////////////////////////////////

/**
* Finds the domain tree item with the given domain index.
* @return Null or the DomainTreeItem pointer.
*/
DomainTreeItem* RootTreeItem::findDomainWithIndex(avdecc::mediaClock::DomainIndex const domainIndex) const
{
	for (auto* item : m_childItems)
	{
		auto* domainTreeItem = static_cast<DomainTreeItem*>(item);
		if (domainTreeItem->domain().getDomainIndex() == domainIndex)
		{
			return domainTreeItem;
		}
	}
	return nullptr;
}

QList<DomainTreeItem*> RootTreeItem::findDomainsWithEntity(la::avdecc::UniqueIdentifier entityId)
{
	QList<DomainTreeItem*> result;
	for (auto* item : m_childItems)
	{
		auto* domainTreeItem = static_cast<DomainTreeItem*>(item);
		for (int i = 0; i < domainTreeItem->childCount(); i++)
		{
			auto* entityTreeItem = static_cast<EntityTreeItem*>(domainTreeItem->childAt(i));

			if (entityTreeItem->entityId() == entityId)
			{
				result.append(domainTreeItem);
				break;
			}
		}
	}
	return result;
}

AbstractTreeItem::TreeItemType RootTreeItem::type() const
{
	return AbstractTreeItem::TreeItemType::Root;
}
