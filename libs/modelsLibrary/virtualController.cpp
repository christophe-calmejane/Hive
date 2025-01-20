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

#include "virtualController.hpp"

#include <la/avdecc/utils.hpp>

namespace hive
{
namespace modelsLibrary
{
// Constructor
VirtualController::VirtualController(ControllerManager const* const controllerManager) noexcept
	: _controllerManager{ controllerManager }
{
}

// Empty implementation of all VirtualController overridable methods
void VirtualController::acquireEntity(la::avdecc::UniqueIdentifier const targetEntityID, bool const /*isPersistent*/, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, AcquireEntityHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotSupported, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), descriptorType, descriptorIndex);
}

void VirtualController::releaseEntity(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, ReleaseEntityHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotSupported, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), descriptorType, descriptorIndex);
}

void VirtualController::lockEntity(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, LockEntityHandler const& handler) const noexcept
{
	auto const controllerEID = _controllerManager ? _controllerManager->getControllerEID() : la::avdecc::UniqueIdentifier::getNullUniqueIdentifier();
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, controllerEID, descriptorType, descriptorIndex);
}

void VirtualController::unlockEntity(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, UnlockEntityHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), descriptorType, descriptorIndex);
}

void VirtualController::queryEntityAvailable(la::avdecc::UniqueIdentifier const targetEntityID, QueryEntityAvailableHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success);
}

void VirtualController::queryControllerAvailable(la::avdecc::UniqueIdentifier const targetEntityID, QueryControllerAvailableHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented);
}

void VirtualController::registerUnsolicitedNotifications(la::avdecc::UniqueIdentifier const targetEntityID, RegisterUnsolicitedNotificationsHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success);
}

void VirtualController::unregisterUnsolicitedNotifications(la::avdecc::UniqueIdentifier const targetEntityID, UnregisterUnsolicitedNotificationsHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success);
}

void VirtualController::readEntityDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, EntityDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyEntityDescriptor = la::avdecc::entity::model::EntityDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, s_emptyEntityDescriptor);
}

void VirtualController::readConfigurationDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, ConfigurationDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyConfigurationDescriptor = la::avdecc::entity::model::ConfigurationDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, s_emptyConfigurationDescriptor);
}

void VirtualController::readAudioUnitDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, AudioUnitDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyAudioUnitDescriptor = la::avdecc::entity::model::AudioUnitDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, audioUnitIndex, s_emptyAudioUnitDescriptor);
}

void VirtualController::readStreamInputDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, StreamInputDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyStreamDescriptor = la::avdecc::entity::model::StreamDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, streamIndex, s_emptyStreamDescriptor);
}

void VirtualController::readStreamOutputDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, StreamOutputDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyStreamDescriptor = la::avdecc::entity::model::StreamDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, streamIndex, s_emptyStreamDescriptor);
}

void VirtualController::readJackInputDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::JackIndex const jackIndex, JackInputDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyJackDescriptor = la::avdecc::entity::model::JackDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, jackIndex, s_emptyJackDescriptor);
}

void VirtualController::readJackOutputDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::JackIndex const jackIndex, JackOutputDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyJackDescriptor = la::avdecc::entity::model::JackDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, jackIndex, s_emptyJackDescriptor);
}

void VirtualController::readAvbInterfaceDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, AvbInterfaceDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyAvbInterfaceDescriptor = la::avdecc::entity::model::AvbInterfaceDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, avbInterfaceIndex, s_emptyAvbInterfaceDescriptor);
}

void VirtualController::readClockSourceDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, ClockSourceDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyClockSourceDescriptor = la::avdecc::entity::model::ClockSourceDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, clockSourceIndex, s_emptyClockSourceDescriptor);
}

void VirtualController::readMemoryObjectDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, MemoryObjectDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyMemoryObjectDescriptor = la::avdecc::entity::model::MemoryObjectDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, memoryObjectIndex, s_emptyMemoryObjectDescriptor);
}

void VirtualController::readLocaleDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::LocaleIndex const localeIndex, LocaleDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyLocaleDescriptor = la::avdecc::entity::model::LocaleDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, localeIndex, s_emptyLocaleDescriptor);
}

