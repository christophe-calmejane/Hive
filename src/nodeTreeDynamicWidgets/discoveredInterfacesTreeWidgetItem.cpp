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

#include "discoveredInterfacesTreeWidgetItem.hpp"
#include "asPathWidget.hpp"
#include "nodeTreeWidget.hpp"
#include "avdecc/helper.hpp"

#include <hive/modelsLibrary/helper.hpp>

#include <unordered_map>
#include <string>

#include <QListWidgetItem>
#include <QString>

DiscoveredInterfaceTreeWidgetItem::DiscoveredInterfaceTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::Entity::InterfaceInformation const& interfaceInfo, QTreeWidgetItem* parent)
	: QTreeWidgetItem(parent)
{
	// Create fields
	_macAddress = new QTreeWidgetItem(this);
	_macAddress->setText(0, "MAC Address");

	_grandmasterID = new QTreeWidgetItem(this);
	_grandmasterID->setText(0, "Grandmaster ID");

	_domainNumber = new QTreeWidgetItem(this);
	_domainNumber->setText(0, "Domain Number");

	_validTime = new QTreeWidgetItem(this);
	_validTime->setText(0, "Valid Time (2sec periods)");

	// Set static info
	_macAddress->setText(1, QString::fromStdString(la::networkInterface::NetworkInterfaceHelper::macAddressToString(interfaceInfo.macAddress, true)));
	_validTime->setText(1, QString::number(interfaceInfo.validTime)); // TODO: Should be a dynamic value, which requires an avdecc controller event

	// Update dynamic info right now
	updateGptpInfo(interfaceInfo.gptpGrandmasterID, interfaceInfo.gptpDomainNumber);

	// Listen for onGptpChanged
	connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::gptpChanged, this,
		[this, eid = entityID, interfaceIndex = avbInterfaceIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
		{
			if (entityID == eid && avbInterfaceIndex == interfaceIndex)
			{
				updateGptpInfo(grandMasterID, grandMasterDomain);
			}
		});
}

void DiscoveredInterfaceTreeWidgetItem::updateGptpInfo(std::optional<la::avdecc::UniqueIdentifier> const& gptpGrandmasterID, std::optional<std::uint8_t> const gptpDomainNumber)
{
	if (gptpGrandmasterID)
	{
		_grandmasterID->setText(1, hive::modelsLibrary::helper::uniqueIdentifierToString(*gptpGrandmasterID));
	}
	else
	{
		_grandmasterID->setText(1, "Not Set");
	}

	if (gptpDomainNumber)
	{
		_domainNumber->setText(1, QString::number(*gptpDomainNumber));
	}
	else
	{
		_domainNumber->setText(1, "Not Set");
	}
}

DiscoveredInterfacesTreeWidgetItem::DiscoveredInterfacesTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::Entity::InterfacesInformation const& interfaces, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
{
	for (auto const& [interfaceIndex, interfaceInfo] : interfaces)
	{
		addInterfaceInformation(interfaceIndex, interfaceInfo);
	}

	// Listen for Discovered Interfaces changes
	connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::entityRedundantInterfaceOnline, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::Entity::InterfaceInformation const& interfaceInfo)
		{
			if (_entityID == entityID)
			{
				addInterfaceInformation(avbInterfaceIndex, interfaceInfo);
			}
		});
	connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::entityRedundantInterfaceOffline, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex)
		{
			if (_entityID == entityID)
			{
				removeInterfaceInformation(avbInterfaceIndex);
			}
		});
}

void DiscoveredInterfacesTreeWidgetItem::addInterfaceInformation(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::Entity::InterfaceInformation const& interfaceInfo)
{
	auto const it = _discoveredInterfaces.find(avbInterfaceIndex);
	if (!AVDECC_ASSERT_WITH_RET(it == _discoveredInterfaces.end(), "Interface should not already exist"))
	{
		it->second->updateGptpInfo(interfaceInfo.gptpGrandmasterID, interfaceInfo.gptpDomainNumber);
		return;
	}

	auto* discoveryItem = new DiscoveredInterfaceTreeWidgetItem(_entityID, avbInterfaceIndex, interfaceInfo, nullptr);
	if (avbInterfaceIndex != la::avdecc::entity::Entity::GlobalAvbInterfaceIndex)
	{
		discoveryItem->setText(0, QString("Interface Index %1").arg(avbInterfaceIndex));
	}
	else
	{
		discoveryItem->setText(0, QString("Global Interface (Index Not Set)"));
	}

	// Insert in our map and insert in TreeWidget
	auto [insertedIt, success] = _discoveredInterfaces.emplace(avbInterfaceIndex, discoveryItem);
	if (success)
	{
		insertChild(std::distance(_discoveredInterfaces.begin(), insertedIt), discoveryItem);
	}

	// Expand the item
	discoveryItem->setExpanded(true);
}

void DiscoveredInterfacesTreeWidgetItem::removeInterfaceInformation(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex)
{
	if (auto const it = _discoveredInterfaces.find(avbInterfaceIndex); it != _discoveredInterfaces.end())
	{
		removeChild(it->second);
		_discoveredInterfaces.erase(it);
	}
}
