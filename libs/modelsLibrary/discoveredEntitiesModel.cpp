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

#include "hive/modelsLibrary/helper.hpp"
#include "hive/modelsLibrary/discoveredEntitiesModel.hpp"
#include "hive/modelsLibrary/controllerManager.hpp"

#include <QString>

#include <vector>
#include <optional>
#include <unordered_set>

namespace hive
{
namespace modelsLibrary
{
class DiscoveredEntitiesModel::pImpl : public QObject
{
public:
	pImpl(Model* const model, QObject* parent = nullptr)
		: QObject(parent)
		, _model(model)
	{
		// Connect hive::modelsLibrary::ControllerManager signals
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::controllerOffline, this, &pImpl::handleControllerOffline);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityOnline, this, &pImpl::handleEntityOnline);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityOffline, this, &pImpl::handleEntityOffline);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityRedundantInterfaceOnline, this, &pImpl::handleEntityRedundantInterfaceOnline);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityRedundantInterfaceOffline, this, &pImpl::handleEntityRedundantInterfaceOffline);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::unsolicitedRegistrationChanged, this, &pImpl::handleUnsolicitedRegistrationChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::compatibilityFlagsChanged, this, &pImpl::handleCompatibilityFlagsChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityCapabilitiesChanged, this, &pImpl::handleEntityCapabilitiesChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::associationIDChanged, this, &pImpl::handleAssociationIDChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::identificationStarted, this, &pImpl::handleIdentificationStarted);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::identificationStopped, this, &pImpl::handleIdentificationStopped);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityNameChanged, this, &pImpl::handleEntityNameChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityGroupNameChanged, this, &pImpl::handleEntityGroupNameChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::acquireStateChanged, this, &pImpl::handleAcquireStateChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::lockStateChanged, this, &pImpl::handleLockStateChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::gptpChanged, this, &pImpl::handleGptpChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::streamInputErrorCounterChanged, this, &pImpl::handleStreamInputErrorCounterChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::statisticsErrorCounterChanged, this, &pImpl::handleStatisticsErrorCounterChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::diagnosticsChanged, this, &pImpl::handleDiagnosticsChanged);
	}

	std::optional<std::reference_wrapper<Entity const>> entity(std::size_t const index) const noexcept
	{
		if (isIndexValid(index))
		{
			return std::cref(_entities[index]);
		}

		return {};
	}

	std::optional<std::reference_wrapper<Entity const>> entity(la::avdecc::UniqueIdentifier const& entityID) const noexcept
	{
		if (auto const index = indexOf(entityID))
		{
			return entity(*index);
		}

		return {};
	}

	std::size_t entitiesCount() const noexcept
	{
		return _entities.size();
	}

