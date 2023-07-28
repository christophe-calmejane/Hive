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

#include "discoveredEntities/view.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/hiveLogItems.hpp"
#include "defaults.hpp"

#include <hive/modelsLibrary/controllerManager.hpp>

namespace discoveredEntities
{
View::View(QWidget* parent)
	: qtMate::widgets::TableView{ parent }
{
	// Setup model with proxy
	_proxyModel.setSourceModel(&_controllerModel);
	setModel(&_proxyModel);
}

View::~View() {}

void View::setupView(hive::VisibilityDefaults const& defaults) noexcept
{
	// Set selection behavior
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setFocusPolicy(Qt::ClickFocus);

	// Set delegate for the entire table
	setItemDelegate(&_controllerModelItemDelegate);

	// Set dynamic header view
	_dynamicHeaderView.setSectionsClickable(true);
	_dynamicHeaderView.setHighlightSections(false);
	_dynamicHeaderView.setMandatorySection(ControllerModelEntityColumn_EntityID);

	// Configure sortable sections
	_headerSectionSortFilter.enable(ControllerModelEntityColumn_Compatibility);
	_headerSectionSortFilter.enable(ControllerModelEntityColumn_EntityID);
	_headerSectionSortFilter.enable(ControllerModelEntityColumn_Name);
	_headerSectionSortFilter.enable(ControllerModelEntityColumn_Group);
	_headerSectionSortFilter.enable(ControllerModelEntityColumn_GrandmasterID);
	_headerSectionSortFilter.enable(ControllerModelEntityColumn_AssociationID);
	_headerSectionSortFilter.enable(ControllerModelEntityColumn_EntityModelID);
	_headerSectionSortFilter.enable(ControllerModelEntityColumn_FirmwareVersion);
	_headerSectionSortFilter.enable(ControllerModelEntityColumn_MediaClockID);
	_headerSectionSortFilter.enable(ControllerModelEntityColumn_MediaClockName);

	// Set Horizontal header view to our dynamic one
	setHorizontalHeader(&_dynamicHeaderView);

	// Disable vertical header row resizing
	verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
	verticalHeader()->setDefaultSectionSize(34);

	// Enable sorting
	setSortingEnabled(true);

	// Enable context menu
	setContextMenuPolicy(Qt::CustomContextMenu);

	// Initialize UI defaults
	setColumnHidden(ControllerModelEntityColumn_EntityLogo, !defaults.controllerTableView_EntityLogo_Visible);
	setColumnHidden(ControllerModelEntityColumn_Compatibility, !defaults.controllerTableView_Compatibility_Visible);
	setColumnHidden(ControllerModelEntityColumn_Name, !defaults.controllerTableView_Name_Visible);
	setColumnHidden(ControllerModelEntityColumn_Group, !defaults.controllerTableView_Group_Visible);
	setColumnHidden(ControllerModelEntityColumn_AcquireState, !defaults.controllerTableView_AcquireState_Visible);
	setColumnHidden(ControllerModelEntityColumn_LockState, !defaults.controllerTableView_LockState_Visible);
	setColumnHidden(ControllerModelEntityColumn_GrandmasterID, !defaults.controllerTableView_GrandmasterID_Visible);
	setColumnHidden(ControllerModelEntityColumn_GPTPDomain, !defaults.controllerTableView_GptpDomain_Visible);
	setColumnHidden(ControllerModelEntityColumn_InterfaceIndex, !defaults.controllerTableView_InterfaceIndex_Visible);
	setColumnHidden(ControllerModelEntityColumn_MacAddress, !defaults.controllerTableView_MacAddress_Visible);
	setColumnHidden(ControllerModelEntityColumn_AssociationID, !defaults.controllerTableView_AssociationID_Visible);
	setColumnHidden(ControllerModelEntityColumn_EntityModelID, !defaults.controllerTableView_EntityModelID_Visible);
	setColumnHidden(ControllerModelEntityColumn_FirmwareVersion, !defaults.controllerTableView_FirmwareVersion_Visible);
	setColumnHidden(ControllerModelEntityColumn_MediaClockID, !defaults.controllerTableView_MediaClockMasterID_Visible);
	setColumnHidden(ControllerModelEntityColumn_MediaClockName, !defaults.controllerTableView_MediaClockMasterName_Visible);
	setColumnHidden(ControllerModelEntityColumn_ClockDomainLockState, !defaults.controllerTableView_ClockDomainLockState_Visible);

	setColumnWidth(ControllerModelEntityColumn_EntityLogo, defaults::ui::AdvancedView::ColumnWidth_Logo);
	setColumnWidth(ControllerModelEntityColumn_Compatibility, defaults::ui::AdvancedView::ColumnWidth_Compatibility);
	setColumnWidth(ControllerModelEntityColumn_EntityID, defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	setColumnWidth(ControllerModelEntityColumn_Name, defaults::ui::AdvancedView::ColumnWidth_Name);
	setColumnWidth(ControllerModelEntityColumn_Group, defaults::ui::AdvancedView::ColumnWidth_Group);
	setColumnWidth(ControllerModelEntityColumn_AcquireState, defaults::ui::AdvancedView::ColumnWidth_SquareIcon);
	setColumnWidth(ControllerModelEntityColumn_LockState, defaults::ui::AdvancedView::ColumnWidth_SquareIcon);
	setColumnWidth(ControllerModelEntityColumn_GrandmasterID, defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	setColumnWidth(ControllerModelEntityColumn_GPTPDomain, defaults::ui::AdvancedView::ColumnWidth_GPTPDomain);
	setColumnWidth(ControllerModelEntityColumn_InterfaceIndex, defaults::ui::AdvancedView::ColumnWidth_InterfaceIndex);
	setColumnWidth(ControllerModelEntityColumn_MacAddress, defaults::ui::AdvancedView::ColumnWidth_MacAddress);
	setColumnWidth(ControllerModelEntityColumn_AssociationID, defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	setColumnWidth(ControllerModelEntityColumn_EntityModelID, defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	setColumnWidth(ControllerModelEntityColumn_FirmwareVersion, defaults::ui::AdvancedView::ColumnWidth_Firmware);
	setColumnWidth(ControllerModelEntityColumn_MediaClockID, defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	setColumnWidth(ControllerModelEntityColumn_MediaClockName, defaults::ui::AdvancedView::ColumnWidth_Name);
	setColumnWidth(ControllerModelEntityColumn_ClockDomainLockState, defaults::ui::AdvancedView::ColumnWidth_SquareIcon);

	// Connect all signals
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();

	// Connect the item delegates with theme color changes
	connect(&_settingsSignaler, &SettingsSignaler::themeColorNameChanged, &_controllerModelItemDelegate, &hive::widgetModelsLibrary::DiscoveredEntitiesTableItemDelegate::setThemeColorName);

	// Listen for entity going offline
	connect(&manager, &hive::modelsLibrary::ControllerManager::entityOffline, this,
		[this](la::avdecc::UniqueIdentifier const eid)
		{
			// The currently selected entity is going offline
			if (eid == _selectedControlledEntity)
			{
				// Force deselecting the view, before the entity is removed from the list, otherwise another entity will automatically be selected (not desirable)
				clearSelection();
			}
		});

	// Listen for model reset
	connect(&_controllerModel, &QAbstractItemModel::modelAboutToBeReset, this,
		[this]()
		{
			// Clear selected entity
			clearSelection();
		});

	// Listen for selection change
	connect(selectionModel(), &QItemSelectionModel::currentChanged, this,
		[this](QModelIndex const& current, QModelIndex const& previous)
		{
			auto const entityOpt = getEntityAtIndex(current);
			auto previousEntityID = _selectedControlledEntity;

			// Selection index is invalid (ie. no selection), or the currently selected entity doesn't exist
			if (!current.isValid() || !entityOpt)
			{
				// Set currently selected ControlledEntity to nothing
				_selectedControlledEntity = la::avdecc::UniqueIdentifier{};
			}
			else
			{
				// Set currently selected ControlledEntity to the new entityID
				auto const& entity = (*entityOpt).get();
				auto const entityID = entity.entityID;
				_selectedControlledEntity = entityID;
			}

			if (previousEntityID != _selectedControlledEntity)
			{
				emit selectedControlledEntityChanged(_selectedControlledEntity);
			}
		});

	// Automatic saving of dynamic header state when changed
	connect(&_dynamicHeaderView, &qtMate::widgets::DynamicHeaderView::sectionChanged, this, &View::saveDynamicHeaderState);
	connect(&_dynamicHeaderView, &qtMate::widgets::DynamicHeaderView::sectionClicked, this, &View::saveDynamicHeaderState);

	connect(this, &QTableView::doubleClicked, this,
		[this](QModelIndex const& index)
		{
			auto const entityOpt = getEntityAtIndex(index);

			if (entityOpt)
			{
				emit doubleClicked(entityOpt->get().entityID);
			}
		});

	connect(this, &QTableView::customContextMenuRequested, this,
		[this](QPoint const& pos)
		{
			auto const index = indexAt(pos);
			auto const entityOpt = getEntityAtIndex(index);
			if (entityOpt)
			{
				emit contextMenuRequested(entityOpt->get(), pos);
			}
		});

	// Start the settings signaler service (will trigger all known signals)
	_settingsSignaler.start();
}

void View::restoreState() noexcept
{
	auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	_dynamicHeaderView.restoreState(settings->getValue(settings::ControllerDynamicHeaderViewState).toByteArray());
}

la::avdecc::UniqueIdentifier View::selectedControlledEntity() const noexcept
{
	return _selectedControlledEntity;
}

void View::selectControlledEntity(la::avdecc::UniqueIdentifier const entityID) noexcept
{
	auto const index = indexOf(entityID);

	if (index.isValid())
	{
		setCurrentIndex(index);
	}
}

void View::saveDynamicHeaderState() const noexcept
{
	auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	settings->setValue(settings::ControllerDynamicHeaderViewState, _dynamicHeaderView.saveState());
}

void View::clearSelection() noexcept
{
	// Clear selected index
	auto const invalidIndex = QModelIndex{};
	setCurrentIndex(invalidIndex);
	// And selected entity
	_selectedControlledEntity = la::avdecc::UniqueIdentifier{};
	// Signal the change
	emit selectedControlledEntityChanged(_selectedControlledEntity);
}

std::optional<std::reference_wrapper<hive::modelsLibrary::DiscoveredEntitiesModel::Entity const>> View::getEntityAtIndex(QModelIndex const& index) const noexcept
{
	auto const* const m = static_cast<QSortFilterProxyModel const*>(model());
	auto const sourceIndex = m->mapToSource(index);
	return _controllerModel.entity(sourceIndex.row());
}

QModelIndex View::indexOf(la::avdecc::UniqueIdentifier const entityID) const noexcept
{
	// When converting QModelIndex from Source to Proxy, we must convert using our Internal Proxy (_proxyModel) first as we are getting indexes from _controllerModel directly
	// Then we have to use View's model() to also convert (if model is NOT our proxy), the instantiator might have installed another proxy
	auto destIndex = _proxyModel.mapFromSource(_controllerModel.indexOf(entityID));

	auto const* const m = static_cast<QSortFilterProxyModel const*>(model());
	// Another proxy has been installed on top of our internal proxy
	if (m != &_proxyModel)
	{
		destIndex = m->mapFromSource(destIndex);
	}

	return destIndex;
}

void View::showEvent(QShowEvent* event)
{
	qtMate::widgets::TableView::showEvent(event);

	static std::once_flag once;
	std::call_once(once,
		[this]()
		{
			// Set default sort section, if current is unsortable
			if (!_headerSectionSortFilter.isEnabled(_dynamicHeaderView.sortIndicatorSection()))
			{
				_dynamicHeaderView.setSortIndicator(ControllerModelEntityColumn_EntityID, Qt::SortOrder::DescendingOrder);
				saveDynamicHeaderState();
			}
		});
}

void View::keyReleaseEvent(QKeyEvent* event)
{
	// If the user pressed the delete key, delete the selected entity
	if (event->key() == Qt::Key::Key_Delete)
	{
		auto const index = currentIndex();
		auto const entityOpt = getEntityAtIndex(index);

		if (entityOpt)
		{
			emit deleteEntityRequested(entityOpt->get().entityID);
		}
	}
}

} // namespace discoveredEntities