void VirtualController::readStringsDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StringsIndex const stringsIndex, StringsDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyStringsDescriptor = la::avdecc::entity::model::StringsDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, stringsIndex, s_emptyStringsDescriptor);
}

void VirtualController::readStreamPortInputDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, StreamPortInputDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyStreamPortDescriptor = la::avdecc::entity::model::StreamPortDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, streamPortIndex, s_emptyStreamPortDescriptor);
}

void VirtualController::readStreamPortOutputDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, StreamPortOutputDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyStreamPortDescriptor = la::avdecc::entity::model::StreamPortDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, streamPortIndex, s_emptyStreamPortDescriptor);
}

void VirtualController::readExternalPortInputDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ExternalPortIndex const externalPortIndex, ExternalPortInputDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyExternalPortDescriptor = la::avdecc::entity::model::ExternalPortDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, externalPortIndex, s_emptyExternalPortDescriptor);
}

void VirtualController::readExternalPortOutputDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ExternalPortIndex const externalPortIndex, ExternalPortOutputDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyExternalPortDescriptor = la::avdecc::entity::model::ExternalPortDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, externalPortIndex, s_emptyExternalPortDescriptor);
}

void VirtualController::readInternalPortInputDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::InternalPortIndex const internalPortIndex, InternalPortInputDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyInternalPortDescriptor = la::avdecc::entity::model::InternalPortDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, internalPortIndex, s_emptyInternalPortDescriptor);
}

void VirtualController::readInternalPortOutputDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::InternalPortIndex const internalPortIndex, InternalPortOutputDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyInternalPortDescriptor = la::avdecc::entity::model::InternalPortDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, internalPortIndex, s_emptyInternalPortDescriptor);
}

void VirtualController::readAudioClusterDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const clusterIndex, AudioClusterDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyAudioClusterDescriptor = la::avdecc::entity::model::AudioClusterDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, clusterIndex, s_emptyAudioClusterDescriptor);
}

void VirtualController::readAudioMapDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MapIndex const mapIndex, AudioMapDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyAudioMapDescriptor = la::avdecc::entity::model::AudioMapDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, mapIndex, s_emptyAudioMapDescriptor);
}

void VirtualController::readControlDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, ControlDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyControlDescriptor = la::avdecc::entity::model::ControlDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, controlIndex, s_emptyControlDescriptor);
}

void VirtualController::readClockDomainDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, ClockDomainDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyClockDomainDescriptor = la::avdecc::entity::model::ClockDomainDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, clockDomainIndex, s_emptyClockDomainDescriptor);
}

void VirtualController::readTimingDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::TimingIndex const timingIndex, TimingDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyTimingDescriptor = la::avdecc::entity::model::TimingDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, timingIndex, s_emptyTimingDescriptor);
}

void VirtualController::readPtpInstanceDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpInstanceIndex const ptpInstanceIndex, PtpInstanceDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyPtpInstanceDescriptor = la::avdecc::entity::model::PtpInstanceDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, ptpInstanceIndex, s_emptyPtpInstanceDescriptor);
}

void VirtualController::readPtpPortDescriptor(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpPortIndex const ptpPortIndex, PtpPortDescriptorHandler const& handler) const noexcept
{
	static auto const s_emptyPtpPortDescriptor = la::avdecc::entity::model::PtpPortDescriptor{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, ptpPortIndex, s_emptyPtpPortDescriptor);
}

void VirtualController::setConfiguration(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, SetConfigurationHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex);
}

void VirtualController::getConfiguration(la::avdecc::UniqueIdentifier const targetEntityID, GetConfigurationHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, la::avdecc::entity::model::ConfigurationIndex{ 0u });
}

void VirtualController::setStreamInputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat, SetStreamInputFormatHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamIndex, streamFormat);
}

void VirtualController::getStreamInputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, GetStreamInputFormatHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamIndex, la::avdecc::entity::model::StreamFormat::getNullStreamFormat());
}

void VirtualController::setStreamOutputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat, SetStreamOutputFormatHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamIndex, streamFormat);
}

void VirtualController::getStreamOutputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, GetStreamOutputFormatHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamIndex, la::avdecc::entity::model::StreamFormat::getNullStreamFormat());
}

