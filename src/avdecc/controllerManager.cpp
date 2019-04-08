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

#include "controllerManager.hpp"
#include <atomic>
#include <la/avdecc/logger.hpp>
#include "avdecc/helper.hpp"
#include "settingsManager/settings.hpp"

#if __cpp_lib_experimental_atomic_smart_pointers
#	define HAVE_ATOMIC_SMART_POINTERS
#endif // __cpp_lib_experimental_atomic_smart_pointers

namespace avdecc
{
class ControllerManagerImpl final : public ControllerManager, private la::avdecc::controller::Controller::Observer, public settings::SettingsManager::Observer
{
public:
	using SharedController = std::shared_ptr<la::avdecc::controller::Controller>;
	using SharedConstController = std::shared_ptr<la::avdecc::controller::Controller const>;

	class ErrorCounterTracker
	{
		class InitCounterVisitor : public la::avdecc::controller::model::EntityModelVisitor
		{
		public:
			InitCounterVisitor(ErrorCounterTracker& errorCounterTracker)
				: _errorCounterTracker{ errorCounterTracker }
			{
			}

		protected:
			// la::avdecc::controller::model::EntityModelVisitor overrides
			virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
			{
				for (auto const& counterKV : node.dynamicModel->counters)
				{
					auto const& flag = counterKV.first;
					auto const& counter = counterKV.second;

					// Initialize internal counter value
					auto& errorCounter = _errorCounterTracker._streamInputCounter[node.descriptorIndex];
					errorCounter.counters[flag] = counter;
				}
			}

		private:
			ErrorCounterTracker& _errorCounterTracker;
		};

		class ClearCounterVisitor : public la::avdecc::controller::model::EntityModelVisitor
		{
		public:
			ClearCounterVisitor(ControllerManager& manager, ErrorCounterTracker& errorCounterTracker)
				: _manager{ manager }
				, _errorCounterTracker{ errorCounterTracker }
			{
			}

		protected:
			// la::avdecc::controller::model::EntityModelVisitor overrides
			virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
			{
				auto& errorCounter = _errorCounterTracker._streamInputCounter[node.descriptorIndex];

				if (!errorCounter.flags.empty())
				{
					errorCounter.flags.clear();

					/*emit*/ _manager.streamInputErrorCounterChanged(entity->getEntity().getEntityID(), node.descriptorIndex, errorCounter.flags);
				}
			}

		private:
			ControllerManager& _manager;
			ErrorCounterTracker& _errorCounterTracker;
		};

	public:
		ErrorCounterTracker()
			: ErrorCounterTracker{ la::avdecc::UniqueIdentifier::getNullUniqueIdentifier() }
		{
		}

		ErrorCounterTracker(la::avdecc::UniqueIdentifier const& entityID)
			: _entityID{ entityID }
		{
			InitCounterVisitor visitor{ *this };
			if (auto entity = ControllerManager::getInstance().getControlledEntity(_entityID))
			{
				entity->accept(&visitor);
			}
		}

		la::avdecc::entity::StreamInputCounterValidFlags getStreamInputCounterValidFlags(la::avdecc::entity::model::StreamIndex const streamIndex) const
		{
			auto const streamIt = _streamInputCounter.find(streamIndex);
			if (streamIt != std::end(_streamInputCounter))
			{
				auto const& errorCounter{ streamIt->second };
				return errorCounter.flags;
			}
			return {};
		}

		// Set the new counter value, returns true if the counter has changed, false otherwise
		bool setStreamInputCounter(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamInputCounterValidFlag const flag, la::avdecc::entity::model::DescriptorCounter const counter)
		{
			auto& errorCounter = _streamInputCounter[streamIndex];

			auto shouldNotify{ false };

			// Detect counter reset (or wrap)
			if (counter < errorCounter.counters[flag])
			{
				errorCounter.counters[flag] = 0;
				errorCounter.flags.reset(flag);
				shouldNotify = true;
			}

			if (counter > errorCounter.counters[flag])
			{
				errorCounter.flags.set(flag);
				shouldNotify = true;
			}

			errorCounter.counters[flag] = counter;

			return shouldNotify;
		}

		// Clear the error for a given flag, returns true if the flag has changed, false otherwise
		bool clearStreamInputCounter(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamInputCounterValidFlag const flag)
		{
			auto& errorCounter = _streamInputCounter[streamIndex];

			if (errorCounter.flags.test(flag))
			{
				errorCounter.flags.reset(flag);
				return true;
			}

			return false;
		}

		// Clear all the error flags for all streams
		void clearAllStreamInputCounters()
		{
			auto& manager = ControllerManager::getInstance();
			ClearCounterVisitor visitor{ manager, *this };
			if (auto entity = manager.getControlledEntity(_entityID))
			{
				entity->accept(&visitor);
			}
		}

