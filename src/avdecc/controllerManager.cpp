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

#include "controllerManager.hpp"
#include <atomic>
#include <la/avdecc/logger.hpp>
#include "avdecc/helper.hpp"
#include "settingsManager/settings.hpp"

#if __cpp_lib_experimental_atomic_smart_pointers
#define HAVE_ATOMIC_SMART_POINTERS
#endif // __cpp_lib_experimental_atomic_smart_pointers

namespace avdecc
{

class ControllerManagerImpl final : public ControllerManager, private la::avdecc::controller::Controller::Observer, public settings::SettingsManager::Observer
{
public:
	using SharedController = std::shared_ptr<la::avdecc::controller::Controller>;
	using SharedConstController = std::shared_ptr<la::avdecc::controller::Controller const>;

	ControllerManagerImpl() noexcept
	{
		qRegisterMetaType<std::uint8_t>("std::uint8_t");
		qRegisterMetaType<AecpCommandType>("avdecc::ControllerManager::AecpCommandType");
		qRegisterMetaType<AcmpCommandType>("avdecc::ControllerManager::AcmpCommandType");
		qRegisterMetaType<la::avdecc::UniqueIdentifier>("la::avdecc::UniqueIdentifier");
		qRegisterMetaType<la::avdecc::entity::ControllerEntity::AemCommandStatus>("la::avdecc::entity::ControllerEntity::AemCommandStatus");
		qRegisterMetaType<la::avdecc::entity::ControllerEntity::ControlStatus>("la::avdecc::entity::ControllerEntity::ControlStatus");
		qRegisterMetaType<la::avdecc::entity::model::ConfigurationIndex>("la::avdecc::entity::model::ConfigurationIndex");
		qRegisterMetaType<la::avdecc::entity::model::DescriptorType>("la::avdecc::entity::model::DescriptorType");
		qRegisterMetaType<la::avdecc::entity::model::DescriptorIndex>("la::avdecc::entity::model::DescriptorIndex");
		qRegisterMetaType<la::avdecc::entity::model::AudioUnitIndex>("la::avdecc::entity::model::AudioUnitIndex");
		qRegisterMetaType<la::avdecc::entity::model::StreamIndex>("la::avdecc::entity::model::StreamIndex");
		qRegisterMetaType<la::avdecc::entity::model::AvbInterfaceIndex>("la::avdecc::entity::model::AvbInterfaceIndex");
		qRegisterMetaType<la::avdecc::entity::model::ClockSourceIndex>("la::avdecc::entity::model::ClockSourceIndex");
		qRegisterMetaType<la::avdecc::entity::model::MemoryObjectIndex>("la::avdecc::entity::model::MemoryObjectIndex");
		qRegisterMetaType<la::avdecc::entity::model::StreamPortIndex>("la::avdecc::entity::model::StreamPortIndex");
		qRegisterMetaType<la::avdecc::entity::model::MapIndex>("la::avdecc::entity::model::MapIndex");
		qRegisterMetaType<la::avdecc::entity::model::ClockDomainIndex>("la::avdecc::entity::model::ClockDomainIndex");
		qRegisterMetaType<la::avdecc::entity::model::SamplingRate>("la::avdecc::entity::model::SamplingRate");
		qRegisterMetaType<la::avdecc::entity::model::StreamFormat>("la::avdecc::entity::model::StreamFormat");
		qRegisterMetaType<la::avdecc::entity::model::StreamInfo>("la::avdecc::entity::model::StreamInfo");
		qRegisterMetaType<la::avdecc::entity::model::AvbInfo>("la::avdecc::entity::model::AvbInfo");
		qRegisterMetaType<la::avdecc::controller::Controller::QueryCommandError>("la::avdecc::controller::Controller::QueryCommandError");
		qRegisterMetaType<la::avdecc::controller::model::AcquireState>("la::avdecc::controller::model::AcquireState");
		qRegisterMetaType<la::avdecc::entity::model::StreamIdentification>("la::avdecc::entity::model::StreamIdentification");
		qRegisterMetaType<la::avdecc::controller::model::StreamConnectionState>("la::avdecc::controller::model::StreamConnectionState");
		qRegisterMetaType<la::avdecc::controller::model::StreamConnections>("la::avdecc::controller::model::StreamConnections");

		// Configure settings observers
		auto& settings = settings::SettingsManager::getInstance();
		settings.registerSettingObserver(settings::AemCacheEnabled.name, this);
	}

