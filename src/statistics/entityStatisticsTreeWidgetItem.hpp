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

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>

#include "avdecc/helper.hpp"
#include "avdecc/controllerManager.hpp"

#include <chrono>

#include <QObject>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>

class EntityStatisticTreeWidgetItem : public QTreeWidgetItem
{
public:
	EntityStatisticTreeWidgetItem(avdecc::ControllerManager::StatisticsErrorCounterFlag const flag, QTreeWidgetItem* parent)
		: QTreeWidgetItem{ parent, la::avdecc::utils::to_integral(NodeTreeWidget::TreeWidgetItemType::EntityStatistic) }
		, _counterFlag{ flag }
	{
	}

	avdecc::ControllerManager::StatisticsErrorCounterFlag counterFlag() const
	{
		return _counterFlag;
	}

private:
	avdecc::ControllerManager::StatisticsErrorCounterFlag const _counterFlag;
};

class EntityStatisticsTreeWidgetItem : public QObject, public QTreeWidgetItem
{
public:
	EntityStatisticsTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const aecpRetryCounter, std::uint64_t const aecpTimeoutCounter, std::uint64_t const aecpUnexpectedResponseCounter, std::chrono::milliseconds const& aecpResponseAverageTime, std::uint64_t const aemAecpUnsolicitedCounter, std::chrono::milliseconds const& enumerationTime, QTreeWidget* parent = nullptr);
	virtual ~EntityStatisticsTreeWidgetItem() override
	{
		takeChildren();
	}

private:
	void setWidgetTextAndColor(EntityStatisticTreeWidgetItem& widget, std::uint64_t const value, avdecc::ControllerManager::StatisticsErrorCounterFlag const flag) noexcept;
	void updateAecpRetryCounter(std::uint64_t const value) noexcept;
	void updateAecpTimeoutCounter(std::uint64_t const value) noexcept;
	void updateAecpUnexpectedResponseCounter(std::uint64_t const value) noexcept;
	void updateAecpResponseAverageTime(std::chrono::milliseconds const& value) noexcept;
	void updateAemAecpUnsolicitedCounter(std::uint64_t const value) noexcept;

	la::avdecc::UniqueIdentifier const _entityID{};

	// Statistics
	EntityStatisticTreeWidgetItem _aecpRetryCounterItem{ avdecc::ControllerManager::StatisticsErrorCounterFlag::AecpRetries, this };
	EntityStatisticTreeWidgetItem _aecpTimeoutCounterItem{ avdecc::ControllerManager::StatisticsErrorCounterFlag::AecpTimeouts, this };
	EntityStatisticTreeWidgetItem _aecpUnexpectedResponseCounterItem{ avdecc::ControllerManager::StatisticsErrorCounterFlag::AecpUnexpectedResponses, this };
	QTreeWidgetItem _aecpResponseAverageTimeItem{ this };
	QTreeWidgetItem _aemAecpUnsolicitedCounterItem{ this };
	QTreeWidgetItem _enumerationTimeItem{ this };
	avdecc::ControllerManager::StatisticsErrorCounters _counters{};
	avdecc::ControllerManager::StatisticsErrorCounters _errorCounters{};
};