	private:
		la::avdecc::UniqueIdentifier _entityID{ la::avdecc::UniqueIdentifier::getNullUniqueIdentifier() };

		struct ErrorCounter
		{
			la::avdecc::entity::StreamInputCounterValidFlags flags; // per flag state 1: error, 0: clear
			std::unordered_map<la::avdecc::entity::StreamInputCounterValidFlag, la::avdecc::entity::model::DescriptorCounter> counters; // per flag counter value
		};

		std::unordered_map<la::avdecc::entity::model::StreamIndex, ErrorCounter> _streamInputCounter;
	};

	ControllerManagerImpl() noexcept
	{
		qRegisterMetaType<std::uint8_t>("std::uint8_t");
		qRegisterMetaType<AecpCommandType>("avdecc::ControllerManager::AecpCommandType");
		qRegisterMetaType<AcmpCommandType>("avdecc::ControllerManager::AcmpCommandType");
		qRegisterMetaType<la::avdecc::UniqueIdentifier>("la::avdecc::UniqueIdentifier");
		qRegisterMetaType<la::avdecc::entity::ControllerEntity::AemCommandStatus>("la::avdecc::entity::ControllerEntity::AemCommandStatus");
		qRegisterMetaType<la::avdecc::entity::ControllerEntity::ControlStatus>("la::avdecc::entity::ControllerEntity::ControlStatus");
		qRegisterMetaType<la::avdecc::entity::StreamInputCounterValidFlags>("la::avdecc::entity::StreamInputCounterValidFlags");
		qRegisterMetaType<la::avdecc::entity::model::AvdeccFixedString>("la::avdecc::entity::model::AvdeccFixedString");
		qRegisterMetaType<la::avdecc::entity::model::ConfigurationIndex>("la::avdecc::entity::model::ConfigurationIndex");
		qRegisterMetaType<la::avdecc::entity::model::DescriptorType>("la::avdecc::entity::model::DescriptorType");
		qRegisterMetaType<la::avdecc::entity::model::DescriptorIndex>("la::avdecc::entity::model::DescriptorIndex");
		qRegisterMetaType<la::avdecc::entity::model::AudioUnitIndex>("la::avdecc::entity::model::AudioUnitIndex");
		qRegisterMetaType<la::avdecc::entity::model::StreamIndex>("la::avdecc::entity::model::StreamIndex");
		qRegisterMetaType<la::avdecc::entity::model::JackIndex>("la::avdecc::entity::model::JackIndex");
		qRegisterMetaType<la::avdecc::entity::model::AvbInterfaceIndex>("la::avdecc::entity::model::AvbInterfaceIndex");
		qRegisterMetaType<la::avdecc::entity::model::ClockSourceIndex>("la::avdecc::entity::model::ClockSourceIndex");
		qRegisterMetaType<la::avdecc::entity::model::MemoryObjectIndex>("la::avdecc::entity::model::MemoryObjectIndex");
		qRegisterMetaType<la::avdecc::entity::model::LocaleIndex>("la::avdecc::entity::model::LocaleIndex");
		qRegisterMetaType<la::avdecc::entity::model::StringsIndex>("la::avdecc::entity::model::StringsIndex");
		qRegisterMetaType<la::avdecc::entity::model::StreamPortIndex>("la::avdecc::entity::model::StreamPortIndex");
		qRegisterMetaType<la::avdecc::entity::model::ExternalPortIndex>("la::avdecc::entity::model::ExternalPortIndex");
		qRegisterMetaType<la::avdecc::entity::model::InternalPortIndex>("la::avdecc::entity::model::InternalPortIndex");
		qRegisterMetaType<la::avdecc::entity::model::ClusterIndex>("la::avdecc::entity::model::ClusterIndex");
		qRegisterMetaType<la::avdecc::entity::model::MapIndex>("la::avdecc::entity::model::MapIndex");
		qRegisterMetaType<la::avdecc::entity::model::ControlIndex>("la::avdecc::entity::model::ControlIndex");
		qRegisterMetaType<la::avdecc::entity::model::SignalSelectorIndex>("la::avdecc::entity::model::SignalSelectorIndex");
		qRegisterMetaType<la::avdecc::entity::model::MixerIndex>("la::avdecc::entity::model::MixerIndex");
		qRegisterMetaType<la::avdecc::entity::model::MatrixIndex>("la::avdecc::entity::model::MatrixIndex");
		qRegisterMetaType<la::avdecc::entity::model::SignalSplitterIndex>("la::avdecc::entity::model::SignalSplitterIndex");
		qRegisterMetaType<la::avdecc::entity::model::SignalCombinerIndex>("la::avdecc::entity::model::SignalCombinerIndex");
		qRegisterMetaType<la::avdecc::entity::model::SignalDemultiplexerIndex>("la::avdecc::entity::model::SignalDemultiplexerIndex");
		qRegisterMetaType<la::avdecc::entity::model::SignalMultiplexerIndex>("la::avdecc::entity::model::SignalMultiplexerIndex");
		qRegisterMetaType<la::avdecc::entity::model::SignalTranscoderIndex>("la::avdecc::entity::model::SignalTranscoderIndex");
		qRegisterMetaType<la::avdecc::entity::model::ClockDomainIndex>("la::avdecc::entity::model::ClockDomainIndex");
		qRegisterMetaType<la::avdecc::entity::model::ControlBlockIndex>("la::avdecc::entity::model::ControlBlockIndex");
		qRegisterMetaType<la::avdecc::entity::model::SamplingRate>("la::avdecc::entity::model::SamplingRate");
		qRegisterMetaType<la::avdecc::entity::model::StreamFormat>("la::avdecc::entity::model::StreamFormat");
		qRegisterMetaType<la::avdecc::entity::model::OperationID>("la::avdecc::entity::model::OperationID");
		qRegisterMetaType<la::avdecc::entity::model::StreamInfo>("la::avdecc::entity::model::StreamInfo");
		qRegisterMetaType<la::avdecc::entity::model::AvbInfo>("la::avdecc::entity::model::AvbInfo");
		qRegisterMetaType<la::avdecc::entity::model::AsPath>("la::avdecc::entity::model::AsPath");
		qRegisterMetaType<la::avdecc::entity::model::StreamIdentification>("la::avdecc::entity::model::StreamIdentification");
		qRegisterMetaType<la::avdecc::entity::model::StreamConnectionState>("la::avdecc::entity::model::StreamConnectionState");
		qRegisterMetaType<la::avdecc::entity::model::StreamConnections>("la::avdecc::entity::model::StreamConnections");
		qRegisterMetaType<la::avdecc::entity::model::EntityCounters>("la::avdecc::entity::model::EntityCounters");
		qRegisterMetaType<la::avdecc::entity::model::AvbInterfaceCounters>("la::avdecc::entity::model::AvbInterfaceCounters");
		qRegisterMetaType<la::avdecc::entity::model::ClockDomainCounters>("la::avdecc::entity::model::ClockDomainCounters");
		qRegisterMetaType<la::avdecc::entity::model::StreamInputCounters>("la::avdecc::entity::model::StreamInputCounters");
		qRegisterMetaType<la::avdecc::entity::model::StreamOutputCounters>("la::avdecc::entity::model::StreamOutputCounters");
		qRegisterMetaType<la::avdecc::controller::Controller::QueryCommandError>("la::avdecc::controller::Controller::QueryCommandError");
		qRegisterMetaType<la::avdecc::controller::ControlledEntity::InterfaceLinkStatus>("la::avdecc::controller::ControlledEntity::InterfaceLinkStatus");
		qRegisterMetaType<la::avdecc::controller::ControlledEntity::CompatibilityFlags>("la::avdecc::controller::ControlledEntity::CompatibilityFlags");
		qRegisterMetaType<la::avdecc::controller::model::AcquireState>("la::avdecc::controller::model::AcquireState");
		qRegisterMetaType<la::avdecc::controller::model::LockState>("la::avdecc::controller::model::LockState");

		// Configure settings observers
		auto& settings = settings::SettingsManager::getInstance();
		settings.registerSettingObserver(settings::AemCacheEnabled.name, this);
	}

