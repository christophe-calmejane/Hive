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

#include "mediaClockManagementDialog.hpp"

#include <QPushButton>
#include <QMenu>

#include "ui_mediaClockManagementDialog.h"
#include "internals/config.hpp"
#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include "settingsManager/settings.hpp"
#include "avdecc/mcDomainManager.hpp"
#include "mediaClock/mediaClockManagementDialog.hpp"
#include "mediaClock/domainTreeModel.hpp"
#include "mediaClock/unassignedListModel.hpp"
#include "entityLogoCache.hpp"

class MediaClockManagementDialogImpl final : private Ui::MediaClockManagementDialog, public QObject
{
public:
	/**
	* Constructor.
	* Sets up the ui.
	* Fills the models.
	* Sets up connections to handle ui signals.
	*/
	MediaClockManagementDialogImpl(::MediaClockManagementDialog* parent)
	{
		// Link UI
		setupUi(parent);

		auto& mediaClockManager = avdecc::mediaClock::MCDomainManager::getInstance();
		auto domains = mediaClockManager.createMediaClockDomainModel();

		treeViewMediaClockDomains->setModel(&_domainTreeModel);
		auto* delegateDomainEntity = new SampleRateDomainDelegate(treeViewMediaClockDomains);
		treeViewMediaClockDomains->setItemDelegateForColumn(static_cast<int>(DomainTreeModelColumn::Domain), delegateDomainEntity);

		auto* delegateMCMaster = new MCMasterSelectionDelegate(treeViewMediaClockDomains);
		treeViewMediaClockDomains->setItemDelegateForColumn(static_cast<int>(DomainTreeModelColumn::MediaClockMaster), delegateMCMaster);

		treeViewMediaClockDomains->setEditTriggers(QAbstractItemView::AllEditTriggers);

		connect(treeViewMediaClockDomains->selectionModel(), &QItemSelectionModel::currentChanged, &_domainTreeModel, &DomainTreeModel::handleClick);
		connect(treeViewMediaClockDomains->selectionModel(), &QItemSelectionModel::currentColumnChanged, &_domainTreeModel, &DomainTreeModel::handleClick);
		connect(treeViewMediaClockDomains->selectionModel(), &QItemSelectionModel::currentRowChanged, &_domainTreeModel, &DomainTreeModel::handleClick);
		_domainTreeModel.setMediaClockDomainModel(domains);

		treeViewMediaClockDomains->resizeColumnToContents((int)DomainTreeModelColumn::Domain);
		treeViewMediaClockDomains->resizeColumnToContents((int)DomainTreeModelColumn::MediaClockMaster);

		listView_UnassignedEntities->setModel(&_unassignedListModel);

		listView_UnassignedEntities->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(listView_UnassignedEntities, &QListView::customContextMenuRequested, this, &MediaClockManagementDialogImpl::onCustomContextMenuRequested);

		_unassignedListModel.setMediaClockDomainModel(domains);

		connect(button_AssignToDomain, &QPushButton::clicked, this, &MediaClockManagementDialogImpl::button_AssignToDomainClicked);
		connect(button_RemoveAssignment, &QPushButton::clicked, this, &MediaClockManagementDialogImpl::button_RemoveAssignmentClicked);
		connect(button_Add, &QPushButton::clicked, this, &MediaClockManagementDialogImpl::button_AddClicked);
		connect(button_Remove, &QPushButton::clicked, this, &MediaClockManagementDialogImpl::button_RemoveClicked);
		connect(button_Clear, &QPushButton::clicked, this, &MediaClockManagementDialogImpl::button_ClearClicked);
		connect(button_Clear, &QPushButton::clicked, this, &MediaClockManagementDialogImpl::button_ClearClicked);
		connect(button_ApplyChanges, &QPushButton::clicked, this, &MediaClockManagementDialogImpl::button_ApplyChangesClicked);
		connect(button_DiscardChanges, &QPushButton::clicked, this, &MediaClockManagementDialogImpl::button_DiscardChangesClicked);

		connect(listView_UnassignedEntities->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MediaClockManagementDialogImpl::handleUnassignedListSelectionChanged);
		connect(treeViewMediaClockDomains->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MediaClockManagementDialogImpl::handleDomainTreeSelectionChanged);
		connect(&_domainTreeModel, &DomainTreeModel::sampleRateSettingChanged, this, &MediaClockManagementDialogImpl::handleDomainTreeDataChanged);
		connect(&_domainTreeModel, &DomainTreeModel::mcMasterSelectionChanged, this, &MediaClockManagementDialogImpl::handleDomainTreeDataChanged);
		connect(&_domainTreeModel, &DomainTreeModel::triggerResizeColumns, this, &MediaClockManagementDialogImpl::resizeMCTreeViewColumns);

		button_AssignToDomain->setEnabled(false);
		button_RemoveAssignment->setEnabled(false);
		button_Remove->setEnabled(false);
		adjustButtonStates(false);
	}

