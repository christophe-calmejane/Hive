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

#include "controlValuesDynamicTreeWidgetItem.hpp"

#include <QMenu>

Q_DECLARE_METATYPE(la::avdecc::entity::model::SamplingRate)

ControlValuesDynamicTreeWidgetItem::ControlValuesDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlNodeStaticModel const& /*staticModel*/, la::avdecc::entity::model::ControlNodeDynamicModel const& /*dynamicModel*/, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _controlIndex(controlIndex)
{
	// Listen for changes
	connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::controlValuesChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlValues const& controlValues)
		{
			if (entityID == _entityID && controlIndex == _controlIndex)
			{
				updateValues(controlValues);
			}
		});
}
