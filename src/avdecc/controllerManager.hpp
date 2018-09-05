/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <la/avdecc/controller/avdeccController.hpp>
#include <memory>
#include <QObject>

namespace avdecc
{

class ControllerManager : public QObject
{
	Q_OBJECT
public:
	static ControllerManager& getInstance() noexcept;

	enum class AecpCommandType
	{
		None = 0,
		AcquireEntity,
		ReleaseEntity,
		SetConfiguration,
		SetStreamFormat,
		SetEntityName,
		SetEntityGroupName,
		SetConfigurationName,
		SetStreamName,
		SetSamplingRate,
		SetClockSource,
		StartStream,
		StopStream,
		AddStreamPortAudioMappings,
		RemoveStreamPortAudioMappings,
	};

	enum class AcmpCommandType
	{
		None = 0,
		ConnectStream,
		DisconnectStream,
		DisconnectTalkerStream,
	};

	/**
	* @brief Creates a new controller, replacing previous one if any.
	* @details Creates a new controller, first removing the previous one if any.
	*          If an error occurs during the setup of the new controller, the previous one is NOT restored.
	* @note Might throw la::avdecc::controller::Controller::Exception.
	*       All observers should be removed from the previous controller before setting a new one.
	*/
	virtual void createController(la::avdecc::EndStation::ProtocolInterfaceType const protocolInterfaceType, QString const& interfaceName, std::uint16_t const progID, la::avdecc::UniqueIdentifier const entityModelID, QString const& preferedLocale) = 0;

	/** Gets the controller's EID */
	virtual la::avdecc::UniqueIdentifier getControllerEID() const noexcept = 0;

	/** Gets a ControlledEntity */
	virtual la::avdecc::controller::ControlledEntityGuard getControlledEntity(la::avdecc::UniqueIdentifier const entityID) const noexcept = 0;

	/* Enumeration and Control Protocol (AECP) */
	virtual void acquireEntity(la::avdecc::UniqueIdentifier const targetEntityID, bool const isPersistent) noexcept = 0;
	virtual void releaseEntity(la::avdecc::UniqueIdentifier const targetEntityID) noexcept = 0;
	virtual void setConfiguration(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex) noexcept = 0;
	virtual void setStreamInputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat) noexcept = 0;
	virtual void setStreamOutputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat) noexcept = 0;
	virtual void setEntityName(la::avdecc::UniqueIdentifier const targetEntityID, QString const& name) noexcept = 0;
	virtual void setEntityGroupName(la::avdecc::UniqueIdentifier const targetEntityID, QString const& name) noexcept = 0;
	virtual void setConfigurationName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& name) noexcept = 0;
	virtual void setStreamInputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name) noexcept = 0;
	virtual void setStreamOutputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name) noexcept = 0;
	virtual void setAudioUnitSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate) noexcept = 0;
	virtual void setClockSource(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex) noexcept = 0;
	virtual void startStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept = 0;
	virtual void stopStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept = 0;
	virtual void startStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept = 0;
	virtual void stopStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept = 0;
	virtual void addStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept = 0;
	virtual void addStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept = 0;
	virtual void removeStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept = 0;
	virtual void removeStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept = 0;
	virtual void startOperation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, std::uint16_t const operationId, la::avdecc::entity::model::MemoryObjectOperations const operationType, la::avdecc::controller::Controller::StartOperationHandler const & handler) noexcept = 0;
	virtual void startUploadOperation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, std::uint64_t const dataLength, la::avdecc::controller::Controller::StartOperationHandler const & handler) noexcept = 0;
	//virtual void onOperationStatus(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType descriptorType, la::avdecc::entity::model::DescriptorIndex descriptorIndex, std::uint16_t operationId, std::uint16_t percentComplete) noexcept = 0;
	//virtual void onOperationStatus(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType descriptorType, la::avdecc::entity::model::DescriptorIndex descriptorIndex, std::uint16_t operationId, std::uint16_t percentComplete) noexcept override

	/* Enumeration and Control Protocol (AECP) AA */
	virtual void readDeviceMemory(la::avdecc::UniqueIdentifier const targetEntityID, std::uint64_t const address, std::uint64_t const length, la::avdecc::controller::Controller::ReadDeviceMemoryHandler const& handler) const noexcept = 0;
	virtual void writeDeviceMemory(la::avdecc::UniqueIdentifier const targetEntityID, std::uint64_t const address, la::avdecc::controller::Controller::DeviceMemoryBuffer memoryBuffer, la::avdecc::controller::Controller::WriteDeviceMemoryHandler const& handler) const noexcept = 0;

	/* Connection Management Protocol (ACMP) */
	virtual void connectStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex) noexcept = 0;
	virtual void disconnectStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex) noexcept = 0;
	virtual void disconnectTalkerStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex) noexcept = 0;
	//virtual void getListenerStreamState(la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex) noexcept = 0;

	/* Static methods */
	static QString typeToString(AecpCommandType const type) noexcept;
	static QString typeToString(AcmpCommandType const type) noexcept;

	/* Controller signals */
	Q_SIGNAL void controllerOnline();
	Q_SIGNAL void controllerOffline();

	/* Entity changed signals */
	Q_SIGNAL void transportError();
	Q_SIGNAL void entityQueryError(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::Controller::QueryCommandError const error);
	Q_SIGNAL void entityOnline(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void entityOffline(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain);
	Q_SIGNAL void acquireStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity);
	Q_SIGNAL void streamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat);
	Q_SIGNAL void streamInfoChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& info);
	Q_SIGNAL void entityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityName);
	Q_SIGNAL void entityGroupNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityGroupName);
	Q_SIGNAL void configurationNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& configurationName);
	Q_SIGNAL void streamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& streamName);
	Q_SIGNAL void audioUnitSamplingRateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate);
	Q_SIGNAL void clockSourceChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const sourceIndex);
	Q_SIGNAL void streamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning);
	Q_SIGNAL void avbInfoChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInfo const& info);
	Q_SIGNAL void streamPortAudioMappingsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex);
	Q_SIGNAL void memoryObjectLengthChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, std::uint64_t const length);

	/* Connection changed signals */
	Q_SIGNAL void streamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& state);
	Q_SIGNAL void streamConnectionsChanged(la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::controller::model::StreamConnections const& connections);

	/* Entity commands signals */
	Q_SIGNAL void beginAecpCommand(la::avdecc::UniqueIdentifier const entityID, avdecc::ControllerManager::AecpCommandType commandType);
	Q_SIGNAL void endAecpCommand(la::avdecc::UniqueIdentifier const entityID, avdecc::ControllerManager::AecpCommandType commandType, la::avdecc::entity::ControllerEntity::AemCommandStatus const status);
	Q_SIGNAL void beginAcmpCommand(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, avdecc::ControllerManager::AcmpCommandType commandType);
	Q_SIGNAL void endAcmpCommand(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, avdecc::ControllerManager::AcmpCommandType commandType, la::avdecc::entity::ControllerEntity::ControlStatus const status);

	/* Unsolicited Response */
	Q_SIGNAL void operationStatus(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType descriptorType, la::avdecc::entity::model::DescriptorIndex descriptorIndex, std::uint16_t operationId, std::uint16_t percentComplete);
};

} // namespace avdecc
