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
		connect(&manager, &ControllerManager::entityNameChanged, this, &MCDomainManagerImpl::onEntityNameChanged);
	}

	~MCDomainManagerImpl() noexcept {}

private:
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
	* Handles the change of a clock source on a stream connection. Checks if the stream is a clock stream and if so emits the mediaClockConnectionsUpdate signal.
	*/
	Q_SLOT void onStreamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& streamConnectionState)
	{
		auto affectsMcMaster = false;
		auto& manager = avdecc::ControllerManager::getInstance();
		auto const& controlledEntity = manager.getControlledEntity(streamConnectionState.listenerStream.entityID);
		if (controlledEntity)
		{
			if (la::avdecc::hasFlag(controlledEntity->getEntity().getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
			{
				try
				{
					auto const& configNode = controlledEntity->getCurrentConfigurationNode();
					auto const activeConfigIndex = configNode.descriptorIndex;

					// find out if the stream connection is set as clock source:
					for (auto const& clockDomainKV : configNode.clockDomains)
					{
						auto const& clockDomain = clockDomainKV.second;
						if (clockDomain.dynamicModel)
						{
							auto const clockSourceIndex = clockDomain.dynamicModel->clockSourceIndex;
							auto const& activeClockSourceNode = controlledEntity->getClockSourceNode(activeConfigIndex, clockSourceIndex);

							if (!activeClockSourceNode.staticModel)
							{
								break;
							}
							switch (activeClockSourceNode.staticModel->clockSourceType)
							{
								case la::avdecc::entity::model::ClockSourceType::Internal:
									break;
								case la::avdecc::entity::model::ClockSourceType::External:
								case la::avdecc::entity::model::ClockSourceType::InputStream:
								{
									if (streamConnectionState.listenerStream.streamIndex == activeClockSourceNode.staticModel->clockSourceLocationIndex)
									{
										affectsMcMaster = true;
									}
									break;
								}
								default: // Unsupported ClockSourceType, ignore it
									break;
							}
						}
					}
				}
				catch (la::avdecc::controller::ControlledEntity::Exception const&)
				{
					// ignore
				}
			}
		}

		if (affectsMcMaster)
		{
			// potentially changes every other entity...
			notifyChanges();
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
	* Handles the change of an entity name and determines if a mc master name is changed therefor.
	*/
	Q_SLOT void onEntityNameChanged(la::avdecc::UniqueIdentifier const entityId, QString const& entityName)
	{
		std::vector<la::avdecc::UniqueIdentifier> changedEntities;
		auto domainIndex = _currentMCDomainMapping.findDomainIndexByMasterEntityId(entityId);
		if (domainIndex)
		{
			for (auto const& entityMcMappingKV : _currentMCDomainMapping.getEntityMediaClockMasterMappings())
			{
				if (std::find(entityMcMappingKV.second.begin(), entityMcMappingKV.second.end(), *domainIndex) != entityMcMappingKV.second.end())
				{
					changedEntities.push_back(entityMcMappingKV.first);
				}
			}
		}
		if (!changedEntities.empty())
		{
			emit mcMasterNameChanged(changedEntities);
		}
	}

	/**
	* Gets the media clock master for an entity.
	* @return A pair of an entity id and an error. Error identifies if an mc master could be determined.
	*/
	virtual std::pair<la::avdecc::UniqueIdentifier, McDeterminationError> getMediaClockMaster(la::avdecc::UniqueIdentifier const entityId) noexcept override
	{
		auto hasErrorIterator = _currentMCDomainMapping.getEntityMcErrors().find(entityId);
		if (hasErrorIterator != _currentMCDomainMapping.getEntityMcErrors().end())
		{
			return std::make_pair(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), hasErrorIterator->second);
		}
		else
		{
			auto entityMcDomainIterator = _currentMCDomainMapping.getEntityMediaClockMasterMappings().find(entityId);
			if (entityMcDomainIterator != _currentMCDomainMapping.getEntityMediaClockMasterMappings().end())
			{
				auto const& associatedDomains = entityMcDomainIterator->second;
				if (associatedDomains.size() > 0)
				{
					auto mediaClockDomainIterator = _currentMCDomainMapping.getMediaClockDomains().find(associatedDomains.at(0));
					if (mediaClockDomainIterator != _currentMCDomainMapping.getMediaClockDomains().end())
					{
						return std::make_pair(mediaClockDomainIterator->second.getMediaClockDomainMaster(), McDeterminationError::NoError);
					}
				}
			}
		}

		return std::make_pair(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), McDeterminationError::UnknownEntity);
	}

	DomainIndex getOrCreateDomainIndexForClockMasterId(MCEntityDomainMapping::Domains& domains, la::avdecc::UniqueIdentifier const mediaClockMasterId) noexcept
	{
		for (auto const& mediaClockDomainKV : domains)
		{
			auto const& domain = mediaClockDomainKV.second;
			if (domain.getMediaClockDomainMaster() == mediaClockMasterId)
			{
				return domain.getDomainIndex();
			}
		}

		// Not found, create a new domain
		auto const nextDomainIndex = domains.size();
		domains.emplace(nextDomainIndex, MCDomain{ nextDomainIndex, mediaClockMasterId });

		return nextDomainIndex;
	}

	/**
	* Builds a media clock mapping object from the current state. This mapping contains information about each entities media clock master.
	* The mappings are grouped by domains. A domain is a virtual construct, which has exactly one media clock master and n entities that get their
	* clock source from this master.
	* @return Entity to domain mappings.
	*/
	virtual MCEntityDomainMapping createMediaClockDomainModel() noexcept override
	{
		auto mappings = MCEntityDomainMapping::Mappings{};
		auto domains = MCEntityDomainMapping::Domains{};
		auto errors = MCEntityDomainMapping::Errors{};

		for (auto const& entityId : _entities)
		{
			std::vector<avdecc::mediaClock::DomainIndex> associatedDomains;
			auto mcMasterIdKV = findMediaClockMaster(entityId);
			auto const& mcMasterId = mcMasterIdKV.first;
			auto const& mcMasterError = mcMasterIdKV.second;
			if (!mcMasterError)
			{
				auto const domainIndex = getOrCreateDomainIndexForClockMasterId(domains, mcMasterId);
				associatedDomains.push_back(domainIndex);
			}
			else
			{
				errors.emplace(entityId, mcMasterError);
			}

			if (mcMasterId == entityId)
			{
				auto secondaryMasterIdKV = findMediaClockMaster(entityId); // get second connection if there is one.
				auto const& secondaryMasterId = secondaryMasterIdKV.first;
				auto const& secondaryMasterError = secondaryMasterIdKV.second;
				if (secondaryMasterId) // check if the id is valid
				{
					if (!secondaryMasterError)
					{
						auto const domainIndex = getOrCreateDomainIndexForClockMasterId(domains, secondaryMasterId);
						associatedDomains.push_back(domainIndex);
					}
				}
			}
			mappings.emplace(entityId, associatedDomains);
		}

		return MCEntityDomainMapping{ std::move(mappings), std::move(domains), std::move(errors) };
	}

	/**
	* Gets the media clock master for an entity.
	* @return A pair of an entity id and an error. Error identifies if an mc master could be determined.
	*/
	std::pair<la::avdecc::UniqueIdentifier, McDeterminationError> findMediaClockMaster(la::avdecc::UniqueIdentifier const entityID) noexcept
	{
		auto error = McDeterminationError::UnknownEntity;
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
			if (!la::avdecc::hasFlag(controlledEntity->getEntity().getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
			{
				keepSearching = false;
				error = McDeterminationError::NotSupported;
				break;
			}

			try
			{
				auto const& entityModel = controlledEntity->getEntityNode();
				auto const& configNode = controlledEntity->getCurrentConfigurationNode();
				auto const activeConfigIndex = configNode.descriptorIndex;

				if (configNode.clockDomains.empty())
				{
					keepSearching = false;
					break;
				}

				for (auto const& clockDomainKV : configNode.clockDomains)
				{
					auto const& clockDomain = clockDomainKV.second;
					if (clockDomain.dynamicModel)
					{
						auto clockSourceIndex = clockDomain.dynamicModel->clockSourceIndex;
						auto const& activeClockSourceNode = controlledEntity->getClockSourceNode(activeConfigIndex, clockSourceIndex);

						if (!activeClockSourceNode.staticModel)
						{
							keepSearching = false;
							break;
						}
						switch (activeClockSourceNode.staticModel->clockSourceType)
						{
							case la::avdecc::entity::model::ClockSourceType::Internal:
								return std::make_pair(currentEntityId, McDeterminationError::NoError);
							case la::avdecc::entity::model::ClockSourceType::External:
								return std::make_pair(currentEntityId, McDeterminationError::ExternalClockSource);
							case la::avdecc::entity::model::ClockSourceType::InputStream:
								if (activeClockSourceNode.staticModel->clockSourceLocationType == la::avdecc::entity::model::DescriptorType::StreamInput)
								{
									auto clockStreamIndex = activeClockSourceNode.staticModel->clockSourceLocationIndex;
									auto* clockStreamDynModel = controlledEntity->getStreamInputNode(activeConfigIndex, clockStreamIndex).dynamicModel;
									if (clockStreamDynModel)
									{
										auto connectedTalker = clockStreamDynModel->connectionState.talkerStream.entityID;
										if (!connectedTalker)
										{
											error = McDeterminationError::StreamNotConnected;
											keepSearching = false;
											break;
										}
										auto connectedTalkerStreamIndex = clockStreamDynModel->connectionState.talkerStream.streamIndex;
										if (searchedEntityIds.count(connectedTalker))
										{
											return std::make_pair(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), McDeterminationError::Recursive);
										}
										currentEntityId = connectedTalker;
										searchedEntityIds.insert(currentEntityId);
									}
									else
									{
										error = McDeterminationError::StreamNotConnected;
										keepSearching = false;
										break;
									}
								}
								else
								{
									keepSearching = false;
									break;
								}
								break;
							default:
								error = McDeterminationError::NotSupported;
								keepSearching = false;
								break;
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

		return std::make_pair(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), error);
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
	bool checkMcMasterOfEntityChanged(std::vector<avdecc::mediaClock::DomainIndex> const& oldEntityDomainMapping, std::vector<avdecc::mediaClock::DomainIndex> const& newEntityDomainMapping, MCEntityDomainMapping::Domains const& oldMcDomains, MCEntityDomainMapping::Domains const& newMcDomains) noexcept
	{
		auto const sizeOldDomainIndexes = oldEntityDomainMapping.size();
		auto const sizeNewDomainIndexes = newEntityDomainMapping.size();
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
		// potentially changes every other entity...
		std::vector<la::avdecc::UniqueIdentifier> changes;
		auto currentMCDomainMapping = createMediaClockDomainModel();
		auto const& currentMCDomains = currentMCDomainMapping.getMediaClockDomains();
		auto const& previousMCDomainMapping = _currentMCDomainMapping;
		auto const& previousMCMasterMappings = previousMCDomainMapping.getEntityMediaClockMasterMappings();
		auto const& previousMCDomains = previousMCDomainMapping.getMediaClockDomains();
		auto const& previousErrors = previousMCDomainMapping.getEntityMcErrors();
		auto const& currentErrors = currentMCDomainMapping.getEntityMcErrors();

		for (auto const& entityDomainKV : currentMCDomainMapping.getEntityMediaClockMasterMappings())
		{
			auto const entityId = entityDomainKV.first;
			auto const& domainIdxs = entityDomainKV.second;
			auto const oldDomainIndexesIterator = previousMCMasterMappings.find(entityId);
			if (oldDomainIndexesIterator != previousMCMasterMappings.end())
			{
				if (checkMcMasterOfEntityChanged(oldDomainIndexesIterator->second, entityDomainKV.second, previousMCDomains, currentMCDomains))
				{
					changes.push_back(entityId);
				}
				else if (previousErrors.count(entityId) > 0 && currentErrors.count(entityId) > 0
					&& previousErrors.at(entityId) != currentErrors.at(entityId)
					|| previousErrors.count(entityId) != currentErrors.count(entityId))
				{
					// if the error type changed add the entity to the changes list as well.
					changes.push_back(entityId);
				}
			}
			else
			{
				// if it wasn't there before, it changed.
				changes.push_back(entityId);
			}
		}

		// Update the model
		_currentMCDomainMapping = std::move(currentMCDomainMapping);

		// Notify the view
		if (!changes.empty())
		{
			emit mediaClockConnectionsUpdate(changes);
		}
	}

	// Private members
	std::set<la::avdecc::UniqueIdentifier> _entities{}; // No lock required, only read/write in the UI thread
	MCEntityDomainMapping _currentMCDomainMapping{};
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

MCEntityDomainMapping::MCEntityDomainMapping() noexcept
{
	// Nothing to do
}

MCEntityDomainMapping::MCEntityDomainMapping(Mappings&& mappings, Domains&& domains, Errors&& errors) noexcept
	: _entityMediaClockMasterMappings(std::move(mappings))
	, _mediaClockDomains(std::move(domains))
	, _entityMcErrors(std::move(errors))
{
	// Nothing to do
}

MCDomain::MCDomain(DomainIndex const index, la::avdecc::UniqueIdentifier const mediaClockMaster, la::avdecc::entity::model::SamplingRate const samplingRate) noexcept
	: _domainIndex(index)
	, _mediaClockMasterId(mediaClockMaster)
	, _samplingRate(samplingRate)
{
	// Nothing to do
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
* Creates a name to display in the ui for this domain from the mc master id.
* @return The name to display.
*/
QString MCDomain::getDisplayName() const noexcept
{
	return QString("Domain ").append(_mediaClockMasterId ? helper::uniqueIdentifierToString(_mediaClockMasterId) : "-");
}

/**
* Gets the mc master of this domain.
* @return The mc master entity id.
*/
la::avdecc::UniqueIdentifier MCDomain::getMediaClockDomainMaster() const noexcept
{
	return _mediaClockMasterId;
}

/**
* Sets the media clock master id.
* @param entityId Entity id to set as mc master.
*/
void MCDomain::setMediaClockDomainMaster(la::avdecc::UniqueIdentifier entityId) noexcept
{
	_mediaClockMasterId = entityId;
}

/**
* Gets the sampling rate.
* @param entityId Entity id to set as mc master.
*/
la::avdecc::entity::model::SamplingRate MCDomain::getDomainSamplingRate() const noexcept
{
	return _samplingRate;
}

/**
* Sets the sampling rate of this domain.
* @param entityId Entity id to set as mc master.
*/
void MCDomain::setDomainSamplingRate(la::avdecc::entity::model::SamplingRate samplingRate) noexcept
{
	_samplingRate = samplingRate;
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
MCEntityDomainMapping::Mappings const& MCEntityDomainMapping::getEntityMediaClockMasterMappings() const noexcept
{
	return _entityMediaClockMasterMappings;
}

/**
* Gets a reference of the media clock domain map.
* @return Reference of the _mediaClockDomains field.
*/
MCEntityDomainMapping::Domains const& MCEntityDomainMapping::getMediaClockDomains() const noexcept
{
	return _mediaClockDomains;
}

/**
* Gets a reference of the entity to mc determination error map.
* @return Reference of the _entityMcErrors field.
*/
MCEntityDomainMapping::Errors const& MCEntityDomainMapping::getEntityMcErrors() const noexcept
{
	return _entityMcErrors;
}

} // namespace mediaClock
} // namespace avdecc
