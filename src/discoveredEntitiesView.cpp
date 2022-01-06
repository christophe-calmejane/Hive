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

#include "discoveredEntitiesView.hpp"
#include "internals/config.hpp"

DiscoveredEntitiesView::DiscoveredEntitiesView(QWidget* parent)
	: QWidget{ parent }
{
	// Setup UI
	auto* const verticalLayout = new QVBoxLayout(this);
	verticalLayout->setSpacing(0);
	verticalLayout->setContentsMargins(0, 0, 0, 0);
	{
		auto* const horizontalLayout = new QHBoxLayout();
		horizontalLayout->setSpacing(6);
		horizontalLayout->setContentsMargins(6, 2, 6, 2);
		{
			horizontalLayout->addWidget(&_searchLineEdit);
			_searchLineEdit.setPlaceholderText(QCoreApplication::translate("DiscoveredEntitiesView", "Entity Name Filter (RegEx)", nullptr));
		}
		{
			auto* const horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
			horizontalLayout->addItem(horizontalSpacer);
		}
		{
			horizontalLayout->addWidget(&_removeAllConnectionsButton);
			_removeAllConnectionsButton.setToolTip(QCoreApplication::translate("DiscoveredEntitiesView", "Remove all active connections", nullptr));
			_removeAllConnectionsButton.setVisible(false);
		}
		{
			horizontalLayout->addWidget(&_clearAllErrorsButton);
			_clearAllErrorsButton.setToolTip(QCoreApplication::translate("DiscoveredEntitiesView", "Clear all error counters", nullptr));
		}
		verticalLayout->addLayout(horizontalLayout);
	}
	verticalLayout->addWidget(&_entitiesView);

	// Setup components
	_searchFilterProxyModel.setSourceModel(_entitiesView.model());
	_entitiesView.setModel(&_searchFilterProxyModel);

	// Connect signals
	connect(&_searchLineEdit, &QLineEdit::textChanged, &_searchLineEdit,
		[this]()
		{
			auto const pattern = QRegularExpression{ _searchLineEdit.text() };
			_searchFilterProxyModel.setFilterKeyColumn(la::avdecc::utils::to_integral(avdecc::ControllerModel::Column::Name));
			_searchFilterProxyModel.setFilterRegularExpression(pattern);
			_searchFilterProxyModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
		});

	connect(&_removeAllConnectionsButton, &qtMate::widgets::FlatIconButton::clicked, &_removeAllConnectionsButton,
		[this]()
		{
			// TODO: Check for filter and update the message (and processed list) depending on that
			if (QMessageBox::Yes == QMessageBox::question(this, {}, "Are you sure you want to remove all established connections?"))
			{
				//
			}
		});
	connect(&_clearAllErrorsButton, &qtMate::widgets::FlatIconButton::clicked, &_clearAllErrorsButton,
		[this]()
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			manager.foreachEntity(
				[&manager](la::avdecc::UniqueIdentifier const& entityID, [[maybe_unused]] la::avdecc::controller::ControlledEntity const& entity)
				{
					manager.clearAllStreamInputCounterValidFlags(entityID);
					manager.clearAllStatisticsCounterValidFlags(entityID);
				});
		});

	connect(&_entitiesView, &discoveredEntities::View::contextMenuRequested, this,
		[this](la::avdecc::UniqueIdentifier const entityID, QPoint const& pos)
		{
			auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);

			if (controlledEntity)
			{
				QMenu menu;

				// Add header
				{
					auto* action = menu.addAction("Entity: " + hive::modelsLibrary::helper::smartEntityName(*controlledEntity));
					auto font = action->font();
					font.setBold(true);
					action->setFont(font);
					action->setEnabled(false);
					menu.addSeparator();
				}

				auto const& entity = controlledEntity->getEntity();
				auto const entityModelID = entity.getEntityModelID();
				auto const isAemSupported = entity.getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported);
				auto const isIdentifyControlValid = !!controlledEntity->getIdentifyControlIndex();

				auto* acquireAction{ static_cast<QAction*>(nullptr) };
				auto* releaseAction{ static_cast<QAction*>(nullptr) };
				auto* lockAction{ static_cast<QAction*>(nullptr) };
				auto* unlockAction{ static_cast<QAction*>(nullptr) };
				auto* deviceView{ static_cast<QAction*>(nullptr) };
				auto* inspect{ static_cast<QAction*>(nullptr) };
				auto* getLogo{ static_cast<QAction*>(nullptr) };
				auto* clearErrorFlags{ static_cast<QAction*>(nullptr) };
				auto* identify{ static_cast<QAction*>(nullptr) };
				auto* dumpFullEntity{ static_cast<QAction*>(nullptr) };
				auto* dumpEntityModel{ static_cast<QAction*>(nullptr) };

				if (isAemSupported)
				{
					// Do not propose Acquire if the device is Milan (not supported)
					if (!controlledEntity->getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
					{
						QString acquireText;
						auto const isAcquired = controlledEntity->isAcquired();
						auto const isAcquiredByOther = controlledEntity->isAcquiredByOther();

						{
							if (isAcquiredByOther)
							{
								acquireText = "Try to acquire";
							}
							else
							{
								acquireText = "Acquire";
							}
							acquireAction = menu.addAction(acquireText);
							acquireAction->setEnabled(!isAcquired);
						}
						{
							releaseAction = menu.addAction("Release");
							releaseAction->setEnabled(isAcquired);
						}
					}
					// Lock
					{
						QString lockText;
						auto const isLocked = controlledEntity->isLocked();
						auto const isLockedByOther = controlledEntity->isLockedByOther();

						{
							if (isLockedByOther)
							{
								lockText = "Try to lock";
							}
							else
							{
								lockText = "Lock";
							}
							lockAction = menu.addAction(lockText);
							lockAction->setEnabled(!isLocked);
						}
						{
							unlockAction = menu.addAction("Unlock");
							unlockAction->setEnabled(isLocked);
						}
					}

					menu.addSeparator();

					// Device Details, Inspect, Logo, ...
					{
						deviceView = menu.addAction("Device Details...");
					}
					{
						inspect = menu.addAction("Inspect Entity Model...");
					}
					{
						getLogo = menu.addAction("Retrieve Entity Logo");
						getLogo->setEnabled(!hive::widgetModelsLibrary::EntityLogoCache::getInstance().isImageInCache(entityID, hive::widgetModelsLibrary::EntityLogoCache::Type::Entity));
					}
					{
						clearErrorFlags = menu.addAction("Acknowledge Counters Errors");
					}
					{
						identify = menu.addAction("Identify Device (10 sec)");
						identify->setEnabled(isIdentifyControlValid);
					}
				}

				menu.addSeparator();

				// Dump Entity
				{
					dumpFullEntity = menu.addAction("Export Full Entity...");
					dumpEntityModel = menu.addAction("Export Entity Model...");
					dumpEntityModel->setEnabled(isAemSupported && entityModelID);
				}

				menu.addSeparator();

				// Cancel
				menu.addAction("Cancel");

				// Release the controlled entity before starting a long operation (menu.exec)
				controlledEntity.reset();

				if (auto* action = menu.exec(_entitiesView.viewport()->mapToGlobal(pos)))
				{
					if (action == acquireAction)
					{
						manager.acquireEntity(entityID, false);
					}
					else if (action == releaseAction)
					{
						manager.releaseEntity(entityID);
					}
					else if (action == lockAction)
					{
						manager.lockEntity(entityID);
					}
					else if (action == unlockAction)
					{
						manager.unlockEntity(entityID);
					}
					else if (action == deviceView)
					{
						DeviceDetailsDialog* dialog = new DeviceDetailsDialog(this);
						dialog->setAttribute(Qt::WA_DeleteOnClose);
						dialog->setControlledEntityID(entityID);
						dialog->show();
					}
					else if (action == inspect)
					{
						auto* inspector = new EntityInspector;
						inspector->setAttribute(Qt::WA_DeleteOnClose);
						inspector->setControlledEntityID(entityID);
						inspector->restoreGeometry(_inspectorGeometry);
						inspector->show();
					}
					else if (action == getLogo)
					{
						hive::widgetModelsLibrary::EntityLogoCache::getInstance().getImage(entityID, hive::widgetModelsLibrary::EntityLogoCache::Type::Entity, true);
					}
					else if (action == clearErrorFlags)
					{
						manager.clearAllStreamInputCounterValidFlags(entityID);
						manager.clearAllStatisticsCounterValidFlags(entityID);
					}
					else if (action == identify)
					{
						manager.identifyEntity(entityID, std::chrono::seconds{ 10 });
					}
					else if (action == dumpFullEntity || action == dumpEntityModel)
					{
						auto baseFileName = QString{};
						auto binaryFilterName = QString{};
						if (action == dumpFullEntity)
						{
							binaryFilterName = "AVDECC Virtual Entity Files (*.ave)";
							baseFileName = QString("%1/Entity_%2").arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).arg(hive::modelsLibrary::helper::uniqueIdentifierToString(entityID));
						}
						else
						{
							// Do some validation
							if (!avdecc::helper::isValidEntityModelID(entityModelID))
							{
								QMessageBox::warning(this, "", "EntityModelID is not valid (invalid Vendor OUI-24), cannot same the Model of this Entity.");
								return;
							}
							if (!avdecc::helper::isEntityModelComplete(entityID))
							{
								QMessageBox::warning(this, "", "'Full AEM Enumeration' option must be Enabled in order to export Model of a multi-configuration Entity.");
								return;
							}
							binaryFilterName = "AVDECC Entity Model Files (*.aem)";
							baseFileName = QString("%1/EntityModel_%2").arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).arg(hive::modelsLibrary::helper::uniqueIdentifierToString(entityModelID));
						}
						auto const dumpFile = [this, &entityID, &manager, isFullEntity = (action == dumpFullEntity)](auto const& baseFileName, auto const& binaryFilterName, auto const isBinary)
						{
							auto const filename = QFileDialog::getSaveFileName(this, "Save As...", baseFileName, binaryFilterName);
							if (!filename.isEmpty())
							{
								auto flags = la::avdecc::entity::model::jsonSerializer::Flags{};
								if (isFullEntity)
								{
									flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics };
								}
								else
								{
									flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel };
								}
								if (isBinary)
								{
									flags.set(la::avdecc::entity::model::jsonSerializer::Flag::BinaryFormat);
								}
								auto [error, message] = manager.serializeControlledEntityAsJson(entityID, filename, flags, avdecc::helper::generateDumpSourceString(hive::internals::applicationShortName, hive::internals::versionString));
								if (!error)
								{
									QMessageBox::information(this, "", "Export successfully completed:\n" + filename);
								}
								else
								{
									if (error == la::avdecc::jsonSerializer::SerializationError::InvalidDescriptorIndex && isFullEntity)
									{
										auto const choice = QMessageBox::question(this, "", QString("EntityID %1 model is not fully IEEE1722.1 compliant.\n%2\n\nDo you want to export anyway?").arg(hive::modelsLibrary::helper::uniqueIdentifierToString(entityID)).arg(message.c_str()), QMessageBox::StandardButton::Yes, QMessageBox::StandardButton::No);
										if (choice == QMessageBox::StandardButton::Yes)
										{
											flags.set(la::avdecc::entity::model::jsonSerializer::Flag::IgnoreAEMSanityChecks);
											auto const result = manager.serializeControlledEntityAsJson(entityID, filename, flags, avdecc::helper::generateDumpSourceString(hive::internals::applicationShortName, hive::internals::versionString));
											error = std::get<0>(result);
											message = std::get<1>(result);
											if (!error)
											{
												QMessageBox::information(this, "", "Export completed but with warnings:\n" + filename);
											}
											// Fallthrough to warning message
										}
									}
									if (!!error)
									{
										QMessageBox::warning(this, "", QString("Export of EntityID %1 failed:\n%2").arg(hive::modelsLibrary::helper::uniqueIdentifierToString(entityID)).arg(message.c_str()));
									}
								}
							}
						};
						if (QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier))
						{
							dumpFile(baseFileName, "JSON Files (*.json)", false);
						}
						else
						{
							dumpFile(baseFileName, binaryFilterName, true);
						}
					}
				}
			}
		});
}

void DiscoveredEntitiesView::setupView(hive::VisibilityDefaults const& defaults) noexcept
{
	_entitiesView.setupView(defaults);
}

discoveredEntities::View* DiscoveredEntitiesView::entitiesTableView() noexcept
{
	return &_entitiesView;
}

void DiscoveredEntitiesView::setInspectorGeometry(QByteArray const& geometry) noexcept
{
	_inspectorGeometry = geometry;
}
