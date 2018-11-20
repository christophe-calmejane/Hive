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

#include "mcDomainManager.hpp"
#include "controllerManager.hpp"
#include "helper.hpp"
#include <la/avdecc/internals/streamFormat.hpp>
#include <atomic>
#include <optional>
#include <unordered_set>

namespace avdecc
{
namespace mediaClock
{
class MCDomainManagerImpl final : public MCDomainManager
{
public:
	/**
	* Constructor. Sets up connections to update the internal model.
	*/
	MCDomainManagerImpl() noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		connect(&manager, &ControllerManager::controllerOffline, this, &MCDomainManagerImpl::onControllerOffline);
		connect(&manager, &ControllerManager::entityOnline, this, &MCDomainManagerImpl::onEntityOnline);
		connect(&manager, &ControllerManager::entityOffline, this, &MCDomainManagerImpl::onEntityOffline);
		connect(&manager, &ControllerManager::streamConnectionChanged, this, &MCDomainManagerImpl::onStreamConnectionChanged);
		connect(&manager, &ControllerManager::clockSourceChanged, this, &MCDomainManagerImpl::onClockSourceChanged);
	}

	~MCDomainManagerImpl() noexcept {}

	/**
	* Removes all entities from the internal list.
	*/
	Q_SLOT void onControllerOffline()
	{
		_entities.clear();
		notifyChanges();
	}

	/**
	* Adds the entity to the internal list.
	*/
	Q_SLOT void onEntityOnline(la::avdecc::UniqueIdentifier const& entityId)
	{
		// add entity to the set
		_entities.insert(entityId);
		notifyChanges();
	}

	/**
	* Removes the entity from the internal list.
	*/
	Q_SLOT void onEntityOffline(la::avdecc::UniqueIdentifier const& entityId)
	{
		// remove entity from the set
		_entities.erase(entityId);
		notifyChanges();
	}

	/**
	* Handles the change of a clock source on a stream connection. Checks if the stream was a clock stream and if so emits the mediaClockConnectionsUpdate signal.
	*/
	Q_SLOT void onStreamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& streamConnectionState)
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto const& controlledEntity = manager.getControlledEntity(streamConnectionState.listenerStream.entityID);
		if (controlledEntity)
		{
			if (la::avdecc::hasFlag(controlledEntity->getEntity().getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
			{
				try
				{
					auto entityModel = controlledEntity->getEntityNode();

					auto const& configNode = controlledEntity->getCurrentConfigurationNode();
					auto activeConfigIndex = configNode.descriptorIndex;

					// find out if the stream connection is a clock stream connection:
					bool isClockStream = false;
					auto const& streamInput = controlledEntity->getStreamInputNode(activeConfigIndex, streamConnectionState.listenerStream.streamIndex);
					if (streamInput.dynamicModel)
					{
						auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamInput.dynamicModel->currentFormat);
						auto streamType = streamFormatInfo->getType();
						if (la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference == streamType)
						{
							isClockStream = true;
						}
					}

					if (isClockStream)
					{
						// potenitally changes every other entity...
						notifyChanges();
					}
				}
				catch (la::avdecc::controller::ControlledEntity::Exception const&)
				{
					// ignore
				}
			}
		}
	}

	/**
	* Handles the change of a clock source on an entity and emits resulting changes via the mediaClockConnectionsUpdate signal.
	*/
	Q_SLOT void onClockSourceChanged(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex)
	{
		notifyChanges();
	}

