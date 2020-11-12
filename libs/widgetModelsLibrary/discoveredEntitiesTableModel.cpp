/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "hive/widgetModelsLibrary/discoveredEntitiesTableModel.hpp"
#include "hive/widgetModelsLibrary/entityLogoCache.hpp"
#include "hive/widgetModelsLibrary/imageItemDelegate.hpp"

#include <hive/modelsLibrary/discoveredEntitiesModel.hpp>
#include <hive/modelsLibrary/helper.hpp>

namespace hive
{
namespace widgetModelsLibrary
{
DiscoveredEntitiesTableModel::DiscoveredEntitiesTableModel(EntityDataFlags const entityDataFlags)
	: _entityDataFlags{ entityDataFlags }
	, _count{ static_cast<decltype(_count)>(_entityDataFlags.count()) }
{
}

// hive::modelsLibrary::DiscoveredEntitiesAbstractTableModel overrides
void DiscoveredEntitiesTableModel::entityInfoChanged(std::size_t const index, hive::modelsLibrary::DiscoveredEntitiesModel::Entity const& /*entity*/, ChangedInfoFlags const changedInfoFlags) noexcept
{
	for (auto const flag : changedInfoFlags)
	{
		if (auto const dataInfoOpt = dataChangedInfoForFlag(flag))
		{
			auto const& [entityDataFlag, roles] = *dataInfoOpt;

			// Is this EntityData active for this model
			if (_entityDataFlags.test(entityDataFlag))
			{
				try
				{
					auto const modelIndex = createIndex(index, static_cast<int>(_entityDataFlags.getPosition(entityDataFlag)));
					emit dataChanged(modelIndex, modelIndex, roles);
				}
				catch (...)
				{
				}
			}
		}
	}
}

// QAbstractTableModel overrides
int DiscoveredEntitiesTableModel::rowCount(QModelIndex const& /*parent*/) const
{
	return static_cast<int>(_model.entitiesCount());
}

int DiscoveredEntitiesTableModel::columnCount(QModelIndex const& /*parent*/) const
{
	return static_cast<int>(_count);
}

QVariant DiscoveredEntitiesTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		try
		{
			auto const entityDataFlag = _entityDataFlags.at(section);

			if (role == Qt::DisplayRole)
			{
				switch (entityDataFlag)
				{
					case EntityDataFlag::EntityLogo:
						return "Logo";
					case EntityDataFlag::Compatibility:
						return "Compat";
					case EntityDataFlag::EntityID:
						return "Entity ID";
					case EntityDataFlag::Name:
						return "Name";
					case EntityDataFlag::Group:
						return "Group";
					case EntityDataFlag::FirmwareVersion:
						return "Firmware Version";
					default:
						break;
				}
			}
		}
		catch (...)
		{
		}
	}

	return {};
}

QVariant DiscoveredEntitiesTableModel::data(QModelIndex const& index, int role) const
{
	auto const row = static_cast<std::size_t>(index.row());
	auto const section = static_cast<decltype(_count)>(index.column());
	if (row < static_cast<std::size_t>(rowCount()) && section < _count)
	{
		if (auto const entityOpt = _model.entity(row))
		{
			auto const& entity = (*entityOpt).get();

			try
			{
				auto const entityDataFlag = _entityDataFlags.at(section);

				switch (role)
				{
					case Qt::DisplayRole:
					{
						switch (entityDataFlag)
						{
							case EntityDataFlag::EntityID:
								return hive::modelsLibrary::helper::uniqueIdentifierToString(entity.entityID);
							case EntityDataFlag::Name:
								return entity.name;
							case EntityDataFlag::Group:
								return entity.groupName;
							case EntityDataFlag::FirmwareVersion:
								return entity.firmwareVersion ? *entity.firmwareVersion : "N/A";
							default:
								break;
						}
						break;
					}
					case ImageItemDelegate::ImageRole:
					{
						switch (entityDataFlag)
						{
							case EntityDataFlag::EntityLogo:
								if (entity.aemSupported)
								{
									auto& logoCache = EntityLogoCache::getInstance();
									return logoCache.getImage(entity.entityID, EntityLogoCache::Type::Entity, true);
								}
								break;
							default:
								break;
						}
						break;
					}
					default:
						break;
				}
			}
			catch (...)
			{
			}
		}
	}

	return {};
}

// Private methods
std::optional<std::pair<DiscoveredEntitiesTableModel::EntityDataFlag, QVector<int>>> DiscoveredEntitiesTableModel::dataChangedInfoForFlag(ChangedInfoFlag const flag) const noexcept
{
	switch (flag)
	{
		case ChangedInfoFlag::Name:
			return std::make_pair(EntityDataFlag::Name, QVector<int>{ Qt::DisplayRole });
		case ChangedInfoFlag::GroupName:
			return std::make_pair(EntityDataFlag::Group, QVector<int>{ Qt::DisplayRole });
		case ChangedInfoFlag::Compatibility:
			return std::make_pair(EntityDataFlag::Compatibility, QVector<int>{ Qt::DisplayRole });
		default:
			break;
	}
	return {};
}

} // namespace widgetModelsLibrary
} // namespace hive
