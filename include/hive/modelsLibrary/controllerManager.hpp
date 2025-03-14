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

#include "commandsExecutor.hpp"
#include <la/avdecc/controller/avdeccController.hpp>

#include <memory>
#include <chrono>
#include <unordered_map>
#include <cstdint>
#include <optional>

#include <QObject>

namespace hive
{
namespace modelsLibrary
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
		AemAecpUnsolicitedLosses = 1u << 3,
		MvuAecpUnsolicitedLosses = 1u << 4,
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
		SetJackName,
		SetAvbInterfaceName,
		SetClockSourceName,
		SetMemoryObjectName,
		SetAudioClusterName,
		SetControlName,
		SetClockDomainName,
		SetTimingName,
		SetPtpInstanceName,
		SetPtpPortName,
		SetAssociationID,
		SetSamplingRate,
		SetClockSource,
		SetControl,
		StartStream,
		StopStream,
		AddStreamPortAudioMappings,
		RemoveStreamPortAudioMappings,
		StartStoreAndRebootMemoryObjectOperation,
		StartUploadMemoryObjectOperation,
		AbortOperation,
		IdentifyEntity,
		SetMaxTransitTime,
	};

	enum class MilanCommandType
	{
		None = 0,
		SetSystemUniqueID,
		SetMediaClockReferenceInfo,
	};

	enum class AcmpCommandType
	{
		None = 0,
		ConnectStream,
		DisconnectStream,
		DisconnectTalkerStream,
	};

	/* AECP handlers to override the global AECP begin process. WARNING: Handlers are always called from the calling thread, before the method returns. */
	using BeginCommandHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID)>;

	/* AECP handlers to override the global AECP result process. WARNING: Handlers are always called from a non-gui thread. */
	using AcquireEntityHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const owningEntity)>;
	using ReleaseEntityHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using LockEntityHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const lockingEntity)>;
	using UnlockEntityHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetConfigurationHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
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
	using SetJackInputNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetJackOutputNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetAvbInterfaceNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetClockSourceNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetMemoryObjectNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetAudioClusterNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetControlNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetClockDomainNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetTimingNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetPtpInstanceNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetPtpPortNameHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetAssociationIDHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetAudioUnitSamplingRateHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetClockSourceHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetControlValuesHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
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
	using IdentifyEntityHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using SetSystemUniqueIDHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::MvuCommandStatus const status)>;
	using SetMediaClockReferenceInfoHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::MvuCommandStatus const status)>;
	/* ACMP handlers to override the global ACMP result process */
	using ConnectStreamHandler = std::function<void(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status)>;
	using DisconnectStreamHandler = std::function<void(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status)>;
	using DisconnectTalkerStreamHandler = std::function<void(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status)>;
	using RequestExclusiveAccessHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::controller::Controller::ExclusiveAccessToken::UniquePointer&& token)>;

	/**
			* @brief Creates a new controller, replacing previous one if any.
			* @details Creates a new controller, first removing the previous one if any.
			*          If an error occurs during the setup of the new controller, the previous one is NOT restored.
			* @note Might throw la::avdecc::controller::Controller::Exception.
			*       All observers should be removed from the previous controller before setting a new one.
			*/
	virtual void createController(la::avdecc::protocol::ProtocolInterface::Type const protocolInterfaceType, QString const& interfaceName, std::uint16_t const progID, la::avdecc::UniqueIdentifier const entityModelID, QString const& preferedLocale, la::avdecc::entity::model::EntityTree const* const entityModel) = 0;

	/** Destroys the currently stored instance of the controller. */
	virtual void destroyController() noexcept = 0;

	/** Gets the controller's EID */
	virtual la::avdecc::UniqueIdentifier getControllerEID() const noexcept = 0;

	/** Gets a ControlledEntity */
	virtual la::avdecc::controller::ControlledEntityGuard getControlledEntity(la::avdecc::UniqueIdentifier const entityID) const noexcept = 0;

	/** Serialize all known ControlledEntities */
	virtual std::tuple<la::avdecc::jsonSerializer::SerializationError, std::string> serializeAllControlledEntitiesAsJson(QString const& filePath, la::avdecc::entity::model::jsonSerializer::Flags const flags, QString const& dumpSource) const noexcept = 0;

	/** Serialize a ControlledEntity */
	virtual std::tuple<la::avdecc::jsonSerializer::SerializationError, std::string> serializeControlledEntityAsJson(la::avdecc::UniqueIdentifier const entityID, QString const& filePath, la::avdecc::entity::model::jsonSerializer::Flags const flags, QString const& dumpSource) const noexcept = 0;

	/** Deserializes a JSON file representing a full network state, and loads it as virtual ControlledEntities. */
	virtual std::tuple<la::avdecc::jsonSerializer::DeserializationError, std::string> loadVirtualEntitiesFromJsonNetworkState(QString const& filePath, la::avdecc::entity::model::jsonSerializer::Flags const flags) noexcept = 0;

	/** Deserializes a JSON file representing an entity, and loads it as a virtual ControlledEntity. */
	virtual std::tuple<la::avdecc::jsonSerializer::DeserializationError, std::string> loadVirtualEntityFromJson(QString const& filePath, la::avdecc::entity::model::jsonSerializer::Flags const flags) noexcept = 0;

	/** Re-enumerates the specified entity (physical entity only). */
	virtual bool refreshEntity(la::avdecc::UniqueIdentifier const entityID) noexcept = 0;

	/** Removes a Virtual Entity from the controller */
	virtual bool unloadVirtualEntity(la::avdecc::UniqueIdentifier const entityID) noexcept = 0;

	/** Returns the StreamFormat among the provided availableFormats, that best matches desiredStreamFormat, using clockValidator delegate callback. Returns invalid StreamFormat if none is available. */
	virtual la::avdecc::entity::model::StreamFormat chooseBestStreamFormat(la::avdecc::entity::model::StreamFormats const& availableFormats, la::avdecc::entity::model::StreamFormat const desiredStreamFormat, std::function<bool(bool const isDesiredClockSync, bool const isAvailableClockSync)> const& clockValidator) noexcept = 0;

	virtual bool isMediaClockStreamFormat(la::avdecc::entity::model::StreamFormat const streamFormat) noexcept = 0;

	/** Returns a checksum of the static entity model of the given ControlledEntity for the given checksumVersion (defaults to the maximum checksum version supported by the library) */
	virtual std::optional<std::string> computeEntityModelChecksum(la::avdecc::controller::ControlledEntity const& controlledEntity, std::uint32_t const checksumVersion = la::avdecc::controller::Controller::ChecksumVersion) noexcept = 0;

	/** Enable/Disable AEM cache */
	virtual void setEnableAemCache(bool const enable) noexcept = 0;

	/** Enable/Disable fast enumeration */
	virtual void setEnableFastEnumeration(bool const enable) noexcept = 0;

	/** Enable/Disable full AEM enumeration */
	virtual void setEnableFullAemEnumeration(bool const enable) noexcept = 0;

	/** Identify entity */
	virtual void identifyEntity(la::avdecc::UniqueIdentifier const targetEntityID, std::chrono::milliseconds const duration, IdentifyEntityHandler const& resultHandler = {}) noexcept = 0;

	/** Counter error flags */
	virtual StreamInputErrorCounters getStreamInputErrorCounters(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept = 0;
	virtual void clearStreamInputCounterValidFlags(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamInputCounterValidFlag const flag) noexcept = 0;
	virtual void clearAllStreamInputCounterValidFlags(la::avdecc::UniqueIdentifier const entityID) noexcept = 0;

	/** Statistics */
	virtual StatisticsErrorCounters getStatisticsCounters(la::avdecc::UniqueIdentifier const entityID) const noexcept = 0;
	virtual void clearStatisticsCounterValidFlags(la::avdecc::UniqueIdentifier const entityID, StatisticsErrorCounterFlag const flag) noexcept = 0;
	virtual void clearAllStatisticsCounterValidFlags(la::avdecc::UniqueIdentifier const entityID) noexcept = 0;

	/** Diagnostics */
	virtual la::avdecc::controller::ControlledEntity::Diagnostics getDiagnostics(la::avdecc::UniqueIdentifier const entityID) const noexcept = 0;
	virtual bool isRedundancyWarning(la::avdecc::UniqueIdentifier const entityID) const noexcept = 0;
	virtual bool getStreamInputLatencyError(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept = 0;
	virtual bool getControlValueOutOfBounds(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex) const noexcept = 0;

	/* Discovery Protocol (ADP) */
	/** Enables entity advertising with available duration included between 2-62 seconds on the specified interfaceIndex if set, otherwise on all interfaces. */
	virtual bool enableEntityAdvertising(std::uint32_t const availableDuration, std::optional<la::avdecc::entity::model::AvbInterfaceIndex> const interfaceIndex = std::nullopt) noexcept = 0;
	/** Disables entity advertising on the specified interfaceIndex if set, otherwise on all interfaces. */
	virtual void disableEntityAdvertising(std::optional<la::avdecc::entity::model::AvbInterfaceIndex> const interfaceIndex = std::nullopt) noexcept = 0;
	/** Requests a remote entities discovery. */
	virtual bool discoverRemoteEntities() const noexcept = 0;
	/** Requests a targetted remote entity discovery. */
	virtual bool discoverRemoteEntity(la::avdecc::UniqueIdentifier const entityID) const noexcept = 0;
	/** Forgets the specified remote entity. */
	virtual bool forgetRemoteEntity(la::avdecc::UniqueIdentifier const entityID) const noexcept = 0;
	/** Sets automatic discovery delay. 0 (default) for no automatic discovery. */
	virtual void setAutomaticDiscoveryDelay(std::chrono::milliseconds const delay) noexcept = 0;

	/* Enumeration and Control Protocol (AECP) AEM */
	/* For all AECP-AEM commands, if a handler is provided it will be called from the network thread. Otherwise the signal endAecpCommand will be emitted. */
	virtual void acquireEntity(la::avdecc::UniqueIdentifier const targetEntityID, bool const isPersistent, BeginCommandHandler const& beginHandler = {}, AcquireEntityHandler const& resultHandler = {}) noexcept = 0;
	virtual void releaseEntity(la::avdecc::UniqueIdentifier const targetEntityID, BeginCommandHandler const& beginHandler = {}, ReleaseEntityHandler const& resultHandler = {}) noexcept = 0;
	virtual void lockEntity(la::avdecc::UniqueIdentifier const targetEntityID, BeginCommandHandler const& beginHandler = {}, LockEntityHandler const& resultHandler = {}) noexcept = 0;
	virtual void unlockEntity(la::avdecc::UniqueIdentifier const targetEntityID, BeginCommandHandler const& beginHandler = {}, UnlockEntityHandler const& resultHandler = {}) noexcept = 0;
	virtual void setConfiguration(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, BeginCommandHandler const& beginHandler = {}, SetConfigurationHandler const& resultHandler = {}) noexcept = 0;
	virtual void setStreamInputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat, BeginCommandHandler const& beginHandler = {}, SetStreamInputFormatHandler const& resultHandler = {}) noexcept = 0;
	virtual void setStreamOutputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat, BeginCommandHandler const& beginHandler = {}, SetStreamOutputFormatHandler const& resultHandler = {}) noexcept = 0;
	//virtual void setStreamInputInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& streamInfo, BeginCommandHandler const& beginHandler = {}, SetStreamInputInfoHandler const& resultHandler = {}) noexcept = 0;
	virtual void setStreamOutputInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& streamInfo, BeginCommandHandler const& beginHandler = {}, SetStreamOutputInfoHandler const& resultHandler = {}) noexcept = 0;
	virtual void setEntityName(la::avdecc::UniqueIdentifier const targetEntityID, QString const& name, BeginCommandHandler const& beginHandler = {}, SetEntityNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setEntityGroupName(la::avdecc::UniqueIdentifier const targetEntityID, QString const& name, BeginCommandHandler const& beginHandler = {}, SetEntityGroupNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setConfigurationName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetConfigurationNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setAudioUnitName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetAudioUnitNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setStreamInputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetStreamInputNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setStreamOutputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetStreamOutputNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setJackInputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::JackIndex const jackIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetJackInputNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setJackOutputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::JackIndex const jackIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetJackOutputNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setAvbInterfaceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetAvbInterfaceNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setClockSourceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetClockSourceNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setMemoryObjectName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetMemoryObjectNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setAudioClusterName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetAudioClusterNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setControlName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetControlNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setClockDomainName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetClockDomainNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setTimingName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::TimingIndex const timingIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetTimingNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setPtpInstanceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpInstanceIndex const ptpInstanceIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetPtpInstanceNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setPtpPortName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpPortIndex const ptpPortIndex, QString const& name, BeginCommandHandler const& beginHandler = {}, SetPtpPortNameHandler const& resultHandler = {}) noexcept = 0;
	virtual void setAssociationID(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::UniqueIdentifier const associationID, BeginCommandHandler const& beginHandler = {}, SetAssociationIDHandler const& resultHandler = {}) noexcept = 0;
	virtual void setAudioUnitSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate, BeginCommandHandler const& beginHandler = {}, SetAudioUnitSamplingRateHandler const& resultHandler = {}) noexcept = 0;
	virtual void setClockSource(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, BeginCommandHandler const& beginHandler = {}, SetClockSourceHandler const& resultHandler = {}) noexcept = 0;
	virtual void setControlValues(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlValues const& controlValues, BeginCommandHandler const& beginHandler = {}, SetControlValuesHandler const& resultHandler = {}) noexcept = 0;
	virtual void startStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, BeginCommandHandler const& beginHandler = {}, StartStreamInputHandler const& resultHandler = {}) noexcept = 0;
	virtual void stopStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, BeginCommandHandler const& beginHandler = {}, StopStreamInputHandler const& resultHandler = {}) noexcept = 0;
	virtual void startStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, BeginCommandHandler const& beginHandler = {}, StartStreamOutputHandler const& resultHandler = {}) noexcept = 0;
	virtual void stopStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, BeginCommandHandler const& beginHandler = {}, StopStreamOutputHandler const& resultHandler = {}) noexcept = 0;
	virtual void addStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, BeginCommandHandler const& beginHandler = {}, AddStreamPortInputAudioMappingsHandler const& resultHandler = {}) noexcept = 0;
	virtual void addStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, BeginCommandHandler const& beginHandler = {}, AddStreamPortOutputAudioMappingsHandler const& resultHandler = {}) noexcept = 0;
	virtual void removeStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, BeginCommandHandler const& beginHandler = {}, RemoveStreamPortInputAudioMappingsHandler const& resultHandler = {}) noexcept = 0;
	virtual void removeStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, BeginCommandHandler const& beginHandler = {}, RemoveStreamPortOutputAudioMappingsHandler const& resultHandler = {}) noexcept = 0;
	virtual void startStoreAndRebootMemoryObjectOperation(la::avdecc::UniqueIdentifier targetEntityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, BeginCommandHandler const& beginHandler = {}, StartStoreAndRebootMemoryObjectOperationHandler const& resultHandler = {}) noexcept = 0;
	virtual void startUploadMemoryObjectOperation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, std::uint64_t const dataLength, BeginCommandHandler const& beginHandler = {}, StartUploadMemoryObjectOperationHandler const& resultHandler = {}) noexcept = 0;
	virtual void abortOperation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::OperationID const operationID, BeginCommandHandler const& beginHandler = {}, AbortOperationHandler const& resultHandler = {}) noexcept = 0;

	/* Enumeration and Control Protocol (AECP) AA */
	/* For all AECP-AA commands, the provided handler will be called from the network thread. */
	virtual void readDeviceMemory(la::avdecc::UniqueIdentifier const targetEntityID, std::uint64_t const address, std::uint64_t const length, la::avdecc::controller::Controller::ReadDeviceMemoryProgressHandler const& progressHandler, la::avdecc::controller::Controller::ReadDeviceMemoryCompletionHandler const& completionHandler) const noexcept = 0;
	virtual void writeDeviceMemory(la::avdecc::UniqueIdentifier const targetEntityID, std::uint64_t const address, la::avdecc::controller::Controller::DeviceMemoryBuffer memoryBuffer, la::avdecc::controller::Controller::WriteDeviceMemoryProgressHandler const& progressHandler, la::avdecc::controller::Controller::WriteDeviceMemoryCompletionHandler const& completionHandler) const noexcept = 0;

	/* Enumeration and Control Protocol (AECP) MVU handlers (Milan Vendor Unique) */
	virtual void setSystemUniqueID(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::SystemUniqueIdentifier const systemUniqueID, BeginCommandHandler const& beginHandler = {}, SetSystemUniqueIDHandler const& resultHandler = {}) noexcept = 0;
	virtual void setMediaClockReferenceInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, std::optional<la::avdecc::entity::model::MediaClockReferencePriority> const userPriority, std::optional<la::avdecc::entity::model::AvdeccFixedString> const& domainName, BeginCommandHandler const& beginHandler = {}, SetMediaClockReferenceInfoHandler const& resultHandler = {}) noexcept = 0;

	/* Connection Management Protocol (ACMP) */
	/* For all ACMP commands, if a handler is provided it will be called from the network thread. Otherwise the signal endAcmpCommand will be emitted. */
	virtual void connectStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, ConnectStreamHandler const& resultHandler = {}) noexcept = 0;
	virtual void disconnectStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, DisconnectStreamHandler const& resultHandler = {}) noexcept = 0;
	virtual void disconnectTalkerStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, DisconnectTalkerStreamHandler const& resultHandler = {}) noexcept = 0;

	/** Requests an ExclusiveAccessToken for the specified entityID. If the call succeeded (AemCommandStatus::Success), a valid token will be returned in the handler (from the network thread). */
	virtual void requestExclusiveAccess(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::Controller::ExclusiveAccessToken::AccessType const type, RequestExclusiveAccessHandler const& handler) noexcept = 0;

	/** Creates a CommandsExecutor for the specified entityID, optionally requesting exclusive access to the entity. The executor will be passed to the handler (in the calling thread) and will start as soon as the handler returns. */
	virtual void createCommandsExecutor(la::avdecc::UniqueIdentifier const entityID, bool const requestExclusiveAccess, std::function<void(hive::modelsLibrary::CommandsExecutor&)> const& handler) noexcept = 0;

	using ControlledEntityCallback = std::function<void(la::avdecc::UniqueIdentifier const&, la::avdecc::controller::ControlledEntity const&)>;
	virtual void foreachEntity(ControlledEntityCallback const& callback) noexcept = 0;

	/* Static methods */
	static QString typeToString(AecpCommandType const type) noexcept;
	static QString typeToString(MilanCommandType const type) noexcept;
	static QString typeToString(AcmpCommandType const type) noexcept;

	/* Controller signals */
	Q_SIGNAL void controllerOnline();
	Q_SIGNAL void controllerOffline();

	/* Entity changed signals */
	Q_SIGNAL void transportError();
	Q_SIGNAL void entityQueryError(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::Controller::QueryCommandError const error);
	Q_SIGNAL void entityOnline(la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const enumerationTime);
	Q_SIGNAL void entityOffline(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void entityRedundantInterfaceOnline(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::Entity::InterfaceInformation const& interfaceInfo);
	Q_SIGNAL void entityRedundantInterfaceOffline(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);
	Q_SIGNAL void unsolicitedRegistrationChanged(la::avdecc::UniqueIdentifier const entityID, bool const isSubscribed);
	Q_SIGNAL void compatibilityFlagsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags);
	Q_SIGNAL void entityCapabilitiesChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::EntityCapabilities const entityCapabilities);
	Q_SIGNAL void associationIDChanged(la::avdecc::UniqueIdentifier const entityID, std::optional<la::avdecc::UniqueIdentifier> const associationID);
	Q_SIGNAL void identificationStarted(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void identificationStopped(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain);
	Q_SIGNAL void acquireStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity);
	Q_SIGNAL void lockStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity);
	Q_SIGNAL void streamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat);
	Q_SIGNAL void streamDynamicInfoChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamDynamicInfo const& info);
	Q_SIGNAL void entityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityName);
	Q_SIGNAL void entityGroupNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityGroupName);
	Q_SIGNAL void configurationNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& configurationName);
	Q_SIGNAL void audioUnitNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, QString const& audioUnitName);
	Q_SIGNAL void streamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& streamName);
	Q_SIGNAL void jackNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::JackIndex const jackIndex, QString const& jackName);
	Q_SIGNAL void avbInterfaceNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, QString const& avbInterfaceName);
	Q_SIGNAL void clockSourceNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, QString const& clockSourceName);
	Q_SIGNAL void memoryObjectNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, QString const& memoryObjectName);
	Q_SIGNAL void audioClusterNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& audioClusterName);
	Q_SIGNAL void controlNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, QString const& controlName);
	Q_SIGNAL void clockDomainNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, QString const& clockDomainName);
	Q_SIGNAL void timingNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::TimingIndex const timingIndex, QString const& timingName);
	Q_SIGNAL void ptpInstanceNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpInstanceIndex const ptpInstanceIndex, QString const& ptpInstanceName);
	Q_SIGNAL void ptpPortNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::PtpPortIndex const ptpPortIndex, QString const& ptpPortName);
	Q_SIGNAL void audioUnitSamplingRateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate);
	Q_SIGNAL void clockSourceChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const sourceIndex);
	Q_SIGNAL void controlValuesChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlValues const& controlValues);
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
	Q_SIGNAL void mediaClockChainChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::controller::model::MediaClockChain const& mcChain);
	Q_SIGNAL void maxTransitTimeChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, std::chrono::nanoseconds const& maxTransitTime);
	Q_SIGNAL void systemUniqueIDChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::SystemUniqueIdentifier const systemUniqueID);
	Q_SIGNAL void mediaClockReferenceInfoChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::MediaClockReferenceInfo const& info);

	/* Connection changed signals */
	Q_SIGNAL void streamInputConnectionChanged(la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamInputConnectionInfo const& info);
	Q_SIGNAL void streamOutputConnectionsChanged(la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamConnections const& connections);

	/* Entity commands signals */
	Q_SIGNAL void beginAecpCommand(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex);
	Q_SIGNAL void endAecpCommand(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::ControllerEntity::AemCommandStatus const status);
	Q_SIGNAL void beginMilanCommand(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::MilanCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex);
	Q_SIGNAL void endMilanCommand(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::MilanCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::ControllerEntity::MvuCommandStatus const status);
	Q_SIGNAL void beginAcmpCommand(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, hive::modelsLibrary::ControllerManager::AcmpCommandType commandType);
	Q_SIGNAL void endAcmpCommand(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, hive::modelsLibrary::ControllerManager::AcmpCommandType commandType, la::avdecc::entity::ControllerEntity::ControlStatus const status);

	/* Counter errors signals */
	Q_SIGNAL void streamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, hive::modelsLibrary::ControllerManager::StreamInputErrorCounters const& errorCounters);

	/* Statistics signals */
	Q_SIGNAL void aecpRetryCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void aecpTimeoutCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void aecpUnexpectedResponseCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void aecpResponseAverageTimeChanged(la::avdecc::UniqueIdentifier const entityID, std::chrono::milliseconds const& value);
	Q_SIGNAL void aemAecpUnsolicitedCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void aemAecpUnsolicitedLossCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void mvuAecpUnsolicitedCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void mvuAecpUnsolicitedLossCounterChanged(la::avdecc::UniqueIdentifier const entityID, std::uint64_t const value);
	Q_SIGNAL void statisticsErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::StatisticsErrorCounters const& errorCounters);

	/* Diagnostics signals */
	Q_SIGNAL void diagnosticsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::Diagnostics const& diagnostics);
	Q_SIGNAL void redundancyWarningChanged(la::avdecc::UniqueIdentifier const entityID, bool const isRedundancyWarning);
	Q_SIGNAL void streamInputLatencyErrorChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isLatencyError);
	Q_SIGNAL void controlCurrentValueOutOfBoundsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, bool const isValueOutOfBounds);
};

} // namespace modelsLibrary
} // namespace hive
