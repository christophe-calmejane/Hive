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
		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::streamPortAudioMappingsChanged, this,
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
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(_entityID);
	if (controlledEntity)
	{
		auto& entity = *controlledEntity;
		auto smartName = hive::modelsLibrary::helper::smartEntityName(entity);

		// Release the controlled entity before starting a long operation
		controlledEntity.reset();

		// Get exclusive access
		manager.requestExclusiveAccess(_entityID, la::avdecc::controller::Controller::ExclusiveAccessToken::AccessType::Lock,
			[smartName, streamPortType = _streamPortType, streamPortIndex = _streamPortIndex](auto const entityID, auto const status, auto&& token)
			{
				// Failed to get the exclusive access
				if (!status || !token)
				{
					// If the device does not support the exclusive access, still proceed
					if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::NotImplemented && status != la::avdecc::entity::ControllerEntity::AemCommandStatus::NotSupported)
					{
						QMessageBox::warning(nullptr, QString(""), QString("Failed to get Exclusive Access on %1:<br>%2").arg(smartName).arg(QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status))));
						return;
					}
				}

				auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
				auto controlledEntity = manager.getControlledEntity(entityID);
				if (controlledEntity)
				{
					// Batch send the remove commands, and let the Exclusive Access Token go out of scope so the entity is unlocked right after (commands are sequentially sent)
					try
					{
						auto& entity = *controlledEntity;

						if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
						{
							avdecc::mappingsHelper::batchRemoveInputAudioMappings(entityID, streamPortIndex, entity.getStreamPortInputNonRedundantAudioMappings(streamPortIndex));
						}
						else if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
						{
							avdecc::mappingsHelper::batchRemoveOutputAudioMappings(entityID, streamPortIndex, entity.getStreamPortOutputNonRedundantAudioMappings(streamPortIndex));
						}
					}
					catch (...)
					{
					}
				}
			});
	}
}

void StreamPortDynamicTreeWidgetItem::updateMappings()
{
	constexpr auto showRedundantMappings = true;

	_mappingsList->clear();

	try
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
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
