#include "counters/countersManager.hpp"
#include <QDebug>

namespace avdecc
{

CountersManager::CountersManager(QObject* parent)
: QObject{ parent }
{
	auto& controllerManager = ControllerManager::getInstance();
	connect(&controllerManager, &ControllerManager::entityOnline, this, &CountersManager::handleEntityOnline);
	connect(&controllerManager, &ControllerManager::entityOffline, this, &CountersManager::handleEntityOffline);
	connect(&controllerManager, &ControllerManager::streamInputCountersChanged, this, &CountersManager::handleStreamInputCountersChanged);
	connect(&controllerManager, &ControllerManager::streamOutputCountersChanged, this, &CountersManager::handleStreamOutputCountersChanged);
}

CountersManager::CounterState CountersManager::counterState(la::avdecc::UniqueIdentifier const entityID) const
{
	auto const it = _entityCounters.find(entityID);
	if (it != std::end(_entityCounters))
	{
		auto const& counterData{ it->second };
		
		// For now, just check if there is something
		if (counterData.streamInputCounters.size() || counterData.streamOutputCounters.size())
		{
			return CounterState::Warning;
		}
	}
	return CounterState::Normal;
}

CountersManager::CounterState CountersManager::streamInputCounterState(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamInputCounterValidFlag const flag) const
{
	auto const it = _entityCounters.find(entityID);
	if (it != std::end(_entityCounters))
	{
		auto const& counterData{ it->second };
		auto const jt = counterData.streamInputCounters.find(streamIndex);
		if (jt != std::end(counterData.streamInputCounters))
		{
			auto const& flagMap{ jt->second };
			auto const kt = flagMap.find(flag);
			if (kt != std::end(flagMap))
			{
				if (kt->second == 0)
				{
					return CounterState::Normal;
				}
				else
				{
					return CounterState::Warning;
				}
			}
		}
	}
	return CountersManager::CounterState::Normal;
}

CountersManager::CounterState CountersManager::streamOutputCounterState(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamOutputCounterValidFlag const flag) const
{
	auto const it = _entityCounters.find(entityID);
	if (it != std::end(_entityCounters))
	{
		auto const& counterData{ it->second };
		auto const jt = counterData.streamOutputCounters.find(streamIndex);
		if (jt != std::end(counterData.streamOutputCounters))
		{
			auto const& flagMap{ jt->second };
			auto const kt = flagMap.find(flag);
			if (kt != std::end(flagMap))
			{
				if (kt->second == 0)
				{
					return CounterState::Normal;
				}
				else
				{
					return CounterState::Warning;
				}
			}
		}
	}
	return CountersManager::CounterState::Normal;
}

void CountersManager::handleEntityOnline(la::avdecc::UniqueIdentifier const entityID)
{
	_entityCounters[entityID] = {};
}

void CountersManager::handleEntityOffline(la::avdecc::UniqueIdentifier const entityID)
{
	_entityCounters.erase(entityID);
}

void CountersManager::handleStreamInputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamInputCounters const& counters)
{
	auto& streamInputCounters = _entityCounters.at(entityID).streamInputCounters[streamIndex];
	for (auto const& kv : counters)
	{
		auto const& flag{ kv.first };
		auto const& counter{ kv.second };
		
		streamInputCounters[flag] = counter;

		if (shouldNotify(flag))
		{
			emit counterStateChanged(entityID);
		}
	}
}

void CountersManager::handleStreamOutputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamOutputCounters const& counters)
{
	auto& streamOutputCounters = _entityCounters.at(entityID).streamOutputCounters[streamIndex];
	for (auto const& kv : counters)
	{
		auto const& flag{ kv.first };
		auto const& counter{ kv.second };
		
		// For now, just update the internal counter and notify no matter what
		streamOutputCounters[flag] = counter;

		if (shouldNotify(flag))
		{
			emit counterStateChanged(entityID);
		}
	}
}

bool CountersManager::shouldNotify(la::avdecc::entity::StreamInputCounterValidFlag const flag) const
{
	switch (flag)
	{
		case la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked:
		case la::avdecc::entity::StreamInputCounterValidFlag::StreamReset:
			return true;
		default:
			return false;
	}
}

bool CountersManager::shouldNotify(la::avdecc::entity::StreamOutputCounterValidFlag const flag) const
{
	switch (flag)
	{
		default:
			return false;
	}
}

} // namespace avdecc
