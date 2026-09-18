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

#pragma once

#include <la/avdecc/controller/avdeccController.hpp>

#include <QString>

#include <optional>

namespace hive
{
namespace modelsLibrary
{
/**
* @brief JSON Pointers (RFC 6901) into the AVE document of an entity, as written by the la_avdecc JSON serializer (the format of the Event Journal snapshots).
* @details The change tracking events of the Event Journal carry a JSON Patch (RFC 6902) whose paths are built with these helpers, so replaying a journal is one generic loop applying the patches to the last snapshot of each entity.
*          The key names are the ones of the la_avdecc serializer (la/avdecc/internals/jsonTypes.hpp for the entity model, the controller's private avdeccControllerJsonTypes.hpp for the state and diagnostics objects), checked by the unit tests against the serializer output of a fixture entity.
*/
namespace avePatch
{
/** Returns the pointer of the entity descriptor node (its dynamic model holds the entity and group names, the current configuration and the entity counters). */
QString entityDescriptorPointer() noexcept;

/** Returns the pointer of a configuration descriptor node. */
QString configurationPointer(la::avdecc::entity::model::ConfigurationIndex const configurationIndex) noexcept;

/** Returns the pointer of the controller state object (acquire and lock states, unsolicited notifications subscription). */
QString statePointer() noexcept;

/** Returns the pointer of the diagnostics object (redundancy warning, stream input latency errors). */
QString diagnosticsPointer() noexcept;

/** Returns the pointer of the compatibility flags array. */
QString compatibilityFlagsPointer() noexcept;

/** Returns the pointer of the Milan compatibility version string. */
QString milanCompatibilityVersionPointer() noexcept;

/** Returns the pointer of the entity capabilities array (ADP information). */
QString entityCapabilitiesPointer() noexcept;

/**
* @brief Returns the pointer of a descriptor node.
* @details The serializer writes the descriptors of a configuration as arrays ordered by descriptor index. The nested descriptors (controls of an audio unit, jack, stream port or PTP instance; stream ports of an audio unit; audio clusters and audio maps of a stream port; PTP ports of a PTP instance) are stored under their parent, at the position "descriptor index minus the base index of the parent", which is looked up in the entity model.
* @param[in] controlledEntity The entity, used to locate the parent of a nested descriptor.
* @param[in] configurationIndex Configuration the descriptor belongs to (ignored for DescriptorType::Entity).
* @param[in] descriptorType Type of the descriptor (DescriptorType::Entity and DescriptorType::Configuration are accepted too).
* @param[in] descriptorIndex Index of the descriptor (the configuration index for DescriptorType::Configuration).
* @return The pointer, or std::nullopt if the descriptor cannot be located (unknown configuration or index, descriptor type not serialized).
*/
std::optional<QString> descriptorPointer(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex) noexcept;

/**
* @brief Returns the pointer of a key of the dynamic model of a descriptor (see descriptorPointer()).
* @param[in] controlledEntity The entity, used to locate the parent of a nested descriptor.
* @param[in] configurationIndex Configuration the descriptor belongs to.
* @param[in] descriptorType Type of the descriptor.
* @param[in] descriptorIndex Index of the descriptor.
* @param[in] key Key of the dynamic model (eg. "stream_format"), a la_avdecc serializer key name.
* @return The pointer, or std::nullopt if the descriptor cannot be located.
*/
std::optional<QString> dynamicModelPointer(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, char const* const key) noexcept;

} // namespace avePatch
} // namespace modelsLibrary
} // namespace hive
