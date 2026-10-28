/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "entityDiagnosticsTreeWidgetItem.hpp"
#include "compatibilityChangeEventsDialog.hpp"
#include "accessibilityHelper.hpp"

#include <QtMate/material/color.hpp>
#include <hive/modelsLibrary/helper.hpp>

#include <map>
#include <QCoreApplication>
#include <QMenu>
#include <QMessageBox>

EntityDiagnosticsTreeWidgetItem::EntityDiagnosticsTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::Diagnostics const& diagnostics, la::avdecc::controller::ControlledEntity::CompatibilityChangedEvents const& compatibilityChangedEvents, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
{
	// Create fields
	_redundancyWarning = new QTreeWidgetItem(this);
	_redundancyWarning->setText(0, "Milan Redundancy Warning");
	auto* compatibilityChangeEvents = new QTreeWidgetItem(this);
	compatibilityChangeEvents->setText(0, "Compatibility Change Events");
	_compatibilityLogButton = new QPushButton("Show Events");
	_compatibilityLogButton->setAccessibleName(QCoreApplication::translate("EntityDiagnosticsTreeWidgetItem", "Show Compatibility Change Events", nullptr));
	connect(_compatibilityLogButton, &QPushButton::clicked, this, &EntityDiagnosticsTreeWidgetItem::showCompatibilityChangeEvents);
	accessibility::setTreeItemWidget(parent, compatibilityChangeEvents, 1, _compatibilityLogButton);
	// Disable the button if there are no events
	if (compatibilityChangedEvents.empty())
	{
		_compatibilityLogButton->setEnabled(false);
	}

	// Update diagnostics right now
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	updateDiagnostics(manager.getDiagnostics(entityID));

	// Listen for diagnosticsChanged
	connect(&manager, &hive::modelsLibrary::ControllerManager::diagnosticsChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::Diagnostics const& diagnostics)
		{
			if (entityID == _entityID)
			{
				updateDiagnostics(diagnostics);
			}
		});

	// Listen for compatibilityChangedEvents
	connect(&manager, &hive::modelsLibrary::ControllerManager::compatibilityChanged, this,
		[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::CompatibilityFlags const /*flags*/, la::avdecc::entity::model::MilanVersion const& /*milanVersion*/)
		{
			if (entityID == _entityID)
			{
				// When this event is triggered, we now an event has occurred, so we can enable the button
				_compatibilityLogButton->setEnabled(true);
			}
		});
}

void EntityDiagnosticsTreeWidgetItem::updateDiagnostics(la::avdecc::controller::ControlledEntity::Diagnostics const& diagnostics)
{
	// Cache diagnostics
	_diagnostics = diagnostics;

	// Redundancy Warning
	{
		auto color = qtMate::material::color::foregroundColor();
		auto text = "No";
		if (_diagnostics.redundancyWarning)
		{
			color = qtMate::material::color::foregroundErrorColorValue(qtMate::material::color::backgroundColorName(), qtMate::material::color::colorSchemeShade());
			text = "Yes";
		}
		_redundancyWarning->setForeground(0, color);
		_redundancyWarning->setForeground(1, color);
		_redundancyWarning->setText(1, text);
	}
}

void EntityDiagnosticsTreeWidgetItem::showCompatibilityChangeEvents()
{
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(_entityID);
	if (controlledEntity)
	{
		// Show compatibility change events
		auto compatibilityChangedEvents = controlledEntity->getCompatibilityChangedEvents();
		if (compatibilityChangedEvents.empty())
		{
			QMessageBox::information(nullptr, "No Compatibility Change Events", "There are no compatibility change events for this entity.");
			return;
		}

		// Generate dialog title with entity name
		auto entityName = hive::modelsLibrary::helper::smartEntityName(*controlledEntity);
		auto title = entityName + " - Compatibility Change Events";

		// Release the controlled entity before starting a long operation (menu.exec)
		controlledEntity.reset();

		// Create and show the dialog
		auto dialog = new CompatibilityChangeEventsDialog{ title, compatibilityChangedEvents, nullptr };
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->show(); // We want the dialog to be modal, so we use show() instead of exec()
	}
}
