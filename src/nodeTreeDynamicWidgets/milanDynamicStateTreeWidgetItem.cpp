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

#include "milanDynamicStateTreeWidgetItem.hpp"
#include "avdecc/euiValidator.hpp"
#include "avdecc/stringValidator.hpp"
#include "accessibilityHelper.hpp"

#include <hive/modelsLibrary/helper.hpp>

#include <QMenu>

MilanDynamicStateTreeWidgetItem::MilanDynamicStateTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::MilanDynamicState const& milanDynamicState, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
{
	auto* systemUniqueIDItem = new QTreeWidgetItem(this);
	systemUniqueIDItem->setText(0, "System Unique ID");
	_systemUniqueID = new AecpCommandTextEntry("", avdecc::EUIValidator::getSharedInstance());
	accessibility::setTreeItemWidget(parent, systemUniqueIDItem, 1, _systemUniqueID);

	auto* systemNameItem = new QTreeWidgetItem(this);
	systemNameItem->setText(0, "System Name");
	_systemName = new AecpCommandTextEntry("", avdecc::AvdeccStringValidator::getSharedInstance());
	accessibility::setTreeItemWidget(parent, systemNameItem, 1, _systemName);

	// Send changes
	_systemUniqueID->setDataChangedHandler(
		[this](QString const& oldText, QString const& newText)
		{
			auto const systemUniqueID = static_cast<la::avdecc::UniqueIdentifier>(la::avdecc::utils::convertFromString<la::avdecc::UniqueIdentifier::value_type>(newText.toStdString().c_str()));
			auto const systemName = _systemName->getCurrentData();
			hive::modelsLibrary::ControllerManager::getInstance().setSystemUniqueID(_entityID, systemUniqueID, systemName, _systemUniqueID->getBeginCommandHandler(hive::modelsLibrary::ControllerManager::MilanCommandType::SetSystemUniqueID), _systemUniqueID->getResultHandler(hive::modelsLibrary::ControllerManager::MilanCommandType::SetSystemUniqueID, oldText));
		});
	_systemName->setDataChangedHandler(
		[this](QString const& oldText, QString const& newText)
		{
			auto const systemUniqueID = static_cast<la::avdecc::UniqueIdentifier>(la::avdecc::utils::convertFromString<la::avdecc::UniqueIdentifier::value_type>(_systemUniqueID->getCurrentData().toStdString().c_str()));
			hive::modelsLibrary::ControllerManager::getInstance().setSystemUniqueID(_entityID, systemUniqueID, newText, _systemUniqueID->getBeginCommandHandler(hive::modelsLibrary::ControllerManager::MilanCommandType::SetSystemUniqueID), _systemUniqueID->getResultHandler(hive::modelsLibrary::ControllerManager::MilanCommandType::SetSystemUniqueID, oldText));
		});

	// Listen for changes
	connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::systemUniqueIDChanged, _systemUniqueID,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::UniqueIdentifier const systemUniqueID, QString const& systemName)
		{
			if (entityID == _entityID)
			{
				updateSystemUniqueID(systemUniqueID, systemName);
			}
		});

	// Update now
	updateSystemUniqueID(milanDynamicState.systemUniqueID ? *milanDynamicState.systemUniqueID : la::avdecc::UniqueIdentifier{}, milanDynamicState.systemName ? QString::fromStdString(*milanDynamicState.systemName) : QString{});
}

void MilanDynamicStateTreeWidgetItem::updateSystemUniqueID(la::avdecc::UniqueIdentifier const systemUniqueID, QString const& systemName) noexcept
{
	_systemUniqueID->setCurrentData(hive::modelsLibrary::helper::uniqueIdentifierToString(systemUniqueID));
	_systemName->setCurrentData(systemName);
}
