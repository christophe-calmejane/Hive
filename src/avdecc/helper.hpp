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

#pragma once

#include <la/avdecc/controller/avdeccController.hpp>
#include <la/avdecc/internals/streamFormatInfo.hpp>
#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <la/avdecc/logger.hpp>
#include <hive/modelsLibrary/commandsExecutor.hpp>

#include <QString>
#include <QObject>
#include <QIcon>
#include <QWidget>

#include <functional>

namespace avdecc
{
namespace helper
{
QString protocolInterfaceTypeName(la::avdecc::protocol::ProtocolInterface::Type const& protocolInterfaceType);

QString descriptorTypeToString(la::avdecc::entity::model::DescriptorType const& descriptorType) noexcept;
QString acquireStateToString(la::avdecc::controller::model::AcquireState const& acquireState, la::avdecc::UniqueIdentifier const& owningController) noexcept;
QString lockStateToString(la::avdecc::controller::model::LockState const& lockState, la::avdecc::UniqueIdentifier const& lockingController) noexcept;

QString samplingRateToString(la::avdecc::entity::model::SamplingRate const& samplingRate) noexcept;
QString streamFormatToString(la::avdecc::entity::model::StreamFormatInfo const& format) noexcept;
QString clockSourceToString(la::avdecc::controller::model::ClockSourceNode const& node) noexcept;

QString flagsToString(la::avdecc::entity::AvbInterfaceFlags const flags) noexcept;
QString flagsToString(la::avdecc::entity::AvbInfoFlags const flags) noexcept;
QString flagsToString(la::avdecc::entity::ClockSourceFlags const flags) noexcept;
QString flagsToString(la::avdecc::entity::PortFlags const flags) noexcept;
QString flagsToString(la::avdecc::entity::StreamInfoFlags const flags) noexcept;
QString flagsToString(la::avdecc::entity::StreamInfoFlagsEx const flags) noexcept;
QString flagsToString(la::avdecc::entity::MilanInfoFeaturesFlags const flags) noexcept;

QString msrpFailureCodeToString(la::avdecc::entity::model::MsrpFailureCode const msrpFailureCode) noexcept;
QString probingStatusToString(la::avdecc::entity::model::ProbingStatus const status) noexcept;

QString capabilitiesToString(la::avdecc::entity::EntityCapabilities const caps) noexcept;
QString capabilitiesToString(la::avdecc::entity::TalkerCapabilities const caps) noexcept;
QString capabilitiesToString(la::avdecc::entity::ListenerCapabilities const caps) noexcept;
QString capabilitiesToString(la::avdecc::entity::ControllerCapabilities const caps) noexcept;

QString clockSourceTypeToString(la::avdecc::entity::model::ClockSourceType const type) noexcept;
QString audioClusterFormatToString(la::avdecc::entity::model::AudioClusterFormat const format) noexcept;
QString controlTypeToString(la::avdecc::entity::model::ControlType const& controlType) noexcept;
QString controlValueTypeToString(la::avdecc::entity::model::ControlValueType::Type const controlValueType) noexcept;
QString controlValueUnitToString(la::avdecc::entity::model::ControlValueUnit::Unit const controlValueUnit) noexcept;

QString memoryObjectTypeToString(la::avdecc::entity::model::MemoryObjectType const type) noexcept;

QString certificationVersionToString(std::uint32_t const certificationVersion) noexcept;

QString loggerLayerToString(la::avdecc::logger::Layer const layer) noexcept;
QString loggerLevelToString(la::avdecc::logger::Level const& level) noexcept;

QString generateDumpSourceString(QString const& shortName, QString const& version) noexcept;
bool isValidEntityModelID(la::avdecc::UniqueIdentifier const entityModelID) noexcept;
bool isEntityModelComplete(la::avdecc::UniqueIdentifier const entityID) noexcept;

/**
 * @brief Changes the stream input format of an entity, removing invalid mappings if any.
 * @details Synchronous method that will check if any mapping would become dangling after the format change, in which case it will ask the user if it wants to proceed (modal popup).
 * @param parent Parent widget to which popups will be attached to. If nullptr is specified, no popup will be shown and dangling mappings will be automatically removed (if needed).
 * @parem autoRemoveMappings If set to true (or if parent is nullptr), dangling mappings will automatically be removed without asking the user.
 * @param entityID Entity on which the format has to be changed.
 * @param streamIndex Input stream index on which the format has to be changed.
 * @param streamFormat Input stream format to change to.
 * @param context Context object for the result handler. Handler will not be called if context is destroyed before completion. Must *not* be nullptr.
 * @param handler Result handler called when the operation completed (with or without error).
*/
void smartChangeInputStreamFormat(QWidget* const parent, bool const autoRemoveMappings, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat, QObject* context, std::function<void(hive::modelsLibrary::CommandsExecutor::ExecutorResult const result)> const& handler) noexcept;

} // namespace helper
} // namespace avdecc
