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

#include "avbInterfaceDynamicTreeWidgetItem.hpp"
#include "asPathWidget.hpp"
#include "avdecc/helper.hpp"

#include <unordered_map>
#include <string>

#include <QListWidgetItem>
#include <QString>

AvbInterfaceDynamicTreeWidgetItem::AvbInterfaceDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::controller::model::AvbInterfaceNodeDynamicModel const* const dynamicModel, QTreeWidget* parent)
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

		// Update info right now
		updateAvbInfo(dynamicModel->avbInfo);

		// Listen for AvbInfoChanged
		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::avbInfoChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInfo const& info)
			{
				if (entityID == _entityID && avbInterfaceIndex == _avbInterfaceIndex)
				{
					updateAvbInfo(info);
				}
			});
	}

	// AsPath
	{
		// Create fields
		auto* item = new QTreeWidgetItem(this);
		item->setText(0, "As Path");
		_asPath = new QListWidget;
		_asPath->setStyleSheet(".QListWidget{margin-top:4px;margin-bottom:4px}");
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
	_flags->setText(1, avdecc::helper::toHexQString(la::avdecc::to_integral(avbInfo.flags), true, true) + QString(" (") + avdecc::helper::flagsToString(avbInfo.flags) + QString(")"));
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
