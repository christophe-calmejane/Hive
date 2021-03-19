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

#include "domainTreeItem.hpp"
#include "entityTreeItem.hpp"

#include "avdecc/helper.hpp"
#include "la/avdecc/utils.hpp"

#include <hive/modelsLibrary/controllerManager.hpp>

#include <QSet>

/**
* Constructor.
*/
DomainTreeItem::DomainTreeItem(avdecc::mediaClock::MCDomain const& data, AbstractTreeItem* parent)
	: m_itemData(data)
{
	m_parentItem = parent;
	m_sampleRateSet = false;
}

/**
* Gets the domain object.
*/
avdecc::mediaClock::MCDomain& DomainTreeItem::domain()
{
	return m_itemData;
}

/**
* Gets all sample rate options for this domain.
* @return The sample rates as pairs, with a formatted string with unit.
*/
QList<QPair<std::optional<la::avdecc::entity::model::SamplingRate>, QString>> DomainTreeItem::sampleRates() const
{
	QSet<la::avdecc::entity::model::SamplingRate> childSampleRates;
	for (const auto& child : m_childItems)
	{
		auto* e = static_cast<EntityTreeItem*>(child);
		auto sampleRate = e->sampleRate();
		if (sampleRate)
		{
			childSampleRates.insert((*sampleRate).first);
		}
	}

	QList<QPair<std::optional<la::avdecc::entity::model::SamplingRate>, QString>> domainSampleRates;
	if (childSampleRates.size() > 1 && !m_sampleRateSet)
	{
		domainSampleRates.prepend(qMakePair<std::optional<la::avdecc::entity::model::SamplingRate>, QString>(std::nullopt, "-"));
	}

	if (m_itemData.getMediaClockDomainMaster())
	{
		auto controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(m_itemData.getMediaClockDomainMaster());
		if (controlledEntity)
		{
			auto configurationIndex = controlledEntity->getCurrentConfigurationNode().descriptorIndex;
			try
			{
				auto* audioUnitStaticModel = controlledEntity->getAudioUnitNode(configurationIndex, 0).staticModel;
				if (audioUnitStaticModel)
				{
					for (const auto& sampleRate : audioUnitStaticModel->samplingRates)
					{
						domainSampleRates << qMakePair(sampleRate, QString::number(sampleRate / 1000) + " kHz");
					}
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const& ex)
			{
				if (ex.getType() == la::avdecc::controller::ControlledEntity::Exception::Type::InvalidDescriptorIndex)
				{
					// audio unit index or configuration index invalid.
				}
			}
		}
	}

	return domainSampleRates;
}

/**
* Gets the sample rate of the domain for this tree item.
* @return The sample rate and a formatted string with unit. Or a pair of std::nullopt and "-" if no sample rate is set for this domain.
*/
QPair<std::optional<la::avdecc::entity::model::SamplingRate>, QString> DomainTreeItem::domainSamplingRate() const
{
	auto domainSampleRate = m_itemData.getDomainSamplingRate();
	return domainSampleRate ? qMakePair<std::optional<la::avdecc::entity::model::SamplingRate>, QString>(domainSampleRate, QString::number((domainSampleRate) / 1000) + " kHz") : qMakePair<std::optional<la::avdecc::entity::model::SamplingRate>, QString>(std::nullopt, "-");
}

/**
* Sets the sample rate of the domain. Will be applied to all child entities, when the apply button is pressed.
*/
void DomainTreeItem::setDomainSamplingRate(la::avdecc::entity::model::SamplingRate sampleRate)
{
	m_itemData.setDomainSamplingRate(sampleRate);
	m_sampleRateSet = true;
}

/**
* Checks all sub entities if they are set to the same sample rate and if not set the domain sample rate to undefined.
* In the case, the entites do have the same sample rate, sets the sample rate for this domain accordingly.
* If the user had actively set the domain sample rate, this method does nothing.
*/
void DomainTreeItem::reevaluateDomainSampleRate()
{
	if (!m_sampleRateSet)
	{
		if (m_childItems.size() == 0)
		{
			return;
		}
		std::optional<QPair<la::avdecc::entity::model::SamplingRate, QString>> referenceSampleRate;

		for (auto* item : m_childItems)
		{
			auto* entityTreeItem = static_cast<EntityTreeItem*>(item);
			if (!referenceSampleRate)
			{
				referenceSampleRate = entityTreeItem->sampleRate();
			}
			auto entitySampleRate = entityTreeItem->sampleRate();
			if (referenceSampleRate && entitySampleRate && (*entitySampleRate).first != (*referenceSampleRate).first)
			{
				referenceSampleRate = std::nullopt;
				m_itemData.setDomainSamplingRate(la::avdecc::entity::model::SamplingRate::getNullSamplingRate());
				return;
			}
		}
		if (referenceSampleRate)
		{
			m_itemData.setDomainSamplingRate((*referenceSampleRate).first);
		}
	}
}

/**
* Finds the child entity with the given id.
*/
EntityTreeItem* DomainTreeItem::findEntityWithId(la::avdecc::UniqueIdentifier const& entityId) const
{
	for (auto* item : m_childItems)
	{
		auto* entityTreeItem = static_cast<EntityTreeItem*>(item);
		if (entityTreeItem->entityId() == entityId)
		{
			return entityTreeItem;
		}
	}
	return nullptr;
}

/**
* Sets the first entity that can be a mc master as this domains master.
*/
void DomainTreeItem::setDefaultMcMaster()
{
	for (int i = 0; i < childCount(); i++)
	{
		auto* entity = static_cast<EntityTreeItem*>(childAt(i));
		if (entity->isMediaClockDomainManageableEntity())
		{
			domain().setMediaClockDomainMaster(entity->entityId());

			auto domainSampleRate = entity->sampleRate();
			if (domainSampleRate)
			{
				domain().setDomainSamplingRate((*domainSampleRate).first);
			}
		}
	}
}

AbstractTreeItem::TreeItemType DomainTreeItem::type() const
{
	return AbstractTreeItem::TreeItemType::Domain;
}
