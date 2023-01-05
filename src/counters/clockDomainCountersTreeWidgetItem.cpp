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

#include "clockDomainCountersTreeWidgetItem.hpp"

#include <QMenu>

ClockDomainCountersTreeWidgetItem::ClockDomainCountersTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockDomainCounters const& counters, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _clockDomainIndex(clockDomainIndex)
{
	static std::map<la::avdecc::entity::ClockDomainCounterValidFlag, QString> s_counterNames{
		{ la::avdecc::entity::ClockDomainCounterValidFlag::Locked, "Locked" },
		{ la::avdecc::entity::ClockDomainCounterValidFlag::Unlocked, "Unlocked" },
		{ la::avdecc::entity::ClockDomainCounterValidFlag::EntitySpecific1, "Entity Specific 1" },
		{ la::avdecc::entity::ClockDomainCounterValidFlag::EntitySpecific2, "Entity Specific 2" },
		{ la::avdecc::entity::ClockDomainCounterValidFlag::EntitySpecific3, "Entity Specific 3" },
		{ la::avdecc::entity::ClockDomainCounterValidFlag::EntitySpecific4, "Entity Specific 4" },
		{ la::avdecc::entity::ClockDomainCounterValidFlag::EntitySpecific5, "Entity Specific 5" },
		{ la::avdecc::entity::ClockDomainCounterValidFlag::EntitySpecific6, "Entity Specific 6" },
		{ la::avdecc::entity::ClockDomainCounterValidFlag::EntitySpecific7, "Entity Specific 7" },
		{ la::avdecc::entity::ClockDomainCounterValidFlag::EntitySpecific8, "Entity Specific 8" },
	};

	// Create fields
	using CounterType = decltype(s_counterNames)::key_type;
	for (auto bitPos = 0u; bitPos < (sizeof(std::underlying_type_t<CounterType>) * 8); ++bitPos)
	{
		auto const flag = static_cast<CounterType>(1u << bitPos);
		auto* widget = new QTreeWidgetItem(this);
		if (auto const nameIt = s_counterNames.find(flag); nameIt != s_counterNames.end())
		{
			widget->setText(0, nameIt->second);
		}
		else
		{
			widget->setText(0, QString{ "Unknown 0x%1" }.arg(1u << bitPos, 8, 16, QChar{ '0' }));
		}
		widget->setHidden(true); // Hide until we get a counter value (so we don't display counters not supported by the entity)
		_counters[flag] = widget;
	}

	// Update counters right now
	updateCounters(counters);

	// Listen for ClockDomainCountersChanged
	connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::clockDomainCountersChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockDomainCounters const& counters)
		{
			if (entityID == _entityID && clockDomainIndex == _clockDomainIndex)
			{
				updateCounters(counters);
			}
		});
}

void ClockDomainCountersTreeWidgetItem::updateCounters(la::avdecc::entity::model::ClockDomainCounters const& counters)
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