	~ControllerManagerImpl() noexcept
	{
		// Remove settings observers
		auto& settings = settings::SettingsManager::getInstance();
		settings.unregisterSettingObserver(settings::AemCacheEnabled.name, this);
	}

private:
	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override
	{
		auto ctrl = getController();
		if (ctrl)
		{
			if (value.toBool())
			{
				ctrl->enableEntityModelCache();
			}
			else
			{
				ctrl->disableEntityModelCache();
			}
		}
	}

	// la::avdecc::controller::Controller::Observer overrides
	// Global notifications
	virtual void onTransportError(la::avdecc::controller::Controller const* const /*controller*/) noexcept override
	{
		emit transportError();
	}
	virtual void onEntityQueryError(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::Controller::QueryCommandError const error) noexcept override
	{
		emit entityQueryError(entity->getEntity().getEntityID(), error);
	}
	// Discovery notifications (ADP)
	virtual void onEntityOnline(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity) noexcept override
	{
		emit entityOnline(entity->getEntity().getEntityID());
	}
	virtual void onEntityOffline(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity) noexcept override
	{
		emit entityOffline(entity->getEntity().getEntityID());
	}
	virtual void onGptpChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain) noexcept override
	{
		auto const& e = entity->getEntity();
		emit gptpChanged(e.getEntityID(), avbInterfaceIndex, grandMasterID, grandMasterDomain);
	}
	// Connection notifications (sniffed ACMP)
	virtual void onStreamConnectionChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::model::StreamConnectionState const& state, bool const /*changedByOther*/) noexcept override
	{
		emit streamConnectionChanged(state);
	}
	virtual void onStreamConnectionsChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamConnections const& connections) noexcept override
	{
		emit streamConnectionsChanged({entity->getEntity().getEntityID(), streamIndex}, connections);
	}
	// Entity model notifications (unsolicited AECP or changes this controller sent)
	virtual void onAcquireStateChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity) noexcept override
	{
		emit acquireStateChanged(entity->getEntity().getEntityID(), acquireState, owningEntity);
	}
	virtual void onStreamInputFormatChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat) noexcept override
	{
		emit streamFormatChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex, streamFormat);
	}
	virtual void onStreamOutputFormatChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat) noexcept override
	{
		emit streamFormatChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamOutput, streamIndex, streamFormat);
	}
	virtual void onStreamInputInfoChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& info) noexcept override
	{
		emit streamInfoChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex, info);
	}
	virtual void onStreamOutputInfoChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& info) noexcept override
	{
		emit streamInfoChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamOutput, streamIndex, info);
	}
	virtual void onEntityNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvdeccFixedString const& entityName) noexcept override
	{
		emit entityNameChanged(entity->getEntity().getEntityID(), QString::fromStdString(entityName));
	}
	virtual void onEntityGroupNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvdeccFixedString const& entityGroupName) noexcept override
	{
		emit entityGroupNameChanged(entity->getEntity().getEntityID(), QString::fromStdString(entityGroupName));
	}
	virtual void onConfigurationNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvdeccFixedString const& configurationName) noexcept override
	{
		emit configurationNameChanged(entity->getEntity().getEntityID(), configurationIndex, QString::fromStdString(configurationName));
	}
	virtual void onStreamInputNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvdeccFixedString const& streamName) noexcept override
	{
		emit streamNameChanged(entity->getEntity().getEntityID(), configurationIndex, la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex, QString::fromStdString(streamName));
	}
	virtual void onStreamOutputNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvdeccFixedString const& streamName) noexcept override
	{
		emit streamNameChanged(entity->getEntity().getEntityID(), configurationIndex, la::avdecc::entity::model::DescriptorType::StreamOutput, streamIndex, QString::fromStdString(streamName));
	}
	virtual void onAudioUnitSamplingRateChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate) noexcept override
	{
		emit audioUnitSamplingRateChanged(entity->getEntity().getEntityID(), audioUnitIndex, samplingRate);
	}
	virtual void onClockSourceChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex) noexcept override
	{
		emit clockSourceChanged(entity->getEntity().getEntityID(), clockDomainIndex, clockSourceIndex);
	}
	virtual void onStreamInputStarted(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept override
	{
		emit streamRunningChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex, true);
	}
	virtual void onStreamOutputStarted(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept override
	{
		emit streamRunningChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamOutput, streamIndex, true);
	}
	virtual void onStreamInputStopped(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept override
	{
		emit streamRunningChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex, false);
	}
	virtual void onStreamOutputStopped(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept override
	{
		emit streamRunningChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamOutput, streamIndex, false);
	}
	virtual void onAvbInfoChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInfo const& info) noexcept override
	{
		emit avbInfoChanged(entity->getEntity().getEntityID(), avbInterfaceIndex, info);
	}
	virtual void onStreamPortInputAudioMappingsChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamPortIndex const streamPortIndex) noexcept override
	{
		emit streamPortAudioMappingsChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamInput, streamPortIndex);
	}
	virtual void onStreamPortOutputAudioMappingsChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamPortIndex const streamPortIndex) noexcept override
	{
		emit streamPortAudioMappingsChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamOutput, streamPortIndex);
	}

	// ControllerManager overrides
	virtual void createController(la::avdecc::EndStation::ProtocolInterfaceType const protocolInterfaceType, QString const& interfaceName, std::uint16_t const progID, la::avdecc::UniqueIdentifier const entityModelID, QString const& preferedLocale) override
	{
		// If we have a previous controller, remove it
		if (_controller)
		{
			// First remove the observer so we don't get any new notifications
			_controller->unregisterObserver(this);

			// And destroy the controller itself
#if HAVE_ATOMIC_SMART_POINTERS
			_controller = Controller{ nullptr };
#else // !HAVE_ATOMIC_SMART_POINTERS
			std::atomic_store(&_controller, SharedController{ nullptr });
#endif // HAVE_ATOMIC_SMART_POINTERS

			emit controllerOffline();
		}

		// Create a new controller and store it
		SharedController controller = la::avdecc::controller::Controller::create(protocolInterfaceType, interfaceName.toStdString(), progID, entityModelID, preferedLocale.toStdString());
#if HAVE_ATOMIC_SMART_POINTERS
		_controller = std::move(controller);
#else // !HAVE_ATOMIC_SMART_POINTERS
		std::atomic_store(&_controller, std::move(controller));
#endif // HAVE_ATOMIC_SMART_POINTERS

		// Re-get the controller, just in case another thread changed the controller at the same moment
		auto ctrl = getController();
		if (ctrl)
		{
			emit controllerOnline();
			ctrl->registerObserver(this);
			//ctrl->enableEntityAdvertising(10);

			// Trigger setting observers
			auto& settings = settings::SettingsManager::getInstance();
			settings.triggerSettingObserver(settings::AemCacheEnabled.name, this);
		}
	}

	virtual la::avdecc::UniqueIdentifier getControllerEID() const noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			return controller->getControllerEID();
		}
		return la::avdecc::UniqueIdentifier{};
	}

	virtual la::avdecc::controller::ControlledEntityGuard getControlledEntity(la::avdecc::UniqueIdentifier const entityID) const noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			return controller->getControlledEntity(entityID);
		}
		return {};
	}

	/* Enumeration and Control Protocol (AECP) */
	virtual void acquireEntity(la::avdecc::UniqueIdentifier const targetEntityID, bool const isPersistent) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::AcquireEntity);
			controller->acquireEntity(targetEntityID, isPersistent, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const owningEntity) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "acquireEntity: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::AcquireEntity, status);
			});
		}
	}

	virtual void releaseEntity(la::avdecc::UniqueIdentifier const targetEntityID) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::ReleaseEntity);
			controller->releaseEntity(targetEntityID, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const owningEntity) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "releaseEntity: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::ReleaseEntity, status);
			});
	}
}

	virtual void setConfiguration(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetConfiguration);
			controller->setConfiguration(targetEntityID, configurationIndex, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "setConfiguration: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::SetConfiguration, status);
			});
		}
	}

	virtual void setStreamInputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetStreamFormat);
			controller->setStreamInputFormat(targetEntityID, streamIndex, streamFormat, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "setStreamInputFormat: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::SetStreamFormat, status);
			});
		}
	}

	virtual void setStreamOutputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetStreamFormat);
			controller->setStreamOutputFormat(targetEntityID, streamIndex, streamFormat, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "setStreamOutputFormat: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::SetStreamFormat, status);
			});
		}
	}

	virtual void setEntityName(la::avdecc::UniqueIdentifier const targetEntityID, QString const& name) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetEntityName);
			controller->setEntityName(targetEntityID, name.toStdString(), [this, targetEntityID, name](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "setEntityName: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::SetEntityName, status);
			});
		}
	}

	virtual void setEntityGroupName(la::avdecc::UniqueIdentifier const targetEntityID, QString const& name) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetEntityGroupName);
			controller->setEntityGroupName(targetEntityID, name.toStdString(), [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "setEntityGroupName: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::SetEntityGroupName, status);
			});
		}
	}

	virtual void setConfigurationName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& name) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetConfigurationName);
			controller->setConfigurationName(targetEntityID, configurationIndex, name.toStdString(), [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "setConfigurationName: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::SetConfigurationName, status);
			});
		}
	}

	virtual void setStreamInputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetStreamName);
			controller->setStreamInputName(targetEntityID, configurationIndex, streamIndex, name.toStdString(), [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "setStreamInputName: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::SetStreamName, status);
			});
		}
	}

	virtual void setStreamOutputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetStreamName);
			controller->setStreamOutputName(targetEntityID, configurationIndex, streamIndex, name.toStdString(), [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "setStreamOutputName: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::SetStreamName, status);
			});
		}
	}

	virtual void setAudioUnitSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetSamplingRate);
			controller->setAudioUnitSamplingRate(targetEntityID, audioUnitIndex, samplingRate, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "setAudioUnitSamplingRate: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::SetSamplingRate, status);
			});
		}
	}

	virtual void setClockSource(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetClockSource);
			controller->setClockSource(targetEntityID, clockDomainIndex, clockSourceIndex, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "setClockSource: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::SetClockSource, status);
			});
		}
	}

	virtual void startStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::StartStream);
			controller->startStreamInput(targetEntityID, streamIndex, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "startStreamInput: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::StartStream, status);
			});
		}
	}

	virtual void stopStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::StopStream);
			controller->stopStreamInput(targetEntityID, streamIndex, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "stopStreamInput: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::StopStream, status);
			});
		}
	}

	virtual void startStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::StartStream);
			controller->startStreamOutput(targetEntityID, streamIndex, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "startStreamOutput: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::StartStream, status);
			});
		}
	}

	virtual void stopStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::StopStream);
			controller->stopStreamOutput(targetEntityID, streamIndex, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "stopStreamOutput: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::StopStream, status);
			});
		}
	}

	virtual void addStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::AddStreamPortAudioMappings);
			controller->addStreamPortInputAudioMappings(targetEntityID, streamPortIndex, mappings, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "addStreamPortInputAudioMappings: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::AddStreamPortAudioMappings, status);
			});
		}
	}

	virtual void addStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::AddStreamPortAudioMappings);
			controller->addStreamPortOutputAudioMappings(targetEntityID, streamPortIndex, mappings, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "addStreamPortOutputAudioMappings: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::AddStreamPortAudioMappings, status);
			});
		}
	}

	virtual void removeStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::RemoveStreamPortAudioMappings);
			controller->removeStreamPortInputAudioMappings(targetEntityID, streamPortIndex, mappings, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "removeStreamPortInputAudioMappings: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::RemoveStreamPortAudioMappings, status);
			});
		}
	}

	virtual void removeStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::RemoveStreamPortAudioMappings);
			controller->removeStreamPortOutputAudioMappings(targetEntityID, streamPortIndex, mappings, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "removeStreamPortOutputAudioMappings: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAecpCommand(targetEntityID, AecpCommandType::RemoveStreamPortAudioMappings, status);
			});
		}
	}

	/* Enumeration and Control Protocol (AECP) AA */
	virtual void readDeviceMemory(la::avdecc::UniqueIdentifier const targetEntityID, std::uint64_t const address, std::uint64_t const length, la::avdecc::controller::Controller::ReadDeviceMemoryHandler const& handler) const noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			controller->readDeviceMemory(targetEntityID, address, length, handler);
		}
	}

	virtual void writeDeviceMemory(la::avdecc::UniqueIdentifier const targetEntityID, std::uint64_t const address, la::avdecc::controller::Controller::DeviceMemoryBuffer memoryBuffer, la::avdecc::controller::Controller::WriteDeviceMemoryHandler const& handler) const noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			controller->writeDeviceMemory(targetEntityID, address, memoryBuffer, handler);
		}
	}

	/* Connection Management Protocol (ACMP) */
	virtual void connectStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::ConnectStream);
			controller->connectStream({ talkerEntityID, talkerStreamIndex }, { listenerEntityID, listenerStreamIndex }, [this, talkerEntityID, listenerEntityID](la::avdecc::controller::ControlledEntity const* const talkerEntity, la::avdecc::controller::ControlledEntity const* const listenerEntity, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "connectStream: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::ConnectStream, status);
			});
		}
	}

	virtual void disconnectStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::DisconnectStream);
			controller->disconnectStream({ talkerEntityID, talkerStreamIndex }, { listenerEntityID, listenerStreamIndex }, [this, talkerEntityID, talkerStreamIndex, listenerEntityID](la::avdecc::controller::ControlledEntity const* const listenerEntity, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "disconnectStream: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::DisconnectStream, status);
			});
		}
	}

	virtual void disconnectTalkerStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::DisconnectTalkerStream);
			controller->disconnectTalkerStream({ talkerEntityID, talkerStreamIndex }, { listenerEntityID, listenerStreamIndex }, [this, talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex](la::avdecc::entity::ControllerEntity::ControlStatus const status) noexcept
			{
				//la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "disconnectTalkerStream: " + la::avdecc::entity::ControllerEntity::statusToString(status));
				emit endAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::DisconnectTalkerStream, status);
			});
		}
	}

	//virtual void getListenerStreamState(la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex) noexcept override
	//{
	//	auto controller = getController();
	//	if (controller)
	//	{
	//		emit beginAecpCommand(targetEntityID, AecpCommandType::RemoveStreamPortAudioMappings);
	//		controller->removeStreamPortOutputAudioMappings(targetEntityID, streamPortIndex, mappings, [this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
	//		{
	//			la::avdecc::Logger::getInstance().log(la::avdecc::Logger::Layer::FirstUserLayer, la::avdecc::Logger::Level::Trace, "removeStreamPortOutputAudioMappings: " + la::avdecc::entity::ControllerEntity::statusToString(status));
	//			emit endAecpCommand(targetEntityID, AecpCommandType::RemoveStreamPortAudioMappings, status);
	//		});
	//	}
	//}

	// Private methods
	SharedController getController() noexcept
	{
#if HAVE_ATOMIC_SMART_POINTERS
		return _controller;
#else // !HAVE_ATOMIC_SMART_POINTERS
		return std::atomic_load(&_controller);
#endif // HAVE_ATOMIC_SMART_POINTERS
	}

	SharedConstController getController() const noexcept
	{
#if HAVE_ATOMIC_SMART_POINTERS
		return _controller;
#else // !HAVE_ATOMIC_SMART_POINTERS
		return std::atomic_load(&_controller);
#endif // HAVE_ATOMIC_SMART_POINTERS
	}

	// Private members
