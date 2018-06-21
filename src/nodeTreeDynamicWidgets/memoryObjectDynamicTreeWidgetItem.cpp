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

#include "memoryObjectDynamicTreeWidgetItem.hpp"

MemoryObjectDynamicTreeWidgetItem::MemoryObjectDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, la::avdecc::controller::model::MemoryObjectNodeDynamicModel const* const dynamicModel, QTreeWidget *parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
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
		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::memoryObjectLengthChanged, this, [this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, std::uint64_t const length)
		{
			if (entityID == _entityID && memoryObjectIndex == _memoryObjectIndex)
			{
				updateMemoryObjectLength(length);
			}
		});
	}
}

void MemoryObjectDynamicTreeWidgetItem::updateMemoryObjectLength(std::uint64_t const length)
{
	_length->setText(1, avdecc::helper::toHexQString(length, false, true));
}
