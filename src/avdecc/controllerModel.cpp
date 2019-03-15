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
#include <algorithm>
#include <array>
#include <QTimer>

Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)


namespace avdecc
{
class ControllerModelPrivate : public QObject, private settings::SettingsManager::Observer
{
	Q_OBJECT
public:
	ControllerModelPrivate(ControllerModel* model);
	virtual ~ControllerModelPrivate();

	int rowCount() const;
	int columnCount() const;
	QVariant data(QModelIndex const& index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	Qt::ItemFlags flags(QModelIndex const& index) const;

	la::avdecc::UniqueIdentifier controlledEntityID(QModelIndex const& index) const;

private:
	int entityRow(la::avdecc::UniqueIdentifier const entityID) const;
	QModelIndex createIndex(la::avdecc::UniqueIdentifier const entityID, ControllerModel::Column column) const;
	void dataChanged(la::avdecc::UniqueIdentifier const entityID, ControllerModel::Column column, QVector<int> const& roles = { Qt::DisplayRole });

	// Slots for avdecc::ControllerManager signals
	Q_SLOT void controllerOffline();
	Q_SLOT void entityOnline(la::avdecc::UniqueIdentifier const entityID);
	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID);
	Q_SLOT void entityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityName);
	Q_SLOT void entityGroupNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityGroupName);
	Q_SLOT void acquireStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity);
	Q_SLOT void lockStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity);
	Q_SLOT void compatibilityFlagsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags);
	Q_SLOT void gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain);
	Q_SLOT void streamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::StreamInputCounterValidFlags const flags);

	// Slots for avdecc::mediaClock::MCDomainManager signals
	Q_SLOT void mediaClockConnectionsUpdated(std::vector<la::avdecc::UniqueIdentifier> const changedEntities);
	Q_SLOT void mcMasterNameChanged(std::vector<la::avdecc::UniqueIdentifier> const changedEntities);

	// Slots for EntityLogoCache signals
	Q_SLOT void imageChanged(la::avdecc::UniqueIdentifier const entityID, EntityLogoCache::Type const type);

	// settings::SettingsManager overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

private:
	ControllerModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(ControllerModel);

	enum class ExclusiveAccessState
	{
		NoAccess = 0,
		NotSupported = 1,
		AccessOther = 2,
		AccessSelf = 3,
	};

	using Entities = std::vector<la::avdecc::UniqueIdentifier>;
	Entities _entities{};

	using StreamsWithErrorCounter = std::set<la::avdecc::entity::model::StreamIndex>;
	using EntitiesWithErrorCounter = std::unordered_map<la::avdecc::UniqueIdentifier, StreamsWithErrorCounter, la::avdecc::UniqueIdentifier::hash>;
	EntitiesWithErrorCounter _entitiesWithErrorCounter{};

	std::array<QImage, 5> _compatibilityImages{
		{
			QImage{ ":/not_compliant.png" },
			QImage{ ":/ieee.png" },
			QImage{ ":/milan.png" },
			QImage{ ":/misbehaving.png" },
			QImage{ ":/milan_redundant.png" },
		},
	};
	std::unordered_map<ExclusiveAccessState, QImage> _excusiveAccessStateImages{
		{ ExclusiveAccessState::NoAccess, QImage{ ":/unlocked.png" } },
		{ ExclusiveAccessState::NotSupported, QImage{ ":/lock_not_supported.png" } },
		{ ExclusiveAccessState::AccessOther, QImage{ ":/locked_by_other.png" } },
		{ ExclusiveAccessState::AccessSelf, QImage{ ":/locked.png" } },
	};
};

//////////////////////////////////////

