/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::clockSourceNameChanged, this, &pImpl::handleClockSourceNameChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::acquireStateChanged, this, &pImpl::handleAcquireStateChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::lockStateChanged, this, &pImpl::handleLockStateChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::gptpChanged, this, &pImpl::handleGptpChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::streamInputErrorCounterChanged, this, &pImpl::handleStreamInputErrorCounterChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::statisticsErrorCounterChanged, this, &pImpl::handleStatisticsErrorCounterChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::diagnosticsChanged, this, &pImpl::handleDiagnosticsChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::mediaClockChainChanged, this, &pImpl::handleMediaClockChainChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::clockDomainCountersChanged, this, &pImpl::handleClockDomainCountersChanged);
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

	std::size_t entitiesCount() const noexcept
	{
		return _entities.size();
	}

private:
	using EntityRowMap = std::unordered_map<la::avdecc::UniqueIdentifier, std::size_t, la::avdecc::UniqueIdentifier::hash>;

	static inline ExclusiveAccessState getExclusiveState(la::avdecc::controller::model::LockState state) noexcept
	{
		switch (state)
		{
			case la::avdecc::controller::model::LockState::NotSupported:
				return ExclusiveAccessState::NotSupported;
			case la::avdecc::controller::model::LockState::Locked:
				return ExclusiveAccessState::AccessSelf;
			case la::avdecc::controller::model::LockState::LockedByOther:
				return ExclusiveAccessState::AccessOther;
			default:
				return ExclusiveAccessState::NoAccess;
		}
	}

	ProtocolCompatibility computeProtocolCompatibility(std::optional<la::avdecc::entity::model::MilanInfo> const& milanInfo, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags) noexcept
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
			auto const isWarning = compatibilityFlags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::IEEE17221Warning);
			return isWarning ? ProtocolCompatibility::IEEEWarning : ProtocolCompatibility::IEEE;
		}

		return ProtocolCompatibility::NotCompliant;
	}

	static inline ExclusiveAccessInfo computeExclusiveInfo(bool const isAemSupported, la::avdecc::controller::model::AcquireState state, la::avdecc::UniqueIdentifier const owner) noexcept
	{
		auto eai = ExclusiveAccessInfo{};

		if (!isAemSupported)
		{
			eai.state = ExclusiveAccessState::NotSupported;
			eai.tooltip = "AEM Not Supported";
		}
		else
		{
			eai.exclusiveID = owner;
			switch (state)
			{
				case la::avdecc::controller::model::AcquireState::Undefined:
					eai.state = ExclusiveAccessState::NoAccess;
					eai.tooltip = "Undefined";
					break;
				case la::avdecc::controller::model::AcquireState::NotSupported:
					eai.state = ExclusiveAccessState::NotSupported;
					eai.tooltip = "Not Supported";
					break;
				case la::avdecc::controller::model::AcquireState::NotAcquired:
					eai.state = ExclusiveAccessState::NoAccess;
					eai.tooltip = "Not Acquired";
					break;
				case la::avdecc::controller::model::AcquireState::AcquireInProgress:
					eai.state = ExclusiveAccessState::NoAccess;
					eai.tooltip = "Acquire In Progress";
					break;
				case la::avdecc::controller::model::AcquireState::Acquired:
					eai.state = ExclusiveAccessState::AccessSelf;
					eai.tooltip = "Acquired";
					break;
				case la::avdecc::controller::model::AcquireState::AcquiredByOther:
				{
					eai.state = ExclusiveAccessState::AccessOther;
					auto text = QString{ "Acquired by " };
					auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
					auto const& controllerEntity = controllerManager.getControlledEntity(owner);
					if (controllerEntity)
					{
						text += hive::modelsLibrary::helper::smartEntityName(*controllerEntity);
					}
					else
					{
						text += hive::modelsLibrary::helper::uniqueIdentifierToString(owner);
					}
					eai.tooltip = text;
					break;
				}
				case la::avdecc::controller::model::AcquireState::ReleaseInProgress:
					eai.state = ExclusiveAccessState::NoAccess;
					eai.tooltip = "Release In Progress";
					break;
				default:
					AVDECC_ASSERT(false, "Not handled!");
					eai.state = ExclusiveAccessState::NoAccess;
					eai.tooltip = "Not Supported";
					break;
			}
		}

		return eai;
	}

	static inline ExclusiveAccessInfo computeExclusiveInfo(bool const isAemSupported, la::avdecc::controller::model::LockState state, la::avdecc::UniqueIdentifier const owner) noexcept
	{
		auto eai = ExclusiveAccessInfo{};

		if (!isAemSupported)
		{
			eai.state = ExclusiveAccessState::NotSupported;
			eai.tooltip = "AEM Not Supported";
		}
		else
		{
			eai.exclusiveID = owner;
			switch (state)
			{
				case la::avdecc::controller::model::LockState::Undefined:
					eai.state = ExclusiveAccessState::NoAccess;
					eai.tooltip = "Undefined";
					break;
				case la::avdecc::controller::model::LockState::NotSupported:
					eai.state = ExclusiveAccessState::NotSupported;
					eai.tooltip = "Not Supported";
					break;
				case la::avdecc::controller::model::LockState::NotLocked:
					eai.state = ExclusiveAccessState::NoAccess;
					eai.tooltip = "Not Locked";
					break;
				case la::avdecc::controller::model::LockState::LockInProgress:
					eai.state = ExclusiveAccessState::NoAccess;
					eai.tooltip = "Lock In Progress";
					break;
				case la::avdecc::controller::model::LockState::Locked:
					eai.state = ExclusiveAccessState::AccessSelf;
					eai.tooltip = "Locked";
					break;
				case la::avdecc::controller::model::LockState::LockedByOther:
				{
					eai.state = ExclusiveAccessState::AccessOther;
					auto text = QString{ "Locked by " };
					auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
					auto const& controllerEntity = controllerManager.getControlledEntity(owner);
					if (controllerEntity)
					{
						text += hive::modelsLibrary::helper::smartEntityName(*controllerEntity);
					}
					else
					{
						text += hive::modelsLibrary::helper::uniqueIdentifierToString(owner);
					}
					eai.tooltip = text;
					break;
				}
				case la::avdecc::controller::model::LockState::UnlockInProgress:
					eai.state = ExclusiveAccessState::NoAccess;
					eai.tooltip = "Unlock In Progress";
					break;
				default:
					AVDECC_ASSERT(false, "Not handled!");
					eai.state = ExclusiveAccessState::NoAccess;
					eai.tooltip = "Not Supported";
					break;
			}
		}

		return eai;
	}

	MediaClockReference computeMediaClockReference(la::avdecc::controller::model::MediaClockChain const& mcChain) noexcept
	{
		auto mcr = MediaClockReference{ {}, "N/A", "N/A", false };
		auto const getClockName = [](auto const* const entity, auto const clockSourceIndex)
		{
			try
			{
				auto const currentConfigIndex = entity->getCurrentConfigurationIndex();
				auto const& sourceNode = entity->getClockSourceNode(currentConfigIndex, clockSourceIndex);
				return helper::objectName(entity, currentConfigIndex, sourceNode);
			}
			catch (...)
			{
			}
			return QString{};
		};

		// Always save the chain
		mcr.mcChain = mcChain;

		if (!mcChain.empty())
		{
			auto const chainSize = mcChain.size();

			// We only need to check the last node
			auto const& mccNode = mcChain.back();

			// Set the referenceID
			mcr.referenceIDString = hive::modelsLibrary::helper::uniqueIdentifierToString(mccNode.entityID);

			switch (mccNode.status)
			{
				case la::avdecc::controller::model::MediaClockChainNode::Status::Active:
				{
					// Get the reference ID
					switch (mccNode.type)
					{
						case la::avdecc::controller::model::MediaClockChainNode::Type::Undefined:
							AVDECC_ASSERT(false, "Should not be possible to have an end of chain Active and Undefined");
							mcr.referenceStatus = "Error Undefined, please report";
							break;
						case la::avdecc::controller::model::MediaClockChainNode::Type::Internal:
						case la::avdecc::controller::model::MediaClockChainNode::Type::External:
						{
							auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
							auto mcRefName = QString{};
							auto clockName = QString{};

							if (auto const clockRefEntity = manager.getControlledEntity(mccNode.entityID))
							{
								if (chainSize > 1)
								{
									mcRefName = hive::modelsLibrary::helper::entityName(*clockRefEntity) + " ";
								}
								clockName = getClockName(clockRefEntity.get(), mccNode.clockSourceIndex);
								if (clockName.isEmpty())
								{
									clockName = mccNode.type == la::avdecc::controller::model::MediaClockChainNode::Type::Internal ? "Internal" : "External";
								}
							}
							mcr.referenceStatus = QString("%1[%2] %3").arg(mcRefName).arg(mccNode.type == la::avdecc::controller::model::MediaClockChainNode::Type::Internal ? "I" : "E").arg(clockName);
							break;
						}
						case la::avdecc::controller::model::MediaClockChainNode::Type::StreamInput:
							AVDECC_ASSERT(false, "Should not be possible to have an end of chain Active and StreamInput");
							mcr.referenceStatus = "Error StreamInput, please report";
							break;
						default:
							break;
					}
					break;
				}
				case la::avdecc::controller::model::MediaClockChainNode::Status::Recursive:
					mcr.referenceIDString = "Recursive";
					mcr.referenceStatus = "Recursive";
					mcr.isError = true;
					break;
				case la::avdecc::controller::model::MediaClockChainNode::Status::StreamNotConnected:
					mcr.referenceStatus = "Stream N/C";
					mcr.isError = true;
					break;
				case la::avdecc::controller::model::MediaClockChainNode::Status::EntityOffline:
					mcr.referenceStatus = "Talker Offline";
					mcr.isError = true;
					break;
				case la::avdecc::controller::model::MediaClockChainNode::Status::UnsupportedClockSource:
					mcr.referenceStatus = "Unsupported CS";
					mcr.isError = true;
					break;
				case la::avdecc::controller::model::MediaClockChainNode::Status::AemError:
					mcr.referenceStatus = "AEM Error";
					mcr.isError = true;
					break;
				case la::avdecc::controller::model::MediaClockChainNode::Status::InternalError:
					mcr.referenceStatus = "Internal Error";
					mcr.isError = true;
					break;
				default:
					mcr.referenceStatus = "Error Unhandled, please report";
					mcr.isError = true;
					break;
			}
		}

		return mcr;
	}

	inline bool isIndexValid(std::size_t const index) const noexcept
	{
		return index < _entities.size();
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
				auto mediaClockReferences = decltype(Entity::mediaClockReferences){};
				auto clockDomainInfo = decltype(Entity::clockDomainInfo){};

				// Build gptp info and macAddresses maps
				auto gptpInfo = decltype(Entity::gptpInfo){};
				auto macAddresses = decltype(Entity::macAddresses){};
				for (auto const& [avbInterfaceIndex, interfaceInformation] : e.getInterfacesInformation())
				{
					gptpInfo.insert(std::make_pair(avbInterfaceIndex, GptpInfo{ interfaceInformation.gptpGrandmasterID, interfaceInformation.gptpDomainNumber }));
					macAddresses.insert(std::make_pair(avbInterfaceIndex, interfaceInformation.macAddress));
				}

				// Check if AEM is supported
				auto const isAemSupported = e.getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported);
				auto const hasAnyConfiguration = entity.hasAnyConfiguration();

				// Get some AEM specific information
				if (isAemSupported)
				{
					// Get firmware version
					auto const& entityNode = entity.getEntityNode();
					auto const& dynamicModel = entityNode.dynamicModel;

					// Get firmware version
					firmwareVersion = dynamicModel.firmwareVersion.data();

					if (hasAnyConfiguration)
					{
						auto const& configurationNode = entity.getCurrentConfigurationNode();

						// Get Firmware Image MemoryIndex, if supported
						for (auto const& [memoryObjectIndex, memoryObjectNode] : configurationNode.memoryObjects)
						{
							if (memoryObjectNode.staticModel.memoryObjectType == la::avdecc::entity::model::MemoryObjectType::FirmwareImage)
							{
								firmwareUploadMemoryIndex = memoryObjectIndex;
								break;
							}
						}

						// Build MediaClockReferences map
						for (auto const& [cdIndex, cdNode] : configurationNode.clockDomains)
						{
							mediaClockReferences.insert(std::make_pair(cdIndex, computeMediaClockReference(cdNode.mediaClockChain)));
						}

						// Get ClockDomain Locked status
						if (!configurationNode.clockDomains.empty())
						{
							auto const& clockDomainNode = configurationNode.clockDomains.begin()->second;
							if (clockDomainNode.dynamicModel.counters)
							{
								clockDomainInfo = computeClockDomainInfo(*(clockDomainNode.dynamicModel.counters));
							}
						}
					}
				}

				// Get diagnostics and errors
				auto const statisticsCounters = manager.getStatisticsCounters(entityID);
				auto const diagnostics = manager.getDiagnostics(entityID);

				// Build a discovered entity
				auto discoveredEntity = Entity{ entityID, isAemSupported, hasAnyConfiguration, entity.isVirtual(), entity.areUnsolicitedNotificationsSupported(), e.getEntityModelID(), firmwareVersion, firmwareUploadMemoryIndex, entity.getMilanInfo(), std::move(macAddresses), helper::entityName(entity), helper::groupName(entity), entity.isSubscribedToUnsolicitedNotifications(), computeProtocolCompatibility(entity.getMilanInfo(), entity.getCompatibilityFlags()), e.getEntityCapabilities(), computeExclusiveInfo(isAemSupported && hasAnyConfiguration, entity.getAcquireState(), entity.getOwningControllerID()), computeExclusiveInfo(isAemSupported && hasAnyConfiguration, entity.getLockState(), entity.getLockingControllerID()), std::move(gptpInfo), e.getAssociationID(), std::move(mediaClockReferences),
					entity.isIdentifying(), !statisticsCounters.empty(), diagnostics.redundancyWarning, std::move(clockDomainInfo), {}, diagnostics.streamInputOverLatency, diagnostics.controlCurrentValueOutOfBounds };

				// Insert at the end
				auto const row = _model->rowCount();
				emit _model->beginInsertRows({}, row, row);

				_entities.push_back(discoveredEntity);

				// Update the cache
				rebuildEntityRowMap();

				emit _model->endInsertRows();
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

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::GrandmasterID, Model::ChangedInfoFlag::GPTPDomain, Model::ChangedInfoFlag::InterfaceIndex });
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
				la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::GrandmasterID, Model::ChangedInfoFlag::GPTPDomain, Model::ChangedInfoFlag::InterfaceIndex });
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
			auto const idx = *index;
			auto& data = _entities[idx];

			try
			{
				auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
				if (auto controlledEntity = manager.getControlledEntity(entityID))
				{
					data.isIdentifying = true;
					la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::Identification });
				}
			}
			catch (...)
			{
				// Ignore
			}
		}
	}

	void handleIdentificationStopped(la::avdecc::UniqueIdentifier const& entityID)
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
					data.isIdentifying = false;
					la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::Identification });
				}
			}
			catch (...)
			{
				// Ignore
			}
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

		// Check all entities for a change in a Media Clock Reference
		auto const length = _entities.size();
		for (auto idx = 0u; idx < length; ++idx)
		{
			auto& data = _entities[idx];

			for (auto const& [cdIndex, mcr] : data.mediaClockReferences)
			{
				if (!mcr.mcChain.empty() && mcr.mcChain.back().entityID == entityID)
				{
					// Recompute the status
					data.mediaClockReferences[cdIndex] = computeMediaClockReference(mcr.mcChain);

					// And notify
					la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::MediaClockReferenceName });
				}
			}
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

	void handleClockSourceNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const /*configurationIndex*/, la::avdecc::entity::model::ClockSourceIndex const /*clockSourceIndex*/, QString const& /*clockSourceName*/)
	{
		// Check all entities for a change in a Media Clock Reference
		auto const length = _entities.size();
		for (auto idx = 0u; idx < length; ++idx)
		{
			auto& data = _entities[idx];

			for (auto const& [cdIndex, mcr] : data.mediaClockReferences)
			{
				if (!mcr.mcChain.empty() && mcr.mcChain.back().entityID == entityID)
				{
					// Recompute the status
					data.mediaClockReferences[cdIndex] = computeMediaClockReference(mcr.mcChain);

					// And notify
					la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::MediaClockReferenceName });
				}
			}
		}
	}

	void handleAcquireStateChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			data.acquireInfo = computeExclusiveInfo(data.isAemSupported && data.hasAnyConfigurationTree, acquireState, owningEntity);

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::AcquireState, Model::ChangedInfoFlag::OwningController });
		}
	}

	void handleLockStateChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			data.lockInfo = computeExclusiveInfo(data.isAemSupported && data.hasAnyConfigurationTree, lockState, lockingEntity);

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

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::GrandmasterID, Model::ChangedInfoFlag::GPTPDomain });
		}
	}

	void handleStreamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, hive::modelsLibrary::ControllerManager::StreamInputErrorCounters const& errorCounters)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			if (!errorCounters.empty())
			{
				data.streamsWithErrorCounter.insert(descriptorIndex);
			}
			else
			{
				data.streamsWithErrorCounter.erase(descriptorIndex);
			}

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::StreamInputCountersError });
		}
	}

	void handleStatisticsErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::StatisticsErrorCounters const& errorCounters)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			data.hasStatisticsError = !errorCounters.empty();

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::StatisticsError });
		}
	}

	void handleDiagnosticsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::Diagnostics const& diagnostics)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			auto const wasRedundancyWarning = data.hasRedundancyWarning;
			auto const wasStreamInputLatencyError = !data.streamsWithLatencyError.empty();
			auto const wasControlValueOutOfBounds = !data.controlsWithOutOfBoundsValue.empty();

			auto nowRedundancyWarning = false;
			auto nowStreamInputLatencyError = false;
			auto nowControlValueOutOfBounds = false;

			// Redundancy Warning
			{
				data.hasRedundancyWarning = diagnostics.redundancyWarning;
				nowRedundancyWarning = diagnostics.redundancyWarning;
			}

			// Stream Input Latency Error
			{
				data.streamsWithLatencyError = diagnostics.streamInputOverLatency;
				nowStreamInputLatencyError = !diagnostics.streamInputOverLatency.empty();
			}

			// Control OutOfBounds Value Error
			{
				data.controlsWithOutOfBoundsValue = diagnostics.controlCurrentValueOutOfBounds;
				nowControlValueOutOfBounds = !diagnostics.controlCurrentValueOutOfBounds.empty();
			}

			// Check what changed
			auto changedFlags = Model::ChangedInfoFlags{};
			if (wasRedundancyWarning != nowRedundancyWarning)
			{
				changedFlags.set(Model::ChangedInfoFlag::RedundancyWarning);
			}
			if (wasStreamInputLatencyError != nowStreamInputLatencyError)
			{
				changedFlags.set(Model::ChangedInfoFlag::StreamInputLatencyError);
			}
			if (wasControlValueOutOfBounds != nowControlValueOutOfBounds)
			{
				changedFlags.set(Model::ChangedInfoFlag::ControlValueOutOfBoundsError);
			}

			// Notify if something changed
			if (!changedFlags.empty())
			{
				la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, changedFlags);
			}
		}
	}

	void handleMediaClockChainChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::controller::model::MediaClockChain const& mcChain)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			data.mediaClockReferences[clockDomainIndex] = computeMediaClockReference(mcChain);

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::MediaClockReferenceID, Model::ChangedInfoFlag::MediaClockReferenceName });
		}
	}

	void handleClockDomainCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const /*clockDomainIndex*/, la::avdecc::entity::model::ClockDomainCounters const& counters)
	{
		if (auto const index = indexOf(entityID))
		{
			auto const idx = *index;
			auto& data = _entities[idx];

			data.clockDomainInfo = computeClockDomainInfo(counters);

			la::avdecc::utils::invokeProtectedMethod(&Model::entityInfoChanged, _model, idx, data, Model::ChangedInfoFlags{ Model::ChangedInfoFlag::ClockDomainLockState });
		}
	}

	ClockDomainInfo computeClockDomainInfo(la::avdecc::entity::model::ClockDomainCounters const& counters) noexcept
	{
		auto clockDomainInfo = ClockDomainInfo{ ClockDomainLockedState::Unknown };

		auto const itLocked = counters.find(la::avdecc::entity::ClockDomainCounterValidFlag::Locked);
		auto const itUnlocked = counters.find(la::avdecc::entity::ClockDomainCounterValidFlag::Unlocked);
		if (itLocked != counters.end() && itUnlocked != counters.end())
		{
			if (itLocked->second > itUnlocked->second)
			{
				clockDomainInfo.state = ClockDomainLockedState::Locked;
				clockDomainInfo.tooltip = QString("Clock Domain is locked on MCR");
			}
			else
			{
				clockDomainInfo.state = ClockDomainLockedState::Unlocked;
				clockDomainInfo.tooltip = QString("Clock Domain is not locked");
			}
		}
		return clockDomainInfo;
	}

	// Private Members
	Model* _model{ nullptr };
	std::vector<Entity> _entities{};
	EntityRowMap _entityRowMap{};
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

std::optional<std::size_t> DiscoveredEntitiesModel::indexOf(la::avdecc::UniqueIdentifier const& entityID) const noexcept
{
	return _pImpl->indexOf(entityID);
}

std::size_t DiscoveredEntitiesModel::entitiesCount() const noexcept
{
	return _pImpl->entitiesCount();
}

} // namespace modelsLibrary
} // namespace hive