	/**
	* Gets the media clock master for an entity.
	* @return A pair of an entity id and an error. Error identifies if an mc master could be determined.
	*/
	virtual std::pair<la::avdecc::UniqueIdentifier, Error> findMediaClockMaster(la::avdecc::UniqueIdentifier const entityID) noexcept
	{
		std::unordered_set<la::avdecc::UniqueIdentifier, la::avdecc::UniqueIdentifier::hash> searchedEntityIds;
		auto currentEntityId = entityID;
		searchedEntityIds.insert(currentEntityId);
		bool keepSearching = true;
		while (keepSearching)
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto const& controlledEntity = manager.getControlledEntity(currentEntityId);
			if (!controlledEntity)
			{
				keepSearching = false;
				break;
			}
			auto const& entityModel = controlledEntity->getEntityNode();

			try
			{
				auto const& configNode = controlledEntity->getCurrentConfigurationNode();
				auto const activeConfigIndex = configNode.descriptorIndex;

				// internal or external?
				bool clockSourceInternal = false;

				for (auto const& clockDomainKV : configNode.clockDomains)
				{
					auto const& clockDomain = clockDomainKV.second;
					if (clockDomain.dynamicModel)
					{
						auto clockSourceIndex = clockDomain.dynamicModel->clockSourceIndex;
						auto const& activeClockSourceNode = controlledEntity->getClockSourceNode(activeConfigIndex, clockSourceIndex);

						if (activeClockSourceNode.staticModel && activeClockSourceNode.staticModel->clockSourceType == la::avdecc::entity::model::ClockSourceType::Internal)
						{
							return std::make_pair(currentEntityId, Error::NoError);
						}
						else
						{
							// find the clock stream:
							auto clockStreamIndex = findClockStreamIndex(configNode);
							if (clockStreamIndex)
							{
								auto* clockStreamDynModel = controlledEntity->getStreamInputNode(activeConfigIndex, clockStreamIndex.value()).dynamicModel;
								if (clockStreamDynModel)
								{
									auto connectedTalker = clockStreamDynModel->connectionState.talkerStream.entityID;
									auto connectedTalkerStreamIndex = clockStreamDynModel->connectionState.talkerStream.streamIndex;
									if (searchedEntityIds.count(connectedTalker))
									{
										return std::make_pair(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), Error::Recursive);
									}
									currentEntityId = connectedTalker;
									searchedEntityIds.insert(currentEntityId);
								}
							}
							else
							{
								keepSearching = false;
								break;
							}
						}
					}
					else
					{
						keepSearching = false;
						break;
					}
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
				keepSearching = false;
				break;
			}
		}

		return std::make_pair(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), Error::UnknownEntity);
	}

	/**
	* Builds a media clock mapping object from the current state. This mapping contains information about each entities media clock master.
	* The mappings are grouped by domains. A domain is a virtual construct, which has exactly one media clock master and n entities that get their
	* clock source from this master.
	* @return Entity to domain mappings.
	*/
	virtual MCEntityDomainMapping createMediaClockDomainModel() noexcept
	{
		MCEntityDomainMapping result;
		auto& entityMediaClockMasterMappings = result.getEntityMediaClockMasterMappings();
		for (auto const& entityId : _entities)
		{
			std::vector<avdecc::mediaClock::DomainIndex> associatedDomains;
			auto mcMasterIdKV = findMediaClockMaster(entityId);
			auto const& mcMasterId = mcMasterIdKV.first;
			auto const& mcMasterError = mcMasterIdKV.second;
			if (!!mcMasterError)
			{
				auto domainIndex = result.findDomainIndexByMasterEntityId(mcMasterId);
				if (!domainIndex)
				{ // domain not created yet
					MCDomain mediaClockDomain;
					mediaClockDomain.setMediaClockDomainMaster(mcMasterId);
					domainIndex = result.getMediaClockDomains().size();
					mediaClockDomain.setDomainIndex(domainIndex.value());
					result.getMediaClockDomains().emplace(domainIndex.value(), mediaClockDomain);
				}
				associatedDomains.push_back(domainIndex.value());
			}

			if (mcMasterId == entityId)
			{
				auto secondaryMasterIdKV = findMediaClockMaster(entityId); // get second connection if there is one.
				auto const& secondaryMasterId = secondaryMasterIdKV.first;
				auto const& secondaryMasterError = secondaryMasterIdKV.second;
				if (secondaryMasterId) // check if the id is valid
				{
					associatedDomains.push_back(secondaryMasterId);
					if (!!secondaryMasterError)
					{
						auto domainIndex = result.findDomainIndexByMasterEntityId(secondaryMasterId);
						if (!domainIndex)
						{
							// domain not created yet
							MCDomain mediaClockDomain;
							mediaClockDomain.setMediaClockDomainMaster(secondaryMasterId);
							domainIndex = result.getMediaClockDomains().size();
							mediaClockDomain.setDomainIndex(domainIndex.value());
							result.getMediaClockDomains().emplace(domainIndex.value(), mediaClockDomain);
						}
						associatedDomains.push_back(domainIndex.value());
					}
				}
			}
			entityMediaClockMasterMappings.emplace(entityId, associatedDomains);
		}

		return result;
	}

