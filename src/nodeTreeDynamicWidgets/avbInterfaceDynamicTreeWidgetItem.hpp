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

#pragma once

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>

#include "avdecc/helper.hpp"
#include "avdecc/controllerManager.hpp"

#include <QObject>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QListWidget>

class AvbInterfaceDynamicTreeWidgetItem : public QObject, public QTreeWidgetItem
{
public:
	AvbInterfaceDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceNodeDynamicModel const* const dynamicModel, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus, QTreeWidget* parent = nullptr);

private:
	void restoreAvbInterfaceInfoVisibility();
	void restoreLinkStatusVisibility();
	void restoreAsPathVisibility();
	void updateGptpInfo(la::avdecc::UniqueIdentifier const& gptpGrandmasterID, std::uint8_t const gptpDomainNumber);
	void updateAvbInterfaceInfo(la::avdecc::entity::model::AvbInterfaceInfo const& avbInfo);
	void updateLinkStatus(la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus);
	void updateAsPath(la::avdecc::entity::model::AsPath const& asPath);

	la::avdecc::UniqueIdentifier const _entityID{};
	la::avdecc::entity::model::AvbInterfaceIndex const _avbInterfaceIndex{ 0u };

	// AvbInfo
	QTreeWidgetItem* _gptpGrandmasterID{ nullptr };
	QTreeWidgetItem* _gptpDomainNumber{ nullptr };
	QTreeWidgetItem* _propagationDelay{ nullptr };
	QTreeWidgetItem* _flags{ nullptr };
	QTreeWidgetItem* _linkStatus{ nullptr };

	// AsPath
	QTreeWidgetItem* _asPathItem{ nullptr };
	QListWidget* _asPath{ nullptr };
};
