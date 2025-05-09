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

#include "streamInputCountersTreeWidgetItem.hpp"

#include <QtMate/material/color.hpp>

#include <map>
#include <QMenu>

StreamInputCountersTreeWidgetItem::StreamInputCountersTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isConnected, la::avdecc::entity::model::StreamInputCounters const& counters, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _streamIndex(streamIndex)
	, _isConnected{ isConnected }
{
	static std::map<la::avdecc::entity::StreamInputCounterValidFlag, QString> s_counterNames{
		{ la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked, "Media Locked" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked, "Media Unlocked" },
		{ la::avdecc::entity::StreamInputCounterValidFlag::StreamInterrupted, "Stream Interrupted" },
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
	using CounterType = decltype(s_counterNames)::key_type;
	for (auto bitPos = 0u; bitPos < (sizeof(std::underlying_type_t<CounterType>) * 8); ++bitPos)
	{
		auto const flag = static_cast<CounterType>(1u << bitPos);
		auto* widget = new StreamInputCounterTreeWidgetItem{ _streamIndex, flag, this };
		if (auto const nameIt = s_counterNames.find(flag); nameIt != s_counterNames.end())
		{
			widget->setText(0, nameIt->second);
		}
		else
		{
			widget->setText(0, QString{ "Unknown 0x%1" }.arg(1u << bitPos, 8, 16, QChar{ '0' }));
		}
		widget->setHidden(true); // Hide until we get a counter value (so we don't display counters not supported by the entity)
		_counterWidgets[flag] = widget;
	}

	// Update counters right now
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	_errorCounters = manager.getStreamInputErrorCounters(_entityID, _streamIndex);
	updateCounters(counters);

	// Listen for StreamInputCountersChanged
	connect(&manager, &hive::modelsLibrary::ControllerManager::streamInputCountersChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInputCounters const& counters)
		{
			if (entityID == _entityID && streamIndex == _streamIndex)
			{
				updateCounters(counters);
			}
		});

	connect(&manager, &hive::modelsLibrary::ControllerManager::streamInputErrorCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, hive::modelsLibrary::ControllerManager::StreamInputErrorCounters const& errorCounters)
		{
			if (entityID == _entityID && streamIndex == _streamIndex)
			{
				_errorCounters = errorCounters;
				updateCounters(_counters);
			}
		});

	connect(&manager, &hive::modelsLibrary::ControllerManager::streamInputConnectionChanged, this,
		[this](la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamInputConnectionInfo const& info)
		{
			if (stream.entityID == _entityID && stream.streamIndex == _streamIndex)
			{
				_isConnected = info.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected;
				updateCounters(_counters);
			}
		});
}

void StreamInputCountersTreeWidgetItem::updateCounters(la::avdecc::entity::model::StreamInputCounters const& counters)
{
	_counters = counters;

	for (auto const [flag, value] : _counters)
	{
		if (auto const it = _counterWidgets.find(flag); it != _counterWidgets.end())
		{
			auto* widget = it->second;
			AVDECC_ASSERT(widget != nullptr, "If widget is found in the map, it should not be nullptr");

			auto color = QColor{ _isConnected ? qtMate::material::color::foregroundColor() : qtMate::material::color::disabledForegroundColor() };
			auto text = QString::number(value);

			auto const errorCounterIt = _errorCounters.find(flag);
			if (errorCounterIt != _errorCounters.end())
			{
				color = QColor{ Qt::red };
				text += QString(" (+%1)").arg(errorCounterIt->second);
			}

			widget->setForeground(0, color);
			widget->setForeground(1, color);

			widget->setText(1, text);
			widget->setHidden(false);
		}
	}

	setText(0, _isConnected ? "Counters" : "Counters (Frozen)");
}
