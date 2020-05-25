/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "entityCountersTreeWidgetItem.hpp"

#include <QMenu>

EntityCountersTreeWidgetItem::EntityCountersTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::EntityCounters const& counters, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
{
	static std::map<la::avdecc::entity::EntityCounterValidFlag, QString> s_counterNames{
		{ la::avdecc::entity::EntityCounterValidFlag::EntitySpecific1, "Entity Specific 1" },
		{ la::avdecc::entity::EntityCounterValidFlag::EntitySpecific2, "Entity Specific 2" },
		{ la::avdecc::entity::EntityCounterValidFlag::EntitySpecific3, "Entity Specific 3" },
		{ la::avdecc::entity::EntityCounterValidFlag::EntitySpecific4, "Entity Specific 4" },
		{ la::avdecc::entity::EntityCounterValidFlag::EntitySpecific5, "Entity Specific 5" },
		{ la::avdecc::entity::EntityCounterValidFlag::EntitySpecific6, "Entity Specific 6" },
		{ la::avdecc::entity::EntityCounterValidFlag::EntitySpecific7, "Entity Specific 7" },
		{ la::avdecc::entity::EntityCounterValidFlag::EntitySpecific8, "Entity Specific 8" },
	};

	// Create fields
	for (auto const nameKV : s_counterNames)
	{
		auto* widget = new QTreeWidgetItem(this);
		widget->setText(0, nameKV.second);
		widget->setHidden(true); // Hide until we get a counter value (so we don't display counters not supported by the entity)
		_counters[nameKV.first] = widget;
	}
	// Update counters right now
	updateCounters(counters);

	// Listen for EntityCountersChanged
	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::entityCountersChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::EntityCounters const& counters)
		{
			if (entityID == _entityID)
			{
				updateCounters(counters);
			}
		});
}

void EntityCountersTreeWidgetItem::updateCounters(la::avdecc::entity::model::EntityCounters const& counters)
{
	for (auto const counterKV : counters)
	{
		auto const counterFlag = counterKV.first;
		if (auto const it = _counters.find(counterFlag); it != _counters.end())
		{
			auto* widget = it->second;
			AVDECC_ASSERT(widget != nullptr, "If widget is found in the map, it should not be nullptr");
			widget->setText(1, QString::number(counterKV.second));
			widget->setHidden(false);
		}
	}
}