ControllerModelPrivate::ControllerModelPrivate(ControllerModel* model)
	: q_ptr(model)
{
	// Connect avdecc::ControllerManager signals
	auto& controllerManager = avdecc::ControllerManager::getInstance();
	connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &ControllerModelPrivate::controllerOffline);
	connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &ControllerModelPrivate::entityOnline);
	connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &ControllerModelPrivate::entityOffline);
	connect(&controllerManager, &avdecc::ControllerManager::entityNameChanged, this, &ControllerModelPrivate::entityNameChanged);
	connect(&controllerManager, &avdecc::ControllerManager::entityGroupNameChanged, this, &ControllerModelPrivate::entityGroupNameChanged);
	connect(&controllerManager, &avdecc::ControllerManager::acquireStateChanged, this, &ControllerModelPrivate::acquireStateChanged);
	connect(&controllerManager, &avdecc::ControllerManager::lockStateChanged, this, &ControllerModelPrivate::lockStateChanged);
	connect(&controllerManager, &avdecc::ControllerManager::compatibilityFlagsChanged, this, &ControllerModelPrivate::compatibilityFlagsChanged);
	connect(&controllerManager, &avdecc::ControllerManager::gptpChanged, this, &ControllerModelPrivate::gptpChanged);
	connect(&controllerManager, &avdecc::ControllerManager::streamInputErrorCounterChanged, this, &ControllerModelPrivate::streamInputErrorCounterChanged);

	auto& mediaClockConnectionManager = avdecc::mediaClock::MCDomainManager::getInstance();
	connect(&mediaClockConnectionManager, &avdecc::mediaClock::MCDomainManager::mediaClockConnectionsUpdate, this, &ControllerModelPrivate::mediaClockConnectionsUpdated);
	connect(&mediaClockConnectionManager, &avdecc::mediaClock::MCDomainManager::mcMasterNameChanged, this, &ControllerModelPrivate::mcMasterNameChanged);

	// Connect EntityLogoCache signals
	auto& logoCache = EntityLogoCache::getInstance();
	connect(&logoCache, &EntityLogoCache::imageChanged, this, &ControllerModelPrivate::imageChanged);

	// Register to settings::SettingsManager
	auto& settings = settings::SettingsManager::getInstance();
	settings.registerSettingObserver(settings::AemCacheEnabled.name, this);
}

ControllerModelPrivate::~ControllerModelPrivate()
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.unregisterSettingObserver(settings::AemCacheEnabled.name, this);
}

int ControllerModelPrivate::rowCount() const
{
	return static_cast<int>(_entities.size());
}

int ControllerModelPrivate::columnCount() const
{
	return la::avdecc::utils::to_integral(ControllerModel::Column::Count);
}

