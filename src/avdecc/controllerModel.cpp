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

#include "controllerModel.hpp"
#include "helper.hpp"
#include <la/avdecc/utils.hpp>
#include <la/avdecc/logger.hpp>
#include "avdecc/controllerManager.hpp"
#include "avdecc/mcDomainManager.hpp"
#include "entityLogoCache.hpp"
#include "settingsManager/settings.hpp"
#include "toolkit/material/color.hpp"
#include <algorithm>
#include <array>
#include <QFont>
#include <set>
#include <unordered_map>
#include <unordered_set>

Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)

namespace avdecc
{
enum class ExclusiveAccessState
{
	NoAccess = 0,
	NotSupported = 1,
	AccessOther = 2,
	AccessSelf = 3,
};

enum class Compatibility
{
	NotCompliant,
	IEEE,
	Milan,
	MilanRedundant,
	Misbehaving,
};

struct MediaClockInfo
{
	QString masterID;
	QString masterName;
};

struct GptpInfo
{
	std::optional<la::avdecc::UniqueIdentifier> grandmasterID;
	std::optional<std::uint8_t> domainNumber;
};
using GptpInfoPerAvbInterfaceIndex = std::map<la::avdecc::entity::model::AvbInterfaceIndex, GptpInfo>;

Compatibility computeCompatibility(std::optional<la::avdecc::entity::model::MilanInfo> const& milanInfo, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags)
{
	if (compatibilityFlags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Misbehaving))
	{
		return Compatibility::Misbehaving;
	}
	else if (compatibilityFlags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
	{
		if (milanInfo && milanInfo->featuresFlags.test(la::avdecc::entity::MilanInfoFeaturesFlag::Redundancy))
		{
			return Compatibility::MilanRedundant;
		}

		return Compatibility::Milan;
	}
	else if (compatibilityFlags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::IEEE17221))
	{
		return Compatibility::IEEE;
	}

	return Compatibility::NotCompliant;
}

ExclusiveAccessState computeAcquireState(la::avdecc::controller::model::AcquireState const acquireState)
{
	switch (acquireState)
	{
		case la::avdecc::controller::model::AcquireState::NotSupported:
			return ExclusiveAccessState::NotSupported;
		case la::avdecc::controller::model::AcquireState::Acquired:
			return ExclusiveAccessState::AccessSelf;
		case la::avdecc::controller::model::AcquireState::AcquiredByOther:
			return ExclusiveAccessState::AccessOther;
		default:
			return ExclusiveAccessState::NoAccess;
	}
}

