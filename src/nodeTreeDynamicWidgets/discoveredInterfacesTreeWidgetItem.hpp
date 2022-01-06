/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <hive/modelsLibrary/controllerManager.hpp>

#include <QObject>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QListWidget>

#include <optional>
#include <cstdint>
#include <map>

class DiscoveredInterfaceTreeWidgetItem final : public QObject, public QTreeWidgetItem
{
public:
	DiscoveredInterfaceTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::Entity::InterfaceInformation const& interfaceInfo, QTreeWidgetItem* parent = nullptr);
	void updateGptpInfo(std::optional<la::avdecc::UniqueIdentifier> const& gptpGrandmasterID, std::optional<std::uint8_t> const gptpDomainNumber);

private:
	QTreeWidgetItem* _macAddress{ nullptr };
	QTreeWidgetItem* _grandmasterID{ nullptr };
	QTreeWidgetItem* _domainNumber{ nullptr };
	QTreeWidgetItem* _validTime{ nullptr };
};

class DiscoveredInterfacesTreeWidgetItem : public QObject, public QTreeWidgetItem
{
public:
	DiscoveredInterfacesTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::Entity::InterfacesInformation const& interfaces, QTreeWidget* parent = nullptr);

private:
	void addInterfaceInformation(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::Entity::InterfaceInformation const& interfaceInfo);
	void removeInterfaceInformation(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);

	la::avdecc::UniqueIdentifier const _entityID{};
	std::map<la::avdecc::entity::model::AvbInterfaceIndex, DiscoveredInterfaceTreeWidgetItem*> _discoveredInterfaces{};
};