#if HAVE_ATOMIC_SMART_POINTERS
	std::atomic_shared_ptr<la::avdecc::controller::Controller> _controller{ nullptr };
#else // !HAVE_ATOMIC_SMART_POINTERS
	SharedController _controller{ nullptr };
#endif // HAVE_ATOMIC_SMART_POINTERS
};

QString ControllerManager::typeToString(AecpCommandType const type) noexcept
{
	switch (type)
	{
		case AecpCommandType::None:
			AVDECC_ASSERT(false, "Should not happen");
			return "Unknown";
		case AecpCommandType::AcquireEntity:
			return "Acquire Entity";
		case AecpCommandType::ReleaseEntity:
			return "Release Entity";
		case AecpCommandType::SetConfiguration:
			return "Set Configuration";
		case AecpCommandType::SetStreamFormat:
			return "Set Stream Format";
		case AecpCommandType::SetEntityName:
			return "Set Entity Name";
		case AecpCommandType::SetEntityGroupName:
			return "Set Entity Group Name";
		case AecpCommandType::SetConfigurationName:
			return "Set Configuration Name";
		case AecpCommandType::SetStreamName:
			return "Set Stream Name";
		case AecpCommandType::SetSamplingRate:
			return "Set Sampling Rate";
		case AecpCommandType::SetClockSource:
			return "Set Clock Source";
		case AecpCommandType::StartStream:
			return "Start Streaming";
		case AecpCommandType::StopStream:
			return "Stop Streaming";
		case AecpCommandType::AddStreamPortAudioMappings:
			return "Add Audio Mappings";
		case AecpCommandType::RemoveStreamPortAudioMappings:
			return "Remove Audio Mappings";
		default:
			AVDECC_ASSERT(false, "Unhandled type");
			return "Unknown";
	}
}

QString ControllerManager::typeToString(AcmpCommandType const type) noexcept
{
	switch (type)
	{
		case AcmpCommandType::None:
			AVDECC_ASSERT(false, "Should not happen");
			return "Unknown";
		case AcmpCommandType::ConnectStream:
			return "Connect Stream";
		case AcmpCommandType::DisconnectStream:
			return "Disconnect Stream";
		case AcmpCommandType::DisconnectTalkerStream:
			return "Disconnect Talker Stream";
		default:
			AVDECC_ASSERT(false, "Unhandled type");
			return "Unknown";
	}
}

ControllerManager& ControllerManager::getInstance() noexcept
{
	static ControllerManagerImpl s_manager{};

	return s_manager;
}

} // namespace avdecc