QVariant ControllerModelPrivate::data(QModelIndex const& index, int role) const
{
	auto const entityID = _entities.at(index.row());
	auto& manager = avdecc::ControllerManager::getInstance();
	auto& clockConnectionManager = avdecc::mediaClock::MCDomainManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(entityID);

	if (!controlledEntity)
		return {};

	auto const column = static_cast<ControllerModel::Column>(index.column());

	if (role == Qt::DisplayRole)
	{
		auto const& entity = controlledEntity->getEntity();

		switch (column)
		{
			case ControllerModel::Column::EntityId:
				return helper::uniqueIdentifierToString(entityID);
			case ControllerModel::Column::Name:
				return helper::entityName(*controlledEntity);
			case ControllerModel::Column::Group:
				return helper::groupName(*controlledEntity);
			case ControllerModel::Column::GrandmasterId:
			{
				// TODO: Do not use begin() but change model to a List
				try
				{
					auto const& interfaceInfo = entity.getInterfacesInformation().begin()->second;
					auto const val = interfaceInfo.gptpGrandmasterID;
					return val ? helper::uniqueIdentifierToString(*val) : "Not Set";
				}
				catch (...)
				{
					return "Err";
				}
			}
			case ControllerModel::Column::GptpDomain:
			{
				try
				{
					auto const& interfaceInfo = entity.getInterfacesInformation().begin()->second;
					auto const val = interfaceInfo.gptpDomainNumber;
					return val ? QString::number(*val) : "Not Set";
				}
				catch (...)
				{
					return "Err";
				}
			}
			case ControllerModel::Column::InterfaceIndex:
			{
				try
				{
					auto const avbInterfaceIndex = entity.getInterfacesInformation().begin()->first;
					return avbInterfaceIndex == la::avdecc::entity::Entity::GlobalAvbInterfaceIndex ? "Not Set" : QString::number(avbInterfaceIndex);
				}
				catch (...)
				{
					return "Err";
				}
			}
			case ControllerModel::Column::AssociationId:
			{
				auto const val = entity.getAssociationID();
				return val ? helper::uniqueIdentifierToString(*val) : "Not Set";
			}
			case ControllerModel::Column::MediaClockMasterId:
			{
				auto const clockMaster = clockConnectionManager.getMediaClockMaster(entityID);
				auto const error = clockMaster.second;
				if (!!error)
				{
					switch (error)
					{
						case mediaClock::McDeterminationError::NotSupported:
							return "Not Supported";
						case mediaClock::McDeterminationError::Recursive:
							return "Recursive";
						case mediaClock::McDeterminationError::StreamNotConnected:
							return "Stream N/C";
						case mediaClock::McDeterminationError::ExternalClockSource:
							return QString("External (").append(helper::uniqueIdentifierToString(controlledEntity->getEntity().getEntityID())).append(")");
						case mediaClock::McDeterminationError::UnknownEntity:
							return "Indeterminable";
						default:
							return "Indeterminable";
					}
				}
				else
				{
					// Self MCM
					if (clockMaster.first == entityID)
					{
						return "Self";
					}
					auto controlledEntity = manager.getControlledEntity(clockMaster.first);
					if (controlledEntity)
					{
						return helper::uniqueIdentifierToString(controlledEntity->getEntity().getEntityID());
					}
					// Entity offline
					return "Unknown";
				}
			}
			case ControllerModel::Column::MediaClockMasterName:
			{
				auto const clockMaster = clockConnectionManager.getMediaClockMaster(entityID);
				auto const error = clockMaster.second;
				if (!!error)
				{
					return "";
				}
				else
				{
					// Self MCM, no need to print it
					if (clockMaster.first == entityID)
					{
						return "";
					}
					auto controlledEntity = manager.getControlledEntity(clockMaster.first);
					if (controlledEntity)
					{
						return helper::entityName(*controlledEntity);
					}
					// Entity offline
					return "";
				}
			}
			default:
				break;
		}
	}
	else if (column == ControllerModel::Column::EntityId)
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
					return QColor{ Qt::red };
				}
			}
		}
	}
	else if (column == ControllerModel::Column::EntityLogo)
	{
		if (role == Qt::UserRole)
		{
			auto const& entity = controlledEntity->getEntity();
			if (entity.getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
			{
				auto& settings = settings::SettingsManager::getInstance();
				auto const& forceDownload{ settings.getValue(settings::AutomaticPNGDownloadEnabled.name).toBool() };

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
				auto const flags = controlledEntity->getCompatibilityFlags();
				if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Misbehaving))
				{
					return _compatibilityImages[3];
				}
				else if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
				{
					try
					{
						auto const& milanInfo = controlledEntity->getMilanInfo();
						if ((milanInfo.featuresFlags & la::avdecc::protocol::MvuFeaturesFlags::Redundancy) == la::avdecc::protocol::MvuFeaturesFlags::Redundancy)
						{
							return _compatibilityImages[4];
						}
					}
					catch (...)
					{
					}
					return _compatibilityImages[2];
				}
				else if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::IEEE17221))
				{
					return _compatibilityImages[1];
				}
				else
				{
					return _compatibilityImages[0];
				}
			}
			case Qt::ToolTipRole:
			{
				auto const flags = controlledEntity->getCompatibilityFlags();
				if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Misbehaving))
				{
					return "Entity is sending incoherent values that can cause undefined behavior";
				}
				else if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
				{
					return "MILAN compatible";
				}
				else if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::IEEE17221))
				{
					return "IEEE 1722.1 compatible";
				}
				else
				{
					return "Not fully IEEE 1722.1 compliant";
				}
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
				auto const acquireState = controlledEntity->getAcquireState();
				auto state = ExclusiveAccessState::NoAccess;
				switch (acquireState)
				{
					case la::avdecc::controller::model::AcquireState::NotSupported:
						state = ExclusiveAccessState::NotSupported;
						break;
					case la::avdecc::controller::model::AcquireState::Acquired:
						state = ExclusiveAccessState::AccessSelf;
						break;
					case la::avdecc::controller::model::AcquireState::AcquiredByOther:
						state = ExclusiveAccessState::AccessOther;
						break;
					default:
						break;
				}
				try
				{
					return _excusiveAccessStateImages.at(state);
				}
				catch (std::out_of_range const&)
				{
					AVDECC_ASSERT(false, "Image missing");
					return {};
				}
			}
			case Qt::ToolTipRole:
				return avdecc::helper::acquireStateToString(controlledEntity->getAcquireState(), controlledEntity->getOwningControllerID());
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
				auto const lockState = controlledEntity->getLockState();
				auto state = ExclusiveAccessState::NoAccess;
				switch (lockState)
				{
					case la::avdecc::controller::model::LockState::NotSupported:
						state = ExclusiveAccessState::NotSupported;
						break;
					case la::avdecc::controller::model::LockState::Locked:
						state = ExclusiveAccessState::AccessSelf;
						break;
					case la::avdecc::controller::model::LockState::LockedByOther:
						state = ExclusiveAccessState::AccessOther;
						break;
					default:
						break;
				}
				try
				{
					return _excusiveAccessStateImages.at(state);
				}
				catch (std::out_of_range const&)
				{
					AVDECC_ASSERT(false, "Image missing");
					return {};
				}
			}
			case Qt::ToolTipRole:
				return avdecc::helper::lockStateToString(controlledEntity->getLockState(), controlledEntity->getLockingControllerID());
			default:
				break;
		}
	}

	return {};
}

