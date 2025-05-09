/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "mcDomainManager.hpp"
#include "helper.hpp"

#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <la/avdecc/internals/streamFormatInfo.hpp>

#include <atomic>
#include <optional>
#include <unordered_set>
#include <math.h>

namespace avdecc
{
namespace mediaClock
{
// **************************************************************
// class MCDomainManagerImpl
// **************************************************************
/**
* @brief    Implements various methods to:
*			- Determine media clock masters of entities.
*			- Create media clock domain models which hold enitity to mc master mappings.
*			- Adjust media clock related data on entities according to a given domain model.
*			- Emit signals to inform about changes related to mc master mappings.
*
* [@author  Marius Erlen]
* [@date    2018-10-04]
*/
class MCDomainManagerImpl final : public MCDomainManager
{
private:
	// Private members
	std::set<la::avdecc::UniqueIdentifier> _entities{}; // No lock required, only read/write in the UI thread
	MCEntityDomainMapping _currentMCDomainMapping{};
	commandChain::SequentialAsyncCommandExecuter _sequentialAcmpCommandExecuter{};

public:
	/**
	* Constructor. Sets up connections to update the internal model.
	*/
	MCDomainManagerImpl() noexcept
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		connect(&manager, &hive::modelsLibrary::ControllerManager::controllerOffline, this, &MCDomainManagerImpl::onControllerOffline);
		connect(&manager, &hive::modelsLibrary::ControllerManager::entityOnline, this, &MCDomainManagerImpl::onEntityOnline);
		connect(&manager, &hive::modelsLibrary::ControllerManager::entityOffline, this, &MCDomainManagerImpl::onEntityOffline);
		connect(&manager, &hive::modelsLibrary::ControllerManager::streamInputConnectionChanged, this, &MCDomainManagerImpl::onStreamInputConnectionChanged);
		connect(&manager, &hive::modelsLibrary::ControllerManager::clockSourceChanged, this, &MCDomainManagerImpl::onClockSourceChanged);
		connect(&manager, &hive::modelsLibrary::ControllerManager::entityNameChanged, this, &MCDomainManagerImpl::onEntityNameChanged);

		qRegisterMetaType<commandChain::CommandExecutionErrors>("CommandExecutionErrors");

		connect(&_sequentialAcmpCommandExecuter, &commandChain::SequentialAsyncCommandExecuter::completed, this,
			[this](commandChain::CommandExecutionErrors errors)
			{
				ApplyInfo info;
				info.entityApplyErrors = errors;
				emit applyMediaClockDomainModelFinished(info);
			});

		connect(&_sequentialAcmpCommandExecuter, &commandChain::SequentialAsyncCommandExecuter::progressUpdate, this,
			[this](size_t const completedCommands, size_t const totalCommands)
			{
				emit applyMediaClockDomainModelProgressUpdate(roundf(((float)completedCommands) / totalCommands * 100));
			});
	}