void VirtualController::getStreamPortInputAudioMap(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::MapIndex const mapIndex, GetStreamPortInputAudioMapHandler const& handler) const noexcept
{
	static auto const s_emptyAudioMappings = la::avdecc::entity::model::AudioMappings{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamPortIndex, 0u, mapIndex, s_emptyAudioMappings);
}

void VirtualController::getStreamPortOutputAudioMap(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::MapIndex const mapIndex, GetStreamPortOutputAudioMapHandler const& handler) const noexcept
{
	static auto const s_emptyAudioMappings = la::avdecc::entity::model::AudioMappings{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamPortIndex, 0u, mapIndex, s_emptyAudioMappings);
}

void VirtualController::addStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, AddStreamPortInputAudioMappingsHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamPortIndex, mappings);
}

void VirtualController::addStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, AddStreamPortOutputAudioMappingsHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamPortIndex, mappings);
}

void VirtualController::removeStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, RemoveStreamPortInputAudioMappingsHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamPortIndex, mappings);
}

void VirtualController::removeStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, RemoveStreamPortOutputAudioMappingsHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamPortIndex, mappings);
}

void VirtualController::setStreamInputInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& /*info*/, SetStreamInputInfoHandler const& handler) const noexcept
{
	// WARNING: When implementing this, we have to return the complete actual stream info, not the one passed in.
	static auto const s_emptyStreamInfo = la::avdecc::entity::model::StreamInfo{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamIndex, s_emptyStreamInfo);
}

void VirtualController::setStreamOutputInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& /*info*/, SetStreamOutputInfoHandler const& handler) const noexcept
{
	// WARNING: When implementing this, we have to return the complete actual stream info, not the one passed in.
	static auto const s_emptyStreamInfo = la::avdecc::entity::model::StreamInfo{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamIndex, s_emptyStreamInfo);
}

void VirtualController::getStreamInputInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, GetStreamInputInfoHandler const& handler) const noexcept
{
	static auto const s_emptyStreamInfo = la::avdecc::entity::model::StreamInfo{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamIndex, s_emptyStreamInfo);
}

void VirtualController::getStreamOutputInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, GetStreamOutputInfoHandler const& handler) const noexcept
{
	static auto const s_emptyStreamInfo = la::avdecc::entity::model::StreamInfo{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamIndex, s_emptyStreamInfo);
}

void VirtualController::setEntityName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AvdeccFixedString const& entityName, SetEntityNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, entityName);
}

void VirtualController::getEntityName(la::avdecc::UniqueIdentifier const targetEntityID, GetEntityNameHandler const& handler) const noexcept
{
	static auto const s_emptyEntityName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, s_emptyEntityName);
}

void VirtualController::setEntityGroupName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AvdeccFixedString const& entityGroupName, SetEntityGroupNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, entityGroupName);
}

void VirtualController::getEntityGroupName(la::avdecc::UniqueIdentifier const targetEntityID, GetEntityGroupNameHandler const& handler) const noexcept
{
	static auto const s_emptyEntityGroupName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, s_emptyEntityGroupName);
}

void VirtualController::setConfigurationName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvdeccFixedString const& configurationName, SetConfigurationNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, configurationName);
}

void VirtualController::getConfigurationName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, GetConfigurationNameHandler const& handler) const noexcept
{
	static auto const s_emptyConfigurationName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, s_emptyConfigurationName);
}

void VirtualController::setAudioUnitName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::AvdeccFixedString const& audioUnitName, SetAudioUnitNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, audioUnitIndex, audioUnitName);
}

void VirtualController::getAudioUnitName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, GetAudioUnitNameHandler const& handler) const noexcept
{
	static auto const s_emptyAudioUnitName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, audioUnitIndex, s_emptyAudioUnitName);
}

void VirtualController::setStreamInputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvdeccFixedString const& streamInputName, SetStreamInputNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, streamIndex, streamInputName);
}

void VirtualController::getStreamInputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, GetStreamInputNameHandler const& handler) const noexcept
{
	static auto const s_emptyStreamName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, streamIndex, s_emptyStreamName);
}

void VirtualController::setStreamOutputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvdeccFixedString const& streamOutputName, SetStreamOutputNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, streamIndex, streamOutputName);
}

void VirtualController::getStreamOutputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, GetStreamOutputNameHandler const& handler) const noexcept
{
	static auto const s_emptyStreamName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, streamIndex, s_emptyStreamName);
}

