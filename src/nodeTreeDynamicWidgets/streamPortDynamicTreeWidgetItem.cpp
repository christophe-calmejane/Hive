/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#ifndef ENABLE_AVDECC_FEATURE_REDUNDANCY
#	error "Hive requires Redundancy Feature to be enabled in AVDECC Library"
#endif // ENABLE_AVDECC_FEATURE_REDUNDANCY

#include "streamPortDynamicTreeWidgetItem.hpp"
#include "mappingMatrix.hpp"
#include "avdecc/mappingsHelper.hpp"
#include <vector>
#include <set>
#include <utility>
#include <cstdint>
#include <QPushButton>
#include <QMessageBox>


/* ************************************************************ */
/* StreamPortDynamicTreeWidgetItem                              */
/* ************************************************************ */
StreamPortDynamicTreeWidgetItem::StreamPortDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamPortType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::StreamPortNodeStaticModel const* const /*staticModel*/, la::avdecc::entity::model::StreamPortNodeDynamicModel const* const /*dynamicModel*/, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _streamPortType(streamPortType)
	, _streamPortIndex(streamPortIndex)
{
	// StreamPortInfo
	{
		// Create fields
		auto* editMappings = new QTreeWidgetItem(this);
		editMappings->setText(0, "Edit Dynamic Mappings");
		auto* editMappingsButton = new QPushButton("Edit");
		connect(editMappingsButton, &QPushButton::clicked, this, &StreamPortDynamicTreeWidgetItem::editMappingsButtonClicked);
		parent->setItemWidget(editMappings, 1, editMappingsButton);

		auto* mappingsItem = new QTreeWidgetItem(this);
		mappingsItem->setText(0, "Dynamic Mappings");
		_mappingsList = new QListWidget;
		_mappingsList->setSelectionMode(QAbstractItemView::NoSelection);
		parent->setItemWidget(mappingsItem, 1, _mappingsList);

		// Update info right now
		updateMappings();

		auto* clearMappings = new QTreeWidgetItem(this);
		clearMappings->setText(0, "Clear All Dynamic Mappings");
		auto* clearMappingsButton = new QPushButton("Clear");
		connect(clearMappingsButton, &QPushButton::clicked, this, &StreamPortDynamicTreeWidgetItem::clearMappingsButtonClicked);
		parent->setItemWidget(clearMappings, 1, clearMappingsButton);

		// Listen for streamPortAudioMappingsChanged
		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamPortAudioMappingsChanged, this,
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex)
			{
				if (entityID == _entityID && descriptorType == _streamPortType && streamPortIndex == _streamPortIndex)
				{
					// Update mappings
					updateMappings();
				}
			});

		// TODO: Listen for entity offline events and close the popup window
	}
}

void StreamPortDynamicTreeWidgetItem::editMappingsButtonClicked()
{
	avdecc::mappingsHelper::showMappingsEditor(this, _entityID, _streamPortType, _streamPortIndex, la::avdecc::entity::model::getInvalidDescriptorIndex());
}

void StreamPortDynamicTreeWidgetItem::clearMappingsButtonClicked()
{
	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(_entityID);
		if (controlledEntity)
		{
			auto& entity = *controlledEntity;
			auto const* streamPortNode = static_cast<la::avdecc::controller::model::StreamPortNode const*>(nullptr);

			auto const& entityNode = entity.getEntityNode();
			if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
			{
				streamPortNode = &entity.getStreamPortInputNode(entityNode.dynamicModel->currentConfiguration, _streamPortIndex);
				manager.removeStreamPortInputAudioMappings(_entityID, _streamPortIndex, streamPortNode->dynamicModel->dynamicAudioMap);
			}
			else if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
			{
				streamPortNode = &entity.getStreamPortOutputNode(entityNode.dynamicModel->currentConfiguration, _streamPortIndex);
				manager.removeStreamPortOutputAudioMappings(_entityID, _streamPortIndex, streamPortNode->dynamicModel->dynamicAudioMap);
			}
		}
	}
	catch (...)
	{
	}
}

void StreamPortDynamicTreeWidgetItem::updateMappings()
{
	constexpr auto showRedundantMappings = true;

	_mappingsList->clear();

	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(_entityID);
		if (controlledEntity)
		{
			auto& entity = *controlledEntity;
			auto mappings = la::avdecc::entity::model::AudioMappings{};

			if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
			{
				if (showRedundantMappings)
				{
					mappings = entity.getStreamPortInputAudioMappings(_streamPortIndex);
				}
				else
				{
					mappings = entity.getStreamPortInputNonRedundantAudioMappings(_streamPortIndex);
				}
			}
			else if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
			{
				if (showRedundantMappings)
				{
					mappings = entity.getStreamPortOutputAudioMappings(_streamPortIndex);
				}
				else
				{
					mappings = entity.getStreamPortOutputNonRedundantAudioMappings(_streamPortIndex);
				}
			}

			for (auto const& mapping : mappings)
			{
				_mappingsList->addItem(QString("%1.%2 > %3.%4").arg(mapping.streamIndex).arg(mapping.streamChannel).arg(mapping.clusterOffset).arg(mapping.clusterChannel));
			}
		}
	}
	catch (...)
	{
	}

	_mappingsList->sortItems();
}
