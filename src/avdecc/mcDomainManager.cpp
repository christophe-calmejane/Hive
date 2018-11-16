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
#include <QDebug>

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
		auto controlledEntity = manager.getControlledEntity(streamConnectionState.listenerStream.entityID);
		if (controlledEntity)
		{
			if (la::avdecc::hasFlag(controlledEntity->getEntity().getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
			{
				try
				{
					la::avdecc::controller::model::EntityNode entityModel = controlledEntity->getEntityNode();

					la::avdecc::controller::model::ConfigurationNode const configNode = controlledEntity->getCurrentConfigurationNode();
					la::avdecc::entity::model::DescriptorIndex activeConfigIndex = configNode.descriptorIndex;

					// find out if the stream connection is a clock stream connection:
					bool isClockStream = false;
					auto const& streamInput = controlledEntity->getStreamInputNode(activeConfigIndex, streamConnectionState.listenerStream.streamIndex);
					if (streamInput.staticModel)
					{
						for (auto const& streamFormat : streamInput.staticModel->formats)
						{
							auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
							la::avdecc::entity::model::StreamFormatInfo::Type streamType = streamFormatInfo->getType();
							if (la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference == streamType)
							{
								isClockStream = true;
							}
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
* Handles the change of a clock source on an entity. Forwards to mediaClockConnectionsUpdate signal.
*/
	virtual std::pair<la::avdecc::UniqueIdentifier, Error> const getMediaClockMaster(la::avdecc::UniqueIdentifier const entityID) noexcept
	{
		std::vector<la::avdecc::UniqueIdentifier> searchedEntityIds;
		auto currentEntityId = entityID;
		searchedEntityIds.push_back(currentEntityId);
		while (true)
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(currentEntityId);
			if (!controlledEntity)
			{
				return std::make_pair<la::avdecc::UniqueIdentifier, Error>(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), Error::UnknownEntity);
			}
			auto entityModel = controlledEntity->getEntityNode();

			try
			{
				auto const configNode = controlledEntity->getCurrentConfigurationNode();
				la::avdecc::entity::model::DescriptorIndex activeConfigIndex = configNode.descriptorIndex;

				// internal or external?
				bool clockSourceInternal = false;

				// assume there is only one clock domain.
				for (auto const& clockDomainKV : configNode.clockDomains)
				{
					auto const& clockDomain = clockDomainKV.second;
					if (clockDomain.dynamicModel)
					{
						auto clockSourceIndex = clockDomain.dynamicModel->clockSourceIndex;
						auto activeClockSourceNode = controlledEntity->getClockSourceNode(activeConfigIndex, clockSourceIndex);

						if (activeClockSourceNode.staticModel && activeClockSourceNode.staticModel->clockSourceType == la::avdecc::entity::model::ClockSourceType::Internal)
						{
							return std::make_pair<la::avdecc::UniqueIdentifier, Error>((la::avdecc::UniqueIdentifier)currentEntityId, Error::NoError);
						}
						else
						{
							// find the clock stream:
							std::optional<la::avdecc::entity::model::StreamIndex> clockStreamIndex = findClockStreamIndex(configNode);
							if (clockStreamIndex)
							{
								auto* clockStreamDynModel = controlledEntity->getStreamInputNode(activeConfigIndex, clockStreamIndex.value()).dynamicModel;
								if (clockStreamDynModel)
								{
									auto connectedTalker = clockStreamDynModel->connectionState.talkerStream.entityID;
									auto connectedTalkerStreamIndex = clockStreamDynModel->connectionState.talkerStream.streamIndex;
									if (std::find(searchedEntityIds.begin(), searchedEntityIds.end(), connectedTalker) != searchedEntityIds.end())
									{
										return std::make_pair<la::avdecc::UniqueIdentifier, Error>(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), Error::Recursive);
									}
									currentEntityId = connectedTalker;
									searchedEntityIds.push_back(currentEntityId);
								}
							}
							else
							{
								return std::make_pair<la::avdecc::UniqueIdentifier, Error>((la::avdecc::UniqueIdentifier)currentEntityId, Error::NoError);
							}
						}
					}
					else
					{
						return std::make_pair<la::avdecc::UniqueIdentifier, Error>((la::avdecc::UniqueIdentifier)currentEntityId, Error::NoError);
					}
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
				// ignore
			}
		}
	}

	/**
* Builds a media clock mapping object from the current state. This mapping contains information about each entities media clock master.
* The mappings are grouped by domains. A domain is a virtual construct, which has exactly one media clock master and n entities that get their
* clock source from this master.
* @return Entity to domain mappings.
*/
	virtual const MCEntityDomainMapping createMediaClockDomainModel() noexcept
	{
		MCEntityDomainMapping result;
		for (auto const& entityId : _entities)
		{
			std::vector<avdecc::mediaClock::DomainIndex> associatedDomains;
			auto mcMasterIdKV = getMediaClockMaster(entityId);

			if (mcMasterIdKV.second == Error::NoError)
			{
				std::optional<DomainIndex> domainIndex = result.findDomainIndexByMasterEntityId(mcMasterIdKV.first);
				if (!domainIndex)
				{ // domain not created yet
					MCDomain mediaClockDomain;
					mediaClockDomain.setMediaClockDomainMaster(mcMasterIdKV.first);
					domainIndex = result.mediaClockDomains().size();
					mediaClockDomain.setDomainIndex(domainIndex.value());
					result.mediaClockDomains().emplace(domainIndex.value(), mediaClockDomain);
				}
				associatedDomains.push_back(domainIndex.value());
			}

			if (mcMasterIdKV.first == entityId)
			{
				auto secondaryMasterIdKV = getMediaClockMaster(entityId); // get second connection if there is one.
				if (secondaryMasterIdKV.first) // check if the id is valid
				{
					associatedDomains.push_back(secondaryMasterIdKV.first);
					if (secondaryMasterIdKV.second == Error::NoError)
					{
						std::optional<DomainIndex> domainIndex = result.findDomainIndexByMasterEntityId(secondaryMasterIdKV.first);
						if (!domainIndex)
						{
							// domain not created yet
							MCDomain mediaClockDomain;
							mediaClockDomain.setMediaClockDomainMaster(secondaryMasterIdKV.first);
							domainIndex = result.mediaClockDomains().size();
							mediaClockDomain.setDomainIndex(domainIndex.value());
							result.mediaClockDomains().emplace(domainIndex.value(), mediaClockDomain);
						}
						associatedDomains.push_back(domainIndex.value());
					}
				}
			}
			result.entityMediaClockMasterMappings().emplace(entityId, associatedDomains);
		}

		return result;
	}

private:
	/**
* Finds the index of the clock stream.
* @return If the clock stream index can not be determined,  std::numeric_limits<la::avdecc::entity::model::StreamIndex>().max() is returned.
*/
	std::optional<la::avdecc::entity::model::StreamIndex> findClockStreamIndex(la::avdecc::controller::model::ConfigurationNode const& configNode)
	{
		for (auto const& streamInputKV : configNode.streamInputs)
		{
			auto const& streamInput = streamInputKV.second;
			for (auto const& streamFormat : streamInput.staticModel->formats)
			{
				auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
				la::avdecc::entity::model::StreamFormatInfo::Type streamType = streamFormatInfo->getType();
				if (la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference == streamType)
				{
					return streamInputKV.first;
				}
			}
		}
		return std::nullopt;
	}

	/**
* Compares the current mc mapping state with the previous one and 
* emits the mediaClockConnectionsUpdate to inform the views about changes.
*/
	void notifyChanges()
	{
		// potenitally changes every other entity...
		std::vector<la::avdecc::UniqueIdentifier> changes;
		auto currentMCDomainMapping = createMediaClockDomainModel();
		for (auto const& entityDomainKV : currentMCDomainMapping.entityMediaClockMasterMappings())
		{
			la::avdecc::UniqueIdentifier const domainMasterId = entityDomainKV.first;
			std::vector<DomainIndex> domainIds = entityDomainKV.second;
			if (_currentMCDomainMapping.entityMediaClockMasterMappings().count(domainMasterId))
			{
				auto oldDomainIndexes = _currentMCDomainMapping.entityMediaClockMasterMappings().at(domainMasterId);
				if (oldDomainIndexes.size() > 0 && domainIds.size() > 0)
				{
					auto oldMaster = _currentMCDomainMapping.mediaClockDomains().at(oldDomainIndexes.at(0)); // the first index is the mc master
					auto newMaster = currentMCDomainMapping.mediaClockDomains().at(domainIds.at(0)); // the first index is the mc master
					if (oldMaster.mediaClockDomainMaster() != newMaster.mediaClockDomainMaster())
					{
						changes.push_back(domainMasterId);
					}
				}
				else if (oldDomainIndexes.size() != domainIds.size())
				{
					changes.push_back(domainMasterId);
				}
			}
			else
			{
				// if it wasn't there before, it changed.
				changes.push_back(entityDomainKV.first);
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
DomainIndex avdecc::mediaClock::MCDomain::domainIndex() const
{
	return _domainIndex;
}

/**
* Sets the domain index.
* @param domainIndex The index of this domain.
*/
void avdecc::mediaClock::MCDomain::setDomainIndex(DomainIndex domainIndex)
{
	_domainIndex = domainIndex;
}

/**
* Creates a name to display in the ui for this domain from the mc master id.
* @return The name to display.
*/
QString avdecc::mediaClock::MCDomain::displayName() const
{
	return QString("Domain ").append(_mcMaster ? helper::uniqueIdentifierToString(_mcMaster) : "-");
}

/**
* Gets the mc master of this domain.
* @return The mc master entity id.
*/
la::avdecc::UniqueIdentifier avdecc::mediaClock::MCDomain::mediaClockDomainMaster() const
{
	return _mcMaster;
}

/**
* Sets the media clock master id.
* @param entityId Entity id to set as mc master.
*/
void avdecc::mediaClock::MCDomain::setMediaClockDomainMaster(la::avdecc::UniqueIdentifier entityId)
{
	_mcMaster = entityId;
}

/**
* Gets the sampling rate.
* @param entityId Entity id to set as mc master.
*/
la::avdecc::entity::model::SamplingRate avdecc::mediaClock::MCDomain::domainSamplingRate() const
{
	return _domainSamplingRate;
}

/**
* Sets the sampling rate of this domain.
* @param entityId Entity id to set as mc master.
*/
void avdecc::mediaClock::MCDomain::setDomainSamplingRate(la::avdecc::entity::model::SamplingRate samplingRate)
{
	_domainSamplingRate = samplingRate;
}

/////////////////////////////////////////////////////////////////////////////////////////

/**
* Finds the corresponding domain index by an entity id (media clock master).
* @param mediaClockMasterId The media clock master id.
* @return The index of the domain which mc master matches the given id.
*/
DomainIndex const MCEntityDomainMapping::findDomainIndexByMasterEntityId(la::avdecc::UniqueIdentifier const mediaClockMasterId) noexcept
{
	for (auto const& mediaClockDomainKV : _mediaClockDomains)
	{
		if (mediaClockDomainKV.second.mediaClockDomainMaster() == mediaClockMasterId)
		{
			return mediaClockDomainKV.second.domainIndex();
		}
	}
	return -1;
}

/**
* Gets a reference of the entity to media clock index map.
* @return Reference of the _entityMediaClockMasterMappings field.
*/
std::map<la::avdecc::UniqueIdentifier, std::vector<DomainIndex>>& MCEntityDomainMapping::entityMediaClockMasterMappings()
{
	return _entityMediaClockMasterMappings;
}

/**
* Gets a reference of the media clock domain map.
* @return Reference of the _mediaClockDomains field.
*/
std::map<DomainIndex, avdecc::mediaClock::MCDomain>& MCEntityDomainMapping::mediaClockDomains()
{
	return _mediaClockDomains;
}

} // namespace mediaClock
} // namespace avdecc
