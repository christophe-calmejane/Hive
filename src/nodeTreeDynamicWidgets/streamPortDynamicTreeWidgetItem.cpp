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
	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(_entityID);

		if (controlledEntity)
		{
			auto const* const entity = controlledEntity.get();
			auto const& entityNode = entity->getEntityNode();
			auto const& configurationNode = entity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);
			mappingMatrix::Nodes outputs;
			mappingMatrix::Nodes inputs;
			mappingMatrix::Connections connections;
			avdecc::mappingsHelper::NodeMappings streamMappings;
			avdecc::mappingsHelper::NodeMappings clusterMappings;

			auto const isValidStream = [](auto const* const streamNode)
			{
				auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode->dynamicModel->streamFormat);
				auto const formatType = sfi->getType();

				if (formatType == la::avdecc::entity::model::StreamFormatInfo::Type::None || formatType == la::avdecc::entity::model::StreamFormatInfo::Type::Unsupported || formatType == la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference)
					return false;

				return true;
			};

			if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
			{
				auto const& streamPortNode = entity->getStreamPortInputNode(entityNode.dynamicModel->currentConfiguration, _streamPortIndex);
				std::vector<la::avdecc::controller::model::StreamInputNode const*> streamNodes;

				// Build list of StreamInput
				for (auto const& streamKV : configurationNode.streamInputs)
				{
					auto const& streamNode = streamKV.second;
					if (!streamNode.isRedundant && isValidStream(&streamNode))
					{
						streamNodes.push_back(&streamNode);
					}
				}

				// Add primary stream of a Redundant Set
				for (auto const& redundantStreamKV : configurationNode.redundantStreamInputs)
				{
					auto const& redundantStreamNode = redundantStreamKV.second;
					auto const* const streamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);
					if (isValidStream(streamNode))
					{
						streamNodes.push_back(streamNode);
					}
				}

				// Build mappingMatrix vectors
				auto clusterResult = avdecc::mappingsHelper::buildClusterMappings(entity, streamPortNode);
				clusterMappings = std::move(clusterResult.first);
				inputs = std::move(clusterResult.second);
				auto streamResult = avdecc::mappingsHelper::buildStreamMappings(entity, streamNodes);
				streamMappings = std::move(streamResult.first);
				outputs = std::move(streamResult.second);
				connections = buildConnections(streamPortNode, streamNodes, streamMappings, clusterMappings,
					[](mappingMatrix::SlotID const streamSlotID, mappingMatrix::SlotID const clusterSlotID)
					{
						return std::make_pair(streamSlotID, clusterSlotID);
					});
			}
			else if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
			{
				auto const& streamPortNode = entity->getStreamPortOutputNode(entityNode.dynamicModel->currentConfiguration, _streamPortIndex);
				std::vector<la::avdecc::controller::model::StreamOutputNode const*> streamNodes;

				// Build list of StreamOutput
				for (auto const& streamKV : configurationNode.streamOutputs)
				{
					auto const& streamNode = streamKV.second;
					if (!streamNode.isRedundant && isValidStream(&streamNode))
					{
						streamNodes.push_back(&streamNode);
					}
				}

				// Add primary stream of a Redundant Set
				for (auto const& redundantStreamKV : configurationNode.redundantStreamOutputs)
				{
					auto const& redundantStreamNode = redundantStreamKV.second;
					auto const* const streamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);
					if (isValidStream(streamNode))
					{
						streamNodes.push_back(streamNode);
					}
				}

				// Build mappingMatrix vectors
				auto clusterResult = avdecc::mappingsHelper::buildClusterMappings(entity, streamPortNode);
				clusterMappings = std::move(clusterResult.first);
				outputs = std::move(clusterResult.second);
				auto streamResult = avdecc::mappingsHelper::buildStreamMappings(entity, streamNodes);
				streamMappings = std::move(streamResult.first);
				inputs = std::move(streamResult.second);
				connections = buildConnections(streamPortNode, streamNodes, streamMappings, clusterMappings,
					[](mappingMatrix::SlotID const streamSlotID, mappingMatrix::SlotID const clusterSlotID)
					{
						return std::make_pair(clusterSlotID, streamSlotID);
					});
			}
			else
			{
				AVDECC_ASSERT(false, "Should not happen");
			}

			if (!outputs.empty() && !inputs.empty())
			{
				auto smartName = avdecc::helper::smartEntityName(*entity);

				// Release the controlled entity before starting a long operation (dialog.exec)
				controlledEntity.reset();

				// Get exclusive access
				manager.requestExclusiveAccess(_entityID, la::avdecc::controller::Controller::ExclusiveAccessToken::AccessType::Lock,
					[this, streamMappings = std::move(streamMappings), clusterMappings = std::move(clusterMappings), smartName = std::move(smartName), outputs = std::move(outputs), inputs = std::move(inputs), connections = std::move(connections)](auto const /*entityID*/, auto const status, auto&& token)
					{
						// Moving the token to the capture will effectively extend the lifetime of the token, keeping the entity locked until the lambda completes (meaning the dialog has been closed and mappings changed)
						QMetaObject::invokeMethod(this,
							[this, status, token = std::move(token), streamMappings = std::move(streamMappings), clusterMappings = std::move(clusterMappings), smartName = std::move(smartName), outputs = std::move(outputs), inputs = std::move(inputs), connections = std::move(connections)]()
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

								// Create the dialog
								auto title = QString("%1 - %2.%3 Dynamic Mappings").arg(smartName).arg(avdecc::helper::descriptorTypeToString(_streamPortType)).arg(_streamPortIndex);
								auto dialog = mappingMatrix::MappingMatrixDialog{ title, outputs, inputs, connections };

								if (dialog.exec() == QDialog::Accepted)
								{
									if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
									{
										avdecc::mappingsHelper::processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortInput>(_entityID, _streamPortIndex, streamMappings, clusterMappings, connections, dialog.connections());
									}
									else if (_streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
									{
										avdecc::mappingsHelper::processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortOutput>(_entityID, _streamPortIndex, streamMappings, clusterMappings, connections, dialog.connections());
									}
								}
							});
					});
			}
		}
	}
	catch (...)
	{
	}
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
