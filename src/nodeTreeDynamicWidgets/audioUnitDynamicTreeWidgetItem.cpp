/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "audioUnitDynamicTreeWidgetItem.hpp"

#include <QMenu>

Q_DECLARE_METATYPE(la::avdecc::entity::model::SamplingRate)

AudioUnitDynamicTreeWidgetItem::AudioUnitDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::AudioUnitNodeStaticModel const* const staticModel, la::avdecc::entity::model::AudioUnitNodeDynamicModel const* const dynamicModel, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _audioUnitIndex(audioUnitIndex)
{
	auto* currentSamplingRateItem = new QTreeWidgetItem(this);
	currentSamplingRateItem->setText(0, "Sampling Rate");

	_samplingRate = new AecpCommandComboBox<la::avdecc::entity::model::SamplingRate>();
	parent->setItemWidget(currentSamplingRateItem, 1, _samplingRate);
	_samplingRate->setAllData(staticModel->samplingRates,
		[](auto const& samplingRate)
		{
#pragma message("TODO: Add a helper method in Hive to convert the returned double value to human readable kHz")
			return QString("%1 Hz").arg(samplingRate.getNominalSampleRate());
		});

	// Send changes
	_samplingRate->setDataChangedHandler(
		[this](auto const& previousSamplingRate, auto const& samplingRate)
		{
			hive::modelsLibrary::ControllerManager::getInstance().setAudioUnitSamplingRate(_entityID, _audioUnitIndex, samplingRate, _samplingRate->getBeginCommandHandler(hive::modelsLibrary::ControllerManager::AecpCommandType::SetSamplingRate), _samplingRate->getResultHandler(hive::modelsLibrary::ControllerManager::AecpCommandType::SetSamplingRate, previousSamplingRate));
		});

	// Listen for changes
	connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::audioUnitSamplingRateChanged, _samplingRate,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate)
		{
			if (entityID == _entityID && audioUnitIndex == _audioUnitIndex)
			{
				updateSamplingRate(samplingRate);
			}
		});

	// Update now
	updateSamplingRate(dynamicModel->currentSamplingRate);
}

void AudioUnitDynamicTreeWidgetItem::updateSamplingRate(la::avdecc::entity::model::SamplingRate const samplingRate)
{
	_samplingRate->setCurrentData(samplingRate);
}
