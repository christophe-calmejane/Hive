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

#include "entityDiagnosticsTreeWidgetItem.hpp"

#include <QtMate/material/color.hpp>

#include <map>
#include <QMenu>

EntityDiagnosticsTreeWidgetItem::EntityDiagnosticsTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::Diagnostics const& diagnostics, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
{
	// Create fields
	_redundancyWarning = new QTreeWidgetItem(this);
	_redundancyWarning->setText(0, "Milan Redundancy Warning");

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
