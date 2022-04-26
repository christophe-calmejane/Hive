/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#pragma once

#include "mappingMatrix.hpp"
#include "helper.hpp"

#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

#include <cstdint>
#include <vector>
#include <set>
#include <utility>
#include <QObject>

namespace avdecc
{
namespace mappingsHelper
{
void showMappingsEditor(QObject* obj, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamPortType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept;
la::avdecc::entity::model::AudioMappings getMaximumAudioMappings(la::avdecc::entity::model::AudioMappings const& mappings, size_t const offset) noexcept;
/** Adds new input audio mappings. Entity is expected to be under ExclusiveAccess. */
void batchAddInputAudioMappings(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, hive::modelsLibrary::ControllerManager::AddStreamPortInputAudioMappingsHandler const& handler = {}) noexcept;
/** Adds new output audio mappings. Entity is expected to be under ExclusiveAccess. */
void batchAddOutputAudioMappings(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, hive::modelsLibrary::ControllerManager::AddStreamPortInputAudioMappingsHandler const& handler = {}) noexcept;
/** Removes new input audio mappings. Entity is expected to be under ExclusiveAccess. */
void batchRemoveInputAudioMappings(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, hive::modelsLibrary::ControllerManager::RemoveStreamPortInputAudioMappingsHandler const& handler = {}) noexcept;
/** Removes new output audio mappings. Entity is expected to be under ExclusiveAccess. */
void batchRemoveOutputAudioMappings(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, hive::modelsLibrary::ControllerManager::RemoveStreamPortInputAudioMappingsHandler const& handler = {}) noexcept;

} // namespace mappingsHelper
} // namespace avdecc
