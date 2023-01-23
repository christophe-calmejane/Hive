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

#include "hive/widgetModelsLibrary/discoveredEntitiesTableModel.hpp"
#include "hive/widgetModelsLibrary/entityLogoCache.hpp"
#include "hive/widgetModelsLibrary/imageItemDelegate.hpp"

#include <hive/modelsLibrary/discoveredEntitiesModel.hpp>
#include <hive/modelsLibrary/helper.hpp>

namespace hive
{
namespace widgetModelsLibrary
{
static std::unordered_map<modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility, QImage> s_compatibilityImagesLight{
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::NotCompliant, QImage{ ":/not_compliant.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::IEEE, QImage{ ":/ieee.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::Milan, QImage{ ":/Milan_Compatible.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanCertified, QImage{ ":/Milan_Certified.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanWarning, QImage{ ":/Milan_Compatible_Warning.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanRedundant, QImage{ ":/Milan_Redundant_Compatible.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanCertifiedRedundant, QImage{ ":/Milan_Redundant_Certified.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanWarningRedundant, QImage{ ":/Milan_Redundant_Compatible_Warning.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::Misbehaving, QImage{ ":/misbehaving.png" } },
};

static std::unordered_map<modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility, QImage> s_compatibilityImagesDark{
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::NotCompliant, QImage{ ":/not_compliant.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::IEEE, QImage{ ":/ieee.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::Milan, QImage{ ":/Milan_Compatible_inv.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanCertified, QImage{ ":/Milan_Certified_inv.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanWarning, QImage{ ":/Milan_Compatible_Warning_inv.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanRedundant, QImage{ ":/Milan_Redundant_Compatible_inv.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanCertifiedRedundant, QImage{ ":/Milan_Redundant_Certified_inv.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanWarningRedundant, QImage{ ":/Milan_Redundant_Compatible_Warning_inv.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::Misbehaving, QImage{ ":/misbehaving.png" } },
};

static std::unordered_map<modelsLibrary::DiscoveredEntitiesModel::ExclusiveAccessState, QImage> s_excusiveAccessStateImagesLight{
	{ modelsLibrary::DiscoveredEntitiesModel::ExclusiveAccessState::NoAccess, QImage{ ":/unlocked.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ExclusiveAccessState::NotSupported, QImage{ ":/lock_not_supported.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ExclusiveAccessState::AccessOther, QImage{ ":/locked_by_other.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ExclusiveAccessState::AccessSelf, QImage{ ":/locked.png" } },
};

static std::unordered_map<modelsLibrary::DiscoveredEntitiesModel::ExclusiveAccessState, QImage> s_excusiveAccessStateImagesDark{
	{ modelsLibrary::DiscoveredEntitiesModel::ExclusiveAccessState::NoAccess, QImage{ ":/unlocked.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ExclusiveAccessState::NotSupported, QImage{ ":/lock_not_supported.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ExclusiveAccessState::AccessOther, QImage{ ":/locked_by_other.png" } },
	{ modelsLibrary::DiscoveredEntitiesModel::ExclusiveAccessState::AccessSelf, QImage{ ":/locked.png" } },
};

DiscoveredEntitiesTableModel::DiscoveredEntitiesTableModel(EntityDataFlags const entityDataFlags)
	: _entityDataFlags{ entityDataFlags }
	, _count{ static_cast<decltype(_count)>(_entityDataFlags.count()) }
{
}

// Data getter
std::optional<std::reference_wrapper<hive::modelsLibrary::DiscoveredEntitiesModel::Entity const>> DiscoveredEntitiesTableModel::entity(int const row) const noexcept
{
	return _model.entity(static_cast<std::size_t>(row));
}

QModelIndex DiscoveredEntitiesTableModel::indexOf(la::avdecc::UniqueIdentifier const& entityID) const noexcept
{
	auto const idxOpt = _model.indexOf(entityID);
	if (idxOpt)
	{
		return createIndex(static_cast<int>(*idxOpt), 0);
	}
	return {};
}

// hive::modelsLibrary::DiscoveredEntitiesAbstractTableModel overrides
void DiscoveredEntitiesTableModel::entityInfoChanged(std::size_t const index, hive::modelsLibrary::DiscoveredEntitiesModel::Entity const& /*entity*/, ChangedInfoFlags const changedInfoFlags) noexcept
{
	for (auto const flag : changedInfoFlags)
	{
		if (auto const dataInfoOpt = dataChangedInfoForFlag(flag))
		{
			auto const& [entityDataFlag, roles] = *dataInfoOpt;

			// Is this EntityData active for this model (or the 'All' bit is set)
			if (entityDataFlag == DiscoveredEntitiesTableModel::EntityDataFlag::All || _entityDataFlags.test(entityDataFlag))
			{
				try
				{
					// 'All' flag means all colomns needs to be refreshed
					if (entityDataFlag == DiscoveredEntitiesTableModel::EntityDataFlag::All)
					{
						auto const startIndex = createIndex(static_cast<int>(index), 0);
						auto const endIndex = createIndex(static_cast<int>(index), columnCount());
						emit dataChanged(startIndex, endIndex, roles);
					}
					// Otherwise selectively refresh a single column
					else
					{
						auto const modelIndex = createIndex(static_cast<int>(index), static_cast<int>(_entityDataFlags.getBitSetPosition(entityDataFlag)));
						emit dataChanged(modelIndex, modelIndex, roles);
					}
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

			switch (role)
			{
				case Qt::DisplayRole:
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
						case EntityDataFlag::AcquireState:
							return "Acquire State";
						case EntityDataFlag::LockState:
							return "Lock State";
						case EntityDataFlag::GrandmasterID:
							return "Grandmaster ID";
						case EntityDataFlag::GPTPDomain:
							return "gPTP Domain";
						case EntityDataFlag::InterfaceIndex:
							return "Interface Idx";
						case EntityDataFlag::AssociationID:
							return "Association ID";
						case EntityDataFlag::EntityModelID:
							return "Entity Model ID";
						case EntityDataFlag::FirmwareVersion:
							return "Firmware Version";
						case EntityDataFlag::MediaClockReferenceID:
							return "MCR ID";
						case EntityDataFlag::MediaClockReferenceStatus:
							return "MCR Status";
						default:
							break;
					}
					break;

				case Qt::ToolTipRole:
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
						case EntityDataFlag::AcquireState:
							return "Acquire State";
						case EntityDataFlag::LockState:
							return "Lock State";
						case EntityDataFlag::GrandmasterID:
							return "Grandmaster ID";
						case EntityDataFlag::GPTPDomain:
							return "gPTP Domain";
						case EntityDataFlag::InterfaceIndex:
							return "Interface Idx";
						case EntityDataFlag::AssociationID:
							return "Association ID";
						case EntityDataFlag::EntityModelID:
							return "Entity Model ID";
						case EntityDataFlag::FirmwareVersion:
							return "Firmware Version";
						case EntityDataFlag::MediaClockReferenceID:
							return "Media Clock Reference ID";
						case EntityDataFlag::MediaClockReferenceStatus:
							return "Media Clock Reference Status";
						default:
							break;
					}
					break;
				default:
					break;
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
							case EntityDataFlag::GrandmasterID:
							{
								auto const& gptpInfo = entity.gptpInfo;

								if (!gptpInfo.empty())
								{
									// Search the first valid gPTP info
									for (auto const& [avbIndex, info] : gptpInfo)
									{
										if (info.grandmasterID)
										{
											return hive::modelsLibrary::helper::uniqueIdentifierToString(*info.grandmasterID);
										}
									}
								}
								return "N/A";
							}
							case EntityDataFlag::GPTPDomain:
							{
								auto const& gptpInfo = entity.gptpInfo;

								if (!gptpInfo.empty())
								{
									// Search the first valid gPTP info
									for (auto const& [avbIndex, info] : gptpInfo)
									{
										if (info.domainNumber)
										{
											return QString::number(*info.domainNumber);
										}
									}
								}
								return "N/A";
							}
							case EntityDataFlag::InterfaceIndex:
							{
								auto const& gptpInfo = entity.gptpInfo;

								if (!gptpInfo.empty())
								{
									// Search the first valid gPTP info
									for (auto const& [avbIndex, info] : gptpInfo)
									{
										return avbIndex == la::avdecc::entity::Entity::GlobalAvbInterfaceIndex ? "N/A" : QString::number(avbIndex);
									}
								}
								return "N/A";
							}
							case EntityDataFlag::AssociationID:
								return entity.associationID ? hive::modelsLibrary::helper::uniqueIdentifierToString(*entity.associationID) : "N/A";
							case EntityDataFlag::EntityModelID:
								return hive::modelsLibrary::helper::uniqueIdentifierToString(entity.entityModelID);
							case EntityDataFlag::FirmwareVersion:
								return entity.firmwareVersion ? *entity.firmwareVersion : "N/A";
							case EntityDataFlag::MediaClockReferenceID:
							{
								auto const& mediaClockReferences = entity.mediaClockReferences;

								if (!mediaClockReferences.empty())
								{
									// Search the first valid mcr
									for (auto const& [cdIndex, mcr] : mediaClockReferences)
									{
										return mcr.referenceIDString;
									}
								}
								return "N/A";
							}
							case EntityDataFlag::MediaClockReferenceStatus:
							{
								auto const& mediaClockReferences = entity.mediaClockReferences;

								if (!mediaClockReferences.empty())
								{
									// Search the first valid mcr
									for (auto const& [cdIndex, mcr] : mediaClockReferences)
									{
										return mcr.referenceStatus;
									}
								}
								return "";
							}
							default:
								break;
						}
						break;
					}
					case la::avdecc::utils::to_integral(QtUserRoles::LightImageRole):
					{
						switch (entityDataFlag)
						{
							case EntityDataFlag::EntityLogo:
							{
								if (entity.isAemSupported && entity.hasAnyConfigurationTree)
								{
									auto& logoCache = EntityLogoCache::getInstance();
									return logoCache.getImage(entity.entityID, EntityLogoCache::Type::Entity, true);
								}
								break;
							}
							case EntityDataFlag::Compatibility:
							{
								try
								{
									return s_compatibilityImagesLight.at(entity.protocolCompatibility);
								}
								catch (std::out_of_range const&)
								{
									AVDECC_ASSERT(false, "Image missing");
									return {};
								}
							}
							case EntityDataFlag::AcquireState:
							{
								try
								{
									return s_excusiveAccessStateImagesLight.at(entity.acquireInfo.state);
								}
								catch (std::out_of_range const&)
								{
									AVDECC_ASSERT(false, "Image missing");
									return {};
								}
							}
							case EntityDataFlag::LockState:
							{
								try
								{
									return s_excusiveAccessStateImagesLight.at(entity.lockInfo.state);
								}
								catch (std::out_of_range const&)
								{
									AVDECC_ASSERT(false, "Image missing");
									return {};
								}
							}
							default:
								break;
						}
						break;
					}
					case la::avdecc::utils::to_integral(QtUserRoles::DarkImageRole):
					{
						switch (entityDataFlag)
						{
							case EntityDataFlag::EntityLogo:
							{
								if (entity.isAemSupported && entity.hasAnyConfigurationTree)
								{
									auto& logoCache = EntityLogoCache::getInstance();
									return logoCache.getImage(entity.entityID, EntityLogoCache::Type::Entity, true);
								}
								break;
							}
							case EntityDataFlag::Compatibility:
							{
								try
								{
									return s_compatibilityImagesDark.at(entity.protocolCompatibility);
								}
								catch (std::out_of_range const&)
								{
									AVDECC_ASSERT(false, "Image missing");
									return {};
								}
							}
							case EntityDataFlag::AcquireState:
							{
								try
								{
									return s_excusiveAccessStateImagesDark.at(entity.acquireInfo.state);
								}
								catch (std::out_of_range const&)
								{
									AVDECC_ASSERT(false, "Image missing");
									return {};
								}
							}
							case EntityDataFlag::LockState:
							{
								try
								{
									return s_excusiveAccessStateImagesDark.at(entity.lockInfo.state);
								}
								catch (std::out_of_range const&)
								{
									AVDECC_ASSERT(false, "Image missing");
									return {};
								}
							}
							default:
								break;
						}
						break;
					}
					case Qt::ToolTipRole:
					{
						switch (entityDataFlag)
						{
							case EntityDataFlag::Compatibility:
							{
								switch (entity.protocolCompatibility)
								{
									case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::Misbehaving:
										return "Entity is sending incoherent values that can cause undefined behavior";
									case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::Milan:
									case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanRedundant:
										return "MILAN compatible";
									case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanCertified:
									case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanCertifiedRedundant:
										return "MILAN certified";
									case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanWarning:
									case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanWarningRedundant:
										return "MILAN with warnings";
									case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::IEEE:
										return "IEEE 1722.1 compatible";
									default:
										return "Not fully IEEE 1722.1 compliant";
								}
							}
							case EntityDataFlag::AcquireState:
								return entity.acquireInfo.tooltip;
							case EntityDataFlag::LockState:
								return entity.lockInfo.tooltip;
							case EntityDataFlag::GrandmasterID:
							case EntityDataFlag::GPTPDomain:
							case EntityDataFlag::InterfaceIndex:
							{
								auto const& gptpInfo = entity.gptpInfo;

								if (!gptpInfo.empty())
								{
									auto list = QStringList{};

									for (auto const& [avbIndex, info] : gptpInfo)
									{
										if (info.grandmasterID && info.domainNumber)
										{
											if (avbIndex == la::avdecc::entity::Entity::GlobalAvbInterfaceIndex)
											{
												list << QString{ "Global gPTP: %1 / %2" }.arg(hive::modelsLibrary::helper::uniqueIdentifierToString(*info.grandmasterID)).arg(*info.domainNumber);
											}
											else
											{
												list << QString{ "gPTP for index %1: %2 / %3" }.arg(avbIndex).arg(hive::modelsLibrary::helper::uniqueIdentifierToString(*info.grandmasterID)).arg(*info.domainNumber);
											}
										}
									}

									if (!list.isEmpty())
									{
										return list.join('\n');
									}
								}
								return "Not set by the entity";
							}
							case EntityDataFlag::MediaClockReferenceID:
							case EntityDataFlag::MediaClockReferenceStatus:
							{
								auto const& mediaClockReferences = entity.mediaClockReferences;

								if (!mediaClockReferences.empty())
								{
									auto list = QStringList{};

									for (auto const& [cdIndex, mcr] : mediaClockReferences)
									{
										list << QString{ "Reference for domain %1: %2" }.arg(cdIndex).arg(mcr.referenceStatus);
									}

									if (!list.isEmpty())
									{
										return list.join('\n');
									}
								}
								return "Undefined";
							}
							default:
								break;
						}
						break;
					}
					case la::avdecc::utils::to_integral(QtUserRoles::ErrorRole):
						return entity.hasStatisticsError || entity.hasRedundancyWarning || !entity.streamsWithErrorCounter.empty() || !entity.streamsWithLatencyError.empty();
					case la::avdecc::utils::to_integral(QtUserRoles::IdentificationRole):
						return entity.isIdentifying;
					case la::avdecc::utils::to_integral(QtUserRoles::SubscribedUnsolRole):
						return entity.isSubscribedToUnsol;
					case la::avdecc::utils::to_integral(QtUserRoles::IsVirtualRole):
						return entity.isVirtual;
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
std::optional<std::pair<DiscoveredEntitiesTableModel::EntityDataFlag, RolesList>> DiscoveredEntitiesTableModel::dataChangedInfoForFlag(ChangedInfoFlag const flag) const noexcept
{
	switch (flag)
	{
		case ChangedInfoFlag::Name:
			return std::make_pair(EntityDataFlag::Name, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::GroupName:
			return std::make_pair(EntityDataFlag::Group, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::SubscribedToUnsol:
			return std::make_pair(EntityDataFlag::All, RolesList{ la::avdecc::utils::to_integral(QtUserRoles::SubscribedUnsolRole) });
		case ChangedInfoFlag::Compatibility:
			return std::make_pair(EntityDataFlag::Compatibility, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::EntityCapabilities:
			// TODO (not displayed yet)
			break;
		case ChangedInfoFlag::AcquireState:
			return std::make_pair(EntityDataFlag::AcquireState, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::OwningController:
			return std::make_pair(EntityDataFlag::AcquireState, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::LockedState:
			return std::make_pair(EntityDataFlag::LockState, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::LockingController:
			return std::make_pair(EntityDataFlag::LockState, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::GrandmasterID:
			return std::make_pair(EntityDataFlag::GrandmasterID, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::GPTPDomain:
			return std::make_pair(EntityDataFlag::GPTPDomain, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::InterfaceIndex:
			return std::make_pair(EntityDataFlag::InterfaceIndex, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::AssociationID:
			return std::make_pair(EntityDataFlag::AssociationID, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::MediaClockReferenceID:
			return std::make_pair(EntityDataFlag::MediaClockReferenceID, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::MediaClockReferenceStatus:
			return std::make_pair(EntityDataFlag::MediaClockReferenceStatus, RolesList{ Qt::DisplayRole });
		case ChangedInfoFlag::Identification:
			return std::make_pair(EntityDataFlag::EntityID, RolesList{ la::avdecc::utils::to_integral(QtUserRoles::IdentificationRole) });
		case ChangedInfoFlag::StatisticsError:
			return std::make_pair(EntityDataFlag::EntityID, RolesList{ la::avdecc::utils::to_integral(QtUserRoles::ErrorRole) });
		case ChangedInfoFlag::RedundancyWarning:
			return std::make_pair(EntityDataFlag::EntityID, RolesList{ la::avdecc::utils::to_integral(QtUserRoles::ErrorRole) });
		case ChangedInfoFlag::StreamInputCountersError:
			return std::make_pair(EntityDataFlag::EntityID, RolesList{ la::avdecc::utils::to_integral(QtUserRoles::ErrorRole) });
		case ChangedInfoFlag::StreamInputLatencyError:
			return std::make_pair(EntityDataFlag::EntityID, RolesList{ la::avdecc::utils::to_integral(QtUserRoles::ErrorRole) });
		default:
			AVDECC_ASSERT(false, "Unhandled");
			break;
	}
	return {};
}

} // namespace widgetModelsLibrary
} // namespace hive