	~MCDomainManagerImpl() noexcept {}

private:
	/**
	* Gets the media clock master for an entity.
	* If no mc master is available, the corresponding error is returned in second std:pair element.
	* If mc is provided via external on the mc master, the ExternalClockSource error is set in addition to mc master id.
	* @param entityId	The id of the entity for which the mc master is requested.
	* @return A pair of an entity id and an error. Error identifies if an mc master could be determined (NoError and ExternalClockSource errors do provide a mc master).
	*/
	virtual std::pair<la::avdecc::UniqueIdentifier, McDeterminationError> getMediaClockMaster(la::avdecc::UniqueIdentifier const entityId) noexcept override
	{
		auto error = McDeterminationError::NoError;
		auto hasErrorIterator = _currentMCDomainMapping.getEntityMcErrors().find(entityId);
		if (hasErrorIterator != _currentMCDomainMapping.getEntityMcErrors().end())
		{
			// If the entityId delivered mc errors, we only can return immediately in case the error is NOT ExternalClockSource.
			// In case it is, we need to determine the id of the device that feeds the external clock into the domain as master id.
			if (hasErrorIterator->second != McDeterminationError::ExternalClockSource)
				return std::make_pair(hasErrorIterator->first, hasErrorIterator->second);
			else
				error = McDeterminationError::ExternalClockSource;
		}

		auto entityMcDomainIterator = _currentMCDomainMapping.getEntityMediaClockMasterMappings().find(entityId);
		if (entityMcDomainIterator != _currentMCDomainMapping.getEntityMediaClockMasterMappings().end())
		{
			auto const& associatedDomains = entityMcDomainIterator->second;
			if (associatedDomains.size() > 0)
			{
				auto mediaClockDomainIterator = _currentMCDomainMapping.getMediaClockDomains().find(associatedDomains.at(0));
				if (mediaClockDomainIterator != _currentMCDomainMapping.getMediaClockDomains().end())
				{
					return std::make_pair(mediaClockDomainIterator->second.getMediaClockDomainMaster(), error);
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
	* Gets the media clock master for an entity. This algortihm might take a while, depending on the chain length.
	* For quick access (and outside of this class) getMediaClockMaster can be used instead.
	*
	* Detailed algorithm description:
	* Determines the mc master by looping through the clock connections of entities until an entity is found, that is
	* it's own mc master (clock source set to internal) or an error occurs.
	* The next entity in the chain is found by traversing the stream connection which is set as clock source.
	* This method returns an error type along with an entity id.
	* The returned la::avdecc::UniqueIdentifier is either:
	*	- the found mc master entity id
	*	- the entity id where the chain ends, because it's clock source is set to external
	*	- la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), if the mc master cannot be determined (error case).
	* Errors can occur when:
	*	- Some entity in the chain cannot be found/accessed using the hive::modelsLibrary::ControllerManager instance.
	*	- Some entity in the chain does not have the AemSupported flag.
	*	- An exception occurs while accessing some data.
	*	- Some dynamic/static model is null.
	*	- The chain of entities is recursive.
	*
	* The searchForAdditionalConnectionsOfMCMaster parameter enables this method to search for secondary mc masters.
	* Secondary mc masters are defined as - mc masters of entities which are mc masters themself.
	* This is the special case, where an mc master also has a clock stream connection.
	*
	* @param entityID The id of the entity to search for the master.
	* @param searchForAdditionalConnectionsOfMCMaster Should be set to true, if the connected mc master is of interest although the given entity is it's own mc master.
	* @return A pair of an entity id and an error. Error identifies if an mc master could be determined.
	*/
	virtual std::pair<la::avdecc::UniqueIdentifier, McDeterminationError> findMediaClockMaster(la::avdecc::UniqueIdentifier const entityID, bool searchForSecondaryMcMaster = false) noexcept
	{
		auto error = McDeterminationError::UnknownEntity;
		// the map is used to keep track of the entities we already visited, to prevent running in circles
		std::unordered_set<la::avdecc::UniqueIdentifier, la::avdecc::UniqueIdentifier::hash> searchedEntityIds;
		auto currentEntityId = entityID;

		// insert the first entity in to the chain
		searchedEntityIds.insert(currentEntityId);
		bool keepSearching = true;
		while (keepSearching)
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto const& controlledEntity = manager.getControlledEntity(currentEntityId);
			if (!controlledEntity)
			{
				error = McDeterminationError::AnyEntityInChainOffline;
				keepSearching = false;
				break;
			}
			if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
			{
				keepSearching = false;
				error = McDeterminationError::NotSupportedNoAem;
				break;
			}

			try
			{
				auto const& configNode = controlledEntity->getCurrentConfigurationNode();
				auto const activeConfigIndex = configNode.descriptorIndex;

				// for now, we only support devices that have exactly 1 clock domain.
				if (configNode.clockDomains.size() > 1)
				{
					keepSearching = false;
					error = McDeterminationError::NotSupportedMultipleClockDomains;
					break;
				}
				else if (configNode.clockDomains.empty())
				{
					keepSearching = false;
					error = McDeterminationError::NotSupportedNoClockDomains;
					break;
				}

				// The first clock domain that can be used, will be used. All further clock domains are ignored.
				for (auto const& clockDomainKV : configNode.clockDomains)
				{
					auto const& clockDomain = clockDomainKV.second;
					auto clockSourceIndex = clockDomain.dynamicModel.clockSourceIndex;
					auto const& activeClockSourceNode = controlledEntity->getClockSourceNode(activeConfigIndex, clockSourceIndex);

					switch (activeClockSourceNode.staticModel.clockSourceType)
					{
						case la::avdecc::entity::model::ClockSourceType::Internal:
							if (!(searchForSecondaryMcMaster && entityID == currentEntityId))
							{
								return std::make_pair(currentEntityId, McDeterminationError::NoError);
							}
							break;
						case la::avdecc::entity::model::ClockSourceType::External:
							return std::make_pair(currentEntityId, McDeterminationError::ExternalClockSource);
						case la::avdecc::entity::model::ClockSourceType::InputStream:
							break;
						default:
							error = McDeterminationError::NotSupportedClockSourceType;
							keepSearching = false;
							return std::make_pair(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), error);
					}

					// find the relevant clock stream index
					std::optional<la::avdecc::entity::model::StreamIndex> clockStreamIndex = std::nullopt;
					if (activeClockSourceNode.staticModel.clockSourceLocationType == la::avdecc::entity::model::DescriptorType::StreamInput)
					{
						// In the case StreamInput as clockSourceLocationType we can get the relevant index directly from the static model
						clockStreamIndex = activeClockSourceNode.staticModel.clockSourceLocationIndex;
					}
					else if (activeClockSourceNode.staticModel.clockSourceType == la::avdecc::entity::model::ClockSourceType::Internal)
					{
						// In the case we are searching for a secondary master, we have to get the index by checking all streams if they are a CRF stream.
						auto indexes = findInputClockStreamIndexInConfiguration(configNode);
						if (!indexes.empty())
						{
							clockStreamIndex = indexes.at(0);
						}
					}

					if (clockStreamIndex)
					{
						auto& clockStreamDynModel = controlledEntity->getStreamInputNode(activeConfigIndex, *clockStreamIndex).dynamicModel;
						auto connectedTalker = clockStreamDynModel.connectionInfo.talkerStream.entityID;
						if (!connectedTalker)
						{
							error = searchedEntityIds.size() == 1 ? McDeterminationError::StreamNotConnected : McDeterminationError::ParentStreamNotConnected;
							keepSearching = false;
							return std::make_pair(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), error);
						}
						if (searchedEntityIds.count(connectedTalker))
						{
							// recusion of entity clock stream connections detected
							return std::make_pair(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), McDeterminationError::Recursive);
						}
						// set the next entity to traverse
						currentEntityId = connectedTalker;
						searchedEntityIds.insert(currentEntityId);
					}
					else
					{
						keepSearching = false;
						return std::make_pair(la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), error);
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
	* Builds a media clock mapping object from the current state. This mapping contains information about each entities media clock master.
	* The mappings are grouped by domains. A domain is a virtual construct, which has exactly one media clock master and n entities that get their
	* clock source from this master.
	*
	* Detailed algorithm description:
	* 1. Get the mc master of every enitity that is known and insert into the the mappings varibale.
	*	 The method getOrCreateDomainIndexForClockMasterId is used to create Domain objects and inserts
	*	 them into the domains variable. A domain contains domain specific data. See \link avdecc::mediaClock::MCDomain MCDomain class \endlink
	* 2. For every entity that is a mc master, a secondary master is tried to be determined.
	* 3. For every domain, the sampling rate is determined by getting the sample rates of all entities assigned to that domain.
	*	 If every entity inside the domain has the same sample rate, the domain sample rate is set to that rate.
	*	 If there are different sample rates or no sample rate, the domain sample rate is set to la::avdecc::entity::model::getNullSamplingRate().
	*
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

			// get mc master if there is one.
			auto mcMasterIdKV = findMediaClockMaster(entityId);
			auto const& mcMasterId = mcMasterIdKV.first;
			auto const mcMasterError = mcMasterIdKV.second;
			if (!mcMasterError)
			{
				auto const domainIndex = getOrCreateDomainIndexForClockMasterId(domains, mcMasterId);
				associatedDomains.push_back(domainIndex);
			}
			else if (mcMasterError == McDeterminationError::ExternalClockSource)
			{
				// in case the mc is provided via external input on mc master, we need to do both set the error and determine the mc master id /domain idx
				auto const domainIndex = getOrCreateDomainIndexForClockMasterId(domains, mcMasterId);
				associatedDomains.push_back(domainIndex);
				errors.emplace(entityId, mcMasterError);
			}
			else
			{
				// errors are stored as well for the findMediaClockMaster method
				errors.emplace(entityId, mcMasterError);
			}

			if (mcMasterId == entityId)
			{
				// get secondary mc master if there is one
				auto secondaryMasterIdKV = findMediaClockMaster(entityId, true);
				auto const& secondaryMasterId = secondaryMasterIdKV.first;
				auto const secondaryMasterError = secondaryMasterIdKV.second;
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

		// loop through all domains, then through all entities in that domain to get the domain sample rate.
		for (auto& domainKV : domains)
		{
			std::set<la::avdecc::entity::model::SamplingRate> sampleRates;
			for (auto const& entityIdKV : mappings)
			{
				for (auto const entityDomainIndex : entityIdKV.second)
				{
					if (domainKV.second.getDomainIndex() == entityDomainIndex)
					{
						auto sampleRate = getSampleRateOfEntity(entityIdKV.first);
						if (sampleRate)
						{
							sampleRates.insert(*sampleRate);
						}
					}
				}
			}
			if (sampleRates.size() > 1 || sampleRates.size() == 0)
			{
				domainKV.second.setDomainSamplingRate(la::avdecc::entity::model::SamplingRate::getNullSamplingRate());
			}
			else
			{
				domainKV.second.setDomainSamplingRate(*sampleRates.begin());
			}
		}

		return MCEntityDomainMapping{ std::move(mappings), std::move(domains), std::move(errors) };
	}

	/**
	* This method compares the current state with the given domain mapping object and attempts to change the configuration
	* of all entities to match the new mapping.
	*
	* Detailed algorithm description:
	* No change is executed directly. All changes are stored in commands and executed using the _sequentialAcmpCommandExecuter instance.
	* 1. All changes regarding sample rates are collected. To change the sample rate of an entity, one has to disconnect all streams of that entity first.
	*	 Therefor all sample rate changes are executed in a sequence of disconnection every stream, changing the sample rate, then reconnecting the streams.
	* 2. The mc stream connections that exist, that are no longer valid are removed.
	*	 When an entity is now in the unassigned list, it's clock source is set to external.
	* 3. All new mc stream connections needed to fullfil the new domain model are created.
	*    Also the clock sources are changed according to the new model in this step. Domain masters clock source is set to internal, domain slaves to input stream.
	*
	* @param domains The mapping to apply.
	*/
	virtual void applyMediaClockDomainModel(MCEntityDomainMapping const& domains) noexcept
	{
		// change all configurations according to the given model.
		// only apply changes

		auto oldDomainModel = createMediaClockDomainModel();
		MCEntityDomainMapping newDomainModel(domains);
		std::vector<commandChain::AsyncParallelCommandSet*> commands;

		// apply sample rates
		// this is done first, because otherwise changes would be overwritten.
		for (const auto& entityKV : newDomainModel.getEntityMediaClockMasterMappings())
		{
			auto const& entityId = entityKV.first;
			auto const& domainIndexes = entityKV.second;
			if (!domainIndexes.empty())
			{
				auto targetSampleRate = newDomainModel.getMediaClockDomains().find(domainIndexes.at(0))->second.getDomainSamplingRate();

				// adjust sampling rate if it changed and is set
				if (targetSampleRate && targetSampleRate != getSampleRateOfEntity(entityId))
				{
					// streams have to be disconnected before switching the sample rate:

					auto* commandsRemoveAllConnections = new commandChain::AsyncParallelCommandSet;
					auto outputStreamConnections = getAllStreamOutputConnections(entityId);
					auto inputStreamConnections = getAllStreamInputConnections(entityId);
					auto commandsRemoveOutputStreams = removeAllStreamOutputConnections(entityId, outputStreamConnections);
					commandsRemoveAllConnections->append(commandsRemoveOutputStreams);
					auto commandsRemoveInputStreams = removeAllStreamInputConnections(entityId, inputStreamConnections);
					commandsRemoveAllConnections->append(commandsRemoveInputStreams);

					auto* commandsSetSamplingRate = new commandChain::AsyncParallelCommandSet(adjustAudioUnitSampleRates(entityId, targetSampleRate));

					auto* commandsRestoreAllConnections = new commandChain::AsyncParallelCommandSet;
					auto commandsRestoreOutputStreams = restoreOutputStreamConnections(entityId, outputStreamConnections);
					commandsRestoreAllConnections->append(commandsRestoreOutputStreams);
					auto commandsRestoreInputStreams = restoreInputStreamConnections(entityId, inputStreamConnections);
					commandsRestoreAllConnections->append(commandsRestoreInputStreams);

					commands.push_back(commandsRemoveAllConnections);
					commands.push_back(commandsSetSamplingRate);
					commands.push_back(commandsRestoreAllConnections);
				}
			}
		}

		// disconnect
		auto* commandsRemoveOldMappingConnections = new commandChain::AsyncParallelCommandSet;
		for (const auto& entityKV : newDomainModel.getEntityMediaClockMasterMappings())
		{
			if (!(entityKV.second.empty() && oldDomainModel.getEntityMediaClockMasterMappings().find(entityKV.first)->second.empty()))
			{
				// check if they are different (by checking if the mc master is the same):
				for (auto domainIndexOld : oldDomainModel.getEntityMediaClockMasterMappings().find(entityKV.first)->second)
				{
					bool removed = true;
					for (auto domainIndexNew : newDomainModel.getEntityMediaClockMasterMappings().find(entityKV.first)->second)
					{
						if (newDomainModel.getMediaClockDomains().find(domainIndexNew)->second.getMediaClockDomainMaster() == oldDomainModel.getMediaClockDomains().find(domainIndexOld)->second.getMediaClockDomainMaster())
						{
							removed = false;
							break;
						}
					}

					if (removed)
					{
						if (entityKV.first != oldDomainModel.getMediaClockDomains().find(domainIndexOld)->second.getMediaClockDomainMaster())
						{
							// no longer existant, remove mc stream connection
							auto commandsRemoveClockStreamConnection = removeClockStreamConnection(oldDomainModel.getMediaClockDomains().find(domainIndexOld)->second.getMediaClockDomainMaster(), entityKV.first);
							commandsRemoveOldMappingConnections->append(commandsRemoveClockStreamConnection);
						}
						else
						{
							if (newDomainModel.getEntityMediaClockMasterMappings().find(entityKV.first)->second.size() == 1 && oldDomainModel.getEntityMediaClockMasterMappings().find(entityKV.first)->second.size() > 1)
							{
								// the entity is the mc master of the domain
								// set it's clock source to internal
								auto command = setEntityClockToCRFInputStream(entityKV.first, 0);
								commandsRemoveOldMappingConnections->append(command);
							}
						}
					}
				}

				// if it's unassigned now set the clock source to external
				if (newDomainModel.getEntityMediaClockMasterMappings().at(entityKV.first).empty())
				{
					auto command = setEntityClockToExternal(entityKV.first, 0);
					commandsRemoveOldMappingConnections->append(command);
				}
			}
		}
		commands.push_back(commandsRemoveOldMappingConnections);

		// connect
		auto* commandsSetupNewMappingConnections = new commandChain::AsyncParallelCommandSet;
		for (const auto& entityKV : newDomainModel.getEntityMediaClockMasterMappings())
		{
			for (auto domainIndexNew : entityKV.second)
			{
				bool added = true;
				for (auto domainIndexOld : oldDomainModel.getEntityMediaClockMasterMappings().find(entityKV.first)->second)
				{
					if (newDomainModel.getMediaClockDomains().find(domainIndexNew)->second.getMediaClockDomainMaster() == oldDomainModel.getMediaClockDomains().find(domainIndexOld)->second.getMediaClockDomainMaster())
					{
						added = false;
						break;
					}
				}

				if (added) // the entity was not in this domain before
				{
					if (newDomainModel.getMediaClockDomains().find(domainIndexNew)->second.getMediaClockDomainMaster() != entityKV.first)
					{
						if (newDomainModel.getEntityMediaClockMasterMappings().find(entityKV.first)->second.size() == 1)
						{
							// set the clock source to crf input stream for clock domain at index 0
							auto commandToExternal = setEntityClockToCRFInputStream(entityKV.first, 0);
							commandsSetupNewMappingConnections->append(commandToExternal);
						}

						// the entity is not the mc master
						// create a clock channel connection.
						auto commandsCreateConnection = createClockStreamConnection(newDomainModel.getMediaClockDomains().find(domainIndexNew)->second.getMediaClockDomainMaster(), entityKV.first);
						commandsSetupNewMappingConnections->append(commandsCreateConnection);
					}
					else
					{
						// the added entity is the mc master of the domain
						// set it's clock source to internal
						auto command = setEntityClockToInternal(entityKV.first, 0);
						commandsSetupNewMappingConnections->append(command);
					}
				}
			}
		}

		commands.push_back(commandsSetupNewMappingConnections);
		ApplyInfo info;
		// execute the command chain
		_sequentialAcmpCommandExecuter.setCommandChain(commands);
		_sequentialAcmpCommandExecuter.start();
	}

	/**
	* Checks if an entity can be used for media clock management.
	*/
	bool isMediaClockDomainManagementCompatible(la::avdecc::UniqueIdentifier const& entityId) noexcept
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const& controlledEntity = manager.getControlledEntity(entityId);
		if (!controlledEntity)
		{
			return false;
		}

		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
		{
			// no AEM support means not managable
			return false;
		}

		return true;
	}

	/**
	* Checks if an entity can not only be detected to belong to a mc domain,
	* but can be actively handled as well (= be connected to a MC domain
	* through stream connections, clock source modification, etc.).
	* E.g. a device that only receives a single audio stream and gets its mediaclocking through this,
	* it cannot be integrated into a MC domain by MCMD. Furthermore if a device has no AEM, it cannot
	* be regarded in the first place.
	*/
	bool isMediaClockDomainManageable(la::avdecc::UniqueIdentifier const& entityId) noexcept
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const& controlledEntity = manager.getControlledEntity(entityId);
		if (!controlledEntity)
		{
			return false;
		}

		if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
		{
			// no AEM support means not managable in the first place
			return false;
		}

		auto const& activeConfiguration{ controlledEntity->getCurrentConfigurationNode() };
		auto configuredClockSourceStreamIndex = getActiveInputClockStreamIndex(entityId);
		if (configuredClockSourceStreamIndex)
		{
			auto hasSingleNonredundantInputStream = (activeConfiguration.streamInputs.size() == 1 && activeConfiguration.redundantStreamInputs.empty());
			auto hasSingleRedundantInputStream = (activeConfiguration.redundantStreamInputs.size() == 1 && activeConfiguration.streamInputs.size() == 2);

			// A 'single input stream entity' can either have one redundant and no nonredundant or no redundant an one nonredundant input stream
			auto hasSingleInputStream = hasSingleNonredundantInputStream || hasSingleRedundantInputStream;

			// For both redundant and non redundant entities, the streamInputs map contains all streams that can potentially be used as clock source.
			auto usesStreamInputAsClock = (activeConfiguration.streamInputs.count(*configuredClockSourceStreamIndex));

			return !hasSingleInputStream || !usesStreamInputAsClock;
		}

		return false;
	}

	/**
	 * Checks if a given Media Clock Domain setup contains a sampling rate conflict
	 * with one of the stream formats of devices belonging to it.
	 * A Media Clock Domain setup that does not create conflicts is one that uses
	 * a sampling rate for each domain that matches the sampling rates in the stream formats
	 * configured for the streams of all devices belonging to that domain.
	 */
	bool isMediaClockDomainConflictingWithStreamFormats(avdecc::mediaClock::MCEntityDomainMapping const& domains) noexcept
	{
		MCEntityDomainMapping domainModel(domains);
		for (const auto& entityKV : domainModel.getEntityMediaClockMasterMappings())
		{
			auto const& entityId = entityKV.first;
			auto const& domainIndexes = entityKV.second;
			if (!domainIndexes.empty())
			{
				auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
				auto const controlledEntity = manager.getControlledEntity(entityId);
				if (controlledEntity)
				{
					if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
					{
						// If not even an AVB Entity Model is supported, a conflict cannot exist
						return false;
					}

					try
					{
						// get the 'media clock domain' pending sampling rate (change) from the domainModel
						auto const& mediaClockMasterSampleRate = domainModel.getMediaClockDomains().find(domainIndexes.at(0))->second.getDomainSamplingRate();

						auto configurationNode = controlledEntity->getCurrentConfigurationNode();
						for (auto const& streamInput : configurationNode.streamInputs)
						{
							// get the streamformat's corresponding sampling rate by first deriving the pull and base freq vals from streamformatinfo
							auto const& streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamInput.second.dynamicModel.streamFormat);
							if (la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference == streamFormatInfo->getType())
								continue;

							auto const& streamFormatSampleRate = streamFormatInfo->getSamplingRate();

							// if the 'media clock master' audioUnit SR and the streamFormat SR do not match, we have found a conflict that the user needs to be warned about
							if (mediaClockMasterSampleRate != streamFormatSampleRate)
								return true;
						}
						for (auto const& streamOutput : configurationNode.streamOutputs)
						{
							// get the streamformat's corresponding sampling rate by first deriving the pull and base freq vals from streamformatinfo
							auto const& streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamOutput.second.dynamicModel.streamFormat);
							if (la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference == streamFormatInfo->getType())
								continue;

							auto const& streamFormatSampleRate = streamFormatInfo->getSamplingRate();

							// if the 'media clock master' audioUnit SR and the streamFormat SR do not match, we have found a conflict that the user needs to be warned about
							if (mediaClockMasterSampleRate != streamFormatSampleRate)
								return true;
						}
					}
					catch (la::avdecc::controller::ControlledEntity::Exception const&)
					{
						// Ignore exception
					}
					catch (...)
					{
						// Uncaught exception
						AVDECC_ASSERT(false, "Uncaught exception");
					}
				}
			}
		}