QVariant ControllerModelPrivate::headerData(int section, Qt::Orientation orientation, int role) const
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
				case ControllerModel::Column::EntityId:
					return "Entity ID";
				case ControllerModel::Column::Name:
					return "Name";
				case ControllerModel::Column::Group:
					return "Group";
				case ControllerModel::Column::AcquireState:
					return "Acquire state";
				case ControllerModel::Column::LockState:
					return "Lock state";
				case ControllerModel::Column::GrandmasterId:
					return "Grandmaster ID";
				case ControllerModel::Column::GptpDomain:
					return "GPTP domain";
				case ControllerModel::Column::InterfaceIndex:
					return "Interface index";
				case ControllerModel::Column::AssociationId:
					return "Association ID";
				case ControllerModel::Column::MediaClockMasterId:
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

Qt::ItemFlags ControllerModelPrivate::flags(QModelIndex const& index) const
{
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

la::avdecc::UniqueIdentifier ControllerModelPrivate::controlledEntityID(QModelIndex const& index) const
{
	if (index.isValid())
	{
		return _entities.at(index.row());
	}
	return la::avdecc::UniqueIdentifier{};
}

int ControllerModelPrivate::entityRow(la::avdecc::UniqueIdentifier const entityID) const
{
	auto const it = std::find(_entities.begin(), _entities.end(), entityID);
	if (it == _entities.end())
	{
		return -1;
	}
	return static_cast<int>(std::distance(_entities.begin(), it));
}

QModelIndex ControllerModelPrivate::createIndex(la::avdecc::UniqueIdentifier const entityID, ControllerModel::Column column) const
{
	Q_Q(const ControllerModel);
	return q->createIndex(entityRow(entityID), la::avdecc::utils::to_integral(column));
}

void ControllerModelPrivate::dataChanged(la::avdecc::UniqueIdentifier const entityID, ControllerModel::Column column, QVector<int> const& roles)
{
	Q_Q(ControllerModel);
	auto const index = createIndex(entityID, column);
	if (index.isValid())
	{
		emit q->dataChanged(index, index, roles);
	}
}

void ControllerModelPrivate::controllerOffline()
{
	Q_Q(ControllerModel);

	q->beginResetModel();
	_entities.clear();
	_entitiesWithErrorCounter.clear();
	q->endResetModel();
}

void ControllerModelPrivate::entityOnline(la::avdecc::UniqueIdentifier const entityID)
{
	auto& manager = avdecc::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(entityID);
	if (controlledEntity)
	{
		Q_Q(ControllerModel);

		auto const row = static_cast<int>(_entities.size());

		emit q->beginInsertRows({}, row, row);
		_entities.emplace_back(entityID);
		emit q->endInsertRows();
	}
}

void ControllerModelPrivate::entityOffline(la::avdecc::UniqueIdentifier const entityID)
{
	auto const it = std::find(_entities.begin(), _entities.end(), entityID);
	if (it != _entities.end())
	{
		Q_Q(ControllerModel);

		auto const row = static_cast<int>(std::distance(_entities.begin(), it));

		emit q->beginRemoveRows({}, row, row);
		_entities.erase(it);
		_entitiesWithErrorCounter.erase(entityID);
		emit q->endRemoveRows();
	}
}

void ControllerModelPrivate::entityNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityName)
{
	dataChanged(entityID, ControllerModel::Column::Name);
}

