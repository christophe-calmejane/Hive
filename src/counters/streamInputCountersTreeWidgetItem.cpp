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

#include "streamInputCountersTreeWidgetItem.hpp"

#include <map>

#include <QMenu>

StreamInputCountersTreeWidgetItem::StreamInputCountersTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamInputCounters const& counters, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _streamIndex(streamIndex)
{
	static std::map<la::avdecc::entity::StreamInputCounterValidFlag, QString> s_counterNames{
		{ la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked, "Media Locked" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked, "Media Unlocked" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::StreamReset, "Stream Reset" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::SeqNumMismatch, "Seq Num Mismatch" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::MediaReset, "Media Reset" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::TimestampUncertain, "Timestamp Uncertain" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::TimestampValid, "Timestamp Valid" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::TimestampNotValid, "Timestamp Not Valid" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::UnsupportedFormat, "Unsupported Format" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::LateTimestamp, "Late Timestamp" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::EarlyTimestamp, "Early Timestamp" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::FramesRx, "Frames RX" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::FramesTx, "Frames TX" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::EntitySpecific1, "Entity Specific 1" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::EntitySpecific2, "Entity Specific 2" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::EntitySpecific3, "Entity Specific 3" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::EntitySpecific4, "Entity Specific 4" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::EntitySpecific5, "Entity Specific 5" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::EntitySpecific6, "Entity Specific 6" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::EntitySpecific7, "Entity Specific 7" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::EntitySpecific8, "Entity Specific 8" },
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

	// Listen for StreamInputCountersChanged
	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamInputCountersChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamInputCounters const& counters)
		{
			if (entityID == _entityID && streamIndex == _streamIndex)
			{
				updateCounters(counters);
			}
		});
}

void StreamInputCountersTreeWidgetItem::updateCounters(la::avdecc::controller::model::StreamInputCounters const& counters)
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
