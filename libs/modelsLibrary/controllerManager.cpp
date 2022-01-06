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

#include "commandsExecutorImpl.hpp"
#include "hive/modelsLibrary/controllerManager.hpp"

#include <la/avdecc/logger.hpp>

#include <atomic>
#include <thread>

#if __cpp_lib_experimental_atomic_smart_pointers
#	define HAVE_ATOMIC_SMART_POINTERS
#endif // __cpp_lib_experimental_atomic_smart_pointers

namespace hive
{
namespace modelsLibrary
{
class ControllerManagerImpl final : public ControllerManager, private la::avdecc::controller::Controller::Observer
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
			virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::EntityNode const& /*node*/) noexcept override
			{
				// Initialize internal counter value, always setting lastClearCount to 0 (Statistics counters always start at 0 in the Controller, contrary to endpoint Counters) so that we directly see any error during enumeration
				{
					auto const counter = entity->getAecpRetryCounter();
					_errorCounterTracker._statisticsCounters[StatisticsErrorCounterFlag::AecpRetries] = StatisticsCounterInfo{ counter, 0u };
				}
				{
					auto const counter = entity->getAecpTimeoutCounter();
					_errorCounterTracker._statisticsCounters[StatisticsErrorCounterFlag::AecpTimeouts] = StatisticsCounterInfo{ counter, 0u };
				}
				{
					auto const counter = entity->getAecpUnexpectedResponseCounter();
					_errorCounterTracker._statisticsCounters[StatisticsErrorCounterFlag::AecpUnexpectedResponses] = StatisticsCounterInfo{ counter, 0u };
				}
			}
			virtual void visit(la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
			{
				if (node.dynamicModel->counters)
				{
					for (auto const [flag, counter] : *node.dynamicModel->counters)
					{
						// Initialize internal counter value
						_errorCounterTracker._streamInputCounters[node.descriptorIndex][flag] = ErrorCounterInfo{ counter, counter };
					}
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
			virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::EntityNode const& /*node*/) noexcept override
			{
				{
					auto& counterInfo = _errorCounterTracker._statisticsCounters[StatisticsErrorCounterFlag::AecpRetries];
					counterInfo.lastClearCount = counterInfo.currentCount;
				}
				{
					auto& counterInfo = _errorCounterTracker._statisticsCounters[StatisticsErrorCounterFlag::AecpTimeouts];
					counterInfo.lastClearCount = counterInfo.currentCount;
				}
				{
					auto& counterInfo = _errorCounterTracker._statisticsCounters[StatisticsErrorCounterFlag::AecpUnexpectedResponses];
					counterInfo.lastClearCount = counterInfo.currentCount;
				}
				emit _manager.statisticsErrorCounterChanged(entity->getEntity().getEntityID(), {});
			}
			virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
			{
				for (auto& counterInfoKV : _errorCounterTracker._streamInputCounters[node.descriptorIndex])
				{
					auto& counterInfo = counterInfoKV.second;
					counterInfo.lastClearCount = counterInfo.currentCount;
				}
				emit _manager.streamInputErrorCounterChanged(entity->getEntity().getEntityID(), node.descriptorIndex, {});
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
				entity->accept(&visitor, false);
			}
		}

		/* ************************************************************ */
		/* StreamInput Error Counters                                   */
		/* ************************************************************ */
		StreamInputErrorCounters getStreamInputErrorCounters(la::avdecc::entity::model::StreamIndex const streamIndex) const
		{
			auto counters = StreamInputErrorCounters{};

			auto const streamIt = _streamInputCounters.find(streamIndex);
			if (streamIt != std::end(_streamInputCounters))
			{
				for (auto const& [flag, errorCounter] : streamIt->second)
				{
					if (errorCounter.currentCount != errorCounter.lastClearCount)
					{
						counters[flag] = errorCounter.currentCount - errorCounter.lastClearCount;
					}
				}
			}

			return counters;
		}

		// Set the new counter value, returns true if the counter has changed, false otherwise
		bool setStreamInputCounter(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamInputCounterValidFlag const flag, la::avdecc::entity::model::DescriptorCounter const counter)
		{
			// Get or create ErrorCounterInfo
			auto& errorCounter = _streamInputCounters[streamIndex][flag];

			auto shouldNotify = false;

			// Detect counter reset (or wrap)
			if (counter < errorCounter.currentCount)
			{
				errorCounter.lastClearCount = 0u; // Reset counter error (we accept to loose the error state if it was in error, in the case of wrapping)
				shouldNotify = true;
			}

			// Detect counter increment
			if (counter > errorCounter.currentCount)
			{
				shouldNotify = true;
			}

			// Always update counter value
			errorCounter.currentCount = counter;

			return shouldNotify;
		}

		// Clear the error for a given flag, returns true if the flag has changed, false otherwise
		bool clearStreamInputCounter(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamInputCounterValidFlag const flag)
		{
			AVDECC_ASSERT(_streamInputCounters[streamIndex].count(flag) != 0, "Should not be possible to clear an error flag that does not exist");
			auto& errorCounter = _streamInputCounters[streamIndex][flag];

			if (errorCounter.lastClearCount != errorCounter.currentCount)
			{
				errorCounter.lastClearCount = errorCounter.currentCount;
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
				entity->accept(&visitor, false);
			}
		}

		/* ************************************************************ */
		/* Statistics Error Counters                                    */
		/* ************************************************************ */
		StatisticsErrorCounters getStatisticsErrorCounters() const
		{
			auto counters = StatisticsErrorCounters{};

			for (auto const& [flag, errorCounter] : _statisticsCounters)
			{
				if (errorCounter.currentCount != errorCounter.lastClearCount)
				{
					counters[flag] = errorCounter.currentCount - errorCounter.lastClearCount;
				}
			}

			return counters;
		}

		// Set the new counter value, returns true if the counter has changed, false otherwise
		bool setStatisticsCounter(StatisticsErrorCounterFlag const flag, std::uint64_t const counter)
		{
			// Get or create StatisticsCounterInfo
			auto& errorCounter = _statisticsCounters[flag];

			auto shouldNotify = false;

			// Detect counter reset (or wrap)
			if (counter < errorCounter.currentCount)
			{
				errorCounter.lastClearCount = 0u; // Reset counter error (we accept to loose the error state if it was in error, in the case of wrapping)
				shouldNotify = true;
			}

			// Detect counter increment
			if (counter > errorCounter.currentCount)
			{
				shouldNotify = true;
			}

			// Always update counter value
			errorCounter.currentCount = counter;

			return shouldNotify;
		}

		// Clear the error for a given flag, returns true if the flag has changed, false otherwise
		bool clearStatisticsCounter(StatisticsErrorCounterFlag const flag)
		{
			AVDECC_ASSERT(_statisticsCounters.count(flag) != 0, "Should not be possible to clear an error flag that does not exist");
			auto& errorCounter = _statisticsCounters[flag];

			if (errorCounter.lastClearCount != errorCounter.currentCount)
			{
				errorCounter.lastClearCount = errorCounter.currentCount;
				return true;
			}

			return false;
		}

		// Clear all the error flags
		void clearAllStatisticsCounters()
		{
			auto& manager = ControllerManager::getInstance();
			ClearCounterVisitor visitor{ manager, *this };
			if (auto entity = manager.getControlledEntity(_entityID))
			{
				entity->accept(&visitor, false);
			}
		}

	private:
		struct ErrorCounterInfo
		{
			la::avdecc::entity::model::DescriptorCounter currentCount{ 0u }; // Current Counter Value
			la::avdecc::entity::model::DescriptorCounter lastClearCount{ 0u }; // Value when last Cleared
		};
		struct StatisticsCounterInfo
		{
			std::uint64_t currentCount{ 0u }; // Current Counter Value
			std::uint64_t lastClearCount{ 0u }; // Value when last Cleared
		};

		la::avdecc::UniqueIdentifier _entityID{ la::avdecc::UniqueIdentifier::getNullUniqueIdentifier() };
		std::unordered_map<la::avdecc::entity::model::StreamIndex, std::unordered_map<la::avdecc::entity::StreamInputCounterValidFlag, ErrorCounterInfo>> _streamInputCounters{};
		std::unordered_map<StatisticsErrorCounterFlag, StatisticsCounterInfo> _statisticsCounters{};
	};

