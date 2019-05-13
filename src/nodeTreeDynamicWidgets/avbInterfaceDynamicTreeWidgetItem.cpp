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

#include "avbInterfaceDynamicTreeWidgetItem.hpp"
#include "asPathWidget.hpp"
#include "nodeTreeWidget.hpp"
#include "avdecc/helper.hpp"

#include <unordered_map>
#include <string>

#include <QListWidgetItem>
#include <QString>

AvbInterfaceDynamicTreeWidgetItem::AvbInterfaceDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceNodeDynamicModel const* const dynamicModel, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _avbInterfaceIndex(avbInterfaceIndex)
{
	// AvbInfo
	{
		// Create fields
		_gptpGrandmasterID = new QTreeWidgetItem(this);
		_gptpGrandmasterID->setText(0, "Grandmaster ID");

		_gptpDomainNumber = new QTreeWidgetItem(this);
		_gptpDomainNumber->setText(0, "Grandmaster Domain Number");

		_propagationDelay = new QTreeWidgetItem(this);
		_propagationDelay->setText(0, "Propagation Delay");

		_flags = new QTreeWidgetItem(this);
		_flags->setText(0, "Flags");

		_linkStatus = new QTreeWidgetItem(this);
		_linkStatus->setText(0, "Link State");

		// Update info right now
		updateAvbInfo(dynamicModel->avbInfo);
		updateLinkStatus(linkStatus);

		// Listen for AvbInfoChanged
		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::avbInfoChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInfo const& info)
			{
				if (entityID == _entityID && avbInterfaceIndex == _avbInterfaceIndex)
				{
					updateAvbInfo(info);
				}
			});
		// Listen for avbInterfaceLinkStatusChanged
		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::avbInterfaceLinkStatusChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus)
			{
				if (entityID == _entityID && avbInterfaceIndex == _avbInterfaceIndex)
				{
					updateLinkStatus(linkStatus);
				}
			});
	}

	// AsPath
	{
		// Create fields
		auto* item = new QTreeWidgetItem(this);
		item->setText(0, "As Path");
		_asPath = new QListWidget;
		_asPath->setSelectionMode(QAbstractItemView::NoSelection);
		parent->setItemWidget(item, 1, _asPath);

		// Update info right now
		updateAsPath(dynamicModel->asPath);

		// Listen for AsPathChanged
		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::asPathChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AsPath const& asPath)
			{
				if (entityID == _entityID && avbInterfaceIndex == _avbInterfaceIndex)
				{
					updateAsPath(asPath);
				}
			});
	}
}

void AvbInterfaceDynamicTreeWidgetItem::updateAvbInfo(la::avdecc::entity::model::AvbInfo const& avbInfo)
{
	_gptpGrandmasterID->setText(1, avdecc::helper::uniqueIdentifierToString(avbInfo.gptpGrandmasterID));
	_gptpDomainNumber->setText(1, QString::number(avbInfo.gptpDomainNumber));
	_propagationDelay->setText(1, QString("%1 nsec").arg(avbInfo.propagationDelay));
	setFlagsItemText(_flags, la::avdecc::utils::forceNumeric(avbInfo.flags.value()), avdecc::helper::flagsToString(avbInfo.flags));
}

void AvbInterfaceDynamicTreeWidgetItem::updateLinkStatus(la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus)
{
	switch (linkStatus)
	{
		case la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Unknown:
			_linkStatus->setText(1, "Unknown");
			break;
		case la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down:
			_linkStatus->setText(1, "Down");
			break;
		case la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Up:
			_linkStatus->setText(1, "Up");
			break;
		default:
			_linkStatus->setText(1, "Unknown");
			AVDECC_ASSERT(false, "Unhandled case");
			break;
	}
}

void AvbInterfaceDynamicTreeWidgetItem::updateAsPath(la::avdecc::entity::model::AsPath const& asPath)
{
	_asPath->clear();

	for (auto const& bridgeID : asPath.sequence)
	{
		auto* widget = new AsPathWidget{ bridgeID, avdecc::helper::getVendorName(bridgeID) };
		auto* item = new QListWidgetItem(_asPath);
		item->setSizeHint(widget->sizeHint());

		_asPath->setItemWidget(item, widget);
	}
}