void VirtualController::setJackInputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::JackIndex const jackIndex, la::avdecc::entity::model::AvdeccFixedString const& jackInputName, SetJackInputNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, jackIndex, jackInputName);
}

void VirtualController::getJackInputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::JackIndex const jackIndex, GetJackInputNameHandler const& handler) const noexcept
{
	static auto const s_emptyJackName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, jackIndex, s_emptyJackName);
}

void VirtualController::setJackOutputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::JackIndex const jackIndex, la::avdecc::entity::model::AvdeccFixedString const& jackOutputName, SetJackOutputNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, jackIndex, jackOutputName);
}

void VirtualController::getJackOutputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::JackIndex const jackIndex, GetJackOutputNameHandler const& handler) const noexcept
{
	static auto const s_emptyJackName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, jackIndex, s_emptyJackName);
}

void VirtualController::setAvbInterfaceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvdeccFixedString const& avbInterfaceName, SetAvbInterfaceNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, avbInterfaceIndex, avbInterfaceName);
}

void VirtualController::getAvbInterfaceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, GetAvbInterfaceNameHandler const& handler) const noexcept
{
	static auto const s_emptyAvbInterfaceName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, avbInterfaceIndex, s_emptyAvbInterfaceName);
}

void VirtualController::setClockSourceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, la::avdecc::entity::model::AvdeccFixedString const& clockSourceName, SetClockSourceNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, clockSourceIndex, clockSourceName);
}

void VirtualController::getClockSourceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, GetClockSourceNameHandler const& handler) const noexcept
{
	static auto const s_emptyClockSourceName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, clockSourceIndex, s_emptyClockSourceName);
}

void VirtualController::setMemoryObjectName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, la::avdecc::entity::model::AvdeccFixedString const& memoryObjectName, SetMemoryObjectNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, memoryObjectIndex, memoryObjectName);
}

void VirtualController::getMemoryObjectName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, GetMemoryObjectNameHandler const& handler) const noexcept
{
	static auto const s_emptyMemoryObjectName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, memoryObjectIndex, s_emptyMemoryObjectName);
}

void VirtualController::setAudioClusterName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, la::avdecc::entity::model::AvdeccFixedString const& audioClusterName, SetAudioClusterNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, audioClusterIndex, audioClusterName);
}

void VirtualController::getAudioClusterName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, GetAudioClusterNameHandler const& handler) const noexcept
{
	static auto const s_emptyAudioClusterName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, audioClusterIndex, s_emptyAudioClusterName);
}

void VirtualController::setControlName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::AvdeccFixedString const& controlName, SetControlNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, controlIndex, controlName);
}

void VirtualController::getControlName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, GetControlNameHandler const& handler) const noexcept
{
	static auto const s_emptyControlName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, controlIndex, s_emptyControlName);
}

void VirtualController::setClockDomainName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::AvdeccFixedString const& clockDomainName, SetClockDomainNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, clockDomainIndex, clockDomainName);
}

void VirtualController::getClockDomainName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, GetClockDomainNameHandler const& handler) const noexcept
{
	static auto const s_emptyClockDomainName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, clockDomainIndex, s_emptyClockDomainName);
}

void VirtualController::setTimingName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::TimingIndex const timingIndex, la::avdecc::entity::model::AvdeccFixedString const& timingName, SetTimingNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, timingIndex, timingName);
}

void VirtualController::getTimingName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::TimingIndex const timingIndex, GetTimingNameHandler const& handler) const noexcept
{
	static auto const s_emptyTimingName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, timingIndex, s_emptyTimingName);
}

void VirtualController::setPtpInstanceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpInstanceIndex const ptpInstanceIndex, la::avdecc::entity::model::AvdeccFixedString const& ptpInstanceName, SetPtpInstanceNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, ptpInstanceIndex, ptpInstanceName);
}

void VirtualController::getPtpInstanceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpInstanceIndex const ptpInstanceIndex, GetPtpInstanceNameHandler const& handler) const noexcept
{
	static auto const s_emptyPtpInstanceName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, ptpInstanceIndex, s_emptyPtpInstanceName);
}