void ControllerModelPrivate::entityGroupNameChanged(la::avdecc::UniqueIdentifier const entityID, QString const& entityGroupName)
{
	dataChanged(entityID, ControllerModel::Column::Group);
}

void ControllerModelPrivate::acquireStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity)
{
	dataChanged(entityID, ControllerModel::Column::AcquireState, { Qt::UserRole });
}

void ControllerModelPrivate::lockStateChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity)
{
	dataChanged(entityID, ControllerModel::Column::LockState, { Qt::UserRole });
}

void ControllerModelPrivate::compatibilityFlagsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::CompatibilityFlags const compatibilityFlags)
{
	dataChanged(entityID, ControllerModel::Column::Compatibility);
}

void ControllerModelPrivate::gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
{
	dataChanged(entityID, ControllerModel::Column::GrandmasterId);
	dataChanged(entityID, ControllerModel::Column::GptpDomain);
}

void ControllerModelPrivate::mediaClockConnectionsUpdated(std::vector<la::avdecc::UniqueIdentifier> const changedEntities)
{
	for (auto const& entityId : changedEntities)
	{
		dataChanged(entityId, ControllerModel::Column::MediaClockMasterId);
		dataChanged(entityId, ControllerModel::Column::MediaClockMasterName);
	}
}

void ControllerModelPrivate::mcMasterNameChanged(std::vector<la::avdecc::UniqueIdentifier> const changedEntities)
{
	for (auto const& entityId : changedEntities)
	{
		dataChanged(entityId, ControllerModel::Column::MediaClockMasterName);
	}
}

void ControllerModelPrivate::streamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::StreamInputCounterValidFlags const flags)
{
	if (!flags.empty())
	{
		_entitiesWithErrorCounter[entityID].insert(descriptorIndex);
	}
	else
	{
		_entitiesWithErrorCounter[entityID].erase(descriptorIndex);
	}

	emit dataChanged(entityID, ControllerModel::Column::EntityId);
}

void ControllerModelPrivate::imageChanged(la::avdecc::UniqueIdentifier const entityID, EntityLogoCache::Type const type)
{
	if (type == EntityLogoCache::Type::Entity)
	{
		dataChanged(entityID, ControllerModel::Column::EntityLogo, { Qt::UserRole });
	}
}

void ControllerModelPrivate::onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept
{
	if (name == settings::AutomaticPNGDownloadEnabled.name)
	{
		if (value.toBool())
		{
			Q_Q(ControllerModel);
			auto const column{ la::avdecc::utils::to_integral(ControllerModel::Column::EntityLogo) };

			auto const top{ q->createIndex(0, column, nullptr) };
			auto const bottom{ q->createIndex(rowCount(), column, nullptr) };

			emit q->dataChanged(top, bottom, { Qt::UserRole });
		}
	}
}

///////////////////////////////////////

ControllerModel::ControllerModel(QObject* parent)
	: QAbstractTableModel(parent)
	, d_ptr(new ControllerModelPrivate(this))
{
}

ControllerModel::~ControllerModel()
{
	delete d_ptr;
}

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

Qt::ItemFlags ControllerModel::flags(QModelIndex const& index) const
{
	Q_D(const ControllerModel);
	return d->flags(index);
}

la::avdecc::UniqueIdentifier ControllerModel::controlledEntityID(QModelIndex const& index) const
{
	Q_D(const ControllerModel);
	return d->controlledEntityID(index);
}

} // namespace avdecc

#include "controllerModel.moc"