private:
	/**
	* Finds the index of the clock stream.
	* @return If the clock stream index can not be determined, std::nullopt is returned.
	*/
	std::optional<la::avdecc::entity::model::StreamIndex> findClockStreamIndex(la::avdecc::controller::model::ConfigurationNode const& configNode) noexcept
	{
		for (auto const& streamInputKV : configNode.streamInputs)
		{
			auto const& streamInput = streamInputKV.second;
			if (streamInput.dynamicModel)
			{
				auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamInput.dynamicModel->currentFormat);
				auto streamType = streamFormatInfo->getType();
				if (la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference == streamType)
				{
					return streamInputKV.first;
				}
			}
		}
		return std::nullopt;
	}

	/**
	* Checks if the domain mc master of an entity changed. The first index in the vector is the only one that matters.
	* Other indexes are secondary masters and are not of relevance here.
	* @param oldEntityDomainMapping The domain indexes of an entity that was determined using createMediaClockDomainModel method at an earlier timepoint.
	* @param newEntityDomainMapping	The domain indexes of an entity that was determined using createMediaClockDomainModel method at an more recent timepoint.
	* @param oldMcDomains			The domain index to mc domain data mapping at an earlier timepoint.
	* @param newMcDomains			The domain index to mc domain data mapping at an more recent timepoint.
	* @return True if the old entity mc master is different from the new one.
	*/
	bool checkMcMasterOfEntityChanged(std::vector<avdecc::mediaClock::DomainIndex> oldEntityDomainMapping, std::vector<avdecc::mediaClock::DomainIndex> newEntityDomainMapping, std::unordered_map<DomainIndex, MCDomain> oldMcDomains, std::unordered_map<DomainIndex, MCDomain> newMcDomains) noexcept
	{
		int sizeOldDomainIndexes = oldEntityDomainMapping.size();
		int sizeNewDomainIndexes = newEntityDomainMapping.size();
		if (sizeOldDomainIndexes > 0 && sizeNewDomainIndexes > 0)
		{
			if (oldMcDomains.count(oldEntityDomainMapping.at(0)) && newMcDomains.count(newEntityDomainMapping.at(0)))
			{
				auto const& oldMaster = oldMcDomains.at(oldEntityDomainMapping.at(0)); // the first index is the mc master
				auto const& newMaster = newMcDomains.at(newEntityDomainMapping.at(0)); // the first index is the mc master
				if (oldMaster.getMediaClockDomainMaster() != newMaster.getMediaClockDomainMaster())
				{
					return true;
				}
			}
		}
		else if (sizeOldDomainIndexes == 0 && sizeNewDomainIndexes > 0 || sizeOldDomainIndexes > 0 && sizeNewDomainIndexes == 0)
		{
			// if one of the lists is empty while the other isn't we have a change on the mc master. (Indeterminable -> ID or vice versa)
			return true;
		}
		return false;
	}

	/**
	* Compares the current mc mapping state with the previous one and 
	* emits the mediaClockConnectionsUpdate to inform the views about changes.
	*/
	void notifyChanges() noexcept
	{
		// potenitally changes every other entity...
		std::vector<la::avdecc::UniqueIdentifier> changes;
		auto currentMCDomainMapping = createMediaClockDomainModel();
		auto& previousMCDomainMapping = _currentMCDomainMapping;
		for (auto const& entityDomainKV : currentMCDomainMapping.getEntityMediaClockMasterMappings())
		{
			auto const entityId = entityDomainKV.first;
			auto const& domainIdxs = entityDomainKV.second;
			auto const oldDomainIndexesIterator = previousMCDomainMapping.getEntityMediaClockMasterMappings().find(entityId);
			if (oldDomainIndexesIterator != previousMCDomainMapping.getEntityMediaClockMasterMappings().end())
			{
				if (checkMcMasterOfEntityChanged(oldDomainIndexesIterator->second, entityDomainKV.second, previousMCDomainMapping.getMediaClockDomains(), currentMCDomainMapping.getMediaClockDomains()))
				{
					changes.push_back(entityId);
				}
			}
			else
			{
				// if it wasn't there before, it changed.
				changes.push_back(entityId);
			}
		}
		_currentMCDomainMapping = currentMCDomainMapping; // update the model
		emit mediaClockConnectionsUpdate(changes);
	}
};