		return false;
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
	bool checkMcMasterOfEntityChanged(std::vector<avdecc::mediaClock::DomainIndex> const& oldEntityDomainMapping, std::vector<avdecc::mediaClock::DomainIndex> const& newEntityDomainMapping, std::unordered_map<DomainIndex, MCDomain> const& oldMcDomains, std::unordered_map<DomainIndex, MCDomain> const& newMcDomains) noexcept
	{
		auto sizeOldDomainIndexes = oldEntityDomainMapping.size();
		auto sizeNewDomainIndexes = newEntityDomainMapping.size();
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
		else if ((sizeOldDomainIndexes == 0 && sizeNewDomainIndexes > 0) || (sizeOldDomainIndexes > 0 && sizeNewDomainIndexes == 0))
		{
			// if one of the lists is empty while the other isn't we have a change on the mc master. (Indeterminable -> ID or vice versa)
			return true;
		}
		return false;
	}

	/**
	* Changes the clock source configuration of an entity to an entry with InputStream type.
	* @param entityId Id of the entity.
	*/
	virtual std::optional<la::avdecc::entity::model::SamplingRate> getSampleRateOfEntity(la::avdecc::UniqueIdentifier const& entityIdSource)
	{
		auto controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(entityIdSource);
		if (controlledEntity)
		{
			try
			{
				auto& audioUnitDynModel = controlledEntity->getAudioUnitNode(controlledEntity->getCurrentConfigurationNode().descriptorIndex, 0).dynamicModel;
				return audioUnitDynModel.currentSamplingRate;
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}
		}
		return std::nullopt;
	}