void VirtualController::setPtpPortName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpPortIndex const ptpPortIndex, la::avdecc::entity::model::AvdeccFixedString const& ptpPortName, SetPtpPortNameHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, ptpPortIndex, ptpPortName);
}

void VirtualController::getPtpPortName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpPortIndex const ptpPortIndex, GetPtpPortNameHandler const& handler) const noexcept
{
	static auto const s_emptyPtpPortName = la::avdecc::entity::model::AvdeccFixedString{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, ptpPortIndex, s_emptyPtpPortName);
}

void VirtualController::setAssociation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::UniqueIdentifier const associationID, SetAssociationHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, associationID);
}

void VirtualController::getAssociation(la::avdecc::UniqueIdentifier const targetEntityID, GetAssociationHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier());
}

void VirtualController::setAudioUnitSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate, SetAudioUnitSamplingRateHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, audioUnitIndex, samplingRate);
}

void VirtualController::getAudioUnitSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, GetAudioUnitSamplingRateHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, audioUnitIndex, la::avdecc::entity::model::SamplingRate::getNullSamplingRate());
}

void VirtualController::setVideoClusterSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClusterIndex const videoClusterIndex, la::avdecc::entity::model::SamplingRate const samplingRate, SetVideoClusterSamplingRateHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, videoClusterIndex, samplingRate);
}

void VirtualController::getVideoClusterSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClusterIndex const videoClusterIndex, GetVideoClusterSamplingRateHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, videoClusterIndex, la::avdecc::entity::model::SamplingRate::getNullSamplingRate());
}

void VirtualController::setSensorClusterSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClusterIndex const sensorClusterIndex, la::avdecc::entity::model::SamplingRate const samplingRate, SetSensorClusterSamplingRateHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, sensorClusterIndex, samplingRate);
}

void VirtualController::getSensorClusterSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClusterIndex const sensorClusterIndex, GetSensorClusterSamplingRateHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, sensorClusterIndex, la::avdecc::entity::model::SamplingRate::getNullSamplingRate());
}

void VirtualController::setClockSource(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, SetClockSourceHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, clockDomainIndex, clockSourceIndex);
}

void VirtualController::getClockSource(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, GetClockSourceHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex{ 0u });
}

void VirtualController::setControlValues(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlValues const& /*controlValues*/, SetControlValuesHandler const& handler) const noexcept
{
	static auto const s_emptyControlValues = la::avdecc::MemoryBuffer{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, controlIndex, s_emptyControlValues);
}

void VirtualController::getControlValues(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ControlIndex const controlIndex, GetControlValuesHandler const& handler) const noexcept
{
	static auto const s_emptyControlValues = la::avdecc::MemoryBuffer{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, controlIndex, s_emptyControlValues);
}

void VirtualController::startStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StartStreamInputHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamIndex);
}

void VirtualController::startStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StartStreamOutputHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamIndex);
}

void VirtualController::stopStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StopStreamInputHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamIndex);
}

void VirtualController::stopStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StopStreamOutputHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamIndex);
}

void VirtualController::getAvbInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, GetAvbInfoHandler const& handler) const noexcept
{
	static auto const s_emptyAvbInfo = la::avdecc::entity::model::AvbInfo{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, avbInterfaceIndex, s_emptyAvbInfo);
}

void VirtualController::getAsPath(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, GetAsPathHandler const& handler) const noexcept
{
	static auto const s_emptyAsPath = la::avdecc::entity::model::AsPath{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, avbInterfaceIndex, s_emptyAsPath);
}

void VirtualController::getEntityCounters(la::avdecc::UniqueIdentifier const targetEntityID, GetEntityCountersHandler const& handler) const noexcept
{
	static auto const s_emptyDescriptorCounters = la::avdecc::entity::model::DescriptorCounters{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, la::avdecc::entity::EntityCounterValidFlags{}, s_emptyDescriptorCounters);
}

void VirtualController::getAvbInterfaceCounters(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, GetAvbInterfaceCountersHandler const& handler) const noexcept
{
	static auto const s_emptyDescriptorCounters = la::avdecc::entity::model::DescriptorCounters{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, avbInterfaceIndex, la::avdecc::entity::AvbInterfaceCounterValidFlags{}, s_emptyDescriptorCounters);
}

