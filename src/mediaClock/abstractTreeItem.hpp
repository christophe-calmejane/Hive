/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <QList>
#include <QVariant>
#include "avdecc/mcDomainManager.hpp"

class DomainTreeItem;

// **************************************************************
// class AbstractTreeItem
// **************************************************************
/**
* @brief    Abstract base implementation for tree items.
* [@author  Marius Erlen]
* [@date    2018-10-22]
*/
class AbstractTreeItem
{
public:
	AbstractTreeItem();
	virtual ~AbstractTreeItem();

	virtual AbstractTreeItem* child(int row);
	virtual int childCount() const;
	virtual int row() const;
	virtual void appendChild(AbstractTreeItem* item);
	virtual AbstractTreeItem* parentItem();
	virtual int indexOf(AbstractTreeItem* child) const;
	virtual bool removeChildAt(int row);
	virtual AbstractTreeItem* childAt(int row);

protected:
	QList<AbstractTreeItem*> m_childItems;
	AbstractTreeItem* m_parentItem;
};

// **************************************************************
// class RootTreeItem
// **************************************************************
/**
* @brief    The root tree item implementation.
* [@author  Marius Erlen]
* [@date    2018-10-22]
*/
class RootTreeItem : public AbstractTreeItem
{
public:
	DomainTreeItem* findDomainWithIndex(avdecc::mediaClock::DomainIndex const& domainIndex) const;
	QList<DomainTreeItem*> findDomainsWithEntity(la::avdecc::UniqueIdentifier entityId);
};