ExclusiveAccessState computeLockState(la::avdecc::controller::model::LockState const lockState)
{
	switch (lockState)
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

MediaClockInfo computeMediaClockInfo(la::avdecc::UniqueIdentifier const& entityID)
{
	MediaClockInfo info;

	try
	{
		auto& clockConnectionManager = avdecc::mediaClock::MCDomainManager::getInstance();
		auto const& [mediaClockMasterID, error] = clockConnectionManager.getMediaClockMaster(entityID);

		if (!!error)
		{
			switch (error)
			{
				case mediaClock::McDeterminationError::NotSupportedClockSourceType:
				case mediaClock::McDeterminationError::NotSupportedNoAem:
				case mediaClock::McDeterminationError::NotSupportedMultipleClockDomains:
				case mediaClock::McDeterminationError::NotSupportedNoClockDomains:
					info.masterID = "N/A";
					break;
				case mediaClock::McDeterminationError::Recursive:
					info.masterID = "Recursive";
					break;
				case mediaClock::McDeterminationError::StreamNotConnected:
					info.masterID = "Stream N/C";
					break;
				case mediaClock::McDeterminationError::ParentStreamNotConnected:
					info.masterID = "Parent Stream N/C";
					break;
				case mediaClock::McDeterminationError::ExternalClockSource:
					if (mediaClockMasterID == entityID)
					{
						info.masterID = "External";
					}
					else
					{
						info.masterID = "External on " + helper::uniqueIdentifierToString(mediaClockMasterID);
					}
					break;
				case mediaClock::McDeterminationError::AnyEntityInChainOffline:
					info.masterID = "Talker Offline";
					break;
				case mediaClock::McDeterminationError::UnknownEntity:
					info.masterID = "Unknown Entity";
					break;
			}
		}
		else
		{
			if (mediaClockMasterID != entityID)
			{
				info.masterID = helper::uniqueIdentifierToString(mediaClockMasterID);

				auto& manager = avdecc::ControllerManager::getInstance();
				if (auto const clockMasterEntity = manager.getControlledEntity(mediaClockMasterID))
				{
					info.masterName = helper::entityName(*clockMasterEntity);
				}
			}
			else
			{
				info.masterID = "Self";
			}
		}
	}
	catch (...)
	{
		return {};
	}

	return info;
}

GptpInfoPerAvbInterfaceIndex buildGptpInfoMap(la::avdecc::entity::Entity::InterfacesInformation const& interfacesInformation)
{
	GptpInfoPerAvbInterfaceIndex map;
	for (auto const& [avbInterfaceIndex, interfaceInformation] : interfacesInformation)
	{
		map.insert(std::make_pair(avbInterfaceIndex, GptpInfo{ interfaceInformation.gptpGrandmasterID, interfaceInformation.gptpDomainNumber }));
	}
	return map;
}

QString computeGptpTooltip(GptpInfoPerAvbInterfaceIndex const& gptp)
{
	QStringList list;

	for (auto const& [avbInterfaceIndex, info] : gptp)
	{
		if (info.grandmasterID && info.domainNumber)
		{
			if (avbInterfaceIndex == la::avdecc::entity::Entity::GlobalAvbInterfaceIndex)
			{
				list << QString{ "Global gPTP: %1 / %2" }.arg(helper::uniqueIdentifierToString(*info.grandmasterID)).arg(*info.domainNumber);
			}
			else
			{
				list << QString{ "gPTP for index %1: %2 / %3" }.arg(avbInterfaceIndex).arg(helper::uniqueIdentifierToString(*info.grandmasterID)).arg(*info.domainNumber);
			}
		}
	}

	return list.join('\n');
}

class ControllerModelPrivate : public QObject, private settings::SettingsManager::Observer
{
	Q_OBJECT
public:
	ControllerModelPrivate(ControllerModel* model)
		: q_ptr{ model }
	{
		// Connect avdecc::ControllerManager signals
		auto& controllerManager = avdecc::ControllerManager::getInstance();
		connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &ControllerModelPrivate::handleControllerOffline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &ControllerModelPrivate::handleEntityOnline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &ControllerModelPrivate::handleEntityOffline);
		connect(&controllerManager, &avdecc::ControllerManager::identificationStarted, this, &ControllerModelPrivate::handleIdentificationStarted);
		connect(&controllerManager, &avdecc::ControllerManager::identificationStopped, this, &ControllerModelPrivate::handleIdentificationStopped);
		connect(&controllerManager, &avdecc::ControllerManager::entityNameChanged, this, &ControllerModelPrivate::handleEntityNameChanged);
		connect(&controllerManager, &avdecc::ControllerManager::entityGroupNameChanged, this, &ControllerModelPrivate::handleEntityGroupNameChanged);
		connect(&controllerManager, &avdecc::ControllerManager::acquireStateChanged, this, &ControllerModelPrivate::handleAcquireStateChanged);
		connect(&controllerManager, &avdecc::ControllerManager::lockStateChanged, this, &ControllerModelPrivate::handleLockStateChanged);
		connect(&controllerManager, &avdecc::ControllerManager::compatibilityFlagsChanged, this, &ControllerModelPrivate::handleCompatibilityFlagsChanged);
		connect(&controllerManager, &avdecc::ControllerManager::gptpChanged, this, &ControllerModelPrivate::handleGptpChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamInputErrorCounterChanged, this, &ControllerModelPrivate::handleStreamInputErrorCounterChanged);

		// Connect avdecc::mediaClock::MCDomainManager signals
		auto& mediaClockConnectionManager = avdecc::mediaClock::MCDomainManager::getInstance();
		connect(&mediaClockConnectionManager, &avdecc::mediaClock::MCDomainManager::mediaClockConnectionsUpdate, this, &ControllerModelPrivate::handleMediaClockConnectionsUpdated);
		connect(&mediaClockConnectionManager, &avdecc::mediaClock::MCDomainManager::mcMasterNameChanged, this, &ControllerModelPrivate::handleMcMasterNameChanged);

		// Connect EntityLogoCache signals
		auto& logoCache = EntityLogoCache::getInstance();
		connect(&logoCache, &EntityLogoCache::imageChanged, this, &ControllerModelPrivate::handleImageChanged);

		// Register to settings::SettingsManager
		auto& settings = settings::SettingsManager::getInstance();
		settings.registerSettingObserver(settings::AemCacheEnabled.name, this);
	}

	virtual ~ControllerModelPrivate()
	{
		// Remove settings observers
		auto& settings = settings::SettingsManager::getInstance();
		settings.unregisterSettingObserver(settings::AemCacheEnabled.name, this);
	}

	int rowCount() const
	{
		return static_cast<int>(_entities.size());
	}

	int columnCount() const
	{
		return la::avdecc::utils::to_integral(ControllerModel::Column::Count);
	}

	QVariant data(QModelIndex const& index, int role) const
	{
		auto const row = index.row();
		if (row < 0 || row >= rowCount())
		{
			return {};
		}

		auto const& data = _entities[row];
		auto const& entityID = data.entityID;
		auto const column = static_cast<ControllerModel::Column>(index.column());

		if (role == Qt::DisplayRole)
		{
			switch (column)
			{
				case ControllerModel::Column::EntityID:
					return helper::uniqueIdentifierToString(entityID);
				case ControllerModel::Column::Name:
					return data.name;
				case ControllerModel::Column::Group:
					return data.groupName;
				case ControllerModel::Column::GrandmasterID:
					return data.gptpGrandmasterIDToString();
				case ControllerModel::Column::GptpDomain:
					return data.gptpDomainNumberToString();
				case ControllerModel::Column::InterfaceIndex:
					return data.avbInterfaceIndexToString();
				case ControllerModel::Column::AssociationID:
					return data.associationIDToString();
				case ControllerModel::Column::MediaClockMasterID:
					return data.mediaClockInfo.masterID;
				case ControllerModel::Column::MediaClockMasterName:
					return data.mediaClockInfo.masterName;
				default:
					break;
			}
		}
		else if (column == ControllerModel::Column::EntityID)
		{
			if (role == Qt::ForegroundRole)
			{
				auto const it = _entitiesWithErrorCounter.find(entityID);
				if (it != std::end(_entitiesWithErrorCounter))
				{
					auto const& streamsWithErrorCounter{ it->second };
					if (!streamsWithErrorCounter.empty())
					{
						// At least one stream contains a counter error
						return qt::toolkit::material::color::value(qt::toolkit::material::color::Name::Red);
					}
				}
			}
			else if (role == Qt::FontRole)
			{
				auto font = QFont{};
				font.setBold(_identifingEntities.count(entityID) > 0);
				return font;
			}
		}
		else if (column == ControllerModel::Column::EntityLogo)
		{
			if (role == Qt::UserRole)
			{
				if (data.aemSupported)
				{
					// TODO, cache the setting value in the model?
					auto& settings = settings::SettingsManager::getInstance();
					auto const& forceDownload = settings.getValue(settings::AutomaticPNGDownloadEnabled.name).toBool();

					auto& logoCache = EntityLogoCache::getInstance();
					return logoCache.getImage(entityID, EntityLogoCache::Type::Entity, forceDownload);
				}
			}
		}
		else if (column == ControllerModel::Column::Compatibility)
		{
			switch (role)
			{
				case Qt::UserRole:
				{
					try
					{
						return _compatibilityImages.at(data.compatibility);
					}
					catch (std::out_of_range const&)
					{
						AVDECC_ASSERT(false, "Image missing");
						return {};
					}
				}
				case Qt::ToolTipRole:
					switch (data.compatibility)
					{
						case Compatibility::Misbehaving:
							return "Entity is sending incoherent values that can cause undefined behavior";
						case Compatibility::Milan:
						case Compatibility::MilanRedundant:
							return "MILAN compatible";
						case Compatibility::IEEE:
							return "IEEE 1722.1 compatible";
						default:
							return "Not fully IEEE 1722.1 compliant";
					}
				default:
					break;
			}
		}
		else if (column == ControllerModel::Column::AcquireState)
		{
			switch (role)
			{
				case Qt::UserRole:
				{
					try
					{
						return _excusiveAccessStateImages.at(data.acquireState);
					}
					catch (std::out_of_range const&)
					{
						AVDECC_ASSERT(false, "Image missing");
						return {};
					}
				}
				case Qt::ToolTipRole:
					return data.acquireStateTooltip;
				default:
					break;
			}
		}
		else if (column == ControllerModel::Column::LockState)
		{
			switch (role)
			{
				case Qt::UserRole:
				{
					try
					{
						return _excusiveAccessStateImages.at(data.lockState);
					}
					catch (std::out_of_range const&)
					{
						AVDECC_ASSERT(false, "Image missing");
						return {};
					}
				}
				case Qt::ToolTipRole:
					return data.lockStateTooltip;
				default:
					break;
			}
		}
		else if (role == Qt::ToolTipRole)
		{
			switch (column)
			{
				case ControllerModel::Column::GrandmasterID:
				case ControllerModel::Column::GptpDomain:
				case ControllerModel::Column::InterfaceIndex:
					return data.gptpTooltip;
				default:
					break;
			}
		}

		return {};
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role) const
	{
		if (orientation == Qt::Horizontal)
		{
			if (role == Qt::DisplayRole)
			{
				switch (static_cast<ControllerModel::Column>(section))
				{
					case ControllerModel::Column::EntityLogo:
						return "Logo";
					case ControllerModel::Column::Compatibility:
						return "Compat";
					case ControllerModel::Column::EntityID:
						return "Entity ID";
					case ControllerModel::Column::Name:
						return "Name";
					case ControllerModel::Column::Group:
						return "Group";
					case ControllerModel::Column::AcquireState:
						return "Acquire State";
					case ControllerModel::Column::LockState:
						return "Lock State";
					case ControllerModel::Column::GrandmasterID:
						return "Grandmaster ID";
					case ControllerModel::Column::GptpDomain:
						return "GPTP Domain";
					case ControllerModel::Column::InterfaceIndex:
						return "Interface Index";
					case ControllerModel::Column::AssociationID:
						return "Association ID";
					case ControllerModel::Column::MediaClockMasterID:
						return "Media Clock Master ID";
					case ControllerModel::Column::MediaClockMasterName:
						return "Media Clock Master Name";
					default:
						break;
				}
			}
		}

		return {};
	}

	la::avdecc::UniqueIdentifier controlledEntityID(QModelIndex const& index) const
	{
		auto const row = index.row();
		if (row >= 0 && row < rowCount())
		{
			return _entities[index.row()].entityID;
		}
		return la::avdecc::UniqueIdentifier{};
	}

private:
	class EntityData
	{
	public:
		EntityData(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::Entity const& entity)
			: entityID{ entityID }
			, name{ helper::entityName(controlledEntity) }
			, groupName{ helper::groupName(controlledEntity) }
			, acquireState{ computeAcquireState(controlledEntity.getAcquireState()) }
			, acquireStateTooltip{ helper::acquireStateToString(controlledEntity.getAcquireState(), controlledEntity.getOwningControllerID()) }
			, lockState{ computeLockState(controlledEntity.getLockState()) }
			, lockStateTooltip{ helper::lockStateToString(controlledEntity.getLockState(), controlledEntity.getLockingControllerID()) }
			, compatibility{ computeCompatibility(controlledEntity.getMilanInfo(), controlledEntity.getCompatibilityFlags()) }
			, aemSupported{ entity.getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) }
			, gptpInfoMap{ buildGptpInfoMap(entity.getInterfacesInformation()) }
			, gptpTooltip{ computeGptpTooltip(gptpInfoMap) }
			, associationID{ entity.getAssociationID() }
			, mediaClockInfo{ computeMediaClockInfo(entityID) }
		{
		}

	public:
		la::avdecc::UniqueIdentifier entityID;

		QString name{};
		QString groupName{};

		ExclusiveAccessState acquireState{};
		QString acquireStateTooltip{};

		ExclusiveAccessState lockState{};
		QString lockStateTooltip{};

		Compatibility compatibility{};

		bool aemSupported{ false };

		GptpInfoPerAvbInterfaceIndex gptpInfoMap{};
		QString gptpTooltip{};

		std::optional<la::avdecc::UniqueIdentifier> associationID{};

		MediaClockInfo mediaClockInfo{};

		// Helper methods

		QString gptpGrandmasterIDToString() const
		{
			try
			{
				auto const& [avbInterfaceIndex, info] = *gptpInfoMap.begin();
				return info.grandmasterID ? helper::uniqueIdentifierToString(*info.grandmasterID) : "Not Set";
			}
			catch (...)
			{
				return "Err";
			}
		}

		QString gptpDomainNumberToString() const
		{
			try
			{
				auto const& [avbInterfaceIndex, info] = *gptpInfoMap.begin();
				return info.domainNumber ? QString::number(*info.domainNumber) : "Not Set";
			}
			catch (...)
			{
				return "Err";
			}
		}

		QString avbInterfaceIndexToString() const
		{
			try
			{
				auto const& [avbInterfaceIndex, info] = *gptpInfoMap.begin();
				return avbInterfaceIndex == la::avdecc::entity::Entity::GlobalAvbInterfaceIndex ? "Not Set" : QString::number(avbInterfaceIndex);
			}
			catch (...)
			{
				return "Err";
			}
		}

		QString associationIDToString() const
		{
			return associationID ? helper::uniqueIdentifierToString(*associationID) : "Not Set";
		}
	};

	using Entities = std::vector<EntityData>;
	using EntityRowMap = std::unordered_map<la::avdecc::UniqueIdentifier, int, la::avdecc::UniqueIdentifier::hash>;

	QModelIndex createIndex(int const row, ControllerModel::Column const column) const
	{
		Q_Q(const ControllerModel);
		return q->createIndex(row, la::avdecc::utils::to_integral(column));
	}

	void dataChanged(int const row, ControllerModel::Column const column, QVector<int> const& roles = { Qt::DisplayRole })
	{
		auto const index = createIndex(row, column);
		if (index.isValid())
		{
			Q_Q(ControllerModel);
			emit q->dataChanged(index, index, roles);
		}
	}

	// avdecc::ControllerManager

	void handleControllerOffline()
	{
		Q_Q(ControllerModel);

		q->beginResetModel();
		_entities.clear();
		_entitiesWithErrorCounter.clear();
		_identifingEntities.clear();
		q->endResetModel();
	}

	void handleEntityOnline(la::avdecc::UniqueIdentifier const& entityID)
	{
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity)
			{
				Q_Q(ControllerModel);

				// Insert at the end
				auto const row = rowCount();
				emit q->beginInsertRows({}, row, row);

				_entities.emplace_back(entityID, *controlledEntity, controlledEntity->getEntity());

				// Update the cache
				rebuildEntityRowMap();

				emit q->endInsertRows();
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
		if (auto const row = entityRow(entityID))
		{
			Q_Q(ControllerModel);
			emit q->beginRemoveRows({}, *row, *row);

			// Remove the entity from the model
			auto const it = std::next(std::begin(_entities), *row);
			_entities.erase(it);

			// Update the cache
			rebuildEntityRowMap();

			emit q->endRemoveRows();
		}
	}

	void handleIdentificationStarted(la::avdecc::UniqueIdentifier const& entityID)
	{
		if (auto const row = entityRow(entityID))
		{
			_identifingEntities.insert(entityID);
			dataChanged(*row, ControllerModel::Column::EntityID, { Qt::FontRole });
		}
	}

	void handleIdentificationStopped(la::avdecc::UniqueIdentifier const& entityID)
	{
		if (auto const row = entityRow(entityID))
		{
			_identifingEntities.erase(entityID);
			dataChanged(*row, ControllerModel::Column::EntityID, { Qt::FontRole });
		}
	}

	void handleEntityNameChanged(la::avdecc::UniqueIdentifier const& entityID, QString const& entityName)
	{
		if (auto const row = entityRow(entityID))
		{
			Q_Q(ControllerModel);

			auto& data = _entities[*row];
			data.name = entityName;

			dataChanged(*row, ControllerModel::Column::Name);
		}
	}

	void handleEntityGroupNameChanged(la::avdecc::UniqueIdentifier const& entityID, QString const& entityGroupName)
	{
		if (auto const row = entityRow(entityID))
		{
			auto& data = _entities[*row];
			data.groupName = entityGroupName;

			dataChanged(*row, ControllerModel::Column::Group);
		}
	}

	void handleAcquireStateChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity)
	{
		if (auto const row = entityRow(entityID))
		{
			auto& data = _entities[*row];

			data.acquireState = computeAcquireState(acquireState);
			data.acquireStateTooltip = helper::acquireStateToString(acquireState, owningEntity);

			dataChanged(*row, ControllerModel::Column::AcquireState, { Qt::UserRole });
		}
	}

	void handleLockStateChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity)
	{
		if (auto const row = entityRow(entityID))
		{
			auto& data = _entities[*row];

			data.lockState = computeLockState(lockState);
			data.lockStateTooltip = helper::lockStateToString(lockState, lockingEntity);

			dataChanged(*row, ControllerModel::Column::LockState, { Qt::UserRole });
		}
	}

	void handleCompatibilityFlagsChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags)
	{
		if (auto const row = entityRow(entityID))
		{
			auto& data = _entities[*row];

			try
			{
				auto& manager = avdecc::ControllerManager::getInstance();
				if (auto controlledEntity = manager.getControlledEntity(entityID))
				{
					data.compatibility = computeCompatibility(controlledEntity->getMilanInfo(), compatibilityFlags);
					dataChanged(*row, ControllerModel::Column::Compatibility);
				}
			}
			catch (...)
			{
				//
			}
		}
	}

	void handleGptpChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
	{
		if (auto const row = entityRow(entityID))
		{
			auto& data = _entities[*row];

			auto& info = data.gptpInfoMap[avbInterfaceIndex];

			info.grandmasterID = grandMasterID;
			info.domainNumber = grandMasterDomain;

			data.gptpTooltip = computeGptpTooltip(data.gptpInfoMap);

			dataChanged(*row, ControllerModel::Column::GrandmasterID);
			dataChanged(*row, ControllerModel::Column::GptpDomain);
		}
	}

	void handleStreamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, ControllerManager::StreamInputErrorCounters const& errorCounters)
	{
		if (auto const row = entityRow(entityID))
		{
			if (!errorCounters.empty())
			{
				_entitiesWithErrorCounter[entityID].insert(descriptorIndex);
			}
			else
			{
				_entitiesWithErrorCounter[entityID].erase(descriptorIndex);
			}

			dataChanged(*row, ControllerModel::Column::EntityID);
		}
	}

	// avdecc::mediaClock::MCDomainManager

	void handleMediaClockConnectionsUpdated(std::vector<la::avdecc::UniqueIdentifier> const& changedEntities)
	{
		for (auto const& entityID : changedEntities)
		{
			if (auto const row = entityRow(entityID))
			{
				auto& data = _entities[*row];

				data.mediaClockInfo = computeMediaClockInfo(entityID);

				dataChanged(*row, ControllerModel::Column::MediaClockMasterID);
				dataChanged(*row, ControllerModel::Column::MediaClockMasterName);
			}
		}
	}

	void handleMcMasterNameChanged(std::vector<la::avdecc::UniqueIdentifier> const& changedEntities)
	{
		for (auto const& entityID : changedEntities)
		{
			if (auto const row = entityRow(entityID))
			{
				auto& data = _entities[*row];

				data.mediaClockInfo = computeMediaClockInfo(entityID);

				dataChanged(*row, ControllerModel::Column::MediaClockMasterName);
			}
		}
	}

	// EntityLogoCache

	void handleImageChanged(la::avdecc::UniqueIdentifier const& entityID, EntityLogoCache::Type const type)
	{
		if (type == EntityLogoCache::Type::Entity)
		{
			if (auto const row = entityRow(entityID))
			{
				dataChanged(*row, ControllerModel::Column::EntityLogo, { Qt::UserRole });
			}
		}
	}

	// settings::SettingsManager overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override
	{
		if (name == settings::AutomaticPNGDownloadEnabled.name)
		{
			if (value.toBool())
			{
				Q_Q(ControllerModel);

				auto constexpr column = la::avdecc::utils::to_integral(ControllerModel::Column::EntityLogo);

				auto const topLeft = q->createIndex(0, column, nullptr);
				auto const bottomRight = q->createIndex(rowCount(), column, nullptr);

				emit q->dataChanged(topLeft, bottomRight, { Qt::UserRole });
			}
		}
	}

	// Build the entityID to row map
	void rebuildEntityRowMap()
	{
		_entityRowMap.clear();

		for (auto row = 0; row < rowCount(); ++row)
		{
			auto const& data = _entities[row];
			_entityRowMap.emplace(std::make_pair(data.entityID, row));
		}
	}

	// Returns the entity row if found in the model
	std::optional<int> entityRow(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto const it = _entityRowMap.find(entityID);
		if (AVDECC_ASSERT_WITH_RET(it != std::end(_entityRowMap), "Invalid entityID"))
		{
			return it->second;
		}
		return {};
	}