/**
* Singleton implementation of the MediaClockConnectionManager class.
* @return The MediaClockConnectionManager instance.
*/
MCDomainManager& MCDomainManager::getInstance() noexcept
{
	static MCDomainManagerImpl s_manager{};

	return s_manager;
}

/**
* Gets the domain index.
* @return The index of this domain.
*/
DomainIndex MCDomain::getDomainIndex() const noexcept
{
	return _domainIndex;
}

/**
* Sets the domain index.
* @param domainIndex The index of this domain.
*/
void MCDomain::setDomainIndex(DomainIndex domainIndex) noexcept
{
	_domainIndex = domainIndex;
}

/**
* Creates a name to display in the ui for this domain from the mc master id.
* @return The name to display.
*/
QString MCDomain::getDisplayName() const noexcept
{
	return QString("Domain ").append(_mcMaster ? helper::uniqueIdentifierToString(_mcMaster) : "-");
}

/**
* Gets the mc master of this domain.
* @return The mc master entity id.
*/
la::avdecc::UniqueIdentifier MCDomain::getMediaClockDomainMaster() const noexcept
{
	return _mcMaster;
}

/**
* Sets the media clock master id.
* @param entityId Entity id to set as mc master.
*/
void MCDomain::setMediaClockDomainMaster(la::avdecc::UniqueIdentifier entityId) noexcept
{
	_mcMaster = entityId;
}

/**
* Gets the sampling rate.
* @param entityId Entity id to set as mc master.
*/
la::avdecc::entity::model::SamplingRate MCDomain::getDomainSamplingRate() const noexcept
{
	return _domainSamplingRate;
}

/**
* Sets the sampling rate of this domain.
* @param entityId Entity id to set as mc master.
*/
void MCDomain::setDomainSamplingRate(la::avdecc::entity::model::SamplingRate samplingRate) noexcept
{
	_domainSamplingRate = samplingRate;
}

/////////////////////////////////////////////////////////////////////////////////////////

/**
* Finds the corresponding domain index by an entity id (media clock master).
* @param mediaClockMasterId The media clock master id.
* @return The index of the domain which mc master matches the given id.
*/
std::optional<DomainIndex> const MCEntityDomainMapping::findDomainIndexByMasterEntityId(la::avdecc::UniqueIdentifier mediaClockMasterId) noexcept
{
	for (auto const& mediaClockDomainKV : _mediaClockDomains)
	{
		if (mediaClockDomainKV.second.getMediaClockDomainMaster() == mediaClockMasterId)
		{
			return mediaClockDomainKV.second.getDomainIndex();
		}
	}
	return std::nullopt;
}

/**
* Gets a reference of the entity to media clock index map.
* @return Reference of the _entityMediaClockMasterMappings field.
*/
std::unordered_map<la::avdecc::UniqueIdentifier, std::vector<DomainIndex>, la::avdecc::UniqueIdentifier::hash>& MCEntityDomainMapping::getEntityMediaClockMasterMappings() noexcept
{
	return _entityMediaClockMasterMappings;
}

/**
* Gets a reference of the media clock domain map.
* @return Reference of the _mediaClockDomains field.
*/
std::unordered_map<DomainIndex, MCDomain>& MCEntityDomainMapping::getMediaClockDomains() noexcept
{
	return _mediaClockDomains;
}

} // namespace mediaClock
} // namespace avdecc
