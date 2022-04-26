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

#include "mediaClockManagementDialog.hpp"
#include "ui_mediaClockManagementDialog.h"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "avdecc/mcDomainManager.hpp"
#include "avdecc/helper.hpp"
#include "mediaClock/mediaClockManagementDialog.hpp"
#include "mediaClock/domainTreeModel.hpp"
#include "mediaClock/unassignedListModel.hpp"

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include <hive/modelsLibrary/helper.hpp>

#include <QPushButton>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>

#include <unordered_set>

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
		: _hasChanges(false)
	{
		// Link UI
		setupUi(parent);

		auto& mediaClockManager = avdecc::mediaClock::MCDomainManager::getInstance();
		auto domains = mediaClockManager.createMediaClockDomainModel();

		connect(&mediaClockManager, &avdecc::mediaClock::MCDomainManager::mediaClockConnectionsUpdate, this, &MediaClockManagementDialogImpl::mediaClockConnectionsUpdate);
		connect(&mediaClockManager, &avdecc::mediaClock::MCDomainManager::applyMediaClockDomainModelFinished, this, &MediaClockManagementDialogImpl::applyMediaClockDomainModelFinished);
		connect(&mediaClockManager, &avdecc::mediaClock::MCDomainManager::applyMediaClockDomainModelProgressUpdate, this, &MediaClockManagementDialogImpl::applyMediaClockDomainModelProgressUpdate);


		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityOffline, this, &MediaClockManagementDialogImpl::entityOffline);

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
		expandAllDomains();

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
		connect(&_unassignedListModel, &UnassignedListModel::domainSetupChanged, this, &MediaClockManagementDialogImpl::handleDomainTreeDataChanged);
		connect(&_domainTreeModel, &DomainTreeModel::domainSetupChanged, this, &MediaClockManagementDialogImpl::handleDomainTreeDataChanged);
		connect(&_domainTreeModel, &DomainTreeModel::triggerResizeColumns, this, &MediaClockManagementDialogImpl::resizeMCTreeViewColumns);
		connect(&_domainTreeModel, &DomainTreeModel::deselectAll, this, &MediaClockManagementDialogImpl::removeMcDomainTreeViewSelections);
		connect(&_domainTreeModel, &DomainTreeModel::expandDomain, this, &MediaClockManagementDialogImpl::expandDomain);

		// drag&drop support
		listView_UnassignedEntities->setDragEnabled(true);
		listView_UnassignedEntities->setDropIndicatorShown(true);
		listView_UnassignedEntities->setDragDropOverwriteMode(false);
		listView_UnassignedEntities->setAcceptDrops(true);
		listView_UnassignedEntities->setDefaultDropAction(Qt::MoveAction);

		treeViewMediaClockDomains->setDragEnabled(true);
		treeViewMediaClockDomains->setDropIndicatorShown(true);
		treeViewMediaClockDomains->setDragDropOverwriteMode(false);
		treeViewMediaClockDomains->setAcceptDrops(true);
		treeViewMediaClockDomains->setDefaultDropAction(Qt::MoveAction);

		treeViewMediaClockDomains->setCurrentIndex(_domainTreeModel.index(-1, -1)); // set selection index to invalid intitally
		button_AssignToDomain->setEnabled(false);
		button_RemoveAssignment->setEnabled(false);
		button_Remove->setEnabled(false);
		adjustButtonStates();

		QFont font("Material Icons");
		font.setBold(true);
		font.setStyleStrategy(QFont::PreferQuality);
		button_Remove->setFont(font);
		button_Remove->setText("remove");

		button_Add->setFont(font);
		button_Add->setText("add");

		button_RemoveAssignment->setFont(font);
		button_RemoveAssignment->setText("arrow_forward");
		button_AssignToDomain->setFont(font);
		button_AssignToDomain->setText("arrow_back");
	}

	void refreshModels()
	{
		// read out again:
		auto& mediaClockManager = avdecc::mediaClock::MCDomainManager::getInstance();
		auto domains = mediaClockManager.createMediaClockDomainModel();

		// setup the models:
		_unassignedListModel.setMediaClockDomainModel(domains);
		_domainTreeModel.setMediaClockDomainModel(domains);
		expandAllDomains();
		resizeMCTreeViewColumns();
	}

	/**
	* Gets the has changes state.
	*/
	bool hasChanges() const
	{
		return _hasChanges;
	}

	/**
	* Enables or disables the apply and discard buttons.
	* @param enabled Set button the given state.
	*/
	void adjustButtonStates()
	{
		button_ApplyChanges->setEnabled(_hasChanges);
		button_DiscardChanges->setEnabled(_hasChanges);
	}

	/**
	* Expands every item in the domain tree view.
	*/
	void expandAllDomains()
	{
		auto const& indexes = _domainTreeModel.match(_domainTreeModel.index(0, 0), Qt::DisplayRole, "*", -1, Qt::MatchWildcard | Qt::MatchRecursive);
		for (auto const& index : indexes)
		{
			treeViewMediaClockDomains->expand(index);
		}
	}

	// Slots

	/**
	* Handles the click of the remove assignment button.
	* Removes the selected entity in the domain tree and adds it to the unassigned list.
	*/
	void button_AssignToDomainClicked(bool /*checked*/)
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

		_hasChanges = true;
		adjustButtonStates();
	}

	/**
	* Handles the click of the remove assignment button.
	* Removes the selected entity in the domain tree and adds it to the unassigned list.
	*/
	void button_RemoveAssignmentClicked()
	{
		auto selectedEntities = _domainTreeModel.getSelectedEntityItems(treeViewMediaClockDomains->selectionModel()->selection());
		if (selectedEntities.empty())
		{
			return;
		}

		for (auto const& selectedEntity : selectedEntities)
		{
			auto const domainIndex = selectedEntity.first;
			auto const entityId = selectedEntity.second;

			// check if this entity occurs on other places in the tree.
			if (!_domainTreeModel.isEntityDoubled(entityId) && avdecc::mediaClock::MCDomainManager::getInstance().isMediaClockDomainManageable(entityId))
			{
				// the entity is not added to the unassigned list if it is classified as not manageable by MCMD.
				// If an entity cannot be added to a domain, the user should not be presented with it and then confused why he cannot use it.
				_unassignedListModel.addEntity(entityId);
			}
			_domainTreeModel.removeEntity(domainIndex, entityId);
		}

		_hasChanges = true;
		adjustButtonStates();
	}

	/**
	* Handles the click of the add button.
	* Removes the selected domain and moves the assinged entities to the unassigned list.
	*/
	void button_AddClicked()
	{
		auto domainIndex = _domainTreeModel.addNewDomain();
		expandDomain(_domainTreeModel.getDomainModelIndex(domainIndex));

		_hasChanges = true;
		adjustButtonStates();
	}

	/**
	* Handles the click of the remove button.
	* Removes the selected domain and moves the assinged entities to the unassigned list.
	*/
	void button_RemoveClicked()
	{
		auto selectedDomains = _domainTreeModel.getSelectedDomainItems(treeViewMediaClockDomains->selectionModel()->selection());
		if (selectedDomains.empty())
		{
			return;
		}

		for (auto const& domainIndex : selectedDomains)
		{
			// remove domain
			auto entities = _domainTreeModel.removeDomain(domainIndex);

			// add domain assigned entities back to the unassigned list.
			for (const auto& entityId : entities)
			{
				_unassignedListModel.addEntity(entityId);
			}
		}

		_hasChanges = true;
		adjustButtonStates();
	}

	/**
	* Handles the click of the clear button.
	* Removes all domains and moves all entities to the unassigned list.
	*/
	void button_ClearClicked()
	{
		// remove domain
		auto entities = _domainTreeModel.removeAllDomains();

		// add domain assigned entities back to the unassigned list.
		for (const auto& entityId : entities)
		{
			_unassignedListModel.addEntity(entityId);
		}

		_hasChanges = true;
		adjustButtonStates();
	}

	/**
	* Handles the click of the apply changes button.
	* Gathers the data from the models and calls the applyMediaClockDomainModel in the MediaClockConnectionManager.
	*/
	void button_ApplyChangesClicked()
	{
		// collect the current Media Clock Domain config scheme as is currently set by the user on UI
		auto mediaClockMappings = _domainTreeModel.createMediaClockMappings();
		auto unassignedEntities = _unassignedListModel.getAllItems();
		for (const auto& unassignedEntity : unassignedEntities)
		{
			mediaClockMappings.getEntityMediaClockMasterMappings().emplace(unassignedEntity, std::vector<avdecc::mediaClock::DomainIndex>());
		}

		auto& mediaClockManager = avdecc::mediaClock::MCDomainManager::getInstance();

		// validate mediaClockMappings regarding conflicts in modified SRs with StreamFormats of the Entities involved
		if (mediaClockManager.isMediaClockDomainConflictingWithStreamFormats(mediaClockMappings))
		{
			auto result = QMessageBox::warning(qobject_cast<QWidget*>(this), "", "The selected Media Clock Domain sample rates are conflicting with the stream formats of the devices belonging to them.\nContinue?", QMessageBox::StandardButton::Abort | QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Abort);
			if (result == QMessageBox::StandardButton::Abort)
			{
				return;
			}
		}

		_hasChanges = false;
		adjustButtonStates();

		_progressDialog = new QProgressDialog("Executing commands...", "Abort apply", 0, 100, qobject_cast<QWidget*>(this));
		_progressDialog->setMinimumWidth(350);
		_progressDialog->setWindowModality(Qt::WindowModal);
		_progressDialog->setMinimumDuration(500);
		mediaClockManager.applyMediaClockDomainModel(mediaClockMappings);
	}

	/**
	* Handles the click of the discard changes button.
	* Loads the domain data again and assigns it to the models.
	*/
	void button_DiscardChangesClicked()
	{
		_hasChanges = false;
		adjustButtonStates();

		refreshModels();
	}

	/**
	* Updates the assign button enabled state.
	*/
	void handleUnassignedListSelectionChanged()
	{
		auto const& assignedDomainSelections = _domainTreeModel.getSelectedDomainItems(treeViewMediaClockDomains->selectionModel()->selection());
		//auto const& assignedEntitySelections = _domainTreeModel.getSelectedEntityItems(treeViewMediaClockDomains->selectionModel()->selection());
		auto const& unassignedEntitySelections = _unassignedListModel.getSelectedItems(listView_UnassignedEntities->selectionModel()->selection());

		// update assign button
		button_AssignToDomain->setEnabled(assignedDomainSelections.size() == 1 && !unassignedEntitySelections.empty());
	}

	/**
	* Updates the unassign button enabled state.
	*/
	void handleDomainTreeSelectionChanged()
	{
		auto const& assignedDomainSelections = _domainTreeModel.getSelectedDomainItems(treeViewMediaClockDomains->selectionModel()->selection());
		auto const& assignedEntitySelections = _domainTreeModel.getSelectedEntityItems(treeViewMediaClockDomains->selectionModel()->selection());
		auto const& unassignedEntitySelections = _unassignedListModel.getSelectedItems(listView_UnassignedEntities->selectionModel()->selection());

		// update unassign button
		button_RemoveAssignment->setEnabled(!assignedEntitySelections.empty());

		// update assign button
		button_AssignToDomain->setEnabled(assignedDomainSelections.size() == 1 && !unassignedEntitySelections.empty());

		// update remove button
		button_Remove->setEnabled(!assignedDomainSelections.empty());
	}

	void removeMcDomainTreeViewSelections()
	{
		treeViewMediaClockDomains->clearSelection();
		const QModelIndex index;
		treeViewMediaClockDomains->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
	}

	/**
	* Handles the change of any data inside the media clock domain tree model. Triggers state change of the buttons.
	*/
	void handleDomainTreeDataChanged()
	{
		_hasChanges = true;
		adjustButtonStates();
	}

	/**
	* Triggers a resize of the columns in the media clock domain tree view.
	*/
	void resizeMCTreeViewColumns()
	{
		treeViewMediaClockDomains->resizeColumnToContents((int)DomainTreeModelColumn::Domain);
		treeViewMediaClockDomains->resizeColumnToContents((int)DomainTreeModelColumn::MediaClockMaster);
	}

	/*
	* When an entity goes offline while in the dialog it is removed from the models.
	*/
	void entityOffline(la::avdecc::UniqueIdentifier entityId)
	{
		_unassignedListModel.removeEntity(entityId);
		_domainTreeModel.removeEntity(entityId);
	}

	/*
	* Whenever the media clock mappings change while this dialog doesn't have unapplied user changes,
	* the model is updated.
	*/
	void mediaClockConnectionsUpdate(std::vector<la::avdecc::UniqueIdentifier> const& /*entityIds*/)
	{
		if (!_hasChanges)
		{
			refreshModels();
			resizeMCTreeViewColumns();
		}
	}

	/*
	* Update the progress dialog.
	*/
	void applyMediaClockDomainModelProgressUpdate(float_t progress)
	{
		_progressDialog->setValue(static_cast<int>(progress));
	}

	/*
	* Display any error that occurs
	*/
	void applyMediaClockDomainModelFinished(avdecc::mediaClock::ApplyInfo applyInfo)
	{
		_progressDialog->setValue(100);
		_progressDialog->close();
		refreshModels();

		std::unordered_set<la::avdecc::UniqueIdentifier, la::avdecc::UniqueIdentifier::hash> iteratedEntityIds;
		for (auto it = applyInfo.entityApplyErrors.begin(), end = applyInfo.entityApplyErrors.end(); it != end; it++) // upper_bound not supported on mac (to iterate over unique keys)
		{
			if (iteratedEntityIds.find(it->first) == iteratedEntityIds.end())
			{
				iteratedEntityIds.insert(it->first);
			}
			else
			{
				continue; // entity already displayed.
			}
			auto controlledEntity = hive::modelsLibrary::ControllerManager::getInstance().getControlledEntity(it->first);
			auto entityName = hive::modelsLibrary::helper::toHexQString(it->first.getValue()); // by default show the id if the entity is offline
			if (controlledEntity)
			{
				entityName = hive::modelsLibrary::helper::smartEntityName(*controlledEntity);
			}
			auto errorsForEntity = applyInfo.entityApplyErrors.equal_range(it->first);
			QString errors;

			for (auto i = errorsForEntity.first; i != errorsForEntity.second; ++i)
			{
				errors += "-";
				if (i->second.commandTypeAcmp)
				{
					switch (*i->second.commandTypeAcmp)
					{
						case hive::modelsLibrary::ControllerManager::AcmpCommandType::ConnectStream:
							errors += "Connecting stream failed. ";
							break;
						case hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectStream:
							errors += "Disconnecting stream failed. ";
							break;
						case hive::modelsLibrary::ControllerManager::AcmpCommandType::DisconnectTalkerStream:
							errors += "Disconnecting talker stream failed. ";
							break;
						default:
							break;
					}
				}
				else if (i->second.commandTypeAecp)
				{
					switch (*i->second.commandTypeAecp)
					{
						case hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockSource:
							errors += "Setting the clock source failed. ";
							break;
						case hive::modelsLibrary::ControllerManager::AecpCommandType::SetSamplingRate:
							errors += "Setting the sampling rate failed. ";
							break;
						default:
							break;
					}
				}
				switch (i->second.errorType)
				{
					case avdecc::commandChain::CommandExecutionError::LockedByOther:
						errors += "Entity is locked.";
						break;
					case avdecc::commandChain::CommandExecutionError::AcquiredByOther:
						errors += "Entity is aquired by an other controller.";
						break;
					case avdecc::commandChain::CommandExecutionError::Timeout:
						errors += "Command timed out. Entity might be offline.";
						break;
					case avdecc::commandChain::CommandExecutionError::EntityError:
						errors += "Entity error. Operation might not be supported.";
						break;
					case avdecc::commandChain::CommandExecutionError::NetworkIssue:
						errors += "Network error.";
						break;
					case avdecc::commandChain::CommandExecutionError::CommandFailure:
						errors += "Command failure.";
						break;
					case avdecc::commandChain::CommandExecutionError::NoMediaClockInputAvailable:
						errors += "Device does not have any compatible media clock inputs.";
						break;
					case avdecc::commandChain::CommandExecutionError::NoMediaClockOutputAvailable:
						errors += "Device does not have any compatible media clock outputs.";
						break;
					default:
						errors += "Unknwon error.";
						break;
				}
				errors += "\n";
			}
			QMessageBox::information(qobject_cast<QWidget*>(this), "Error while applying", QString("Error(s) occured on %1 while applying the configuration:\n\n%2").arg(entityName).arg(errors));
		}
	}

	/**
	* Expands every item in the domain tree view.
	*/
	void expandDomain(QModelIndex index)
	{
		treeViewMediaClockDomains->expand(index);
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
		expandDomain(_domainTreeModel.getDomainModelIndex(newDomainIndex));
		for (auto const& entityId : _unassignedListModel.getSelectedItems(listView_UnassignedEntities->selectionModel()->selection()))
		{
			bool success = _domainTreeModel.addEntityToDomain(newDomainIndex, entityId);
			if (success)
			{
				_unassignedListModel.removeEntity(entityId);
			}
		}
		_hasChanges = true;
		adjustButtonStates();
	}

private:
	DomainTreeModel _domainTreeModel;
	UnassignedListModel _unassignedListModel;
	bool _hasChanges;

	QProgressDialog* _progressDialog;
};

/**
* Constructor.
*/
MediaClockManagementDialog::MediaClockManagementDialog(QWidget* parent)
	: QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
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

/**
* Overloaded method from QDialog that handles local MCMD changes that have not been applied
* by querying the user if he is certain about closing the dialog, discarding his changes.
*/
void MediaClockManagementDialog::reject()
{
	QMessageBox::StandardButton resBtn = QMessageBox::Yes;
	if (_pImpl->hasChanges())
	{
		resBtn = (QMessageBox::StandardButton)QMessageBox::question(this, "", "You have unapplied changes that will be discarded. Continue?\n", QMessageBox::Yes, QMessageBox::No);
	}
	if (resBtn == QMessageBox::Yes)
	{
		QDialog::reject();
	}
}