	~ControllerManagerImpl() noexcept
	{
		// The controller should already have been destroyed by now, but just in case, clean it we don't want further notifications
		if (!AVDECC_ASSERT_WITH_RET(!_controller, "Controller should have been destroyed before the singleton destructor is called"))
		{
			destroyController();
		}

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
	// Global controller notifications
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
		auto const entityID{ entity->getEntity().getEntityID() };
		_entityErrorCounterTrackers[entityID] = ErrorCounterTracker{ entityID };
		emit entityOnline(entityID);
	}
	virtual void onEntityOffline(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity) noexcept override
	{
		auto const entityID{ entity->getEntity().getEntityID() };
		_entityErrorCounterTrackers.erase(entityID);
		emit entityOffline(entityID);
	}
	virtual void onEntityCapabilitiesChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const /*entity*/) noexcept override
	{
#pragma message("TODO: Add new signal and listen to it")
	}
	virtual void onEntityAssociationChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const /*entity*/) noexcept override
	{
#pragma message("TODO: Add new signal and listen to it")
	}
	virtual void onGptpChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain) noexcept override
	{
		auto const& e = entity->getEntity();
		emit gptpChanged(e.getEntityID(), avbInterfaceIndex, grandMasterID, grandMasterDomain);
	}
	// Global entity notifications
	virtual void onUnsolicitedRegistrationChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, bool const /*isSubscribed*/) noexcept override
	{
#pragma message("TODO: Listen to the signal")
		emit unsolicitedRegistrationChanged(entity->getEntity().getEntityID());
	}
	virtual void onCompatibilityFlagsChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags) noexcept override
	{
		emit compatibilityFlagsChanged(entity->getEntity().getEntityID(), compatibilityFlags);
	}
	virtual void onIdentificationStarted(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity) noexcept override
	{
		emit identificationStarted(entity->getEntity().getEntityID());
	}
	virtual void onIdentificationStopped(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity) noexcept override
	{
		emit identificationStopped(entity->getEntity().getEntityID());
	}
	// Connection notifications (sniffed ACMP)
	virtual void onStreamConnectionChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::entity::model::StreamConnectionState const& state, bool const /*changedByOther*/) noexcept override
	{
		emit streamConnectionChanged(state);
	}
	virtual void onStreamConnectionsChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamConnections const& connections) noexcept override
	{
		emit streamConnectionsChanged({ entity->getEntity().getEntityID(), streamIndex }, connections);
	}
	// Entity model notifications (unsolicited AECP or changes this controller sent)
	virtual void onAcquireStateChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity) noexcept override
	{
		emit acquireStateChanged(entity->getEntity().getEntityID(), acquireState, owningEntity);
	}
	virtual void onLockStateChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity) noexcept override
	{
		emit lockStateChanged(entity->getEntity().getEntityID(), lockState, lockingEntity);
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
	virtual void onAudioUnitNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::AvdeccFixedString const& audioUnitName) noexcept override
	{
		emit audioUnitNameChanged(entity->getEntity().getEntityID(), configurationIndex, audioUnitIndex, QString::fromStdString(audioUnitName));
	}
	virtual void onStreamInputNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvdeccFixedString const& streamName) noexcept override
	{
		emit streamNameChanged(entity->getEntity().getEntityID(), configurationIndex, la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex, QString::fromStdString(streamName));
	}
	virtual void onStreamOutputNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvdeccFixedString const& streamName) noexcept override
	{
		emit streamNameChanged(entity->getEntity().getEntityID(), configurationIndex, la::avdecc::entity::model::DescriptorType::StreamOutput, streamIndex, QString::fromStdString(streamName));
	}
	virtual void onAvbInterfaceNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvdeccFixedString const& avbInterfaceName) noexcept override
	{
		emit avbInterfaceNameChanged(entity->getEntity().getEntityID(), configurationIndex, avbInterfaceIndex, QString::fromStdString(avbInterfaceName));
	}
	virtual void onClockSourceNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, la::avdecc::entity::model::AvdeccFixedString const& clockSourceName) noexcept override
	{
		emit clockSourceNameChanged(entity->getEntity().getEntityID(), configurationIndex, clockSourceIndex, QString::fromStdString(clockSourceName));
	}
	virtual void onMemoryObjectNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, la::avdecc::entity::model::AvdeccFixedString const& memoryObjectName) noexcept override
	{
		emit memoryObjectNameChanged(entity->getEntity().getEntityID(), configurationIndex, memoryObjectIndex, QString::fromStdString(memoryObjectName));
	}
	virtual void onAudioClusterNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, la::avdecc::entity::model::AvdeccFixedString const& audioClusterName) noexcept override
	{
		emit audioClusterNameChanged(entity->getEntity().getEntityID(), configurationIndex, audioClusterIndex, QString::fromStdString(audioClusterName));
	}
	virtual void onClockDomainNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::AvdeccFixedString const& clockDomainName) noexcept override
	{
		emit clockDomainNameChanged(entity->getEntity().getEntityID(), configurationIndex, clockDomainIndex, QString::fromStdString(clockDomainName));
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
	virtual void onAsPathChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AsPath const& asPath) noexcept override
	{
		emit asPathChanged(entity->getEntity().getEntityID(), avbInterfaceIndex, asPath);
	}
	virtual void onAvbInterfaceLinkStatusChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus) noexcept override
	{
		emit avbInterfaceLinkStatusChanged(entity->getEntity().getEntityID(), avbInterfaceIndex, linkStatus);
	}
	virtual void onEntityCountersChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::EntityCounters const& counters) noexcept override
	{
		emit entityCountersChanged(entity->getEntity().getEntityID(), counters);
	}
	virtual void onAvbInterfaceCountersChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceCounters const& counters) noexcept override
	{
		emit avbInterfaceCountersChanged(entity->getEntity().getEntityID(), avbInterfaceIndex, counters);
	}
	virtual void onClockDomainCountersChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockDomainCounters const& counters) noexcept override
	{
		emit clockDomainCountersChanged(entity->getEntity().getEntityID(), clockDomainIndex, counters);
	}
	virtual void onStreamInputCountersChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInputCounters const& counters) noexcept override
	{
		auto const entityID{ entity->getEntity().getEntityID() };

		if (auto* errorCounterFlags = entityErrorCounterTracker(entityID))
		{
			auto const prevFlags = errorCounterFlags->getStreamInputCounterValidFlags(streamIndex);

			for (auto const& counterKV : counters)
			{
				auto const& flag = counterKV.first;
				auto const& counter = counterKV.second;

				switch (flag)
				{
					//case la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked:
					case la::avdecc::entity::StreamInputCounterValidFlag::StreamInterrupted:
					case la::avdecc::entity::StreamInputCounterValidFlag::SeqNumMismatch:
					case la::avdecc::entity::StreamInputCounterValidFlag::LateTimestamp:
					case la::avdecc::entity::StreamInputCounterValidFlag::EarlyTimestamp:
					case la::avdecc::entity::StreamInputCounterValidFlag::UnsupportedFormat:
						errorCounterFlags->setStreamInputCounter(streamIndex, flag, counter);
						break;
					default:
						break;
				}
			}

			auto const newFlags = errorCounterFlags->getStreamInputCounterValidFlags(streamIndex);
			if (newFlags != prevFlags)
			{
				emit streamInputErrorCounterChanged(entityID, streamIndex, newFlags);
			}
		}

		emit streamInputCountersChanged(entityID, streamIndex, counters);
	}
	virtual void onStreamOutputCountersChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamOutputCounters const& counters) noexcept override
	{
		emit streamOutputCountersChanged(entity->getEntity().getEntityID(), streamIndex, counters);
	}
	virtual void onMemoryObjectLengthChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, std::uint64_t const length) noexcept override
	{
		emit memoryObjectLengthChanged(entity->getEntity().getEntityID(), configurationIndex, memoryObjectIndex, length);
	}
	virtual void onStreamPortInputAudioMappingsChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamPortIndex const streamPortIndex) noexcept override
	{
		emit streamPortAudioMappingsChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamPortInput, streamPortIndex);
	}
	virtual void onStreamPortOutputAudioMappingsChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamPortIndex const streamPortIndex) noexcept override
	{
		emit streamPortAudioMappingsChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamPortOutput, streamPortIndex);
	}
	virtual void onOperationProgress(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::OperationID const operationID, float const percentComplete) noexcept override
	{
		emit operationProgress(entity->getEntity().getEntityID(), descriptorType, descriptorIndex, operationID, percentComplete);
	}
	virtual void onOperationCompleted(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::OperationID const operationID, bool const failed) noexcept override
	{
		emit operationCompleted(entity->getEntity().getEntityID(), descriptorType, descriptorIndex, operationID, failed);
	}

	// ControllerManager overrides
	virtual void createController(la::avdecc::protocol::ProtocolInterface::Type const protocolInterfaceType, QString const& interfaceName, std::uint16_t const progID, la::avdecc::UniqueIdentifier const entityModelID, QString const& preferedLocale) override
	{
		// If we have a previous controller, remove it
		if (_controller)
		{
			destroyController();
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

	virtual void destroyController() noexcept override
	{
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
			return controller->getControlledEntityGuard(entityID);
		}
		return {};
	}

	virtual std::tuple<la::avdecc::jsonSerializer::SerializationError, std::string> serializeAllControlledEntitiesAsReadableJson(QString const& filePath) const noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			return controller->serializeAllControlledEntitiesAsReadableJson(filePath.toStdString(), true);
		}
		return { la::avdecc::jsonSerializer::SerializationError::InternalError, "Controller offline" };
	}

	virtual std::tuple<la::avdecc::jsonSerializer::SerializationError, std::string> serializeControlledEntityAsReadableJson(la::avdecc::UniqueIdentifier const entityID, QString const& filePath) const noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			return controller->serializeControlledEntityAsReadableJson(entityID, filePath.toStdString());
		}
		return { la::avdecc::jsonSerializer::SerializationError::InternalError, "Controller offline" };
	}

	ErrorCounterTracker const* entityErrorCounterTracker(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto const it = _entityErrorCounterTrackers.find(entityID);
		if (it != std::end(_entityErrorCounterTrackers))
		{
			return &it->second;
		}
		return nullptr;
	}

	ErrorCounterTracker* entityErrorCounterTracker(la::avdecc::UniqueIdentifier const entityID) noexcept
	{
		return const_cast<ErrorCounterTracker*>(static_cast<ControllerManagerImpl const*>(this)->entityErrorCounterTracker(entityID));
	}

	virtual la::avdecc::entity::StreamInputCounterValidFlags getStreamInputErrorCounterFlags(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept
	{
		if (auto* errorCounterTracker = entityErrorCounterTracker(entityID))
		{
			return errorCounterTracker->getStreamInputCounterValidFlags(streamIndex);
		}
		return {};
	}

	virtual void clearStreamInputCounterValidFlags(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamInputCounterValidFlag const flag) noexcept override
	{
		if (auto* errorCounterTracker = entityErrorCounterTracker(entityID))
		{
			if (errorCounterTracker->clearStreamInputCounter(streamIndex, flag))
			{
				auto const flags = errorCounterTracker->getStreamInputCounterValidFlags(streamIndex);
				emit streamInputErrorCounterChanged(entityID, streamIndex, flags);
			}
		}
	}

	virtual void clearAllStreamInputCounterValidFlags(la::avdecc::UniqueIdentifier const entityID) noexcept override
	{
		if (auto* errorCounterTracker = entityErrorCounterTracker(entityID))
		{
			errorCounterTracker->clearAllStreamInputCounters();
		}
	}

	/* Enumeration and Control Protocol (AECP) */
	virtual void acquireEntity(la::avdecc::UniqueIdentifier const targetEntityID, bool const isPersistent, AcquireEntityHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::AcquireEntity);
			controller->acquireEntity(targetEntityID, isPersistent,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const owningEntity) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status, owningEntity);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::AcquireEntity, status);
					}
				});
		}
	}

	virtual void releaseEntity(la::avdecc::UniqueIdentifier const targetEntityID, ReleaseEntityHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::ReleaseEntity);
			controller->releaseEntity(targetEntityID,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const owningEntity) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::ReleaseEntity, status);
					}
				});
		}
	}

	virtual void lockEntity(la::avdecc::UniqueIdentifier const targetEntityID, LockEntityHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::LockEntity);
			controller->lockEntity(targetEntityID,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const lockingEntity) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status, lockingEntity);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::LockEntity, status);
					}
				});
		}
	}

	virtual void unlockEntity(la::avdecc::UniqueIdentifier const targetEntityID, UnlockEntityHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::UnlockEntity);
			controller->unlockEntity(targetEntityID,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const lockingEntity) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::UnlockEntity, status);
					}
				});
		}
	}

	virtual void setConfiguration(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetConfiguration);
			controller->setConfiguration(targetEntityID, configurationIndex,
				[this, targetEntityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					emit endAecpCommand(targetEntityID, AecpCommandType::SetConfiguration, status);
				});
		}
	}

	virtual void setStreamInputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat, SetStreamInputFormatHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetStreamFormat);
			controller->setStreamInputFormat(targetEntityID, streamIndex, streamFormat,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetStreamFormat, status);
					}
				});
		}
	}

	virtual void setStreamOutputFormat(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat, SetStreamOutputFormatHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetStreamFormat);
			controller->setStreamOutputFormat(targetEntityID, streamIndex, streamFormat,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetStreamFormat, status);
					}
				});
		}
	}

	virtual void setEntityName(la::avdecc::UniqueIdentifier const targetEntityID, QString const& name, SetEntityNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetEntityName);
			controller->setEntityName(targetEntityID, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetEntityName, status);
					}
				});
		}
	}

	virtual void setEntityGroupName(la::avdecc::UniqueIdentifier const targetEntityID, QString const& name, SetEntityGroupNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetEntityGroupName);
			controller->setEntityGroupName(targetEntityID, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetEntityGroupName, status);
					}
				});
		}
	}

	virtual void setConfigurationName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& name, SetConfigurationNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetConfigurationName);
			controller->setConfigurationName(targetEntityID, configurationIndex, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetConfigurationName, status);
					}
				});
		}
	}

	virtual void setAudioUnitName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, QString const& name, SetAudioUnitNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetAudioUnitName);
			controller->setAudioUnitName(targetEntityID, configurationIndex, audioUnitIndex, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetAudioUnitName, status);
					}
				});
		}
	}

	virtual void setStreamInputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name, SetStreamInputNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetStreamName);
			controller->setStreamInputName(targetEntityID, configurationIndex, streamIndex, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetStreamName, status);
					}
				});
		}
	}

	virtual void setStreamOutputName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name, SetStreamOutputNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetStreamName);
			controller->setStreamOutputName(targetEntityID, configurationIndex, streamIndex, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetStreamName, status);
					}
				});
		}
	}

	virtual void setAvbInterfaceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, QString const& name, SetAvbInterfaceNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetAvbInterfaceName);
			controller->setAvbInterfaceName(targetEntityID, configurationIndex, avbInterfaceIndex, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetAvbInterfaceName, status);
					}
				});
		}
	}

	virtual void setClockSourceName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, QString const& name, SetClockSourceNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetClockSourceName);
			controller->setClockSourceName(targetEntityID, configurationIndex, clockSourceIndex, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetClockSourceName, status);
					}
				});
		}
	}

	virtual void setMemoryObjectName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, QString const& name, SetMemoryObjectNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetMemoryObjectName);
			controller->setMemoryObjectName(targetEntityID, configurationIndex, memoryObjectIndex, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetMemoryObjectName, status);
					}
				});
		}
	}

	virtual void setAudioClusterName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& name, SetAudioClusterNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetAudioClusterName);
			controller->setAudioClusterName(targetEntityID, configurationIndex, audioClusterIndex, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetAudioClusterName, status);
					}
				});
		}
	}

	virtual void setClockDomainName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, QString const& name, SetClockDomainNameHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetClockDomainName);
			controller->setClockDomainName(targetEntityID, configurationIndex, clockDomainIndex, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetClockDomainName, status);
					}
				});
		}
	}

	virtual void setAudioUnitSamplingRate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate, SetAudioUnitSamplingRateHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetSamplingRate);
			controller->setAudioUnitSamplingRate(targetEntityID, audioUnitIndex, samplingRate,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetSamplingRate, status);
					}
				});
		}
	}

	virtual void setClockSource(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, SetClockSourceHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetClockSource);
			controller->setClockSource(targetEntityID, clockDomainIndex, clockSourceIndex,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetClockSource, status);
					}
				});
		}
	}

	virtual void startStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StartStreamInputHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::StartStream);
			controller->startStreamInput(targetEntityID, streamIndex,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::StartStream, status);
					}
				});
		}
	}

	virtual void stopStreamInput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StopStreamInputHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::StopStream);
			controller->stopStreamInput(targetEntityID, streamIndex,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::StopStream, status);
					}
				});
		}
	}

	virtual void startStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StartStreamOutputHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::StartStream);
			controller->startStreamOutput(targetEntityID, streamIndex,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::StartStream, status);
					}
				});
		}
	}

	virtual void stopStreamOutput(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, StopStreamOutputHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::StopStream);
			controller->stopStreamOutput(targetEntityID, streamIndex,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::StopStream, status);
					}
				});
		}
	}

	virtual void addStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, AddStreamPortInputAudioMappingsHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::AddStreamPortAudioMappings);
			controller->addStreamPortInputAudioMappings(targetEntityID, streamPortIndex, mappings,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::AddStreamPortAudioMappings, status);
					}
				});
		}
	}

	virtual void addStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, AddStreamPortOutputAudioMappingsHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::AddStreamPortAudioMappings);
			controller->addStreamPortOutputAudioMappings(targetEntityID, streamPortIndex, mappings,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::AddStreamPortAudioMappings, status);
					}
				});
		}
	}

	virtual void removeStreamPortInputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, RemoveStreamPortInputAudioMappingsHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::RemoveStreamPortAudioMappings);
			controller->removeStreamPortInputAudioMappings(targetEntityID, streamPortIndex, mappings,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::RemoveStreamPortAudioMappings, status);
					}
				});
		}
	}

	virtual void removeStreamPortOutputAudioMappings(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::AudioMappings const& mappings, RemoveStreamPortOutputAudioMappingsHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::RemoveStreamPortAudioMappings);
			controller->removeStreamPortOutputAudioMappings(targetEntityID, streamPortIndex, mappings,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::RemoveStreamPortAudioMappings, status);
					}
				});
		}
	}

	virtual void startStoreAndRebootMemoryObjectOperation(la::avdecc::UniqueIdentifier targetEntityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, StartStoreAndRebootMemoryObjectOperationHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			if (!handler)
			{
				emit beginAecpCommand(targetEntityID, AecpCommandType::StartStoreAndRebootMemoryObjectOperation);
			}
			controller->startStoreAndRebootMemoryObjectOperation(targetEntityID, descriptorIndex,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::entity::model::OperationID const operationID) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status, operationID);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::StartStoreAndRebootMemoryObjectOperation, status);
					}
				});
		}
	}

	virtual void startUploadMemoryObjectOperation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, std::uint64_t const dataLength, StartUploadMemoryObjectOperationHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			if (!handler)
			{
				emit beginAecpCommand(targetEntityID, AecpCommandType::StartUploadMemoryObjectOperation);
			}
			controller->startUploadMemoryObjectOperation(targetEntityID, descriptorIndex, dataLength,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::entity::model::OperationID const operationID) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status, operationID);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::StartUploadMemoryObjectOperation, status);
					}
				});
		}
	}

	virtual void abortOperation(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::OperationID const operationID, AbortOperationHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			if (!handler)
			{
				emit beginAecpCommand(targetEntityID, AecpCommandType::AbortOperation);
			}
			controller->abortOperation(targetEntityID, descriptorType, descriptorIndex, operationID,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::AbortOperation, status);
					}
				});
		}
	}

	/* Enumeration and Control Protocol (AECP) AA */
	virtual void readDeviceMemory(la::avdecc::UniqueIdentifier const targetEntityID, std::uint64_t const address, std::uint64_t const length, la::avdecc::controller::Controller::ReadDeviceMemoryProgressHandler const& progressHandler, la::avdecc::controller::Controller::ReadDeviceMemoryCompletionHandler const& completionHandler) const noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			controller->readDeviceMemory(targetEntityID, address, length, progressHandler, completionHandler);
		}
	}

	virtual void writeDeviceMemory(la::avdecc::UniqueIdentifier const targetEntityID, std::uint64_t const address, la::avdecc::controller::Controller::DeviceMemoryBuffer memoryBuffer, la::avdecc::controller::Controller::WriteDeviceMemoryProgressHandler const& progressHandler, la::avdecc::controller::Controller::WriteDeviceMemoryCompletionHandler const& completionHandler) const noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			controller->writeDeviceMemory(targetEntityID, address, memoryBuffer, progressHandler, completionHandler);
		}
	}

	/* Connection Management Protocol (ACMP) */
	virtual void connectStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, ConnectStreamHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::ConnectStream);
			controller->connectStream({ talkerEntityID, talkerStreamIndex }, { listenerEntityID, listenerStreamIndex },
				[this, talkerEntityID, listenerEntityID, handler](la::avdecc::controller::ControlledEntity const* const talkerEntity, la::avdecc::controller::ControlledEntity const* const listenerEntity, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, status);
					}
					else
					{
						emit endAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::ConnectStream, status);
					}
				});
		}
	}

	virtual void disconnectStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, DisconnectStreamHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::DisconnectStream);
			controller->disconnectStream({ talkerEntityID, talkerStreamIndex }, { listenerEntityID, listenerStreamIndex },
				[this, talkerEntityID, talkerStreamIndex, listenerEntityID, handler](la::avdecc::controller::ControlledEntity const* const listenerEntity, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, status);
					}
					else
					{
						emit endAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::DisconnectStream, status);
					}
				});
		}
	}

	virtual void disconnectTalkerStream(la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, DisconnectTalkerStreamHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::DisconnectTalkerStream);
			controller->disconnectTalkerStream({ talkerEntityID, talkerStreamIndex }, { listenerEntityID, listenerStreamIndex },
				[this, talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, handler](la::avdecc::entity::ControllerEntity::ControlStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, status);
					}
					else
					{
						emit endAcmpCommand(talkerEntityID, talkerStreamIndex, listenerEntityID, listenerStreamIndex, AcmpCommandType::DisconnectTalkerStream, status);
					}
				});
		}
	}

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

	// Store per entity error counter flags
	std::unordered_map<la::avdecc::UniqueIdentifier, ErrorCounterTracker, la::avdecc::UniqueIdentifier::hash> _entityErrorCounterTrackers;
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
		case AecpCommandType::LockEntity:
			return "Lock Entity";
		case AecpCommandType::UnlockEntity:
			return "Unlock Entity";
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
		case AecpCommandType::SetAudioUnitName:
			return "Set Audio Unit Name";
		case AecpCommandType::SetStreamName:
			return "Set Stream Name";
		case AecpCommandType::SetAvbInterfaceName:
			return "Set AVB Interface Name";
		case AecpCommandType::SetClockSourceName:
			return "Set Clock Source Name";
		case AecpCommandType::SetMemoryObjectName:
			return "Set Memory Object Name";
		case AecpCommandType::SetAudioClusterName:
			return "Set Audio Cluster Name";
		case AecpCommandType::SetClockDomainName:
			return "Set Clock Domain Name";
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
		case AecpCommandType::StartStoreAndRebootMemoryObjectOperation:
			return "Store and Reboot Operation";
		case AecpCommandType::StartUploadMemoryObjectOperation:
			return "Upload Operation";
		case AecpCommandType::AbortOperation:
			return "Abort Operation";
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
