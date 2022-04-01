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

#include "streamInputDiagnosticsTreeWidgetItem.hpp"

#include <map>
#include <QMenu>

StreamInputDiagnosticsTreeWidgetItem::StreamInputDiagnosticsTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isConnected, la::avdecc::controller::ControlledEntity::Diagnostics const& diagnostics, QTreeWidget* parent)
	: QTreeWidgetItem(parent)
	, _entityID(entityID)
	, _streamIndex(streamIndex)
	, _isConnected{ isConnected }
{
	// Create fields
	_latencyError = new QTreeWidgetItem(this);
	_latencyError->setText(0, "MSRP Latency Error");

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

	connect(&manager, &hive::modelsLibrary::ControllerManager::streamInputConnectionChanged, this,
		[this](la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamInputConnectionInfo const& info)
		{
			if (stream.entityID == _entityID && stream.streamIndex == _streamIndex)
			{
				_isConnected = info.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected;
				updateDiagnostics(_diagnostics);
			}
		});
}

void StreamInputDiagnosticsTreeWidgetItem::updateDiagnostics(la::avdecc::controller::ControlledEntity::Diagnostics const& diagnostics)
{
	// Cache diagnostics
	_diagnostics = diagnostics;

	// Latency Error
	{
		auto color = QColor{ _isConnected ? Qt::black : Qt::gray };
		auto text = "No";
		if (_isConnected)
		{
			if (_diagnostics.streamInputOverLatency[_streamIndex])
			{
				color = QColor{ Qt::red };
				text = "Yes";
			}
		}
		_latencyError->setForeground(0, color);
		_latencyError->setForeground(1, color);
		_latencyError->setText(1, text);
	}
}