	/**
	* Changes the clock source configuration of an entity to an entry with InputStream type.
	* @param entityId Id of the entity.
	*/
	virtual commandChain::AsyncParallelCommandSet::AsyncCommand setEntityClockToCRFInputStream(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::DescriptorIndex const clockDomainIndex) const noexcept
	{
		return [=](commandChain::AsyncParallelCommandSet* const parentCommandSet, int const commandIndex) -> bool
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityId);
			if (controlledEntity)
			{
				if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
				{
					return false;
				}
				try
				{
					auto const& configNode = controlledEntity->getCurrentConfigurationNode();

					for (auto const& clockSource : configNode.clockSources)
					{
						if (clockSource.second.staticModel.clockSourceType == la::avdecc::entity::model::ClockSourceType::InputStream && clockSource.second.staticModel.clockSourceLocationType == la::avdecc::entity::model::DescriptorType::StreamInput && isStreamInputOfType(entityId, clockSource.second.staticModel.clockSourceLocationIndex, la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference))
						{
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockSource);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							manager.setClockSource(entityId, clockDomainIndex, clockSource.first, nullptr, responseHandler);
							return true;
						}
					}
				}
				catch (la::avdecc::controller::ControlledEntity::Exception const&)
				{
				}
			}
			return false;
		};
	}

	/**
	* Changes the clock source configuration of an entity to an entry with InputStream type.
	* @param entityId Id of the entity.
	*/
	virtual commandChain::AsyncParallelCommandSet::AsyncCommand setEntityClockToExternal(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::DescriptorIndex const clockDomainIndex) const noexcept
	{
		return [=](commandChain::AsyncParallelCommandSet* const parentCommandSet, int const commandIndex) -> bool
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityId);
			if (controlledEntity)
			{
				if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
				{
					return false;
				}

				try
				{
					auto const& configNode = controlledEntity->getCurrentConfigurationNode();

					for (const auto& clockSource : configNode.clockSources)
					{
						if (clockSource.second.staticModel.clockSourceType == la::avdecc::entity::model::ClockSourceType::External)
						{
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockSource);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							manager.setClockSource(entityId, clockDomainIndex, clockSource.first, nullptr, responseHandler);
							return true;
						}
					}
				}
				catch (la::avdecc::controller::ControlledEntity::Exception const&)
				{
				}
			}
			return false;
		};
	}

	/**
	* Changes the clock source configuration of an entity to an entry with Internal type.
	* @param entityId Id of the entity.
	*/
	virtual commandChain::AsyncParallelCommandSet::AsyncCommand setEntityClockToInternal(la::avdecc::UniqueIdentifier const& entityId, la::avdecc::entity::model::DescriptorIndex const clockDomainIndex) const noexcept
	{
		return [=](commandChain::AsyncParallelCommandSet* const parentCommandSet, int const commandIndex) -> bool
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto const controlledEntity = manager.getControlledEntity(entityId);
			if (controlledEntity)
			{
				if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
				{
					return false;
				}
				try
				{
					auto const& configNode = controlledEntity->getCurrentConfigurationNode();

					for (auto const& clockSource : configNode.clockSources)
					{
						if (clockSource.second.staticModel.clockSourceType == la::avdecc::entity::model::ClockSourceType::Internal)
						{
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockSource);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							manager.setClockSource(entityId, clockDomainIndex, clockSource.first, nullptr, responseHandler);
							return true;
						}
					}
				}
				catch (la::avdecc::controller::ControlledEntity::Exception const&)
				{
				}
			}
			return false;
		};
	}

	/**
	* Creates a clock channel connection between two entities.
	* @param entityIdSource Id of the talker entity.
	* @param entityIdTarget Id of the listener entity.
	*/
	virtual std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> createClockStreamConnection(la::avdecc::UniqueIdentifier const& entityIdSource, la::avdecc::UniqueIdentifier const& entityIdTarget) const noexcept
	{
		std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> tasks;
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const controlledSourceEntity = manager.getControlledEntity(entityIdSource);
		auto const controlledTargetEntity = manager.getControlledEntity(entityIdTarget);
		if (controlledSourceEntity && controlledTargetEntity)
		{
			try
			{
				auto const& sourceConfigNode = controlledSourceEntity->getCurrentConfigurationNode();
				auto const& targetConfigNode = controlledTargetEntity->getCurrentConfigurationNode();
				auto const outputClockStreamIndexes = findOutputClockStreamIndexInConfiguration(sourceConfigNode);
				auto const inputClockStreamIndexes = findInputClockStreamIndexInConfiguration(targetConfigNode);
				if (!outputClockStreamIndexes.empty() && !inputClockStreamIndexes.empty())
				{
					// disconnect every connection (also redundant) that are used for clocking
					for (size_t i = 0; i < outputClockStreamIndexes.size(); i++)
					{
						if (i < inputClockStreamIndexes.size())
						{
							tasks.push_back(
								[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, int const commandIndex) -> bool
								{
									auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
									if (!doesStreamConnectionExist(entityIdSource, outputClockStreamIndexes.at(i), entityIdTarget, inputClockStreamIndexes.at(i)))
									{
										auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::entity::ControllerEntity::ControlStatus const status)
										{
											// notify SequentialAsyncCommandExecuter that the command completed.
											auto error = commandChain::AsyncParallelCommandSet::controlStatusToCommandError(status);
											if (error != commandChain::CommandExecutionError::NoError)
											{
												switch (status)
												{
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerMisbehaving:
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerUnknownID:
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerDestMacFail:
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoBandwidth:
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoStreamIndex:
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerExclusive:
														parentCommandSet->addErrorInfo(talkerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
														break;
													case la::avdecc::entity::LocalEntity::ControlStatus::ListenerMisbehaving:
													case la::avdecc::entity::LocalEntity::ControlStatus::ListenerUnknownID:
													case la::avdecc::entity::LocalEntity::ControlStatus::ListenerExclusive:
														parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
														break;
													default:
														parentCommandSet->addErrorInfo(talkerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
														parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
												}
											}
											parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
										};
										manager.connectStream(entityIdSource, outputClockStreamIndexes.at(i), entityIdTarget, inputClockStreamIndexes.at(i), responseHandler);
										return true;
									}
									return false;
								});
						}
					}
				}
				else
				{
					if (outputClockStreamIndexes.empty())
					{
						// Notify user about the error.
						tasks.push_back(
							[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, int const /*commandIndex*/) -> bool
							{
								parentCommandSet->addErrorInfo(entityIdSource, commandChain::CommandExecutionError::NoMediaClockOutputAvailable);
								return false;
							});
					}
					if (inputClockStreamIndexes.empty())
					{
						// Notify user about the error.
						tasks.push_back(
							[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, int const /*commandIndex*/) -> bool
							{
								parentCommandSet->addErrorInfo(entityIdTarget, commandChain::CommandExecutionError::NoMediaClockInputAvailable);
								return false;
							});
					}
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}
		}
		return tasks;
	}

	/**
	* Removes an existing clock channel connection between two entities.
	* @param entityIdSource Id of the talker entity.
	* @param entityIdTarget Id of the listener entity.
	*/
	virtual std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> removeClockStreamConnection(la::avdecc::UniqueIdentifier const& entityIdSource, la::avdecc::UniqueIdentifier const& entityIdTarget) const noexcept
	{
		std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> tasks;
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const controlledSourceEntity = manager.getControlledEntity(entityIdSource);
		auto const controlledTargetEntity = manager.getControlledEntity(entityIdTarget);
		if (controlledSourceEntity && controlledTargetEntity)
		{
			try
			{
				auto const& sourceConfigNode = controlledSourceEntity->getCurrentConfigurationNode();
				auto const& targetConfigNode = controlledTargetEntity->getCurrentConfigurationNode();
				auto const outputClockStreamIndexes = findOutputClockStreamIndexInConfiguration(sourceConfigNode);
				auto const inputClockStreamIndexes = findInputClockStreamIndexInConfiguration(targetConfigNode);
				if (!outputClockStreamIndexes.empty() && !inputClockStreamIndexes.empty())
				{
					// disconnect every connection (also redundant) that are used for clocking
					for (size_t i = 0; i < outputClockStreamIndexes.size(); i++)
					{
						if (i < inputClockStreamIndexes.size())
						{
							tasks.push_back(
								[=](commandChain::AsyncParallelCommandSet* parentCommandSet, uint32_t commandIndex) -> bool
								{
									// can connect the streams
									if (doesStreamConnectionExist(entityIdSource, outputClockStreamIndexes.at(i), entityIdTarget, inputClockStreamIndexes.at(i)))
									{
										auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
										auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::entity::ControllerEntity::ControlStatus const status)
										{
											// notify SequentialAsyncCommandExecuter that the command completed.
											auto error = commandChain::AsyncParallelCommandSet::controlStatusToCommandError(status);
											if (error != commandChain::CommandExecutionError::NoError)
											{
												switch (status)
												{
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerMisbehaving:
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerUnknownID:
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerDestMacFail:
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoBandwidth:
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoStreamIndex:
													case la::avdecc::entity::LocalEntity::ControlStatus::TalkerExclusive:
														parentCommandSet->addErrorInfo(talkerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
														break;
													case la::avdecc::entity::LocalEntity::ControlStatus::ListenerMisbehaving:
													case la::avdecc::entity::LocalEntity::ControlStatus::ListenerUnknownID:
													case la::avdecc::entity::LocalEntity::ControlStatus::ListenerExclusive:
														parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
														break;
													default:
														parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
														parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
												}
											}
											parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
										};

										manager.disconnectStream(entityIdSource, outputClockStreamIndexes.at(i), entityIdTarget, inputClockStreamIndexes.at(i), responseHandler);
										return true;
									}
									return false;
								});
						}
					}
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}
		}
		return tasks;
	}

	/**
	* Checks if an entities stream input is of the given type.
	* @param entityId The id of the entity to check.
	* @param streamIndex Index of the stream to check.
	* @return Returns true if the stream type is set to the given type.
	*/
	bool isStreamInputOfType(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormatInfo::Type const expectedStreamType) const noexcept
	{
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityId);
		if (controlledEntity)
		{
			if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
			{
				return false;
			}
			auto const& config = controlledEntity->getCurrentConfigurationNode();
			try
			{
				auto const& streamInput = controlledEntity->getStreamInputNode(config.descriptorIndex, streamIndex);

				auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamInput.dynamicModel.streamFormat);
				auto const streamType = streamFormatInfo->getType();
				if (expectedStreamType == streamType)
				{
					return true;
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}
		}
		return false;
	}

	/**
	* Exmines if a stream connection exists between 2 entities.
	* @param talkerEntityId The entity id with the outgoing stream connection.
	* @param talkerStreamIndex The index of the talker stream to check.
	* @param listenerEntityId The entity id that receives the stream.
	* @param listenerStreamIndex The index of the listener stream to check.
	* @return True if the connection exists.
	*/
	virtual bool doesStreamConnectionExist(la::avdecc::UniqueIdentifier const talkerEntityId, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityId, la::avdecc::entity::model::StreamIndex const listenerStreamIndex) const noexcept
	{
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const controlledListenerEntity = manager.getControlledEntity(listenerEntityId);
		if (controlledListenerEntity)
		{
			auto const& inputNode = controlledListenerEntity->getStreamInputNode(controlledListenerEntity->getCurrentConfigurationNode().descriptorIndex, listenerStreamIndex); // this doesn't work for redundant streams.
			auto const& connectionState = inputNode.dynamicModel.connectionInfo;
			if (connectionState.talkerStream.entityID == talkerEntityId && connectionState.talkerStream.streamIndex == talkerStreamIndex)
			{
				return true;
			}
		}
		return false;
	}

	/**
	* Determines the stream index (output) of the clock stream on the given configuration.
	* @param config The configuration node to search through.
	* @return		The index of the stream.
	*/
	virtual std::optional<la::avdecc::entity::model::StreamIndex> getActiveInputClockStreamIndex(la::avdecc::UniqueIdentifier const& entityId) const noexcept
	{
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityId);
		if (controlledEntity)
		{
			if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
			{
				return std::nullopt;
			}
			auto const& config = controlledEntity->getCurrentConfigurationNode();
			for (auto const& clockDomainKV : config.clockDomains)
			{
				auto const& clockDomain = clockDomainKV.second;
				auto const clockSourceIndex = clockDomain.dynamicModel.clockSourceIndex;
				auto const& activeClockSourceNode = controlledEntity->getClockSourceNode(config.descriptorIndex, clockSourceIndex);
				return activeClockSourceNode.staticModel.clockSourceLocationIndex;
			}
		}
		return std::nullopt;
	}


	/**
	* Determines the stream index (output) of the clock stream on the given configuration.
	* @param config The configuration node to search through.
	* @return		The index of the stream.
	*/
	virtual std::vector<la::avdecc::entity::model::StreamIndex> findOutputClockStreamIndexInConfiguration(la::avdecc::controller::model::ConfigurationNode const& config) const noexcept
	{
		std::vector<la::avdecc::entity::model::StreamIndex> streamIndexes;

		for (auto const& streamOutput : config.streamOutputs)
		{
			auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamOutput.second.dynamicModel.streamFormat);
			auto const streamType = streamFormatInfo->getType();
			if (la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference == streamType)
			{
				streamIndexes.push_back(streamOutput.first);
				return streamIndexes;
			}
		}

		// none could be found
		return streamIndexes;
	}

	/**
	* Determines the stream index (input) of the clock stream on the given configuration.
	* @param config The configuration node to search through.
	* @return		The index of the stream.
	*/
	virtual std::vector<la::avdecc::entity::model::StreamIndex> findInputClockStreamIndexInConfiguration(la::avdecc::controller::model::ConfigurationNode const& config) const noexcept
	{
		std::vector<la::avdecc::entity::model::StreamIndex> streamIndexes;

		for (auto const& streamInput : config.streamInputs)
		{
			auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamInput.second.dynamicModel.streamFormat);
			auto const streamType = streamFormatInfo->getType();
			if (la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference == streamType)
			{
				streamIndexes.push_back(streamInput.first);
				return streamIndexes;
			}
		}

		return streamIndexes;
	}

	/**
	* Determines if the gptp grandmaster of an entity matches it's mc masters gptp grandmaster.
	* @param entityId The id of the entity to check
	* @return True if in sync.
	*/
	virtual bool checkGPTPInSync(la::avdecc::UniqueIdentifier const entityId) noexcept
	{
		// get the mc clock connection of this entity, then check it's gptp mc id and compare it with the gptp id of the entity.
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();

		auto const talkerEntity = manager.getControlledEntity(findMediaClockMaster(entityId).first);
		auto const listenerEntity = manager.getControlledEntity(entityId);

		if (talkerEntity && listenerEntity)
		{
			auto& talkerEntityInfo = talkerEntity->getEntity();
			auto& listenerEntityInfo = listenerEntity->getEntity();

			auto const& listenerInterfaces = listenerEntityInfo.getInterfacesInformation();
			auto const& talkerInterfaces = talkerEntityInfo.getInterfacesInformation();
			bool allGPTPIdsMatch = true;
			for (auto const& talkerInterfaceInfoKV : talkerInterfaces)
			{
				bool gptpMathForTalker = false;
				for (auto const& listenerInterfaceInfoKV : listenerInterfaces)
				{
					if (listenerInterfaceInfoKV.second.gptpGrandmasterID == talkerInterfaceInfoKV.second.gptpGrandmasterID)
					{
						gptpMathForTalker = true;
					}
				}
				if (!gptpMathForTalker)
				{
					allGPTPIdsMatch = false;
					break;
				}
			}
			return allGPTPIdsMatch;
		}
		return false;
	}

	/**
	* Iterates over the list of known entities and returns all connections that originate from the given talker.
	*/
	std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> getAllStreamOutputConnections(la::avdecc::UniqueIdentifier const talkerEntityId)
	{
		auto disconnectedStreams = std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>>{};
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		for (auto const& potentialListenerEntityId : _entities)
		{
			auto const controlledEntity = manager.getControlledEntity(potentialListenerEntityId);
			if (controlledEntity)
			{
				if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
				{
					continue;
				}
				try
				{
					auto const& configNode = controlledEntity->getCurrentConfigurationNode();
					for (auto const& [streamIndex, streamInputNode] : configNode.streamInputs)
					{
						auto& streamInputDynamicModel = streamInputNode.dynamicModel;
						if (streamInputDynamicModel.connectionInfo.talkerStream.entityID == talkerEntityId)
						{
							disconnectedStreams.push_back({ { potentialListenerEntityId, streamIndex }, streamInputDynamicModel.connectionInfo });
						}
					}
				}
				catch (la::avdecc::controller::ControlledEntity::Exception const&)
				{
				}
			}
		}
		return disconnectedStreams;
	}

	/**
	* Gets all entities that have stream connection to the given listener entity.
	*/
	std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> getAllStreamInputConnections(la::avdecc::UniqueIdentifier const targetEntityId)
	{
		auto streamsToDisconnect = std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>>{};
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();

		auto const controlledEntity = manager.getControlledEntity(targetEntityId);
		if (controlledEntity)
		{
			if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
			{
				return streamsToDisconnect;
			}
			try
			{
				auto const& configNode = controlledEntity->getCurrentConfigurationNode();

				for (auto const& [streamIndex, streamInputNode] : configNode.streamInputs)
				{
					auto& streamInputDynModel = streamInputNode.dynamicModel;
					auto connectionState = streamInputDynModel.connectionInfo;
					if (connectionState.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected)
					{
						streamsToDisconnect.push_back({ { targetEntityId, streamIndex }, connectionState });
					}
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}
		}
		return streamsToDisconnect;
	}

	/**
	* Disconnects all output streams from an entity and returns a list of all streams that were disconnected.
	* @param entityId The id of the entity to disconnect the streams from.
	* @return A list of all streams that were disconnected.
	*/
	std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> removeAllStreamOutputConnections(la::avdecc::UniqueIdentifier const entityId, std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> const& connections)
	{
		std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commands;
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const controlledEntity = manager.getControlledEntity(entityId);
		if (controlledEntity)
		{
			for (auto const& [listenerStream, connectionInfo] : connections)
			{
				auto const& sourceEntityId = entityId;
				auto const& sourceStreamIndex = connectionInfo.talkerStream.streamIndex;
				auto const& targetEntityId = listenerStream.entityID;
				auto const& targetStreamIndex = listenerStream.streamIndex;
				commands.push_back(
					[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
					{
						auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
						if (doesStreamConnectionExist(sourceEntityId, sourceStreamIndex, targetEntityId, targetStreamIndex))
						{
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::entity::ControllerEntity::ControlStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::controlStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									switch (status)
									{
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerMisbehaving:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerUnknownID:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerDestMacFail:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoBandwidth:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoStreamIndex:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerExclusive:
											parentCommandSet->addErrorInfo(talkerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
											break;
										case la::avdecc::entity::LocalEntity::ControlStatus::ListenerMisbehaving:
										case la::avdecc::entity::LocalEntity::ControlStatus::ListenerUnknownID:
										case la::avdecc::entity::LocalEntity::ControlStatus::ListenerExclusive:
											parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
											break;
										default:
											parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
											parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
									}
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							manager.disconnectStream(sourceEntityId, sourceStreamIndex, targetEntityId, targetStreamIndex, responseHandler);
							return true;
						}
						return false;
					});
			}
		}
		return commands;
	}

	/**
	* Disconnects all input streams from an entity and returns a list of all streams that were disconnected.
	* @param entityId The id of the entity to disconnect the streams from.
	* @return A list of all streams that were disconnected.
	*/
	std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> removeAllStreamInputConnections(la::avdecc::UniqueIdentifier const entityId, std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> const& connections)
	{
		std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commands;
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const controlledEntity = manager.getControlledEntity(entityId);
		if (controlledEntity)
		{
			for (auto const& [listenerStream, connectionInfo] : connections)
			{
				auto const& sourceEntityId = connectionInfo.talkerStream.entityID;
				auto const& sourceStreamIndex = connectionInfo.talkerStream.streamIndex;
				auto const& targetEntityId = listenerStream.entityID;
				auto const& targetStreamIndex = listenerStream.streamIndex;
				commands.push_back(
					[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
					{
						auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
						if (doesStreamConnectionExist(sourceEntityId, sourceStreamIndex, targetEntityId, targetStreamIndex))
						{
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::entity::ControllerEntity::ControlStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::controlStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									switch (status)
									{
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerMisbehaving:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerUnknownID:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerDestMacFail:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoBandwidth:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoStreamIndex:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerExclusive:
											parentCommandSet->addErrorInfo(talkerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
											break;
										case la::avdecc::entity::LocalEntity::ControlStatus::ListenerMisbehaving:
										case la::avdecc::entity::LocalEntity::ControlStatus::ListenerUnknownID:
										case la::avdecc::entity::LocalEntity::ControlStatus::ListenerExclusive:
											parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
											break;
										default:
											parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
											parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream);
									}
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							manager.disconnectStream(sourceEntityId, sourceStreamIndex, targetEntityId, targetStreamIndex, responseHandler);
							return true;
						}
						return false;
					});
			}
		}
		return commands;
	}

	/**
	* Sets the sampling rate of all audio units in the active configuration of a device.
	* @param entityId The id of the entity to change.
	* @param sampleRate The sampling rate to apply.
	* @return The commands to be executed to apply the change.
	*/
	std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> adjustAudioUnitSampleRates(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::SamplingRate const sampleRate)
	{
		std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commands;
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const controlledEntity = manager.getControlledEntity(entityId);
		if (controlledEntity)
		{
			if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) || !controlledEntity->hasAnyConfiguration())
			{
				return commands;
			}
			try
			{
				auto const& audioUnits = controlledEntity->getCurrentConfigurationNode().audioUnits;
				for (auto const& audioUnitKV : audioUnits)
				{
					auto audioUnitIndex = audioUnitKV.first;
					commands.push_back(
						[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
						{
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::aemCommandStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									parentCommandSet->addErrorInfo(entityID, error, hive::modelsLibrary::ControllerManager::AecpCommandType::SetSamplingRate);
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
							manager.setAudioUnitSamplingRate(entityId, audioUnitIndex, sampleRate, nullptr, responseHandler);
							return true;
						});
				}
			}
			catch (la::avdecc::controller::ControlledEntity::Exception const&)
			{
			}
		}
		return commands;
	}

	/**
	* Creates all stream connections given in the parameters.
	* @param entityId The id of the entity to connect the streams from.
	* @param A list of all streams that shall be connected.
	*/
	std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> restoreOutputStreamConnections(la::avdecc::UniqueIdentifier entityId, std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> const& connections)
	{
		std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commands;
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const controlledEntity = manager.getControlledEntity(entityId);
		if (controlledEntity)
		{
			for (auto const& [listenerStream, connectionInfo] : connections)
			{
				auto const& sourceEntityId = entityId;
				auto const& sourceStreamIndex = connectionInfo.talkerStream.streamIndex;
				auto const& targetEntityId = listenerStream.entityID;
				auto const& targetStreamIndex = listenerStream.streamIndex;
				commands.push_back(
					[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
					{
						auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
						if (!doesStreamConnectionExist(sourceEntityId, sourceStreamIndex, targetEntityId, targetStreamIndex))
						{
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::entity::ControllerEntity::ControlStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::controlStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									switch (status)
									{
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerMisbehaving:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerUnknownID:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerDestMacFail:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoBandwidth:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoStreamIndex:
										case la::avdecc::entity::LocalEntity::ControlStatus::TalkerExclusive:
											parentCommandSet->addErrorInfo(talkerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
											break;
										case la::avdecc::entity::LocalEntity::ControlStatus::ListenerMisbehaving:
										case la::avdecc::entity::LocalEntity::ControlStatus::ListenerUnknownID:
										case la::avdecc::entity::LocalEntity::ControlStatus::ListenerExclusive:
											parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
											break;
										default:
											parentCommandSet->addErrorInfo(talkerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
											parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
									}
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							manager.connectStream(sourceEntityId, sourceStreamIndex, targetEntityId, targetStreamIndex, responseHandler);
							return true;
						}
						return false;
					});
			}
		}
		return commands;
	}

	/**
	* Creates all stream connections given in the parameters.
	* @param entityId The id of the entity to connect the streams from.
	* @param A list of all streams that shall be connected.
	*/
	std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> restoreInputStreamConnections(la::avdecc::UniqueIdentifier const entityId, std::vector<std::pair<la::avdecc::entity::model::StreamIdentification, la::avdecc::entity::model::StreamInputConnectionInfo>> const& connections)
	{
		std::vector<commandChain::AsyncParallelCommandSet::AsyncCommand> commands;
		auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const controlledEntity = manager.getControlledEntity(entityId);
		if (controlledEntity)
		{
			for (auto const& [listenerStream, connectionInfo] : connections)
			{
				auto const& sourceEntityId = connectionInfo.talkerStream.entityID;
				auto const& sourceStreamIndex = connectionInfo.talkerStream.streamIndex;
				auto const& targetEntityId = listenerStream.entityID;
				auto const& targetStreamIndex = listenerStream.streamIndex;
				commands.push_back(
					[=](commandChain::AsyncParallelCommandSet* const parentCommandSet, uint32_t const commandIndex) -> bool
					{
						auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
						if (!doesStreamConnectionExist(sourceEntityId, sourceStreamIndex, targetEntityId, targetStreamIndex))
						{
							auto responseHandler = [parentCommandSet, commandIndex](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const, la::avdecc::entity::ControllerEntity::ControlStatus const status)
							{
								// notify SequentialAsyncCommandExecuter that the command completed.
								auto error = commandChain::AsyncParallelCommandSet::controlStatusToCommandError(status);
								if (error != commandChain::CommandExecutionError::NoError)
								{
									if (error != commandChain::CommandExecutionError::NoError)
									{
										switch (status)
										{
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerMisbehaving:
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerUnknownID:
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerDestMacFail:
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoBandwidth:
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoStreamIndex:
											case la::avdecc::entity::LocalEntity::ControlStatus::TalkerExclusive:
												parentCommandSet->addErrorInfo(talkerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
												break;
											case la::avdecc::entity::LocalEntity::ControlStatus::ListenerMisbehaving:
											case la::avdecc::entity::LocalEntity::ControlStatus::ListenerUnknownID:
											case la::avdecc::entity::LocalEntity::ControlStatus::ListenerExclusive:
												parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
												break;
											default:
												parentCommandSet->addErrorInfo(talkerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
												parentCommandSet->addErrorInfo(listenerEntityID, error, hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream);
										}
									}
								}
								parentCommandSet->invokeCommandCompleted(commandIndex, error != commandChain::CommandExecutionError::NoError);
							};
							manager.connectStream(sourceEntityId, sourceStreamIndex, targetEntityId, targetStreamIndex, responseHandler);
							return true;
						}
						return false;
					});
			}
		}
		return commands;
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
		auto& currentMCDomains = currentMCDomainMapping.getMediaClockDomains();
		auto& previousMCDomainMapping = _currentMCDomainMapping;
		auto const& previousMCMasterMappings = previousMCDomainMapping.getEntityMediaClockMasterMappings();
		auto const& previousMCDomains = previousMCDomainMapping.getMediaClockDomains();
		auto const& previousErrors = previousMCDomainMapping.getEntityMcErrors();
		auto const& currentErrors = currentMCDomainMapping.getEntityMcErrors();

		for (auto const& entityDomainKV : currentMCDomainMapping.getEntityMediaClockMasterMappings())
		{
			auto const entityId = entityDomainKV.first;
			auto const oldDomainIndexesIterator = previousMCMasterMappings.find(entityId);
			if (oldDomainIndexesIterator != previousMCMasterMappings.end())
			{
				if (checkMcMasterOfEntityChanged(oldDomainIndexesIterator->second, entityDomainKV.second, previousMCDomains, currentMCDomains))
				{
					changes.push_back(entityId);
				}
				else if ((previousErrors.count(entityId) > 0 && currentErrors.count(entityId) > 0 && previousErrors.at(entityId) != currentErrors.at(entityId)) || previousErrors.count(entityId) != currentErrors.count(entityId))
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


	// Slots

	/**
	* Removes all entities from the internal list.
	*/
	void onControllerOffline()
	{
		_entities.clear();
		notifyChanges();
	}

	/**
	* Adds the entity to the internal list.
	*/
	void onEntityOnline(la::avdecc::UniqueIdentifier const& entityId)
	{
		// add entity to the set
		_entities.insert(entityId);
		notifyChanges();
	}

	/**
	* Removes the entity from the internal list.
	*/
	void onEntityOffline(la::avdecc::UniqueIdentifier const& entityId)
	{
		// remove entity from the set
		_entities.erase(entityId);
		notifyChanges();
	}

	/**
	* Handles the change of a clock source on a stream connection. Checks if the stream is a clock stream and if so emits the mediaClockConnectionsUpdate signal.
	*/
	void onStreamInputConnectionChanged(la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamInputConnectionInfo const& /*info*/)
	{
		auto affectsMcMaster = false;
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const& controlledEntity = manager.getControlledEntity(stream.entityID);
		if (controlledEntity)
		{
			if (controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) && controlledEntity->hasAnyConfiguration())
			{
				try
				{
					auto const& configNode = controlledEntity->getCurrentConfigurationNode();
					auto const activeConfigIndex = configNode.descriptorIndex;

					// find out if the stream connection is set as clock source:
					for (auto const& clockDomainKV : configNode.clockDomains)
					{
						auto const& clockDomain = clockDomainKV.second;
						auto const clockSourceIndex = clockDomain.dynamicModel.clockSourceIndex;
						auto const& activeClockSourceNode = controlledEntity->getClockSourceNode(activeConfigIndex, clockSourceIndex);

						switch (activeClockSourceNode.staticModel.clockSourceType)
						{
							case la::avdecc::entity::model::ClockSourceType::Internal:
								break;
							case la::avdecc::entity::model::ClockSourceType::External:
							case la::avdecc::entity::model::ClockSourceType::InputStream:
							{
								if (stream.streamIndex == activeClockSourceNode.staticModel.clockSourceLocationIndex)
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
	void onClockSourceChanged(la::avdecc::UniqueIdentifier const /*entityId*/, la::avdecc::entity::model::ClockDomainIndex const /*clockDomainIndex*/, la::avdecc::entity::model::ClockSourceIndex const /*clockSourceIndex*/)
	{
		notifyChanges();
	}

	/**
	* Handles the change of an entity name and determines if a mc master name is changed therefor.
	*/
	void onEntityNameChanged(la::avdecc::UniqueIdentifier const entityId, QString const& /*entityName*/)
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
	return QString("Domain ").append(_mediaClockMasterId ? hive::modelsLibrary::helper::uniqueIdentifierToString(_mediaClockMasterId) : "-");
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
* Sets the domain index of this domain.
*/
void MCDomain::setDomainIndex(DomainIndex const index) noexcept
{
	_domainIndex = index;
}

/**
* Sets the media clock master id.
* @param entityId Entity id to set as mc master.
*/
void MCDomain::setMediaClockDomainMaster(la::avdecc::UniqueIdentifier const entityId) noexcept
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
void MCDomain::setDomainSamplingRate(la::avdecc::entity::model::SamplingRate const samplingRate) noexcept
{
	_samplingRate = samplingRate;
}

/////////////////////////////////////////////////////////////////////////////////////////

/**
* Finds the corresponding domain index by an entity id (media clock master).
* @param mediaClockMasterId The media clock master id.
* @return The index of the domain which mc master matches the given id.
*/
std::optional<DomainIndex> const MCEntityDomainMapping::findDomainIndexByMasterEntityId(la::avdecc::UniqueIdentifier const mediaClockMasterId) noexcept
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
* Gets a reference of the entity to mc determination error map.
* @return Reference of the _entityMcErrors field.
*/
MCEntityDomainMapping::Errors& MCEntityDomainMapping::getEntityMcErrors() noexcept
{
	return _entityMcErrors;
}

/**
* Gets a reference of the entity to media clock index map.
* @return Reference of the _entityMediaClockMasterMappings field.
*/
MCEntityDomainMapping::Mappings& MCEntityDomainMapping::getEntityMediaClockMasterMappings() noexcept
{
	return _entityMediaClockMasterMappings;
}

/**
		* Gets a reference of the media clock domain map.
		* @return Reference of the _mediaClockDomains field.
		*/
MCEntityDomainMapping::Domains& MCEntityDomainMapping::getMediaClockDomains() noexcept
{
	return _mediaClockDomains;
}


} // namespace mediaClock
} // namespace avdecc
