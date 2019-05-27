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
#include <QStringList>

#include "abstractTreeItem.hpp"
#include "avdecc/mcDomainManager.hpp"

class EntityTreeItem;

// **************************************************************
// class DomainTreeItem
// **************************************************************
/**
* @brief    The tree item implementation for domains.
* [@author  Marius Erlen]
* [@date    2018-10-22]
*/
class DomainTreeItem : public AbstractTreeItem
{
public:
	explicit DomainTreeItem(avdecc::mediaClock::MCDomain const& data, AbstractTreeItem* parentItem = 0);

	virtual avdecc::mediaClock::MCDomain& domain();
	QList<QPair<std::optional<la::avdecc::entity::model::SamplingRate>, QString>> sampleRates() const;
	QPair<std::optional<la::avdecc::entity::model::SamplingRate>, QString> domainSamplingRate() const;
	void setDomainSamplingRate(la::avdecc::entity::model::SamplingRate sampleRate);
	void reevaluateDomainSampleRate();
	EntityTreeItem* findEntityWithId(la::avdecc::UniqueIdentifier const& entityId) const;

	void setDefaultMcMaster();

private:
	avdecc::mediaClock::MCDomain m_itemData;
	bool m_sampleRateSet;
};
