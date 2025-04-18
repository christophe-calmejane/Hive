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

#pragma once

#include "hive/widgetModelsLibrary/qtUserRoles.hpp"

#include <hive/modelsLibrary/discoveredEntitiesModel.hpp>

#include <optional>
#include <type_traits>

namespace hive
{
namespace widgetModelsLibrary
{
class DiscoveredEntitiesTableModel : public hive::modelsLibrary::DiscoveredEntitiesAbstractTableModel
{
public:
	/** Available data columns for the DiscoveredEntities table */
	enum class EntityDataFlag : std::uint32_t
	{
		All = 0u,
		EntityStatus = 1u << 0,
		EntityLogo = 1u << 1,
		Compatibility = 1u << 2,
		EntityID = 1u << 3,
		Name = 1u << 4,
		Group = 1u << 5,
		AcquireState = 1u << 6,
		LockState = 1u << 7,
		GrandmasterID = 1u << 8,
		GPTPDomain = 1u << 9,
		InterfaceIndex = 1u << 10,
		MacAddress = 1u << 11,
		AssociationID = 1u << 12,
		EntityModelID = 1u << 13,
		FirmwareVersion = 1u << 14,
		MediaClockReferenceID = 1u << 15,
		MediaClockReferenceName = 1u << 16,
		ClockDomainLockState = 1u << 17,
	};
	/** List of columns to be displayed */
	using EntityDataFlags = la::avdecc::utils::EnumBitfield<EntityDataFlag>;

	DiscoveredEntitiesTableModel(EntityDataFlags const entityDataFlags);

	// Data getter
	std::optional<std::reference_wrapper<hive::modelsLibrary::DiscoveredEntitiesModel::Entity const>> entity(int const row) const noexcept;
	QModelIndex indexOf(la::avdecc::UniqueIdentifier const& entityID) const noexcept;

private:
	// hive::modelsLibrary::DiscoveredEntitiesAbstractTableModel overrides
	virtual void entityInfoChanged(std::size_t const index, hive::modelsLibrary::DiscoveredEntitiesModel::Entity const& entity, ChangedInfoFlags const changedInfoFlags) noexcept override;

	// QAbstractTableModel overrides
	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual int columnCount(QModelIndex const& parent = {}) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual QVariant data(QModelIndex const& index, int role) const override;

	// Private methods
	std::optional<std::pair<EntityDataFlag, RolesList>> dataChangedInfoForFlag(ChangedInfoFlag const flag) const noexcept;

	// Private members
	hive::modelsLibrary::DiscoveredEntitiesModel _model{ this };
	EntityDataFlags _entityDataFlags{};
	std::underlying_type_t<EntityDataFlag> _count{ 0u };
};

} // namespace widgetModelsLibrary
} // namespace hive
