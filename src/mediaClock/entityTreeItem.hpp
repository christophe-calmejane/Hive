/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#pragma once

#include <QList>
#include <QVariant>

#include "abstractTreeItem.hpp"
#include "avdecc/mcDomainManager.hpp"

// **************************************************************
// class EntityTreeItem
// **************************************************************
/**
* @brief    The tree item implementation for entities.
* [@author  Marius Erlen]
* [@date    2018-10-22]
*/
class EntityTreeItem : public AbstractTreeItem
{
public:
	explicit EntityTreeItem(const la::avdecc::UniqueIdentifier& entityID, AbstractTreeItem* parent = 0);

	la::avdecc::UniqueIdentifier entityId() const;
	QString entityName() const;
	std::optional<QPair<la::avdecc::entity::model::SamplingRate, QString>> sampleRate() const;
	bool isGPTPInSync() const;

	bool isMediaClockDomainManageableEntity() const;

	virtual AbstractTreeItem::TreeItemType type() const;

private:
	la::avdecc::UniqueIdentifier m_entityID;
};