	ControllerManagerImpl() noexcept
	{
		qRegisterMetaType<std::uint8_t>("std::uint8_t");
		qRegisterMetaType<std::uint16_t>("std::uint16_t");
		qRegisterMetaType<std::uint64_t>("std::uint64_t");
		qRegisterMetaType<std::chrono::milliseconds>("std::chrono::milliseconds");
		qRegisterMetaType<AecpCommandType>("hive::modelsLibrary::ControllerManager::AecpCommandType");
		qRegisterMetaType<AcmpCommandType>("hive::modelsLibrary::ControllerManager::AcmpCommandType");
		qRegisterMetaType<StreamInputErrorCounters>("hive::modelsLibrary::ControllerManager::StreamInputErrorCounters");
		qRegisterMetaType<StatisticsErrorCounters>("hive::modelsLibrary::ControllerManager::StatisticsErrorCounters");
		qRegisterMetaType<la::avdecc::UniqueIdentifier>("la::avdecc::UniqueIdentifier");
		qRegisterMetaType<std::optional<la::avdecc::UniqueIdentifier>>("std::optional<la::avdecc::UniqueIdentifier>");
		qRegisterMetaType<la::avdecc::entity::ControllerEntity::AemCommandStatus>("la::avdecc::entity::ControllerEntity::AemCommandStatus");
		qRegisterMetaType<la::avdecc::entity::ControllerEntity::ControlStatus>("la::avdecc::entity::ControllerEntity::ControlStatus");
		qRegisterMetaType<la::avdecc::entity::StreamInputCounterValidFlags>("la::avdecc::entity::StreamInputCounterValidFlags");
		qRegisterMetaType<la::avdecc::entity::Entity::InterfaceInformation>("la::avdecc::entity::Entity::InterfaceInformation");
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
		qRegisterMetaType<la::avdecc::entity::model::StreamDynamicInfo>("la::avdecc::entity::model::StreamDynamicInfo");
		qRegisterMetaType<la::avdecc::entity::model::AvbInterfaceInfo>("la::avdecc::entity::model::AvbInterfaceInfo");
		qRegisterMetaType<la::avdecc::entity::model::AsPath>("la::avdecc::entity::model::AsPath");
		qRegisterMetaType<la::avdecc::entity::model::ControlValues>("la::avdecc::entity::model::ControlValues");
		qRegisterMetaType<la::avdecc::entity::model::StreamIdentification>("la::avdecc::entity::model::StreamIdentification");
		qRegisterMetaType<la::avdecc::entity::model::StreamInputConnectionInfo>("la::avdecc::entity::model::StreamInputConnectionInfo");
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
	}