private:
	ControllerModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(ControllerModel);

	Entities _entities{};
	EntityRowMap _entityRowMap{};

	using StreamsWithErrorCounter = std::set<la::avdecc::entity::model::StreamIndex>;
	using EntitiesWithErrorCounter = std::unordered_map<la::avdecc::UniqueIdentifier, StreamsWithErrorCounter, la::avdecc::UniqueIdentifier::hash>;
	EntitiesWithErrorCounter _entitiesWithErrorCounter{};
	std::unordered_set<la::avdecc::UniqueIdentifier, la::avdecc::UniqueIdentifier::hash> _identifingEntities;

	std::unordered_map<Compatibility, QImage> _compatibilityImages{
		{ Compatibility::NotCompliant, QImage{ ":/not_compliant.png" } },
		{ Compatibility::IEEE, QImage{ ":/ieee.png" } },
		{ Compatibility::Milan, QImage{ ":/milan.png" } },
		{ Compatibility::Misbehaving, QImage{ ":/misbehaving.png" } },
		{ Compatibility::MilanRedundant, QImage{ ":/milan_redundant.png" } },
	};
	std::unordered_map<ExclusiveAccessState, QImage> _excusiveAccessStateImages{
		{ ExclusiveAccessState::NoAccess, QImage{ ":/unlocked.png" } },
		{ ExclusiveAccessState::NotSupported, QImage{ ":/lock_not_supported.png" } },
		{ ExclusiveAccessState::AccessOther, QImage{ ":/locked_by_other.png" } },
		{ ExclusiveAccessState::AccessSelf, QImage{ ":/locked.png" } },
	};
};

ControllerModel::ControllerModel(QObject* parent)
	: QAbstractTableModel{ parent }
	, d_ptr{ new ControllerModelPrivate(this) }
{
}

ControllerModel::~ControllerModel() = default;

int ControllerModel::rowCount(QModelIndex const& parent) const
{
	Q_D(const ControllerModel);
	return d->rowCount();
}

int ControllerModel::columnCount(QModelIndex const& parent) const
{
	Q_D(const ControllerModel);
	return d->columnCount();
}

QVariant ControllerModel::data(QModelIndex const& index, int role) const
{
	Q_D(const ControllerModel);
	return d->data(index, role);
}

QVariant ControllerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_D(const ControllerModel);
	return d->headerData(section, orientation, role);
}

la::avdecc::UniqueIdentifier ControllerModel::controlledEntityID(QModelIndex const& index) const
{
	Q_D(const ControllerModel);
	return d->controlledEntityID(index);
}

} // namespace avdecc

#include "controllerModel.moc"
