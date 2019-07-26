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

#pragma once

#include <la/avdecc/controller/avdeccController.hpp>

#include <memory>
#include <chrono>
#include <unordered_map>
#include <cstdint>

#include <QObject>

namespace avdecc
{
class ControllerManager : public QObject
{
	Q_OBJECT
public:
	static ControllerManager& getInstance() noexcept;

	/* ControllerManager Types */
	enum class StatisticsErrorCounterFlag : std::uint32_t
	{
		None = 0u,
		AecpRetries = 1u << 0,
		AecpTimeouts = 1u << 1,
		AecpUnexpectedResponses = 1u << 2,
	};

	using StreamInputErrorCounters = std::unordered_map<la::avdecc::entity::StreamInputCounterValidFlag, la::avdecc::entity::model::DescriptorCounter>;
	using StatisticsErrorCounters = std::unordered_map<StatisticsErrorCounterFlag, std::uint64_t>;

	enum class AecpCommandType
	{
		None = 0,
		AcquireEntity,
		ReleaseEntity,
		LockEntity,
		UnlockEntity,
		SetConfiguration,
		SetStreamFormat,
		SetStreamInfo,
		SetEntityName,
		SetEntityGroupName,
		SetConfigurationName,
		SetAudioUnitName,
		SetStreamName,
		SetAvbInterfaceName,
		SetClockSourceName,
		SetMemoryObjectName,
		SetAudioClusterName,
		SetClockDomainName,
		SetSamplingRate,
		SetClockSource,
		StartStream,
		StopStream,
		AddStreamPortAudioMappings,
		RemoveStreamPortAudioMappings,
		StartStoreAndRebootMemoryObjectOperation,
		StartUploadMemoryObjectOperation,
		AbortOperation,
	};

	enum class AcmpCommandType
	{
		None = 0,
		ConnectStream,
		DisconnectStream,
		DisconnectTalkerStream,
	};

