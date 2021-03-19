/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "entityTreeItem.hpp"
#include "avdecc/helper.hpp"

#include <la/avdecc/utils.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>

/**
 * Constructor.
 */
EntityTreeItem::EntityTreeItem(la::avdecc::UniqueIdentifier const& entityID, AbstractTreeItem* parent)
{
	m_parentItem = parent;
	m_entityID = entityID;
}

/**
 * Gets the entity id for this tree item.
 */
la::avdecc::UniqueIdentifier EntityTreeItem::entityId() const
{
	return m_entityID;
}

/**
 * Gets the entity name for this tree item.
 */
QString EntityTreeItem::entityName() const
{
	auto const controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(m_entityID);
	if (controlledEntity)
	{
		return hive::modelsLibrary::helper::smartEntityName(*controlledEntity);
	}
	return "";
}

/**
 * Gets the sample rate of the entity for this tree item.
 * @return The sample rate formatted as string with unit.
 */
std::optional<QPair<la::avdecc::entity::model::SamplingRate, QString>> EntityTreeItem::sampleRate() const
{
	auto const controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(m_entityID);
	if (controlledEntity)
	{
		try
		{
			auto* audioUnitDynamicModel = controlledEntity->getAudioUnitNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex, 0).dynamicModel;
			if (audioUnitDynamicModel)
			{
				auto samplingRate = audioUnitDynamicModel->currentSamplingRate;
				return qMakePair(samplingRate, QString::number(samplingRate / 1000) + " kHz");
			}
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// ignore
		}
	}

	return std::nullopt;
}

/**
* Gets the gptp sync state of the device.
*/
bool EntityTreeItem::isGPTPInSync() const
{
	return avdecc::mediaClock::MCDomainManager::getInstance().checkGPTPInSync(m_entityID);
}

/**
* Returns true if the entity is managable in terms of media clocking setup.
* The call is simply forwarded to the respective method in MCDomainManager.
*/
bool EntityTreeItem::isMediaClockDomainManageableEntity() const
{
	return avdecc::mediaClock::MCDomainManager::getInstance().isMediaClockDomainManageable(m_entityID);
}

AbstractTreeItem::TreeItemType EntityTreeItem::type() const
{
	return AbstractTreeItem::TreeItemType::Entity;
}
