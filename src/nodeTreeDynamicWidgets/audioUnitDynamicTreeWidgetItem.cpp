/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

AudioUnitDynamicTreeWidgetItem::AudioUnitDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::controller::model::AudioUnitNodeStaticModel const* const staticModel, la::avdecc::controller::model::AudioUnitNodeDynamicModel const* const dynamicModel, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _audioUnitIndex(audioUnitIndex)
{
	auto* currentSamplingRateItem = new QTreeWidgetItem(this);
	currentSamplingRateItem->setText(0, "Sampling Rate");

	_samplingRate = new AecpCommandComboBox(entityID, avdecc::ControllerManager::AecpCommandType::SetSamplingRate);
	parent->setItemWidget(currentSamplingRateItem, 1, _samplingRate);

	for (auto const samplingRate : staticModel->samplingRates)
	{
#pragma message("TODO : Use a proper helper method(in avdecc) to convert the packed samplingRate to an integer value in Hz")
#pragma message("TODO: Add a helper method in Hive to convert Hz to kHz")
		auto const readableSamplingRate = QString("%1 Hz").arg(samplingRate);
		_samplingRate->addItem(readableSamplingRate, samplingRate);
	}

	// Send changes
	connect(_samplingRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[this]()
		{
			auto const samplingRate = _samplingRate->currentData().value<la::avdecc::entity::model::SamplingRate>();
			avdecc::ControllerManager::getInstance().setAudioUnitSamplingRate(_entityID, _audioUnitIndex, samplingRate);
		});

	// Listen for changes
	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::audioUnitSamplingRateChanged, _samplingRate,
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
	QSignalBlocker const lg{ _samplingRate }; // Block internal signals so setCurrentIndex do not trigger "currentIndexChanged"
	auto const index = _samplingRate->findData(samplingRate);
	_samplingRate->setCurrentIndex(index);
}
