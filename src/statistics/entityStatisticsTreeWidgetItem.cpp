/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QtMate/material/color.hpp>

#include <QMenu>

EntityStatisticsTreeWidgetItem::EntityStatisticsTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const& enumerationTime, bool const showPerInterfaceStatistics, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _showPerInterfaceStatistics(showPerInterfaceStatistics)
{
	// Setup widgets
	_aecpRetryCounterItem.setText(0, "AECP Retries");
	_aecpTimeoutCounterItem.setText(0, "AECP Timeouts");
	_aecpUnexpectedResponseCounterItem.setText(0, "AECP Unexpected Responses");
	_aecpResponseAverageTimeItem.setText(0, "AECP Average Response Time");
	_aemAecpUnsolicitedCounterItem.setText(0, "AEM Unsolicited Responses");
	_aemAecpUnsolicitedLossCounterItem.setText(0, "AEM Unsolicited Loss");
	_mvuAecpUnsolicitedCounterItem.setText(0, "MVU Unsolicited Responses");
	_mvuAecpUnsolicitedLossCounterItem.setText(0, "MVU Unsolicited Loss");
	_enumerationTimeItem.setText(0, "Enumeration Time");

	// Update statistics right now (the per-interface values are maintained by the library, the displayed entity-wide values are the sum of every interface)
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	_errorCounters = manager.getStatisticsCounters(_entityID);
	_perInterfaceStatistics = manager.getPerInterfaceStatistics(_entityID);
	updateAecpRetryCounter(interfaceTotal(&hive::modelsLibrary::ControllerManager::InterfaceStatistics::aecpRetryCounter));
	updateAecpTimeoutCounter(interfaceTotal(&hive::modelsLibrary::ControllerManager::InterfaceStatistics::aecpTimeoutCounter));
	updateAecpUnexpectedResponseCounter(interfaceTotal(&hive::modelsLibrary::ControllerManager::InterfaceStatistics::aecpUnexpectedResponseCounter));
	updateAecpResponseAverageTime();
	updateAemAecpUnsolicitedCounter(interfaceTotal(&hive::modelsLibrary::ControllerManager::InterfaceStatistics::aemAecpUnsolicitedCounter));
	updateAemAecpUnsolicitedLossCounter(interfaceTotal(&hive::modelsLibrary::ControllerManager::InterfaceStatistics::aemAecpUnsolicitedLossCounter));
	updateMvuAecpUnsolicitedCounter(interfaceTotal(&hive::modelsLibrary::ControllerManager::InterfaceStatistics::mvuAecpUnsolicitedCounter));
	updateMvuAecpUnsolicitedLossCounter(interfaceTotal(&hive::modelsLibrary::ControllerManager::InterfaceStatistics::mvuAecpUnsolicitedLossCounter));
	_enumerationTimeItem.setText(1, QString::number(enumerationTime.count()) + " msec");

	// Listen for signals
	connect(&manager, &hive::modelsLibrary::ControllerManager::aecpRetryCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			if (entityID == _entityID)
			{
				_perInterfaceStatistics[la::avdecc::utils::to_integral(interfaceType)].aecpRetryCounter = interfaceValue;
				updateAecpRetryCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::aecpTimeoutCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			if (entityID == _entityID)
			{
				_perInterfaceStatistics[la::avdecc::utils::to_integral(interfaceType)].aecpTimeoutCounter = interfaceValue;
				updateAecpTimeoutCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::aecpUnexpectedResponseCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			if (entityID == _entityID)
			{
				_perInterfaceStatistics[la::avdecc::utils::to_integral(interfaceType)].aecpUnexpectedResponseCounter = interfaceValue;
				updateAecpUnexpectedResponseCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::aecpResponseAverageTimeChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const& value, la::avdecc::controller::InterfaceType const interfaceType)
		{
			if (entityID == _entityID)
			{
				_perInterfaceStatistics[la::avdecc::utils::to_integral(interfaceType)].aecpResponseAverageTime = value;
				updateAecpResponseAverageTime();
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::aemAecpUnsolicitedCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			if (entityID == _entityID)
			{
				_perInterfaceStatistics[la::avdecc::utils::to_integral(interfaceType)].aemAecpUnsolicitedCounter = interfaceValue;
				updateAemAecpUnsolicitedCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::aemAecpUnsolicitedLossCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			if (entityID == _entityID)
			{
				_perInterfaceStatistics[la::avdecc::utils::to_integral(interfaceType)].aemAecpUnsolicitedLossCounter = interfaceValue;
				updateAemAecpUnsolicitedLossCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::mvuAecpUnsolicitedCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			if (entityID == _entityID)
			{
				_perInterfaceStatistics[la::avdecc::utils::to_integral(interfaceType)].mvuAecpUnsolicitedCounter = interfaceValue;
				updateMvuAecpUnsolicitedCounter(value);
			}
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::mvuAecpUnsolicitedLossCounterChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value, la::avdecc::controller::InterfaceType const interfaceType, std::uint64_t const interfaceValue)
		{
			if (entityID == _entityID)
			{
				_perInterfaceStatistics[la::avdecc::utils::to_integral(interfaceType)].mvuAecpUnsolicitedLossCounter = interfaceValue;
				updateMvuAecpUnsolicitedLossCounter(value);
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
				updateMvuAecpUnsolicitedLossCounter(_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::MvuAecpUnsolicitedLosses]);
			}
		});
}

std::uint64_t EntityStatisticsTreeWidgetItem::interfaceTotal(std::uint64_t hive::modelsLibrary::ControllerManager::InterfaceStatistics::*const interfaceField) const noexcept
{
	auto total = std::uint64_t{ 0ull };
	for (auto const& interfaceStatistics : _perInterfaceStatistics)
	{
		total += interfaceStatistics.*interfaceField;
	}
	return total;
}

QString EntityStatisticsTreeWidgetItem::perInterfaceSuffix(std::uint64_t hive::modelsLibrary::ControllerManager::InterfaceStatistics::*const interfaceField) const noexcept
{
	auto const primaryValue = _perInterfaceStatistics[la::avdecc::utils::to_integral(la::avdecc::controller::InterfaceType::Primary)].*interfaceField;
	auto const secondaryValue = _perInterfaceStatistics[la::avdecc::utils::to_integral(la::avdecc::controller::InterfaceType::Secondary)].*interfaceField;
	return QString{ " (Primary: %1, Secondary: %2)" }.arg(primaryValue).arg(secondaryValue);
}

void EntityStatisticsTreeWidgetItem::setWidgetTextAndColor(EntityStatisticTreeWidgetItem& widget, std::uint64_t const value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag const flag, std::uint64_t hive::modelsLibrary::ControllerManager::InterfaceStatistics::*const interfaceField) noexcept
{
	auto color = qtMate::material::color::foregroundColor();
	auto text = QString::number(value);

	auto const errorCounterIt = _errorCounters.find(flag);
	if (errorCounterIt != _errorCounters.end())
	{
		color = qtMate::material::color::foregroundErrorColorValue(qtMate::material::color::backgroundColorName(), qtMate::material::color::colorSchemeShade());
		text += QString(" (+%1)").arg(errorCounterIt->second);
	}

	if (_showPerInterfaceStatistics)
	{
		text += perInterfaceSuffix(interfaceField);
	}

	widget.setForeground(0, color);
	widget.setForeground(1, color);

	widget.setText(1, text);
	widget.setHidden(false);
}

void EntityStatisticsTreeWidgetItem::updateAecpRetryCounter(std::uint64_t const value) noexcept
{
	_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpRetries] = value;
	setWidgetTextAndColor(_aecpRetryCounterItem, value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpRetries, &hive::modelsLibrary::ControllerManager::InterfaceStatistics::aecpRetryCounter);
}

void EntityStatisticsTreeWidgetItem::updateAecpTimeoutCounter(std::uint64_t const value) noexcept
{
	_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpTimeouts] = value;
	setWidgetTextAndColor(_aecpTimeoutCounterItem, value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpTimeouts, &hive::modelsLibrary::ControllerManager::InterfaceStatistics::aecpTimeoutCounter);
}

void EntityStatisticsTreeWidgetItem::updateAecpUnexpectedResponseCounter(std::uint64_t const value) noexcept
{
	_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpUnexpectedResponses] = value;
	setWidgetTextAndColor(_aecpUnexpectedResponseCounterItem, value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AecpUnexpectedResponses, &hive::modelsLibrary::ControllerManager::InterfaceStatistics::aecpUnexpectedResponseCounter);
}

void EntityStatisticsTreeWidgetItem::updateAecpResponseAverageTime() noexcept
{
	auto const& primaryValue = _perInterfaceStatistics[la::avdecc::utils::to_integral(la::avdecc::controller::InterfaceType::Primary)].aecpResponseAverageTime;
	auto text = QString{};
	if (_showPerInterfaceStatistics)
	{
		// Each interface maintains its own average, there is no meaningful entity-wide value
		auto const& secondaryValue = _perInterfaceStatistics[la::avdecc::utils::to_integral(la::avdecc::controller::InterfaceType::Secondary)].aecpResponseAverageTime;
		text = QString{ "Primary: %1 msec, Secondary: %2 msec" }.arg(primaryValue.count()).arg(secondaryValue.count());
	}
	else
	{
		text = QString::number(primaryValue.count()) + " msec";
	}
	_aecpResponseAverageTimeItem.setText(1, text);
}

void EntityStatisticsTreeWidgetItem::updateAemAecpUnsolicitedCounter(std::uint64_t const value) noexcept
{
	auto text = QString::number(value);
	if (_showPerInterfaceStatistics)
	{
		text += perInterfaceSuffix(&hive::modelsLibrary::ControllerManager::InterfaceStatistics::aemAecpUnsolicitedCounter);
	}
	_aemAecpUnsolicitedCounterItem.setText(1, text);
}

void EntityStatisticsTreeWidgetItem::updateAemAecpUnsolicitedLossCounter(std::uint64_t const value) noexcept
{
	_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AemAecpUnsolicitedLosses] = value;
	setWidgetTextAndColor(_aemAecpUnsolicitedLossCounterItem, value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::AemAecpUnsolicitedLosses, &hive::modelsLibrary::ControllerManager::InterfaceStatistics::aemAecpUnsolicitedLossCounter);
}

void EntityStatisticsTreeWidgetItem::updateMvuAecpUnsolicitedCounter(std::uint64_t const value) noexcept
{
	auto text = QString::number(value);
	if (_showPerInterfaceStatistics)
	{
		text += perInterfaceSuffix(&hive::modelsLibrary::ControllerManager::InterfaceStatistics::mvuAecpUnsolicitedCounter);
	}
	_mvuAecpUnsolicitedCounterItem.setText(1, text);
}

void EntityStatisticsTreeWidgetItem::updateMvuAecpUnsolicitedLossCounter(std::uint64_t const value) noexcept
{
	_counters[hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::MvuAecpUnsolicitedLosses] = value;
	setWidgetTextAndColor(_mvuAecpUnsolicitedLossCounterItem, value, hive::modelsLibrary::ControllerManager::StatisticsErrorCounterFlag::MvuAecpUnsolicitedLosses, &hive::modelsLibrary::ControllerManager::InterfaceStatistics::mvuAecpUnsolicitedLossCounter);
}