private:
	struct EntityWithErrorCounter
	{
		bool statisticsError{ false };
		std::set<la::avdecc::entity::model::StreamIndex> streamsWithErrorCounter{};
		std::set<la::avdecc::entity::model::StreamIndex> streamsWithLatencyError{};
		constexpr bool hasError() const noexcept
		{
			return statisticsError || !streamsWithErrorCounter.empty() || !streamsWithLatencyError.empty();
		}
	};

	using EntityRowMap = std::unordered_map<la::avdecc::UniqueIdentifier, std::size_t, la::avdecc::UniqueIdentifier::hash>;
	using EntitiesWithErrorCounter = std::unordered_map<la::avdecc::UniqueIdentifier, EntityWithErrorCounter, la::avdecc::UniqueIdentifier::hash>;

	ProtocolCompatibility computeProtocolCompatibility(std::optional<la::avdecc::entity::model::MilanInfo> const& milanInfo, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags)
	{
		if (compatibilityFlags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Misbehaving))
		{
			return ProtocolCompatibility::Misbehaving;
		}
		else if (compatibilityFlags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
		{
			auto const isRedundant = milanInfo && milanInfo->featuresFlags.test(la::avdecc::entity::MilanInfoFeaturesFlag::Redundancy);
			auto const isCertifiedV1 = milanInfo && milanInfo->certificationVersion >= 0x01000000;
			auto const isWarning = compatibilityFlags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::MilanWarning);

			if (isWarning)
			{
				return isRedundant ? ProtocolCompatibility::MilanWarningRedundant : ProtocolCompatibility::MilanWarning;
			}

			if (isCertifiedV1)
			{
				return isRedundant ? ProtocolCompatibility::MilanCertifiedRedundant : ProtocolCompatibility::MilanCertified;
			}

			return isRedundant ? ProtocolCompatibility::MilanRedundant : ProtocolCompatibility::Milan;
		}
		else if (compatibilityFlags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::IEEE17221))
		{
			return ProtocolCompatibility::IEEE;
		}

		return ProtocolCompatibility::NotCompliant;
	}

	inline bool isIndexValid(std::size_t const index) const noexcept
	{
		return index < _entities.size();
	}

	// Returns the entity index if found in the model
	std::optional<std::size_t> indexOf(la::avdecc::UniqueIdentifier const& entityID) const noexcept
	{
		// Check if the entity is still online
		if (auto const it = _entityRowMap.find(entityID); it != std::end(_entityRowMap))
		{
			return it->second;
		}
		return {};
	}

	// Build the entityID to row map
	void rebuildEntityRowMap() noexcept
	{
		_entityRowMap.clear();

		for (auto row = 0u; row < _entities.size(); ++row)
		{
			auto const& data = _entities[row];
			_entityRowMap.emplace(std::make_pair(data.entityID, row));
		}
	}

	// hive::modelsLibrary::ControllerManager
	void handleControllerOffline()
	{
		emit _model->beginResetModel();
		_entities.clear();
		_entityRowMap.clear();
		_entitiesWithErrorCounter.clear();
		_identifingEntities.clear();
		emit _model->endResetModel();
	}

	void handleEntityOnline(la::avdecc::UniqueIdentifier const& entityID)
	{
		try
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			// Check if the entity is actually online (it might happen that the entity quickly switched from online to offline)
			if (controlledEntity)
			{
				auto const& entity = *controlledEntity;
				auto const& e = entity.getEntity();
				auto firmwareUploadMemoryIndex = decltype(Entity::firmwareUploadMemoryIndex){ std::nullopt };
				auto firmwareVersion = decltype(Entity::firmwareVersion){ std::nullopt };

				// Build gptp info map
				auto gptpInfo = decltype(Entity::gptpInfo){};
				for (auto const& [avbInterfaceIndex, interfaceInformation] : e.getInterfacesInformation())
				{
					gptpInfo.insert(std::make_pair(avbInterfaceIndex, GptpInfo{ interfaceInformation.gptpGrandmasterID, interfaceInformation.gptpDomainNumber }));
				}

				// Check if AEM is supported
				auto isAemSupported = e.getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported);

				// Get some AEM specific information
				if (isAemSupported)
				{
					auto const& entityNode = entity.getEntityNode();
					if (entityNode.dynamicModel)
					{
						auto const& dynamicModel = *entityNode.dynamicModel;

						// Get firmware version
						firmwareVersion = dynamicModel.firmwareVersion.data();

						auto const configurationIndex = dynamicModel.currentConfiguration;
						auto const& configurationNode = entity.getConfigurationNode(configurationIndex);

						// Get Firmware Image MemoryIndex, if supported
						for (auto const& [memoryObjectIndex, memoryObjectNode] : configurationNode.memoryObjects)
						{
							if (memoryObjectNode.staticModel->memoryObjectType == la::avdecc::entity::model::MemoryObjectType::FirmwareImage)
							{
								firmwareUploadMemoryIndex = memoryObjectIndex;
								break;
							}
						}
					}
				}

				// Build a discovered entity
				auto discoveredEntity = Entity{ entityID, isAemSupported, e.getEntityModelID(), firmwareVersion, firmwareUploadMemoryIndex, entity.getMilanInfo(), helper::entityName(entity), helper::groupName(entity), entity.isSubscribedToUnsolicitedNotifications(), computeProtocolCompatibility(entity.getMilanInfo(), entity.getCompatibilityFlags()), e.getEntityCapabilities(), entity.getAcquireState(), entity.getOwningControllerID(), entity.getLockState(), entity.getLockingControllerID(), gptpInfo, e.getAssociationID() };

				// Insert at the end
				auto const row = _model->rowCount();
				emit _model->beginInsertRows({}, row, row);

				_entities.push_back(discoveredEntity);

				// Update the cache
				rebuildEntityRowMap();

				emit _model->endInsertRows();

				// Trigger Error Counters, Statistics and Diagnostics
				// TODO: Error Counters
				handleStatisticsErrorCounterChanged(entityID, manager.getStatisticsCounters(entityID));
				handleDiagnosticsChanged(entityID, manager.getDiagnostics(entityID));
			}
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	void handleEntityOffline(la::avdecc::UniqueIdentifier const& entityID)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			emit _model->beginRemoveRows({}, idx, idx);

			// Remove the entity from the model
			auto const it = std::next(std::begin(_entities), idx);
			_entities.erase(it);

			// Update the cache
			rebuildEntityRowMap();

			emit _model->endRemoveRows();
		}
	}

	void handleEntityRedundantInterfaceOnline(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::Entity::InterfaceInformation const& interfaceInfo)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			auto& info = data.gptpInfo[avbInterfaceIndex];

			info.grandmasterID = interfaceInfo.gptpGrandmasterID;
			info.domainNumber = interfaceInfo.gptpDomainNumber;

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::GptpInfo });
		}
	}

	void handleEntityRedundantInterfaceOffline(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			if (data.gptpInfo.erase(avbInterfaceIndex) == 1)
			{
				la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::GptpInfo });
			}
		}
	}

	void handleUnsolicitedRegistrationChanged(la::avdecc::UniqueIdentifier const& entityID, bool const isSubscribed)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			try
			{
				auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
				if (auto controlledEntity = manager.getControlledEntity(entityID))
				{
					data.isSubscribedToUnsol = isSubscribed;

					la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::SubscribedToUnsol });
				}
			}
			catch (...)
			{
				// Ignore
			}
		}
	}

	void handleCompatibilityFlagsChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			try
			{
				auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
				if (auto controlledEntity = manager.getControlledEntity(entityID))
				{
					data.milanInfo = controlledEntity->getMilanInfo();
					data.protocolCompatibility = computeProtocolCompatibility(data.milanInfo, compatibilityFlags);

					la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::Compatibility });
				}
			}
			catch (...)
			{
				// Ignore
			}
		}
	}

	void handleEntityCapabilitiesChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::EntityCapabilities const entityCapabilities)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			try
			{
				auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
				if (auto controlledEntity = manager.getControlledEntity(entityID))
				{
					data.entityCapabilities = entityCapabilities;

					la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::EntityCapabilities });
				}
			}
			catch (...)
			{
				// Ignore
			}
		}
	}

	void handleAssociationIDChanged(la::avdecc::UniqueIdentifier const& entityID, std::optional<la::avdecc::UniqueIdentifier> const associationID)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			try
			{
				auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
				if (auto controlledEntity = manager.getControlledEntity(entityID))
				{
					data.associationID = associationID;

					la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::AssociationID });
				}
			}
			catch (...)
			{
				// Ignore
			}
		}
	}

	void handleIdentificationStarted(la::avdecc::UniqueIdentifier const& entityID)
	{
		if (auto const index = indexOf(entityID))
		{
			_identifingEntities.insert(entityID);
			la::avdecc::utils::invokeProtectedMethod(&Model::identificationStarted, _model, *index);
		}
	}

	void handleIdentificationStopped(la::avdecc::UniqueIdentifier const& entityID)
	{
		if (auto const index = indexOf(entityID))
		{
			_identifingEntities.erase(entityID);
			la::avdecc::utils::invokeProtectedMethod(&Model::identificationStopped, _model, *index);
		}
	}

	void handleEntityNameChanged(la::avdecc::UniqueIdentifier const& entityID, QString const& entityName)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			data.name = entityName;

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::Name });
		}
	}

	void handleEntityGroupNameChanged(la::avdecc::UniqueIdentifier const& entityID, QString const& entityGroupName)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			data.groupName = entityGroupName;

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::GroupName });
		}
	}

	void handleAcquireStateChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			data.acquireState = acquireState;
			data.owningController = owningEntity;

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::AcquireState, Model::ChangedInfoFlag::OwningController });
		}
	}

	void handleLockStateChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			data.lockState = lockState;
			data.lockingController = lockingEntity;

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::LockedState, Model::ChangedInfoFlag::LockingController });
		}
	}

	void handleGptpChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			auto& info = data.gptpInfo[avbInterfaceIndex];

			info.grandmasterID = grandMasterID;
			info.domainNumber = grandMasterDomain;

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::GptpInfo });
		}
	}

	void handleStreamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, hive::modelsLibrary::ControllerManager::StreamInputErrorCounters const& errorCounters)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;

			if (!errorCounters.empty())
			{
				_entitiesWithErrorCounter[entityID].streamsWithErrorCounter.insert(descriptorIndex);
			}
			else
			{
				_entitiesWithErrorCounter[entityID].streamsWithErrorCounter.erase(descriptorIndex);
			}

			la::avdecc::utils::invokeProtectedMethod(&Model::entityErrorCountersChanged, _model, idx, Model::ChangedErrorCounterFlags{ Model::ChangedErrorCounterFlag::StreamInputCounters });
		}
	}

	void handleStatisticsErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::StatisticsErrorCounters const& errorCounters)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;

			_entitiesWithErrorCounter[entityID].statisticsError = !errorCounters.empty();

			la::avdecc::utils::invokeProtectedMethod(&Model::entityErrorCountersChanged, _model, idx, Model::ChangedErrorCounterFlags{ Model::ChangedErrorCounterFlag::Statistics });
		}
	}

	void handleDiagnosticsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::Diagnostics const& diagnostics)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;

			auto const wasError = !_entitiesWithErrorCounter[entityID].streamsWithLatencyError.empty();
			auto nowInError = false;

			// Clear previous streamsWithLatencyError values
			_entitiesWithErrorCounter[entityID].streamsWithLatencyError.clear();

			// Rebuild it entirely
			for (auto const& [streamIndex, isError] : diagnostics.streamInputOverLatency)
			{
				if (isError)
				{
					_entitiesWithErrorCounter[entityID].streamsWithLatencyError.insert(streamIndex);
					nowInError = true;
				}
			}

			if (wasError != nowInError)
			{
				la::avdecc::utils::invokeProtectedMethod(&Model::entityErrorCountersChanged, _model, idx, Model::ChangedErrorCounterFlags{ Model::ChangedErrorCounterFlag::StreamInputLatency });
			}
		}
	}

	// Private Members
	Model* _model{ nullptr };
	std::vector<Entity> _entities{};
	EntityRowMap _entityRowMap{};
	EntitiesWithErrorCounter _entitiesWithErrorCounter{};
	std::unordered_set<la::avdecc::UniqueIdentifier, la::avdecc::UniqueIdentifier::hash> _identifingEntities;
};

DiscoveredEntitiesModel::DiscoveredEntitiesModel(Model* const model, QObject* parent)
	: _pImpl{ std::make_unique<pImpl>(model, parent) }
{
}

DiscoveredEntitiesModel::~DiscoveredEntitiesModel() = default;

std::optional<std::reference_wrapper<DiscoveredEntitiesModel::Entity const>> DiscoveredEntitiesModel::entity(std::size_t const index) const noexcept
{
	return _pImpl->entity(index);
}

std::optional<std::reference_wrapper<DiscoveredEntitiesModel::Entity const>> DiscoveredEntitiesModel::entity(la::avdecc::UniqueIdentifier const& entityID) const noexcept
{
	return _pImpl->entity(entityID);
}

std::size_t DiscoveredEntitiesModel::entitiesCount() const noexcept
{
	return _pImpl->entitiesCount();
}

} // namespace modelsLibrary
} // namespace hive