void VirtualController::getClockDomainCounters(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, GetClockDomainCountersHandler const& handler) const noexcept
{
	static auto const s_emptyDescriptorCounters = la::avdecc::entity::model::DescriptorCounters{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, clockDomainIndex, la::avdecc::entity::ClockDomainCounterValidFlags{}, s_emptyDescriptorCounters);
}

void VirtualController::getStreamInputCounters(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, GetStreamInputCountersHandler const& handler) const noexcept
{
	static auto const s_emptyDescriptorCounters = la::avdecc::entity::model::DescriptorCounters{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamIndex, la::avdecc::entity::StreamInputCounterValidFlags{}, s_emptyDescriptorCounters);
}

void VirtualController::getStreamOutputCounters(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, GetStreamOutputCountersHandler const& handler) const noexcept
{
	static auto const s_emptyDescriptorCounters = la::avdecc::entity::model::DescriptorCounters{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamIndex, la::avdecc::entity::StreamOutputCounterValidFlags{}, s_emptyDescriptorCounters);
}

void VirtualController::reboot(la::avdecc::UniqueIdentifier const targetEntityID, RebootHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented);
}

void VirtualController::rebootToFirmware(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, RebootToFirmwareHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, memoryObjectIndex);
}

void VirtualController::startOperation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::MemoryObjectOperationType const operationType, la::avdecc::MemoryBuffer const& /*memoryBuffer*/, StartOperationHandler const& handler) const noexcept
{
	static auto const s_emptyMemoryBuffer = la::avdecc::MemoryBuffer{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, descriptorType, descriptorIndex, la::avdecc::entity::model::OperationID{ 0u }, operationType, s_emptyMemoryBuffer);
}

void VirtualController::abortOperation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::OperationID const operationID, AbortOperationHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, descriptorType, descriptorIndex, operationID);
}

void VirtualController::setMemoryObjectLength(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, std::uint64_t const length, SetMemoryObjectLengthHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, configurationIndex, memoryObjectIndex, length);
}

void VirtualController::getMemoryObjectLength(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, GetMemoryObjectLengthHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, configurationIndex, memoryObjectIndex, std::uint64_t{ 0u });
}

void VirtualController::getDynamicInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::controller::DynamicInfoParameters const& parameters, GetDynamicInfoHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, parameters);
}

void VirtualController::setMaxTransitTime(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, std::chrono::nanoseconds const& maxTransitTime, SetMaxTransitTimeHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::Success, streamIndex, maxTransitTime);
}

void VirtualController::getMaxTransitTime(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, GetMaxTransitTimeHandler const& handler) const noexcept
{
	static auto const s_emptyMaxTransitTime = std::chrono::nanoseconds{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented, streamIndex, s_emptyMaxTransitTime);
}

void VirtualController::addressAccess(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::addressAccess::Tlvs const& /*tlvs*/, AddressAccessHandler const& handler) const noexcept
{
	static auto const s_emptyTlvs = la::avdecc::entity::addressAccess::Tlvs{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::AaCommandStatus::NotImplemented, s_emptyTlvs);
}

void VirtualController::getMilanInfo(la::avdecc::UniqueIdentifier const targetEntityID, GetMilanInfoHandler const& handler) const noexcept
{
	static auto const s_emptyMilanInfo = la::avdecc::entity::model::MilanInfo{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::MvuCommandStatus::NotImplemented, s_emptyMilanInfo);
}

void VirtualController::setSystemUniqueID(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::SystemUniqueIdentifier const systemUniqueID, SetSystemUniqueIDHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::MvuCommandStatus::Success, systemUniqueID);
}

void VirtualController::getSystemUniqueID(la::avdecc::UniqueIdentifier const targetEntityID, GetSystemUniqueIDHandler const& handler) const noexcept
{
	static auto const s_emptySystemUniqueID = la::avdecc::entity::model::SystemUniqueIdentifier{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::MvuCommandStatus::NotImplemented, s_emptySystemUniqueID);
}

