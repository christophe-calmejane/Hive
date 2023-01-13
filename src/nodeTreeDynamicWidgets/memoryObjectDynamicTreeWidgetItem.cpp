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

#include "memoryObjectDynamicTreeWidgetItem.hpp"

#include <hive/modelsLibrary/helper.hpp>

MemoryObjectDynamicTreeWidgetItem::MemoryObjectDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, la::avdecc::entity::model::MemoryObjectNodeDynamicModel const* const dynamicModel, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _configurationIndex(configurationIndex)
	, _memoryObjectIndex(memoryObjectIndex)
{
	// MemoryObjectLength
	{
		// Create fields
		_length = new QTreeWidgetItem(this);
		_length->setText(0, "Length");

		// Update info right now
		updateMemoryObjectLength(dynamicModel->length);

		// Listen for MemoryObjectLengthChanged
		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::memoryObjectLengthChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, std::uint64_t const length)
			{
				if (entityID == _entityID && configurationIndex == _configurationIndex && memoryObjectIndex == _memoryObjectIndex)
				{
					updateMemoryObjectLength(length);
				}
			});
	}
}

void MemoryObjectDynamicTreeWidgetItem::updateMemoryObjectLength(std::uint64_t const length)
{
	_length->setText(1, hive::modelsLibrary::helper::toHexQString(length, false, true));
}
