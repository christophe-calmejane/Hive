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
	_proxyModel.setSourceModel(&_model);
	setModel(&_proxyModel);
}

View::~View() {}

void View::setupView(hive::VisibilityDefaults const& defaults) noexcept
{
	// Set selection behavior
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setFocusPolicy(Qt::ClickFocus);

	// Set column delegates
	setItemDelegateForColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityLogo), &_imageItemDelegate);
	setItemDelegateForColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Compatibility), &_imageItemDelegate);
	setItemDelegateForColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AcquireState), &_imageItemDelegate);
	setItemDelegateForColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::LockState), &_imageItemDelegate);
	setItemDelegateForColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityID), &_errorItemDelegate);

	// Set dynamic header view
	_dynamicHeaderView.setSectionsClickable(true);
	_dynamicHeaderView.setHighlightSections(false);
	_dynamicHeaderView.setMandatorySection(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityID));

	// Configure sortable sections
	_headerSectionSortFilter.enable(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Compatibility));
	_headerSectionSortFilter.enable(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityID));
	_headerSectionSortFilter.enable(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Name));
	_headerSectionSortFilter.enable(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Group));
	_headerSectionSortFilter.enable(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GrandmasterID));
	_headerSectionSortFilter.enable(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterID));
	_headerSectionSortFilter.enable(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterName));

	// Set Horizontal header view to our dynamic one
	setHorizontalHeader(&_dynamicHeaderView);

	// Disable vertical header row resizing
	verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

	// Enable sorting
	setSortingEnabled(true);

	// Enable context menu
	setContextMenuPolicy(Qt::CustomContextMenu);

	// Initialize UI defaults
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityLogo), !defaults.controllerTableView_EntityLogo_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Compatibility), !defaults.controllerTableView_Compatibility_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Name), !defaults.controllerTableView_Name_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Group), !defaults.controllerTableView_Group_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AcquireState), !defaults.controllerTableView_AcquireState_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::LockState), !defaults.controllerTableView_LockState_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GrandmasterID), !defaults.controllerTableView_GrandmasterID_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GptpDomain), !defaults.controllerTableView_GptpDomain_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::InterfaceIndex), !defaults.controllerTableView_InterfaceIndex_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AssociationID), !defaults.controllerTableView_AssociationID_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterID), !defaults.controllerTableView_MediaClockMasterID_Visible);
	setColumnHidden(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterName), !defaults.controllerTableView_MediaClockMasterName_Visible);

	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityLogo), defaults::ui::AdvancedView::ColumnWidth_Logo);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Compatibility), defaults::ui::AdvancedView::ColumnWidth_Compatibility);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityID), defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Name), defaults::ui::AdvancedView::ColumnWidth_Name);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Group), defaults::ui::AdvancedView::ColumnWidth_Group);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AcquireState), defaults::ui::AdvancedView::ColumnWidth_ExclusiveAccessState);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::LockState), defaults::ui::AdvancedView::ColumnWidth_ExclusiveAccessState);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GrandmasterID), defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::GptpDomain), defaults::ui::AdvancedView::ColumnWidth_GPTPDomain);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::InterfaceIndex), defaults::ui::AdvancedView::ColumnWidth_InterfaceIndex);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::AssociationID), defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterID), defaults::ui::AdvancedView::ColumnWidth_UniqueIdentifier);
	setColumnWidth(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::MediaClockMasterName), defaults::ui::AdvancedView::ColumnWidth_Name);

	// Connect all signals

	// Connect the error item delegate with theme color changes
	connect(&_settingsSignaler, &SettingsSignaler::themeColorNameChanged, &_errorItemDelegate, &hive::widgetModelsLibrary::ErrorItemDelegate::setThemeColorName);

	// Listen for entity going offline
	connect(&_model, &avdecc::ControllerModel::entityOffline, this,
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
	connect(&_model, &QAbstractItemModel::modelAboutToBeReset, this,
		[this]()
		{
			// Clear selected entity
			clearSelection();
		});

	// Listen for selection change
	connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
		[this](QItemSelection const& selected, QItemSelection const& deselected)
		{
			auto index = QModelIndex{};
			auto const& selectedIndexes = selected.indexes();
			if (!selectedIndexes.empty())
			{
				index = *selectedIndexes.begin();
			}

			auto const entityID = controlledEntityIDAtIndex(index);
			auto previousEntityID = _selectedControlledEntity;

			// Selection index is invalid (ie. no selection), or the currently selected entity doesn't exist
			if (!index.isValid() || !entityID.isValid())
			{
				// Set currently selected ControlledEntity to nothing
				_selectedControlledEntity = la::avdecc::UniqueIdentifier{};
			}
			else
			{
				// Set currently selected ControlledEntity to the new entityID
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
			auto const entityID = controlledEntityIDAtIndex(index);

			if (entityID.isValid())
			{
				emit doubleClicked(entityID);
			}
		});

	connect(this, &QTableView::customContextMenuRequested, this,
		[this](QPoint const& pos)
		{
			auto const index = indexAt(pos);
			auto const entityID = controlledEntityIDAtIndex(index);
			emit contextMenuRequested(entityID, pos);
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

la::avdecc::UniqueIdentifier View::controlledEntityIDAtIndex(QModelIndex const& index) const noexcept
{
	auto const* m = static_cast<QSortFilterProxyModel const*>(model());
	auto const sourceIndex = m->mapToSource(index);
	return _model.getControlledEntityID(sourceIndex);
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
				_dynamicHeaderView.setSortIndicator(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::EntityID), Qt::SortOrder::DescendingOrder);
				saveDynamicHeaderState();
			}
		});
}

} // namespace discoveredEntities
