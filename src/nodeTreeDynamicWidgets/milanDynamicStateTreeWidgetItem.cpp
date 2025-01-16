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

#include "milanDynamicStateTreeWidgetItem.hpp"
#include "avdecc/numberValidator.hpp"

#include <QMenu>

Q_DECLARE_METATYPE(la::avdecc::entity::model::SystemUniqueIdentifier)

MilanDynamicStateTreeWidgetItem::MilanDynamicStateTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::MilanDynamicState const& milanDynamicState, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
{
	auto* systemUniqueIDItem = new QTreeWidgetItem(this);
	systemUniqueIDItem->setText(0, "System Unique ID");

	static_assert(!std::is_same_v<la::avdecc::entity::model::SystemUniqueIdentifier, la::avdecc::UniqueIdentifier>, "Update this code and use avdecc::EUIValidator::getSharedInstance()");
	_systemUniqueID = new AecpCommandTextEntry("", avdecc::PositiveIntegerValidator<std::numeric_limits<la::avdecc::entity::model::SystemUniqueIdentifier>::max()>::getSharedInstance());
	parent->setItemWidget(systemUniqueIDItem, 1, _systemUniqueID);

	// Send changes
	_systemUniqueID->setDataChangedHandler(
		[this](QString const& oldText, QString const& newText)
		{
			auto const systemUniqueID = static_cast<la::avdecc::entity::model::SystemUniqueIdentifier>(la::avdecc::utils::convertFromString<la::avdecc::entity::model::SystemUniqueIdentifier>(newText.toStdString().c_str()));
			hive::modelsLibrary::ControllerManager::getInstance().setSystemUniqueID(_entityID, systemUniqueID, _systemUniqueID->getBeginCommandHandler(hive::modelsLibrary::ControllerManager::MilanCommandType::SetSystemUniqueID), _systemUniqueID->getResultHandler(hive::modelsLibrary::ControllerManager::MilanCommandType::SetSystemUniqueID, oldText));
		});

	// Listen for changes
	connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::systemUniqueIDChanged, _systemUniqueID,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::SystemUniqueIdentifier const systemUniqueID)
		{
			if (entityID == _entityID)
			{
				updateSystemUniqueID(systemUniqueID);
			}
		});

	// Update now
	updateSystemUniqueID(*milanDynamicState.systemUniqueID);
}

void MilanDynamicStateTreeWidgetItem::updateSystemUniqueID(la::avdecc::entity::model::SystemUniqueIdentifier const systemUniqueID) noexcept
{
	static_assert(!std::is_same_v<la::avdecc::entity::model::SystemUniqueIdentifier, la::avdecc::UniqueIdentifier>, "Update this code to handle SystemUniqueIdentifier as a UniqueIdentifier using: hive::modelsLibrary::helper::uniqueIdentifierToString(systemUniqueID)");
	_systemUniqueID->setCurrentData(QString::number(systemUniqueID));
}