	/* AECP handlers to override the global AECP result process */
	using AcquireEntityHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const owningEntity)>;
	using ReleaseEntityHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using LockEntityHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const lockingEntity)>;
	using UnlockEntityHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetStreamInputFormatHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetStreamOutputFormatHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	//using SetStreamInputInfoHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetStreamOutputInfoHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetEntityNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetEntityGroupNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetConfigurationNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetAudioUnitNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetStreamInputNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetStreamOutputNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetAvbInterfaceNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetClockSourceNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetMemoryObjectNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetAudioClusterNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetClockDomainNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetAudioUnitSamplingRateHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetClockSourceHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using StartStreamInputHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using StopStreamInputHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using StartStreamOutputHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using StopStreamOutputHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using AddStreamPortInputAudioMappingsHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using AddStreamPortOutputAudioMappingsHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using RemoveStreamPortInputAudioMappingsHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using RemoveStreamPortOutputAudioMappingsHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using StartStoreAndRebootMemoryObjectOperationHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::entity::model::OperationID const operationID)>;
	using StartUploadMemoryObjectOperationHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::entity::model::OperationID const operationID)>;
	using AbortOperationHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	/* ACMP handlers to override the global ACMP result process */
	using ConnectStreamHandler = std::function<void(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status)>;
	using DisconnectStreamHandler = std::function<void(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status)>;
	using DisconnectTalkerStreamHandler = std::function<void(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status)>;

	/**
	* @brief Creates a new controller, replacing previous one if any.
	* @details Creates a new controller, first removing the previous one if any.
	*          If an error occurs during the setup of the new controller, the previous one is NOT restored.
	* @note Might throw la::avdecc::controller::Controller::Exception.
	*       All observers should be removed from the previous controller before setting a new one.
	*/
	virtual void createController(la::avdecc::protocol::ProtocolInterface::Type const protocolInterfaceType, QString const& interfaceName, std::uint16_t const progID, la::avdecc::UniqueIdentifier const entityModelID, QString const& preferedLocale) = 0;

	/** Destroys the currently stored instance of the controller. */
	virtual void destroyController() noexcept = 0;

	/** Gets the controller's EID */
	virtual la::avdecc::UniqueIdentifier getControllerEID() const noexcept = 0;

	/** Gets a ControlledEntity */
	virtual la::avdecc::controller::ControlledEntityGuard getControlledEntity(la::avdecc::UniqueIdentifier const entityID) const noexcept = 0;

	/** Serialize all known ControlledEntities */
	virtual std::tuple<la::avdecc::jsonSerializer::SerializationError, std::string> serializeAllControlledEntitiesAsReadableJson(QString const& filePath, bool const ignoreSanityChecks) const noexcept = 0;

	/** Serialize a ControlledEntity */
	virtual std::tuple<la::avdecc::jsonSerializer::SerializationError, std::string> serializeControlledEntityAsReadableJson(la::avdecc::UniqueIdentifier const entityID, QString const& filePath, bool const ignoreSanityChecks) const noexcept = 0;

	/** Deserializes a readable JSON file representing an entity, and loads it as a virtual ControlledEntity. */
	virtual std::tuple<la::avdecc::jsonSerializer::DeserializationError, std::string> loadVirtualEntityFromReadableJson(QString const& filePath, bool const ignoreSanityChecks) noexcept = 0;

	/** Counter error flags */
	virtual StreamInputErrorCounters getStreamInputErrorCounters(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept = 0;
	virtual void clearStreamInputCounterValidFlags(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamInputCounterValidFlag const flag) noexcept = 0;
	virtual void clearAllStreamInputCounterValidFlags(la::avdecc::UniqueIdentifier const entityID) noexcept = 0;
	virtual StatisticsErrorCounters getStatisticsCounters(la::avdecc::UniqueIdentifier const entityID) const noexcept = 0;
	virtual void clearStatisticsCounterValidFlags(la::avdecc::UniqueIdentifier const entityID, StatisticsErrorCounterFlag const flag) noexcept = 0;
	virtual void clearAllStatisticsCounterValidFlags(la::avdecc::UniqueIdentifier const entityID) noexcept = 0;

	/* Enumeration and Control Protocol (AECP) */
	virtual void acquireEntity(la::avdecc::UniqueIdentifier const targetEntityID, bool const isPersistent, AcquireEntityHandler const& handler = {}) noexcept = 0;
	virtual void releaseEntity(la::avdecc::UniqueIdentifier const targetEntityID, ReleaseEntityHandler const& handler = {}) noexcept = 0;
	virtual void lockEntity(la::avdecc::UniqueIdentifier const targetEntityID, LockEntityHandler const& handler = {}) noexcept = 0;
	virtual void unlockEntity(la::avdecc::UniqueIdentifier const targetEntityID, UnlockEntityHandler const& handler = {}) noexcept = 0;
	virtual void setConfiguration(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex) noexcept = 0;
	virtual void setStreamInputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat, SetStreamInputFormatHandler const& handler = {}) noexcept = 0;
	virtual void setStreamOutputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat, SetStreamOutputFormatHandler const& handler = {}) noexcept = 0;
	//virtual void setStreamInputInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& streamInfo, SetStreamInputInfoHandler const& handler = {}) noexcept = 0;
	virtual void setStreamOutputInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& streamInfo, SetStreamOutputInfoHandler const& handler = {}) noexcept = 0;
	virtual void setEntityName(la::avdecc::UniqueIdentifier const targetEntityID, QString const& name, SetEntityNameHandler const& handler = {}) noexcept = 0;
	virtual void setEntityGroupName(la::avdecc::UniqueIdentifier const targetEntityID, QString const& name, SetEntityGroupNameHandler const& handler = {}) noexcept = 0;
	virtual void setConfigurationName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& name, SetConfigurationNameHandler const& handler = {}) noexcept = 0;
	virtual void setAudioUnitName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, QString const& name, SetAudioUnitNameHandler const& handler = {}) noexcept = 0;
	virtual void setStreamInputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name, SetStreamInputNameHandler const& handler = {}) noexcept = 0;
	virtual void setStreamOutputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name, SetStreamOutputNameHandler const& handler = {}) noexcept = 0;
	virtual void setAvbInterfaceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, QString const& name, SetAvbInterfaceNameHandler const& handler = {}) noexcept = 0;
	virtual void setClockSourceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, QString const& name, SetClockSourceNameHandler const& handler = {}) noexcept = 0;
	virtual void setMemoryObjectName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, QString const& name, SetMemoryObjectNameHandler const& handler = {}) noexcept = 0;
	virtual void setAudioClusterName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& name, SetAudioClusterNameHandler const& handler = {}) noexcept = 0;
	virtual void setClockDomainName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, QString const& name, SetClockDomainNameHandler const& handler = {}) noexcept = 0;
	virtual void setAudioUnitSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate, SetAudioUnitSamplingRateHandler const& handler = {}) noexcept = 0;
	virtual void setClockSource(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, SetClockSourceHandler const& handler = {}) noexcept = 0;
	virtual void startStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StartStreamInputHandler const& handler = {}) noexcept = 0;
	virtual void stopStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StopStreamInputHandler const& handler = {}) noexcept = 0;
	virtual void startStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StartStreamOutputHandler const& handler = {}) noexcept = 0;
	virtual void stopStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StopStreamOutputHandler const& handler = {}) noexcept = 0;
	virtual void addStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, AddStreamPortInputAudioMappingsHandler const& handler = {}) noexcept = 0;
	virtual void addStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, AddStreamPortOutputAudioMappingsHandler const& handler = {}) noexcept = 0;
	virtual void removeStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, RemoveStreamPortInputAudioMappingsHandler const& handler = {}) noexcept = 0;
	virtual void removeStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, RemoveStreamPortOutputAudioMappingsHandler const& handler = {}) noexcept = 0;
	virtual void startStoreAndRebootMemoryObjectOperation(la::avdecc::UniqueIdentifier targetEntityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, StartStoreAndRebootMemoryObjectOperationHandler const& handler = {}) noexcept = 0;
	virtual void startUploadMemoryObjectOperation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, std::uint64_t const dataLength, StartUploadMemoryObjectOperationHandler const& handler = {}) noexcept = 0;
	virtual void abortOperation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::OperationID const operationID, AbortOperationHandler const& handler = {}) noexcept = 0;

	/* Enumeration and Control Protocol (AECP) AA */
	virtual void readDeviceMemory(la::avdecc::UniqueIdentifier const targetEntityID, std::uint64_t const address, std::uint64_t const length, la::avdecc::controller::Controller::ReadDeviceMemoryProgressHandler const& progressHandler, la::avdecc::controller::Controller::ReadDeviceMemoryCompletionHandler const& completionHandler) const noexcept = 0;
	virtual void writeDeviceMemory(la::avdecc::UniqueIdentifier const targetEntityID, std::uint64_t const address, la::avdecc::controller::Controller::DeviceMemoryBuffer memoryBuffer, la::avdecc::controller::Controller::WriteDeviceMemoryProgressHandler const& progressHandler, la::avdecc::controller::Controller::WriteDeviceMemoryCompletionHandler const& completionHandler) const noexcept = 0;

	/* Connection Management Protocol (ACMP) */
	virtual void connectStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, ConnectStreamHandler const& handler = {}) noexcept = 0;
	virtual void disconnectStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, DisconnectStreamHandler const& handler = {}) noexcept = 0;
	virtual void disconnectTalkerStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, DisconnectTalkerStreamHandler const& handler = {}) noexcept = 0;

	using ControlledEntityCallback = std::function<void(la::avdecc::UniqueIdentifier const&, la::avdecc::controller::ControlledEntity const&)>;
	virtual void foreachEntity(ControlledEntityCallback const& callback) noexcept = 0;

	/* Static methods */
	static QString typeToString(AecpCommandType const type) noexcept;
	static QString typeToString(AcmpCommandType const type) noexcept;

	/* Controller signals */
	Q_SIGNAL void controllerOnline();
	Q_SIGNAL void controllerOffline();

	/* Entity changed signals */
	Q_SIGNAL void transportError();
	Q_SIGNAL void entityQueryError(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::Controller::QueryCommandError const error);
	Q_SIGNAL void entityOnline(la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const enumerationTime);
	Q_SIGNAL void entityOffline(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void unsolicitedRegistrationChanged(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void compatibilityFlagsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags);
	Q_SIGNAL void identificationStarted(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void identificationStopped(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain);
	Q_SIGNAL void acquireStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity);
	Q_SIGNAL void lockStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity);
	Q_SIGNAL void streamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat);
	Q_SIGNAL void streamInfoChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& info);
	Q_SIGNAL void entityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityName);
	Q_SIGNAL void entityGroupNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityGroupName);
	Q_SIGNAL void configurationNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& configurationName);
	Q_SIGNAL void audioUnitNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, QString const& audioUnitName);
	Q_SIGNAL void streamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& streamName);
	Q_SIGNAL void avbInterfaceNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, QString const& avbInterfaceName);
	Q_SIGNAL void clockSourceNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, QString const& clockSourceName);
	Q_SIGNAL void memoryObjectNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, QString const& memoryObjectName);
	Q_SIGNAL void audioClusterNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& audioClusterName);
	Q_SIGNAL void clockDomainNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, QString const& clockDomainName);
	Q_SIGNAL void audioUnitSamplingRateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate);
	Q_SIGNAL void clockSourceChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const sourceIndex);
	Q_SIGNAL void streamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning);
	Q_SIGNAL void avbInterfaceInfoChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceInfo const& info);
	Q_SIGNAL void asPathChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AsPath const& asPath);
	Q_SIGNAL void avbInterfaceLinkStatusChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus);
	Q_SIGNAL void entityCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::EntityCounters const& counters);
	Q_SIGNAL void avbInterfaceCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceCounters const& counters);
	Q_SIGNAL void clockDomainCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockDomainCounters const& counters);
	Q_SIGNAL void streamInputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInputCounters const& counters);
	Q_SIGNAL void streamOutputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamOutputCounters const& counters);
	Q_SIGNAL void memoryObjectLengthChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, std::uint64_t const length);
	Q_SIGNAL void streamPortAudioMappingsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex);
	Q_SIGNAL void operationProgress(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::OperationID const operationID, float const percentComplete); // A negative percentComplete value means the progress is unknown but still continuing
	Q_SIGNAL void operationCompleted(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::OperationID const operationID, bool const failed);

	/* Connection changed signals */
	Q_SIGNAL void streamConnectionChanged(la::avdecc::entity::model::StreamConnectionState const& state);
	Q_SIGNAL void streamConnectionsChanged(la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamConnections const& connections);

	/* Entity commands signals */
	Q_SIGNAL void beginAecpCommand(la::avdecc::UniqueIdentifier const entityID, avdecc::ControllerManager::AecpCommandType commandType);
	Q_SIGNAL void endAecpCommand(la::avdecc::UniqueIdentifier const entityID, avdecc::ControllerManager::AecpCommandType commandType, la::avdecc::entity::ControllerEntity::AemCommandStatus const status);
	Q_SIGNAL void beginAcmpCommand(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, avdecc::ControllerManager::AcmpCommandType commandType);
	Q_SIGNAL void endAcmpCommand(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, avdecc::ControllerManager::AcmpCommandType commandType, la::avdecc::entity::ControllerEntity::ControlStatus const status);

	/* Counter errors signals */
	Q_SIGNAL void streamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, avdecc::ControllerManager::StreamInputErrorCounters const& errorCounters);

	/* Statistics signals */
	Q_SIGNAL void aecpRetryCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void aecpTimeoutCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void aecpUnexpectedResponseCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void aecpResponseAverageTimeChanged(la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const& value);
	Q_SIGNAL void aemAecpUnsolicitedCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void statisticsErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, avdecc::ControllerManager::StatisticsErrorCounters const& errorCounters);

}; // namespace avdecc

} // namespace avdecc
