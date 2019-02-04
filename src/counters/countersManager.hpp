#pragma once

#include <QObject>
#include "avdecc/controllerManager.hpp"

namespace avdecc
{

// Track counters and triggers event for special events/values
class CountersManager : public QObject
{
	Q_OBJECT
public:
	CountersManager(QObject* parent = nullptr);
	
	enum class CounterState
	{
		Normal,
		Warning,
	};
	
	// Returns per entity, a global counters state
	CounterState counterState(la::avdecc::UniqueIdentifier const entityID) const;

	// Returns per entity / stream / flag the counter state
	CounterState streamInputCounterState(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamInputCounterValidFlag const flag) const;
	CounterState streamOutputCounterState(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::StreamOutputCounterValidFlag const flag) const;

	// Emitted whenever a tracked counter state changes @see shouldNotify
	// Note this is global to the entity for now, but we should also wrap the stream index and the counter flag as well in order to identify the counter in the trees
	Q_SIGNAL void counterStateChanged(la::avdecc::UniqueIdentifier const entityID);
	
protected:
	Q_SLOT void handleEntityOnline(la::avdecc::UniqueIdentifier const entityID);
	Q_SLOT void handleEntityOffline(la::avdecc::UniqueIdentifier const entityID);
	Q_SLOT void handleStreamInputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamInputCounters const& counters);
	Q_SLOT void handleStreamOutputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamOutputCounters const& counters);

	bool shouldNotify(la::avdecc::entity::StreamInputCounterValidFlag const flag) const;
	bool shouldNotify(la::avdecc::entity::StreamOutputCounterValidFlag const flag) const;
	
private:
	struct CountersData
	{
		std::unordered_map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamInputCounters> streamInputCounters;
		std::unordered_map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamOutputCounters> streamOutputCounters;
	};
	
	std::unordered_map<la::avdecc::UniqueIdentifier, CountersData, la::avdecc::UniqueIdentifier::hash> _entityCounters;
};

} // namespace avdecc
