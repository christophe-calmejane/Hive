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

#include "latencyItemDelegate.hpp"

#include <cmath>

QWidget* LatencyItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	auto latencyData = qvariant_cast<LatencyTableRowEntry>(index.data());

	la::avdecc::entity::model::StreamNodeStaticModel const* staticModel = nullptr;
	la::avdecc::entity::model::StreamNodeDynamicModel const* dynamicModel = nullptr;

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
	if (dynamicModel)
	{
		updatePossibleLatencyValues(combobox, dynamicModel->streamFormat);
		updateCurrentLatencyValue(combobox, std::chrono::nanoseconds{ dynamicModel->streamDynamicInfo ? (dynamicModel->streamDynamicInfo->msrpAccumulatedLatency ? *dynamicModel->streamDynamicInfo->msrpAccumulatedLatency : 0u) : 0u });
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
				updatePossibleLatencyValues(combobox, streamFormat);

				auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
				auto controlledEntity = manager.getControlledEntity(_entityID);
				if (controlledEntity)
				{
					auto const& entityNode = controlledEntity->getEntityNode();
					auto const configurationIndex = entityNode.dynamicModel.currentConfiguration;

					auto const& streamOutput = controlledEntity->getStreamOutputNode(configurationIndex, latencyData.streamIndex);
					updateCurrentLatencyValue(combobox, std::chrono::nanoseconds{ streamOutput.dynamicModel.streamDynamicInfo ? (streamOutput.dynamicModel.streamDynamicInfo->msrpAccumulatedLatency ? *streamOutput.dynamicModel.streamDynamicInfo->msrpAccumulatedLatency : 0u) : 0u });
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

void LatencyItemDelegate::updatePossibleLatencyValues(LatencyComboBox* const combobox, la::avdecc::entity::model::StreamFormat const& streamFormat) const noexcept
{
	using namespace std::chrono_literals;
	auto const PresentationTimes = std::set<std::chrono::microseconds>{ 250us, 500us, 750us, 1000us, 1250us, 1500us, 1750us, 2000us, 2250us, 2500us, 2750us, 3000us };

	auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
	auto const freq = static_cast<std::uint32_t>(streamFormatInfo->getSamplingRate().getNominalSampleRate());
	auto latencyDatas = std::set<LatencyComboBox_t, LatencyComboBoxCompare>{};
	for (auto const& presentationTime : PresentationTimes)
	{
		// Compute the number of samples for the given desired presentation time, rounding to the nearest integer
		auto const numberOfSamplesInBuffer = std::lround(presentationTime.count() * freq / std::remove_reference_t<decltype(presentationTime)>::period::den);

		// Compute the required duration of the buffer to hold the desired number of samples
		auto const bufferDuration = std::chrono::nanoseconds{ std::lround(numberOfSamplesInBuffer * std::chrono::nanoseconds::period::den / freq) };

		latencyDatas.insert(LatencyComboBox_t{ bufferDuration, labelFromLatency(bufferDuration), false });
	}
	latencyDatas.insert(LatencyComboBox_t{ std::chrono::nanoseconds{}, "Custom", true });
	combobox->setLatencyDatas(latencyDatas);
}

void LatencyItemDelegate::updateCurrentLatencyValue(LatencyComboBox* const combobox, std::chrono::nanoseconds const& latency) const noexcept
{
	combobox->setCurrentLatencyData(LatencyComboBox_t{ latency, labelFromLatency(latency), std::nullopt });
}

std::string LatencyItemDelegate::labelFromLatency(std::chrono::nanoseconds const& latency) const noexcept
{
	// Convert desired latency from nanoseconds to floating point milliseconds with 3 digits after the decimal point (only if not zero)
	auto const latencyInMs = static_cast<float>(latency.count()) / static_cast<float>(std::chrono::nanoseconds::period::den / std::chrono::milliseconds::period::den);
	auto const latencyInMsRounded = std::round(latencyInMs * 1000.0f) / 1000.0f;
	auto ss = std::stringstream{};
	ss << std::fixed << std::setprecision(3) << latencyInMsRounded;

	return ss.str() + " msec";
}