void VirtualController::setMediaClockReferenceInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, std::optional<la::avdecc::entity::model::MediaClockReferencePriority> const userPriority, std::optional<la::avdecc::entity::model::AvdeccFixedString> const& domainName, SetMediaClockReferenceInfoHandler const& handler) const noexcept
{
	auto const mediaClockReferenceInfo = la::avdecc::entity::model::MediaClockReferenceInfo{ userPriority, domainName };
	auto defaultPrio = la::avdecc::entity::model::DefaultMediaClockReferencePriority::Default;
	// Get defaultPrio value from the entity model of the targetEntityID
	auto controlledEntity = _controllerManager->getControlledEntity(targetEntityID);
	if (controlledEntity)
	{
		try
		{
			defaultPrio = controlledEntity->getClockDomainNode(controlledEntity->getCurrentConfigurationIndex(), clockDomainIndex).staticModel.defaultMediaClockPriority;
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// Ignore the exception
		}
	}

	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::MvuCommandStatus::Success, clockDomainIndex, defaultPrio, mediaClockReferenceInfo);
}

void VirtualController::getMediaClockReferenceInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, GetMediaClockReferenceInfoHandler const& handler) const noexcept
{
	static auto const s_emptyMediaClockReferenceInfo = la::avdecc::entity::model::MediaClockReferenceInfo{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, targetEntityID, la::avdecc::entity::LocalEntity::MvuCommandStatus::NotImplemented, clockDomainIndex, la::avdecc::entity::model::DefaultMediaClockReferencePriority::Default, s_emptyMediaClockReferenceInfo);
}

void VirtualController::connectStream(la::avdecc::entity::model::StreamIdentification const& talkerStream, la::avdecc::entity::model::StreamIdentification const& listenerStream, ConnectStreamHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, talkerStream, listenerStream, std::uint16_t{ 1u }, la::avdecc::entity::ConnectionFlags{}, la::avdecc::entity::LocalEntity::ControlStatus::Success);
}

void VirtualController::disconnectStream(la::avdecc::entity::model::StreamIdentification const& talkerStream, la::avdecc::entity::model::StreamIdentification const& listenerStream, DisconnectStreamHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, talkerStream, listenerStream, std::uint16_t{ 0u }, la::avdecc::entity::ConnectionFlags{}, la::avdecc::entity::LocalEntity::ControlStatus::Success);
}

void VirtualController::disconnectTalkerStream(la::avdecc::entity::model::StreamIdentification const& talkerStream, la::avdecc::entity::model::StreamIdentification const& listenerStream, DisconnectTalkerStreamHandler const& handler) const noexcept
{
	la::avdecc::utils::invokeProtectedHandler(handler, this, talkerStream, listenerStream, std::uint16_t{ 0u }, la::avdecc::entity::ConnectionFlags{}, la::avdecc::entity::LocalEntity::ControlStatus::Success);
}

void VirtualController::getTalkerStreamState(la::avdecc::entity::model::StreamIdentification const& talkerStream, GetTalkerStreamStateHandler const& handler) const noexcept
{
	static auto const s_emptyStreamIdentification = la::avdecc::entity::model::StreamIdentification{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, talkerStream, s_emptyStreamIdentification, std::uint16_t{ 0u }, la::avdecc::entity::ConnectionFlags{}, la::avdecc::entity::LocalEntity::ControlStatus::NotSupported);
}

void VirtualController::getListenerStreamState(la::avdecc::entity::model::StreamIdentification const& listenerStream, GetListenerStreamStateHandler const& handler) const noexcept
{
	static auto const s_emptyStreamIdentification = la::avdecc::entity::model::StreamIdentification{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, s_emptyStreamIdentification, listenerStream, std::uint16_t{ 0u }, la::avdecc::entity::ConnectionFlags{}, la::avdecc::entity::LocalEntity::ControlStatus::NotSupported);
}

void VirtualController::getTalkerStreamConnection(la::avdecc::entity::model::StreamIdentification const& talkerStream, std::uint16_t const /*connectionIndex*/, GetTalkerStreamConnectionHandler const& handler) const noexcept
{
	static auto const s_emptyStreamIdentification = la::avdecc::entity::model::StreamIdentification{};
	la::avdecc::utils::invokeProtectedHandler(handler, this, talkerStream, s_emptyStreamIdentification, std::uint16_t{ 0u }, la::avdecc::entity::ConnectionFlags{}, la::avdecc::entity::LocalEntity::ControlStatus::NotSupported);
}

} // namespace modelsLibrary
} // namespace hive