	/**
	* Handles the click of the remove assignment button.
	* Removes the selected entity in the domain tree and adds it to the unassigned list.
	*/
	Q_SLOT void button_AssignToDomainClicked(bool checked)
	{
		auto const& entityIds = _unassignedListModel.getSelectedItems(listView_UnassignedEntities->selectionModel()->selection());
		for (auto const& entityId : entityIds)
		{
			bool success = _domainTreeModel.addEntityToSelection(treeViewMediaClockDomains->currentIndex(), entityId);
			if (success)
			{
				_unassignedListModel.removeEntity(entityId);
			}
		}

		adjustButtonStates(true);
	}

	/**
	* Handles the click of the remove assignment button.
	* Removes the selected entity in the domain tree and adds it to the unassigned list.
	*/
	Q_SLOT void button_RemoveAssignmentClicked()
	{
		auto entityDomainInfo = _domainTreeModel.getSelectedEntity(treeViewMediaClockDomains->currentIndex());
		if (!entityDomainInfo.first)
		{
			// no entity selected.
			return;
		}
		if (!avdecc::mediaClock::MCDomainManager::getInstance().isSingleAudioListener(entityDomainInfo.second))
		{
			// the entity is not added to the unassigned list if it doesn't have a clock source stream port.
			_unassignedListModel.addEntity(entityDomainInfo.second);
		}
		_domainTreeModel.removeEntity(*entityDomainInfo.first, entityDomainInfo.second);

		adjustButtonStates(true);
	}

	/**
	* Handles the click of the add button.
	* Removes the selected domain and moves the assinged entities to the unassigned list.
	*/
	Q_SLOT void button_AddClicked()
	{
		_domainTreeModel.addNewDomain();

		adjustButtonStates(true);
	}

	/**
	* Handles the click of the remove button.
	* Removes the selected domain and moves the assinged entities to the unassigned list.
	*/
	Q_SLOT void button_RemoveClicked()
	{
		// remove domain
		auto entities = _domainTreeModel.removeSelectedDomain(treeViewMediaClockDomains->currentIndex());

		// add domain assigned entities back to the unassigned list.
		for (const auto& entityId : entities)
		{
			_unassignedListModel.addEntity(entityId);
		}

		adjustButtonStates(true);
	}

	/**
	* Handles the click of the clear button.
	* Removes all domains and moves all entities to the unassigned list.
	*/
	Q_SLOT void button_ClearClicked()
	{
		// remove domain
		auto entities = _domainTreeModel.removeAllDomains();

		// add domain assigned entities back to the unassigned list.
		for (const auto& entityId : entities)
		{
			_unassignedListModel.addEntity(entityId);
		}

		adjustButtonStates(true);
	}

	/**
	* Handles the click of the apply changes button.
	* Gathers the data from the models and calls the applyMediaClockDomainModel in the MediaClockConnectionManager.
	*/
	Q_SLOT void button_ApplyChangesClicked()
	{
		adjustButtonStates(false);

		auto mediaClockMappings = _domainTreeModel.createMediaClockMappings();
		auto unassignedEntities = _unassignedListModel.getAllItems();
		for (const auto& unassignedEntity : unassignedEntities)
		{
			mediaClockMappings.getEntityMediaClockMasterMappings().emplace(unassignedEntity, std::vector<avdecc::mediaClock::DomainIndex>());
		}


		auto& mediaClockManager = avdecc::mediaClock::MCDomainManager::getInstance();
		mediaClockManager.applyMediaClockDomainModel(mediaClockMappings);
	}

