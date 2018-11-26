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
	Q_SLOT void gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain);

	// Slots for avdecc::mediaClock::MCDomainManager signals
	Q_SLOT void mediaClockConnectionsUpdated(std::vector<la::avdecc::UniqueIdentifier> const changedEntities);

	// Slots for EntityLogoCache signals
	Q_SLOT void imageChanged(la::avdecc::UniqueIdentifier const entityID, EntityLogoCache::Type const type);

	// Slots for settings::SettingsManager signals
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

private:
	ControllerModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(ControllerModel);

	using Entities = std::vector<la::avdecc::UniqueIdentifier>;
	Entities _entities{};

	std::array<QImage, 3> _acquireStateImages{
		{ QImage{ ":/unlocked.png" }, QImage{ ":/locked.png" }, QImage{ ":/locked_by_other.png" } },
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
	connect(&controllerManager, &avdecc::ControllerManager::gptpChanged, this, &ControllerModelPrivate::gptpChanged);

	// Connect avdecc::mediaClock::MCDomainManager signals
	auto& mediaClockConnectionManager = avdecc::mediaClock::MCDomainManager::getInstance();
	connect(&mediaClockConnectionManager, &avdecc::mediaClock::MCDomainManager::mediaClockConnectionsUpdate, this, &ControllerModelPrivate::mediaClockConnectionsUpdated);

	// Connect EntityLogoCache signals
	auto& logoCache = EntityLogoCache::getInstance();
	connect(&logoCache, &EntityLogoCache::imageChanged, this, &ControllerModelPrivate::imageChanged);

	// Connect settings::SettingsManager signals
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
	return la::avdecc::to_integral(ControllerModel::Column::Count);
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
#pragma message("TODO: Do not recompute the MCM for this entity here (findMediaClockMaster), instead cache the value in the clockConnectionManager and just call a getMediaClockMaster(entityID) method here")
				auto const clockMaster = clockConnectionManager.findMediaClockMaster(entityID);
				auto const error = clockMaster.second;
				if (!!error)
				{
					switch (error)
					{
						case mediaClock::MCDomainManager::Error::NotSupported:
							return "Not Supported";
						case mediaClock::MCDomainManager::Error::Recursive:
							return "Recursive";
						case mediaClock::MCDomainManager::Error::StreamNotConnected:
							return "Stream N/C";
						case mediaClock::MCDomainManager::Error::UnknownEntity:
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
#pragma message("TODO: Do not recompute the MCM for this entity here (findMediaClockMaster), instead cache the value in the clockConnectionManager and just call a getMediaClockMaster(entityID) method here")
				auto const clockMaster = clockConnectionManager.findMediaClockMaster(entityID);
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
	else if (column == ControllerModel::Column::EntityLogo)
	{
		if (role == Qt::UserRole)
		{
			auto const& entity = controlledEntity->getEntity();
			if (la::avdecc::hasFlag(entity.getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
			{
				auto& settings = settings::SettingsManager::getInstance();
				auto const& forceDownload{ settings.getValue(settings::AutomaticPNGDownloadEnabled.name).toBool() };

				auto& logoCache = EntityLogoCache::getInstance();
				return logoCache.getImage(entityID, EntityLogoCache::Type::Entity, forceDownload);
			}
		}
	}
	else if (column == ControllerModel::Column::AcquireState)
	{
		switch (role)
		{
			case Qt::UserRole:
				return _acquireStateImages[controlledEntity->isAcquiredByOther() ? 2 : (controlledEntity->isAcquired() ? 1 : 0)];
			case Qt::ToolTipRole:
				return controlledEntity->isAcquiredByOther() ? "Acquired by another controller" : (controlledEntity->isAcquired() ? "Acquired" : "Not acquired");
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
				case ControllerModel::Column::EntityId:
					return "Entity ID";
				case ControllerModel::Column::Name:
					return "Name";
				case ControllerModel::Column::Group:
					return "Group";
				case ControllerModel::Column::AcquireState:
					return "Acquire state";
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
	return q->createIndex(entityRow(entityID), la::avdecc::to_integral(column));
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
			auto const column{ la::avdecc::to_integral(ControllerModel::Column::EntityLogo) };

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