	~ControllerManagerImpl() noexcept
	{
		// Invalidate all executors
		{
			auto const lg = std::lock_guard{ _lock };
			for (auto& [ex, executor] : _commandsExecutors)
			{
				executor->invalidate();
			}
		}

		// The controller should already have been destroyed by now, but just in case, clean it we don't want further notifications
		if (!AVDECC_ASSERT_WITH_RET(!_controller, "Controller should have been destroyed before the singleton destructor is called"))
		{
			destroyController();
		}
	}

private:
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
		// Invoke all the code manipulating class members to the main thread, as onEntityOnline and onEntityOffline can happen at the same time from different threads (as of current avdecc_controller library)
		// We don't want a class member to be reset by onEntityOffline while the entity is going Online again at the same time, so invoke in a queued manner in the same (main) thread

		// Create the CounterTracker in this thread as it will try to lock the ControlledEntity
		auto const entityID = entity->getEntity().getEntityID();
		auto tracker = ErrorCounterTracker{ entityID };

		QMetaObject::invokeMethod(this,
			[this, entityID, tracker = std::move(tracker), enumerationTime = entity->getEnumerationTime()]()
			{
				{
					auto const lg = std::lock_guard{ _lock };
					_entities.insert(entityID);
					_entityErrorCounterTrackers[entityID] = std::move(tracker);
				}

				emit entityOnline(entityID, enumerationTime);
			});
	}
	virtual void onEntityOffline(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity) noexcept override
	{
		// Invoke all the code manipulating class members to the main thread, as onEntityOnline and onEntityOffline can happen at the same time from different threads (as of current avdecc_controller library)
		// We don't want a class member to be reset by onEntityOffline while the entity is going Online again at the same time, so invoke in a queued manner in the same (main) thread
		QMetaObject::invokeMethod(this,
			[this, entityID = entity->getEntity().getEntityID()]()
			{
				{
					auto const lg = std::lock_guard{ _lock };
					_entities.erase(entityID);
					_entityErrorCounterTrackers.erase(entityID);
				}

				emit entityOffline(entityID);
			});
	}
	virtual void onEntityRedundantInterfaceOnline(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::Entity::InterfaceInformation const& interfaceInfo) noexcept override
	{
		auto const& e = entity->getEntity();
		emit entityRedundantInterfaceOnline(e.getEntityID(), avbInterfaceIndex, interfaceInfo);
	}
	virtual void onEntityRedundantInterfaceOffline(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex) noexcept override
	{
		auto const& e = entity->getEntity();
		emit entityRedundantInterfaceOffline(e.getEntityID(), avbInterfaceIndex);
	}
	virtual void onEntityCapabilitiesChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity) noexcept override
	{
		auto const& e = entity->getEntity();
		emit entityCapabilitiesChanged(e.getEntityID(), e.getEntityCapabilities());
	}
	virtual void onEntityAssociationIDChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity) noexcept override
	{
		auto const& e = entity->getEntity();
		auto const associationID = e.getAssociationID();
		emit associationIDChanged(e.getEntityID(), associationID);
	}
	virtual void onGptpChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain) noexcept override
	{
		auto const& e = entity->getEntity();
		emit gptpChanged(e.getEntityID(), avbInterfaceIndex, grandMasterID, grandMasterDomain);
	}
	// Global entity notifications
	virtual void onUnsolicitedRegistrationChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, bool const isSubscribed) noexcept override
	{
		emit unsolicitedRegistrationChanged(entity->getEntity().getEntityID(), isSubscribed);
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
	virtual void onStreamInputConnectionChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInputConnectionInfo const& info, bool const /*changedByOther*/) noexcept override
	{
		emit streamInputConnectionChanged({ entity->getEntity().getEntityID(), streamIndex }, info);
	}
	virtual void onStreamOutputConnectionsChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamConnections const& connections) noexcept override
	{
		emit streamOutputConnectionsChanged({ entity->getEntity().getEntityID(), streamIndex }, connections);
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
	virtual void onStreamInputDynamicInfoChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamDynamicInfo const& info) noexcept override
	{
		emit streamDynamicInfoChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex, info);
	}
	virtual void onStreamOutputDynamicInfoChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamDynamicInfo const& info) noexcept override
	{
		emit streamDynamicInfoChanged(entity->getEntity().getEntityID(), la::avdecc::entity::model::DescriptorType::StreamOutput, streamIndex, info);
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
	virtual void onControlNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::AvdeccFixedString const& controlName) noexcept override
	{
		emit controlNameChanged(entity->getEntity().getEntityID(), configurationIndex, controlIndex, QString::fromStdString(controlName));
	}
	virtual void onClockDomainNameChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::AvdeccFixedString const& clockDomainName) noexcept override
	{
		emit clockDomainNameChanged(entity->getEntity().getEntityID(), configurationIndex, clockDomainIndex, QString::fromStdString(clockDomainName));
	}
	virtual void onAssociationIDChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, std::optional<la::avdecc::UniqueIdentifier> const associationID) noexcept override
	{
		emit associationIDChanged(entity->getEntity().getEntityID(), associationID);
	}
	virtual void onAudioUnitSamplingRateChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, la::avdecc::entity::model::SamplingRate const samplingRate) noexcept override
	{
		emit audioUnitSamplingRateChanged(entity->getEntity().getEntityID(), audioUnitIndex, samplingRate);
	}
	virtual void onClockSourceChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex) noexcept override
	{
		emit clockSourceChanged(entity->getEntity().getEntityID(), clockDomainIndex, clockSourceIndex);
	}
	virtual void onControlValuesChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlValues const& controlValues) noexcept override
	{
		emit controlValuesChanged(entity->getEntity().getEntityID(), controlIndex, controlValues);
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
	virtual void onAvbInterfaceInfoChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::AvbInterfaceInfo const& info) noexcept override
	{
		emit avbInterfaceInfoChanged(entity->getEntity().getEntityID(), avbInterfaceIndex, info);
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
		// Invoke all the code manipulating class members to the main thread, as onEntityOnline and onEntityOffline can happen at the same time from different threads (as of current avdecc_controller library)
		// We don't want a class member to be reset by onEntityOffline while the entity is going Online again at the same time, so invoke in a queued manner in the same (main) thread

		auto const entityID = entity->getEntity().getEntityID();

		// As we don't want to manipulate stored counter errors from this thread, we have to check for which counter flag we'll want to detect a change
		auto checkForChange = la::avdecc::entity::model::StreamInputCounters{};
		for (auto const [flag, counter] : counters)
		{
			switch (flag)
			{
				case la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked:
				{
					try
					{
						auto const& entityNode = entity->getEntityNode();
						auto const& streamNode = entity->getStreamInputNode(entityNode.dynamicModel->currentConfiguration, streamIndex);
						if (streamNode.dynamicModel->connectionInfo.state != la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected)
						{
							// Only consider MediaUnlocked as an error if the stream is connected, otherwise juste ignore this
							break;
						}
					}
					catch (la::avdecc::controller::ControlledEntity::Exception const&)
					{
						// Ignore exception
						break;
					}
					catch (...)
					{
						// Uncaught exception
						AVDECC_ASSERT(false, "Uncaught exception");
						break;
					}
					[[fallthrough]];
				}
				case la::avdecc::entity::StreamInputCounterValidFlag::StreamInterrupted:
				case la::avdecc::entity::StreamInputCounterValidFlag::SeqNumMismatch:
				case la::avdecc::entity::StreamInputCounterValidFlag::LateTimestamp:
				case la::avdecc::entity::StreamInputCounterValidFlag::EarlyTimestamp:
				case la::avdecc::entity::StreamInputCounterValidFlag::UnsupportedFormat:
					checkForChange[flag] = counter;
					break;
				default:
					break;
			}
		}

		QMetaObject::invokeMethod(this,
			[this, entityID = entity->getEntity().getEntityID(), streamIndex, counters, checkForChange = std::move(checkForChange)]()
			{
				if (auto* errorCounterFlags = entityErrorCounterTracker(entityID))
				{
					auto changed = false;
					for (auto const [flag, counter] : checkForChange)
					{
						changed |= errorCounterFlags->setStreamInputCounter(streamIndex, flag, counter);
					}
					if (changed)
					{
						emit streamInputErrorCounterChanged(entityID, streamIndex, errorCounterFlags->getStreamInputErrorCounters(streamIndex));
					}
				}

				emit streamInputCountersChanged(entityID, streamIndex, counters);
			});
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
	// Statistics
	virtual void onAecpRetryCounterChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, std::uint64_t const value) noexcept override
	{
		// Invoke all the code manipulating class members to the main thread, as onEntityOnline and onEntityOffline can happen at the same time from different threads (as of current avdecc_controller library)
		// We don't want a class member to be reset by onEntityOffline while the entity is going Online again at the same time, so invoke in a queued manner in the same (main) thread
		QMetaObject::invokeMethod(this,
			[this, entityID = entity->getEntity().getEntityID(), value]()
			{
				if (auto* errorCounterFlags = entityErrorCounterTracker(entityID))
				{
					if (errorCounterFlags->setStatisticsCounter(StatisticsErrorCounterFlag::AecpRetries, value))
					{
						emit statisticsErrorCounterChanged(entityID, errorCounterFlags->getStatisticsErrorCounters());
					}
				}

				emit aecpRetryCounterChanged(entityID, value);
			});
	}
	virtual void onAecpTimeoutCounterChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, std::uint64_t const value) noexcept override
	{
		// Invoke all the code manipulating class members to the main thread, as onEntityOnline and onEntityOffline can happen at the same time from different threads (as of current avdecc_controller library)
		// We don't want a class member to be reset by onEntityOffline while the entity is going Online again at the same time, so invoke in a queued manner in the same (main) thread
		QMetaObject::invokeMethod(this,
			[this, entityID = entity->getEntity().getEntityID(), value]()
			{
				if (auto* errorCounterFlags = entityErrorCounterTracker(entityID))
				{
					if (errorCounterFlags->setStatisticsCounter(StatisticsErrorCounterFlag::AecpTimeouts, value))
					{
						emit statisticsErrorCounterChanged(entityID, errorCounterFlags->getStatisticsErrorCounters());
					}
				}

				emit aecpTimeoutCounterChanged(entityID, value);
			});
	}
	virtual void onAecpUnexpectedResponseCounterChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, std::uint64_t const value) noexcept override
	{
		// Invoke all the code manipulating class members to the main thread, as onEntityOnline and onEntityOffline can happen at the same time from different threads (as of current avdecc_controller library)
		// We don't want a class member to be reset by onEntityOffline while the entity is going Online again at the same time, so invoke in a queued manner in the same (main) thread
		QMetaObject::invokeMethod(this,
			[this, entityID = entity->getEntity().getEntityID(), value]()
			{
				if (auto* errorCounterFlags = entityErrorCounterTracker(entityID))
				{
					if (errorCounterFlags->setStatisticsCounter(StatisticsErrorCounterFlag::AecpUnexpectedResponses, value))
					{
						emit statisticsErrorCounterChanged(entityID, errorCounterFlags->getStatisticsErrorCounters());
					}
				}

				emit aecpUnexpectedResponseCounterChanged(entityID, value);
			});
	}
	virtual void onAecpResponseAverageTimeChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, std::chrono::milliseconds const& value) noexcept override
	{
		emit aecpResponseAverageTimeChanged(entity->getEntity().getEntityID(), value);
	}
	virtual void onAemAecpUnsolicitedCounterChanged(la::avdecc::controller::Controller const* const /*controller*/, la::avdecc::controller::ControlledEntity const* const entity, std::uint64_t const value) noexcept override
	{
		emit aemAecpUnsolicitedCounterChanged(entity->getEntity().getEntityID(), value);
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

			ctrl->setAutomaticDiscoveryDelay(_discoveryDelay);

			if (_enableAemCache)
			{
				ctrl->enableEntityModelCache();
			}
			else
			{
				ctrl->disableEntityModelCache();
			}

			if (_fullAemEnumeration)
			{
				ctrl->enableFullStaticEntityModelEnumeration();
			}
			else
			{
				ctrl->disableFullStaticEntityModelEnumeration();
			}
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

			// Wipe all entities
			{
				auto const lg = std::lock_guard{ _lock };
				_entities.clear();
				_entityErrorCounterTrackers.clear();
			}

			// Notify
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

	virtual std::tuple<la::avdecc::jsonSerializer::SerializationError, std::string> serializeAllControlledEntitiesAsJson(QString const& filePath, la::avdecc::entity::model::jsonSerializer::Flags const flags, QString const& dumpSource) const noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			return controller->serializeAllControlledEntitiesAsJson(filePath.toStdString(), flags, dumpSource.toStdString(), true);
		}
		return { la::avdecc::jsonSerializer::SerializationError::InternalError, "Controller offline" };
	}

	virtual std::tuple<la::avdecc::jsonSerializer::SerializationError, std::string> serializeControlledEntityAsJson(la::avdecc::UniqueIdentifier const entityID, QString const& filePath, la::avdecc::entity::model::jsonSerializer::Flags const flags, QString const& dumpSource) const noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			return controller->serializeControlledEntityAsJson(entityID, filePath.toStdString(), flags, dumpSource.toStdString());
		}
		return { la::avdecc::jsonSerializer::SerializationError::InternalError, "Controller offline" };
	}

	virtual std::tuple<la::avdecc::jsonSerializer::DeserializationError, std::string> loadVirtualEntitiesFromJsonNetworkState(QString const& filePath, la::avdecc::entity::model::jsonSerializer::Flags const flags) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			return controller->loadVirtualEntitiesFromJsonNetworkState(filePath.toStdString(), flags, true);
		}
		return { la::avdecc::jsonSerializer::DeserializationError::InternalError, "Controller offline" };
	}

	virtual std::tuple<la::avdecc::jsonSerializer::DeserializationError, std::string> loadVirtualEntityFromJson(QString const& filePath, la::avdecc::entity::model::jsonSerializer::Flags const flags) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			return controller->loadVirtualEntityFromJson(filePath.toStdString(), flags);
		}
		return { la::avdecc::jsonSerializer::DeserializationError::InternalError, "Controller offline" };
	}

	virtual void setEnableAemCache(bool const enable) noexcept override
	{
		_enableAemCache = enable;
	}

	virtual void setEnableFullAemEnumeration(bool const enable) noexcept override
	{
		_fullAemEnumeration = enable;
	}

	virtual void identifyEntity(la::avdecc::UniqueIdentifier const targetEntityID, std::chrono::milliseconds const duration, IdentifyEntityHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			controller->identifyEntity(targetEntityID, duration,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::IdentifyEntity, status);
					}
				});
		}
	}

	ErrorCounterTracker const* entityErrorCounterTracker(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto const lg = std::lock_guard{ _lock };

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

	virtual StreamInputErrorCounters getStreamInputErrorCounters(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept override
	{
		if (auto* errorCounterTracker = entityErrorCounterTracker(entityID))
		{
			return errorCounterTracker->getStreamInputErrorCounters(streamIndex);
		}
		return {};
	}

	virtual void clearStreamInputCounterValidFlags(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamInputCounterValidFlag const flag) noexcept override
	{
		if (auto* errorCounterTracker = entityErrorCounterTracker(entityID))
		{
			if (errorCounterTracker->clearStreamInputCounter(streamIndex, flag))
			{
				emit streamInputErrorCounterChanged(entityID, streamIndex, errorCounterTracker->getStreamInputErrorCounters(streamIndex));
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

	virtual StatisticsErrorCounters getStatisticsCounters(la::avdecc::UniqueIdentifier const entityID) const noexcept override
	{
		if (auto* errorCounterTracker = entityErrorCounterTracker(entityID))
		{
			return errorCounterTracker->getStatisticsErrorCounters();
		}
		return {};
	}

	virtual void clearStatisticsCounterValidFlags(la::avdecc::UniqueIdentifier const entityID, StatisticsErrorCounterFlag const flag) noexcept override
	{
		if (auto* errorCounterTracker = entityErrorCounterTracker(entityID))
		{
			if (errorCounterTracker->clearStatisticsCounter(flag))
			{
				emit statisticsErrorCounterChanged(entityID, errorCounterTracker->getStatisticsErrorCounters());
			}
		}
	}

	virtual void clearAllStatisticsCounterValidFlags(la::avdecc::UniqueIdentifier const entityID) noexcept override
	{
		if (auto* errorCounterTracker = entityErrorCounterTracker(entityID))
		{
			errorCounterTracker->clearAllStatisticsCounters();
		}
	}

	/* Discovery Protocol (ADP) */
	virtual bool enableEntityAdvertising(std::uint32_t const availableDuration, std::optional<la::avdecc::entity::model::AvbInterfaceIndex> const interfaceIndex) noexcept
	{
		auto controller = getController();
		if (controller)
		{
			try
			{
				controller->enableEntityAdvertising(availableDuration, interfaceIndex);
				return true;
			}
			catch (...)
			{
			}
		}
		return false;
	}

	virtual void disableEntityAdvertising(std::optional<la::avdecc::entity::model::AvbInterfaceIndex> const interfaceIndex) noexcept
	{
		auto controller = getController();
		if (controller)
		{
			controller->disableEntityAdvertising(interfaceIndex);
		}
	}

	virtual bool discoverRemoteEntities() const noexcept
	{
		auto controller = getController();
		if (controller)
		{
			return controller->discoverRemoteEntities();
		}
		return false;
	}

	virtual bool discoverRemoteEntity(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto controller = getController();
		if (controller)
		{
			return controller->discoverRemoteEntity(entityID);
		}
		return false;
	}

	virtual void setAutomaticDiscoveryDelay(std::chrono::milliseconds const delay) noexcept
	{
		_discoveryDelay = delay;
		// No need to re-create the controller, simply update this live parameter if the controller has been created
		auto controller = getController();
		if (controller)
		{
			return controller->setAutomaticDiscoveryDelay(delay);
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const owningEntity) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const /*owningEntity*/) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const lockingEntity) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::UniqueIdentifier const /*lockingEntity*/) noexcept
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
				[this, targetEntityID](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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

	virtual void setStreamOutputInfo(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInfo const& streamInfo, SetStreamOutputInfoHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetStreamInfo);
			controller->setStreamOutputInfo(targetEntityID, streamIndex, streamInfo,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetStreamInfo, status);
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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

	virtual void setControlName(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, QString const& name, SetControlNameHandler const& handler = {}) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetControlName);
			controller->setControlName(targetEntityID, configurationIndex, controlIndex, name.toStdString(),
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetControlName, status);
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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

	virtual void setAssociationID(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::UniqueIdentifier const associationID, SetAssociationIDHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetAssociationID);
			controller->setAssociationID(targetEntityID, associationID,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetAssociationID, status);
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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

	virtual void setControlValues(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlValues const& controlValues, SetControlValuesHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			emit beginAecpCommand(targetEntityID, AecpCommandType::SetControl);
			controller->setControlValues(targetEntityID, controlIndex, controlValues,
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
				{
					if (handler)
					{
						la::avdecc::utils::invokeProtectedHandler(handler, targetEntityID, status);
					}
					else
					{
						emit endAecpCommand(targetEntityID, AecpCommandType::SetControl, status);
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::entity::model::OperationID const operationID) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status, la::avdecc::entity::model::OperationID const operationID) noexcept
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
				[this, targetEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
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
				[this, talkerEntityID, listenerEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*talkerEntity*/, la::avdecc::controller::ControlledEntity const* const /*listenerEntity*/, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status) noexcept
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
				[this, talkerEntityID, talkerStreamIndex, listenerEntityID, handler](la::avdecc::controller::ControlledEntity const* const /*listenerEntity*/, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status) noexcept
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

	virtual void requestExclusiveAccess(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::Controller::ExclusiveAccessToken::AccessType const type, RequestExclusiveAccessHandler const& handler) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			controller->requestExclusiveAccess(entityID, type,
				[this, entityID, handler](auto const* const /*entity*/, auto const status, auto&& token) noexcept
				{
					la::avdecc::utils::invokeProtectedHandler(handler, entityID, status, std::move(token));
				});
		}
	}

	virtual void createCommandsExecutor(la::avdecc::UniqueIdentifier const entityID, bool const requestExclusiveAccess, std::function<void(hive::modelsLibrary::CommandsExecutor&)> const& handler) noexcept override
	{
		auto executor = std::make_unique<CommandsExecutorImpl>(this, entityID, requestExclusiveAccess);
		auto& ex = *executor;
		la::avdecc::utils::invokeProtectedHandler(handler, ex);
		if (!!ex)
		{
			// Store the executor
			{
				auto const lg = std::lock_guard{ _lock };
				_commandsExecutors.emplace(executor.get(), std::move(executor));
			}
			// Sets the completion handler
			ex.setCompletionHandler(
				[this](CommandsExecutorImpl const* const executor)
				{
					auto const lg = std::lock_guard{ _lock };
					_commandsExecutors.erase(executor);
				});
			// Start execution
			ex.exec();
		}
	}

	virtual void foreachEntity(ControlledEntityCallback const& callback) noexcept override
	{
		auto controller = getController();
		if (controller)
		{
			// Build a vector of all locked entities
			auto controlledEntities = std::vector<la::avdecc::controller::ControlledEntityGuard>{};

			{
				auto const lg = std::lock_guard{ _lock };
				for (auto& entityID : _entities)
				{
					auto ceg = getControlledEntity(entityID);
					if (AVDECC_ASSERT_WITH_RET(!!ceg, "ControllerManager model not up-to-date with avdecc::controller"))
					{
						controlledEntities.push_back(std::move(ceg));
					}
				}
			}

			for (auto const& controlledEntity : controlledEntities)
			{
				callback(controlledEntity->getEntity().getEntityID(), *controlledEntity);
			}
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

	mutable std::mutex _lock{}; // Data members exclusive access
	std::set<la::avdecc::UniqueIdentifier> _entities; // Online entities
	std::unordered_map<la::avdecc::UniqueIdentifier, ErrorCounterTracker, la::avdecc::UniqueIdentifier::hash> _entityErrorCounterTrackers; // Entities error counter flags
	std::unordered_map<CommandsExecutorImpl const*, std::unique_ptr<CommandsExecutorImpl>> _commandsExecutors{};
	std::chrono::milliseconds _discoveryDelay{};
	bool _enableAemCache{ false };
	bool _fullAemEnumeration{ false };
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
		case AecpCommandType::SetStreamInfo:
			return "Set Stream Info";
		case AecpCommandType::SetAvbInterfaceName:
			return "Set AVB Interface Name";
		case AecpCommandType::SetClockSourceName:
			return "Set Clock Source Name";
		case AecpCommandType::SetMemoryObjectName:
			return "Set Memory Object Name";
		case AecpCommandType::SetAudioClusterName:
			return "Set Audio Cluster Name";
		case AecpCommandType::SetControlName:
			return "Set Control Name";
		case AecpCommandType::SetClockDomainName:
			return "Set Clock Domain Name";
		case AecpCommandType::SetAssociationID:
			return "Set Association ID";
		case AecpCommandType::SetSamplingRate:
			return "Set Sampling Rate";
		case AecpCommandType::SetClockSource:
			return "Set Clock Source";
		case AecpCommandType::SetControl:
			return "Set Control Values";
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
		case AecpCommandType::IdentifyEntity:
			return "Identify Entity";
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

} // namespace modelsLibrary
} // namespace hive
