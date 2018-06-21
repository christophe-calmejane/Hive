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

#pragma once

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>

#include "avdecc/helper.hpp"
#include "avdecc/controllerManager.hpp"

#include <QObject>
#include <QTreeWidgetItem>

class MemoryObjectDynamicTreeWidgetItem : public QObject, public QTreeWidgetItem
{
public:
	MemoryObjectDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, la::avdecc::controller::model::MemoryObjectNodeDynamicModel const* const dynamicModel, QTreeWidget *parent = nullptr);

private:
	void updateMemoryObjectLength(std::uint64_t const memoryObjectLength);

	la::avdecc::UniqueIdentifier const _entityID{ la::avdecc::getUninitializedIdentifier() };
	la::avdecc::entity::model::MemoryObjectIndex const _memoryObjectIndex{ 0u };

	// AvbInfo
	QTreeWidgetItem* _length { nullptr };
};