	/**
	* Handles the click of the discard changes button.
	* Loads the domain data again and assigns it to the models.
	*/
	Q_SLOT void button_DiscardChangesClicked()
	{
		adjustButtonStates(false);

		// read out again:
		auto& mediaClockManager = avdecc::mediaClock::MCDomainManager::getInstance();
		auto domains = mediaClockManager.createMediaClockDomainModel();

		// setup the models:
		_unassignedListModel.setMediaClockDomainModel(domains);
		_domainTreeModel.setMediaClockDomainModel(domains);
	}

	/**
	* Updates the assign button enabled state.
	*/
	Q_SLOT void handleUnassignedListSelectionChanged()
	{
		auto selectedDomain = _domainTreeModel.getSelectedDomain(treeViewMediaClockDomains->currentIndex());
		auto selectedEntity = _domainTreeModel.getSelectedEntity(treeViewMediaClockDomains->currentIndex());

		button_AssignToDomain->setEnabled((selectedDomain || selectedEntity.first) && !_unassignedListModel.getSelectedItems(listView_UnassignedEntities->selectionModel()->selection()).empty());
	}

	/**
	* Updates the unassign button enabled state.
	*/
	Q_SLOT void handleDomainTreeSelectionChanged()
	{
		auto selectedEntity = _domainTreeModel.getSelectedEntity(treeViewMediaClockDomains->currentIndex());
		auto selectedDomain = _domainTreeModel.getSelectedDomain(treeViewMediaClockDomains->currentIndex());

		// update unassign button
		button_RemoveAssignment->setEnabled(!!selectedEntity.first);

		// update assign button
		button_AssignToDomain->setEnabled((selectedDomain || selectedEntity.first) && !_unassignedListModel.getSelectedItems(listView_UnassignedEntities->selectionModel()->selection()).empty());

		// update remove button
		button_Remove->setEnabled(!!selectedDomain);
	}

	/**
	* Handles the change of any data inside the media clock domain tree model. Triggers state change of the buttons.
	*/
	Q_SLOT void handleDomainTreeDataChanged()
	{
		adjustButtonStates(true);
	}

	/**
	* Triggers a resize of the columns in the media clock domain tree view.
	*/
	Q_SLOT void resizeMCTreeViewColumns()
	{
		treeViewMediaClockDomains->resizeColumnToContents((int)DomainTreeModelColumn::Domain);
		treeViewMediaClockDomains->resizeColumnToContents((int)DomainTreeModelColumn::MediaClockMaster);
	}

	/**
	* Enables or disables the apply and discard buttons.
	* @param enabled Set button the given state.
	*/
	void adjustButtonStates(bool enabled)
	{
		button_ApplyChanges->setEnabled(enabled);
		button_DiscardChanges->setEnabled(enabled);
	}

	/**
	* Creates the context menu for the list view.
	* @param pos The position the user clicked on.
	*/
	void onCustomContextMenuRequested(QPoint const& pos)
	{
		QMenu contextMenu("Context menu", listView_UnassignedEntities);

		QAction createNewDomainAction("Create domain from selection", this);
		connect(&createNewDomainAction, &QAction::triggered, this, &MediaClockManagementDialogImpl::onCreateNewDomainActionTriggered);
		contextMenu.addAction(&createNewDomainAction);

		contextMenu.exec(listView_UnassignedEntities->mapToGlobal(pos));
	}

	/**
	* Handles the click of the context menu item.
	*/
	void onCreateNewDomainActionTriggered()
	{
		auto newDomainIndex = _domainTreeModel.addNewDomain();
		for (auto const& entityId : _unassignedListModel.getSelectedItems(listView_UnassignedEntities->selectionModel()->selection()))
		{
			bool success = _domainTreeModel.addEntityToDomain(newDomainIndex, entityId);
			if (success)
			{
				_unassignedListModel.removeEntity(entityId);
			}
		}
		adjustButtonStates(true);
	}

private:
	DomainTreeModel _domainTreeModel;
	UnassignedListModel _unassignedListModel;
};

/**
* Constructor.
*/
MediaClockManagementDialog::MediaClockManagementDialog(QWidget* parent)
	: QDialog(parent)
	, _pImpl(new MediaClockManagementDialogImpl(this))
{
	setWindowTitle(QCoreApplication::applicationName() + " Media Clock Management");
	layout()->setSizeConstraint(QLayout::SetFixedSize);
	setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);
}

/**
* Destructor.
*/
MediaClockManagementDialog::~MediaClockManagementDialog() noexcept
{
	delete _pImpl;
}
