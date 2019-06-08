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

#include "entityStatisticsTreeWidgetItem.hpp"

#include <QMenu>

EntityStatisticsTreeWidgetItem::EntityStatisticsTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const aecpRetryCounter, std::uint64_t const aecpTimeoutCounter, std::uint64_t const aecpUnexpectedResponseCounter, std::chrono::milliseconds const& aecpResponseAverageTime, std::uint64_t const aemAecpUnsolicitedCounter, std::chrono::milliseconds const& enumerationTime, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
{
	// Setup widgets
	_aecpRetryCounterItem.setText(0, "AECP Retries");
	_aecpTimeoutCounterItem.setText(0, "AECP Timeouts");
	_aecpUnexpectedResponseCounterItem.setText(0, "AECP Unexpected Responses");
	_aecpResponseAverageTimeItem.setText(0, "AECP Average Response Time");
	_aemAecpUnsolicitedCounterItem.setText(0, "AEM Unsolicited Responses");
	_enumerationTimeItem.setText(0, "Enumeration Time");

	// Update statistics right now
	updateAecpRetryCounter(aecpRetryCounter);
	updateAecpTimeoutCounter(aecpTimeoutCounter);
	updateAecpUnexpectedResponseCounter(aecpUnexpectedResponseCounter);
	updateAecpResponseAverageTime(aecpResponseAverageTime);
	updateAemAecpUnsolicitedCounter(aemAecpUnsolicitedCounter);
	_enumerationTimeItem.setText(1, QString::number(enumerationTime.count()) + " msec");

	// Listen for signals
	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::aecpRetryCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value)
		{
			if (entityID == _entityID)
			{
				updateAecpRetryCounter(value);
			}
		});
	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::aecpTimeoutCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value)
		{
			if (entityID == _entityID)
			{
				updateAecpTimeoutCounter(value);
			}
		});
	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::aecpUnexpectedResponseCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value)
		{
			if (entityID == _entityID)
			{
				updateAecpUnexpectedResponseCounter(value);
			}
		});
	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::aecpResponseAverageTimeChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const& value)
		{
			if (entityID == _entityID)
			{
				updateAecpResponseAverageTime(value);
			}
		});
	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::aemAecpUnsolicitedCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value)
		{
			if (entityID == _entityID)
			{
				updateAemAecpUnsolicitedCounter(value);
			}
		});
}

void EntityStatisticsTreeWidgetItem::updateAecpRetryCounter(std::uint64_t const value) noexcept
{
	_aecpRetryCounterItem.setText(1, QString::number(value));
}

void EntityStatisticsTreeWidgetItem::updateAecpTimeoutCounter(std::uint64_t const value) noexcept
{
	_aecpTimeoutCounterItem.setText(1, QString::number(value));
}

void EntityStatisticsTreeWidgetItem::updateAecpUnexpectedResponseCounter(std::uint64_t const value) noexcept
{
	_aecpUnexpectedResponseCounterItem.setText(1, QString::number(value));
}

void EntityStatisticsTreeWidgetItem::updateAecpResponseAverageTime(std::chrono::milliseconds const& value) noexcept
{
	_aecpResponseAverageTimeItem.setText(1, QString::number(value.count()) + " msec");
}

void EntityStatisticsTreeWidgetItem::updateAemAecpUnsolicitedCounter(std::uint64_t const value) noexcept
{
	_aemAecpUnsolicitedCounterItem.setText(1, QString::number(value));
}
