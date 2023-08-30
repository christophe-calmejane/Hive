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

#pragma once

#include "settingsManager/settings.hpp"
#include "settingsManager/settingsSignaler.hpp"
#include "visibilitySettings.hpp"

#include <QtMate/widgets/tableView.hpp>
#include <QtMate/widgets/dynamicHeaderView.hpp>
#include <QtMate/widgets/headerViewSortSectionFilter.hpp>
#include <hive/widgetModelsLibrary/discoveredEntitiesTableModel.hpp>
#include <hive/widgetModelsLibrary/discoveredEntitiesTableItemDelegate.hpp>

#include <QSortFilterProxyModel>
#include <QKeyEvent>
#include <QShowEvent>

namespace discoveredEntities
{
class View final : public qtMate::widgets::TableView
{
	Q_OBJECT
	static constexpr auto ControllerModelEntityDataFlags = hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlags{ hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityStatus, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityLogo, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Compatibility, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityID, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Name, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Group, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::AcquireState,
		hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::LockState, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::GrandmasterID, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::GPTPDomain, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::InterfaceIndex, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::MacAddress, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::AssociationID, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityModelID, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::FirmwareVersion, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::MediaClockReferenceID,
		hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::MediaClockReferenceName, hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::ClockDomainLockState };

public:
	static constexpr auto ControllerModelEntityColumn_EntityStatus = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityStatus);
	static constexpr auto ControllerModelEntityColumn_EntityLogo = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityLogo);
	static constexpr auto ControllerModelEntityColumn_Compatibility = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Compatibility);
	static constexpr auto ControllerModelEntityColumn_EntityID = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityID);
	static constexpr auto ControllerModelEntityColumn_Name = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Name);
	static constexpr auto ControllerModelEntityColumn_Group = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::Group);
	static constexpr auto ControllerModelEntityColumn_AcquireState = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::AcquireState);
	static constexpr auto ControllerModelEntityColumn_LockState = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::LockState);
	static constexpr auto ControllerModelEntityColumn_GrandmasterID = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::GrandmasterID);
	static constexpr auto ControllerModelEntityColumn_GPTPDomain = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::GPTPDomain);
	static constexpr auto ControllerModelEntityColumn_InterfaceIndex = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::InterfaceIndex);
	static constexpr auto ControllerModelEntityColumn_MacAddress = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::MacAddress);
	static constexpr auto ControllerModelEntityColumn_AssociationID = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::AssociationID);
	static constexpr auto ControllerModelEntityColumn_EntityModelID = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::EntityModelID);
	static constexpr auto ControllerModelEntityColumn_FirmwareVersion = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::FirmwareVersion);
	static constexpr auto ControllerModelEntityColumn_MediaClockID = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::MediaClockReferenceID);
	static constexpr auto ControllerModelEntityColumn_MediaClockName = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::MediaClockReferenceName);
	static constexpr auto ControllerModelEntityColumn_ClockDomainLockState = ControllerModelEntityDataFlags.getBitSetPosition(hive::widgetModelsLibrary::DiscoveredEntitiesTableModel::EntityDataFlag::ClockDomainLockState);

	View(QWidget* parent = nullptr);
	virtual ~View();

	void setupView(hive::VisibilityDefaults const& defaults, bool const firstSetup) noexcept;
	void restoreState() noexcept;
	la::avdecc::UniqueIdentifier selectedControlledEntity() const noexcept;
	void selectControlledEntity(la::avdecc::UniqueIdentifier const entityID) noexcept;

	// Public signals
	Q_SIGNAL void selectedControlledEntityChanged(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void doubleClicked(la::avdecc::UniqueIdentifier const entityID);
	Q_SIGNAL void contextMenuRequested(hive::modelsLibrary::DiscoveredEntitiesModel::Entity const& entity, QPoint const& pos);
	Q_SIGNAL void deleteEntityRequested(la::avdecc::UniqueIdentifier const entityID);

private:
	void saveDynamicHeaderState() const noexcept;
	void clearSelection() noexcept;
	std::optional<std::reference_wrapper<hive::modelsLibrary::DiscoveredEntitiesModel::Entity const>> getEntityAtIndex(QModelIndex const& index) const noexcept;
	QModelIndex indexOf(la::avdecc::UniqueIdentifier const entityID) const noexcept;

	// qtMate::widgets::TableView overrides
	virtual void showEvent(QShowEvent* event) override;
	virtual void keyReleaseEvent(QKeyEvent* event) override;

private:
	QSortFilterProxyModel _proxyModel{};
	qtMate::widgets::DynamicHeaderView _dynamicHeaderView{ Qt::Horizontal, this };
	qtMate::widgets::HeaderViewSortSectionFilter _headerSectionSortFilter{ &_dynamicHeaderView };
	hive::widgetModelsLibrary::DiscoveredEntitiesTableModel _controllerModel{ ControllerModelEntityDataFlags };
	hive::widgetModelsLibrary::DiscoveredEntitiesTableItemDelegate _controllerModelItemDelegate{ qtMate::material::color::Palette::name(qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>()->getValue(settings::General_ThemeColorIndex.name).toInt()), this };
	SettingsSignaler _settingsSignaler{};
	la::avdecc::UniqueIdentifier _selectedControlledEntity{};
	bool _firstSetup{ false };
};

} // namespace discoveredEntities
