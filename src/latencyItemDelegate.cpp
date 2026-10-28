/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "latencyItemDelegate.hpp"
#include "deviceDetailsLatencyTableModel.hpp"

#include <QCoreApplication>

#include <cmath>

QWidget* LatencyItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	auto latencyData = qvariant_cast<LatencyTableRowEntry>(index.data());

	la::avdecc::entity::model::StreamNodeStaticModel const* staticModel = nullptr;
	la::avdecc::entity::model::StreamOutputNodeDynamicModel const* dynamicModel = nullptr;

	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(_entityID);
	if (controlledEntity)
	{
		auto const& entityNode = controlledEntity->getEntityNode();
		auto const configurationIndex = entityNode.dynamicModel.currentConfiguration;

		auto const& streamOutput = controlledEntity->getStreamOutputNode(configurationIndex, latencyData.streamIndex);
		staticModel = &streamOutput.staticModel;
		dynamicModel = static_cast<decltype(dynamicModel)>(&streamOutput.dynamicModel);
	}

	auto* combobox = new LatencyComboBox(parent);
	// The editor is displayed in the cell, assistive technologies can't associate it with the stream of its row
	combobox->setAccessibleName(QCoreApplication::translate("LatencyItemDelegate", "%1 Latency", nullptr).arg(index.siblingAtColumn(static_cast<int>(DeviceDetailsLatencyTableModelColumn::StreamName)).data().toString()));
	if (dynamicModel)
	{
		combobox->updatePossibleLatencyValues(dynamicModel->streamFormat);
		updateCurrentLatencyValue(combobox, dynamicModel->presentationTimeOffset);
	}

	// Send changes
	combobox->setDataChangedHandler(
		[this, combobox]([[maybe_unused]] LatencyComboBox_t const& previousLatency, [[maybe_unused]] LatencyComboBox_t const& newLatency)
		{
			auto* p = const_cast<LatencyItemDelegate*>(this);
			emit p->commitData(combobox);
		});

	// Listen for changes
	connect(&manager, &hive::modelsLibrary::ControllerManager::streamFormatChanged, combobox,
		[this, combobox, latencyData](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
		{
			if (entityID == _entityID && descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput && streamIndex == latencyData.streamIndex)
			{
				combobox->updatePossibleLatencyValues(streamFormat);

				auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
				auto controlledEntity = manager.getControlledEntity(_entityID);
				if (controlledEntity)
				{
					try
					{
						auto const& entityNode = controlledEntity->getEntityNode();
						auto const configurationIndex = entityNode.dynamicModel.currentConfiguration;

						auto const& streamOutput = controlledEntity->getStreamOutputNode(configurationIndex, latencyData.streamIndex);
						updateCurrentLatencyValue(combobox, streamOutput.dynamicModel.presentationTimeOffset);
					}
					catch (la::avdecc::controller::ControlledEntity::Exception const&)
					{
						// Ignore
					}
				}
			}

			auto* p = const_cast<LatencyItemDelegate*>(this);
			emit p->commitData(combobox);
		});
	connect(&manager, &hive::modelsLibrary::ControllerManager::maxTransitTimeChanged, combobox,
		[this, combobox, latencyData](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, std::chrono::nanoseconds const& maxTransitTime)
		{
			if (entityID == _entityID && streamIndex == latencyData.streamIndex)
			{
				updateCurrentLatencyValue(combobox, maxTransitTime);
			}

			auto* p = const_cast<LatencyItemDelegate*>(this);
			emit p->commitData(combobox);
		});

	return combobox;
}

void LatencyItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	Q_UNUSED(editor);
	Q_UNUSED(index);
}

/**
* This is used to set changed stream latency dropdown value.
*/
void LatencyItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, QModelIndex const& index) const
{
	auto* edit = static_cast<LatencyComboBox*>(editor);
	if (edit != nullptr)
	{
		auto const latencyData = edit->getCurrentLatencyData();
		auto newLatencyData = qvariant_cast<LatencyTableRowEntry>(index.data());
		newLatencyData.latency = std::get<0>(latencyData);
		model->setData(index, QVariant::fromValue<LatencyTableRowEntry>(newLatencyData), Qt::EditRole);
	}
}

void LatencyItemDelegate::updateCurrentLatencyValue(LatencyComboBox* const combobox, std::chrono::nanoseconds const& latency) const noexcept
{
	combobox->setCurrentLatencyData(LatencyComboBox_t{ latency, LatencyComboBox::labelFromLatency(latency), std::nullopt });
}
