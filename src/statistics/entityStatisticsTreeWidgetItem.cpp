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

#include "entityStatisticsTreeWidgetItem.hpp"

#include <QMenu>

EntityStatisticsTreeWidgetItem::EntityStatisticsTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const aecpRetryCounter, std::uint64_t const aecpTimeoutCounter, std::uint64_t const aecpUnexpectedResponseCounter, std::chrono::milliseconds const& aecpResponseAverageTime, std::uint64_t const aemAecpUnsolicitedCounter, std::uint64_t const aemAecpUnsolicitedLossCounter, std::chrono::milliseconds const& enumerationTime, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
{
	// Setup widgets
	_aecpRetryCounterItem.setText(0, "AECP Retries");
	_aecpTimeoutCounterItem.setText(0, "AECP Timeouts");
	_aecpUnexpectedResponseCounterItem.setText(0, "AECP Unexpected Responses");
	_aecpResponseAverageTimeItem.setText(0, "AECP Average Response Time");
	_aemAecpUnsolicitedCounterItem.setText(0, "AEM Unsolicited Responses");
	_aemAecpUnsolicitedLossCounterItem.setText(0, "AEM Unsolicited Loss");
	_enumerationTimeItem.setText(0, "Enumeration Time");

	// Update statistics right now
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	_errorCounters = manager.getStatisticsCounters(_entityID);
	updateAecpRetryCounter(aecpRetryCounter);
	updateAecpTimeoutCounter(aecpTimeoutCounter);
	updateAecpUnexpectedResponseCounter(aecpUnexpectedResponseCounter);
	updateAecpResponseAverageTime(aecpResponseAverageTime);
	updateAemAecpUnsolicitedCounter(aemAecpUnsolicitedCounter);
	updateAemAecpUnsolicitedLossCounter(aemAecpUnsolicitedLossCounter);
	_enumerationTimeItem.setText(1, QString::number(enumerationTime.count()) + " msec");

	// Listen for signals
	connect(&manager, &hive::modelsLibrary::ControllerManager::aecpRetryCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value)
		{
			if (entityID == _entityID)
			{
				updateAecpRetryCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::aecpTimeoutCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value)
		{
			if (entityID == _entityID)
			{
				updateAecpTimeoutCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::aecpUnexpectedResponseCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value)
		{
			if (entityID == _entityID)
			{
				updateAecpUnexpectedResponseCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::aecpResponseAverageTimeChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const& value)
		{
			if (entityID == _entityID)
			{
				updateAecpResponseAverageTime(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::aemAecpUnsolicitedCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value)
		{
			if (entityID == _entityID)
			{
				updateAemAecpUnsolicitedCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::aemAecpUnsolicitedLossCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value)
		{
			if (entityID == _entityID)
			{
				updateAemAecpUnsolicitedLossCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::statisticsErrorCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::StatisticsErrorCounters const& errorCounters)
		{
			if (entityID == _entityID)
			{
				_errorCounters = errorCounters;
				updateAecpRetryCounter(_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpRetries]);
				updateAecpTimeoutCounter(_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpTimeouts]);
				updateAecpUnexpectedResponseCounter(_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpUnexpectedResponses]);
				updateAemAecpUnsolicitedLossCounter(_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AemAecpUnsolicitedLosses]);
			}
		});
}

void EntityStatisticsTreeWidgetItem::setWidgetTextAndColor(EntityStatisticTreeWidgetItem& widget, std::uint64_t const value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag const flag) noexcept
{
	auto color = qtMate::material::color::foregroundColor();
	auto text = QString::number(value);

	auto const errorCounterIt = _errorCounters.find(flag);
	if (errorCounterIt != _errorCounters.end())
	{
		color = qtMate::material::color::foregroundErrorColorValue(qtMate::material::color::backgroundColorName(), qtMate::material::color::colorSchemeShade());
		text += QString(" (+%1)").arg(errorCounterIt->second);
	}

	widget.setForeground(0, color);
	widget.setForeground(1, color);

	widget.setText(1, text);
	widget.setHidden(false);
}

void EntityStatisticsTreeWidgetItem::updateAecpRetryCounter(std::uint64_t const value) noexcept
{
	_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpRetries] = value;
	setWidgetTextAndColor(_aecpRetryCounterItem, value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpRetries);
}

void EntityStatisticsTreeWidgetItem::updateAecpTimeoutCounter(std::uint64_t const value) noexcept
{
	_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpTimeouts] = value;
	setWidgetTextAndColor(_aecpTimeoutCounterItem, value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpTimeouts);
}

void EntityStatisticsTreeWidgetItem::updateAecpUnexpectedResponseCounter(std::uint64_t const value) noexcept
{
	_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpUnexpectedResponses] = value;
	setWidgetTextAndColor(_aecpUnexpectedResponseCounterItem, value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpUnexpectedResponses);
}

void EntityStatisticsTreeWidgetItem::updateAecpResponseAverageTime(std::chrono::milliseconds const& value) noexcept
{
	_aecpResponseAverageTimeItem.setText(1, QString::number(value.count()) + " msec");
}

void EntityStatisticsTreeWidgetItem::updateAemAecpUnsolicitedCounter(std::uint64_t const value) noexcept
{
	_aemAecpUnsolicitedCounterItem.setText(1, QString::number(value));
}

void EntityStatisticsTreeWidgetItem::updateAemAecpUnsolicitedLossCounter(std::uint64_t const value) noexcept
{
	_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AemAecpUnsolicitedLosses] = value;
	setWidgetTextAndColor(_aemAecpUnsolicitedLossCounterItem, value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AemAecpUnsolicitedLosses);
}
