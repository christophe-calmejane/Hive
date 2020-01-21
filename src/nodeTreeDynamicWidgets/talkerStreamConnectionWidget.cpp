/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "talkerStreamConnectionWidget.hpp"

#include <QMenu>
#include <QStyle>

TalkerStreamConnectionWidget::TalkerStreamConnectionWidget(la::avdecc::entity::model::StreamIdentification talkerConnection, la::avdecc::entity::model::StreamIdentification listenerConnection, QWidget* parent)
	: QWidget(parent)
	, _talkerConnection(std::move(talkerConnection))
	, _listenerConnection(std::move(listenerConnection))
{
	_layout.setContentsMargins(0, 0, 0, 0);

	_layout.addWidget(&_streamConnectionLabel, 1);
	_layout.addWidget(&_entityNameLabel, 2);
	_layout.addWidget(&_disconnectButton);

	_streamConnectionLabel.setText(avdecc::helper::uniqueIdentifierToString(_listenerConnection.entityID) + ":" + QString::number(_listenerConnection.streamIndex));

	_entityNameLabel.setObjectName("EntityNameLabel");
	_disconnectButton.setObjectName("DisconnectButton");

	updateData();

	// Connect ControllerManager signals
	auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();

	// EntityOnline
	connect(&manager, &hive::modelsLibrary::ControllerManager::entityOnline, this,
		[this](la::avdecc::UniqueIdentifier const entityID)
		{
			if (entityID == _listenerConnection.entityID)
				updateData();
		});

	// EntityOffline
	connect(&manager, &hive::modelsLibrary::ControllerManager::entityOffline, this,
		[this](la::avdecc::UniqueIdentifier const entityID)
		{
			if (entityID == _listenerConnection.entityID)
				updateData();
		});

	// Connect Widget signals
	// Disconnect button
	connect(&_disconnectButton, &QPushButton::clicked, this,
		[this]()
		{
			hive::modelsLibrary::ControllerManager::getInstance().disconnectTalkerStream(_talkerConnection.entityID, _talkerConnection.streamIndex, _listenerConnection.entityID, _listenerConnection.streamIndex);
		});
	// Row context menu
#pragma message("TODO: Pas a faire ici mais dans la table complete!!")
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &TalkerStreamConnectionWidget::customContextMenuRequested, this,
		[this](QPoint const& pos)
		{
			QMenu menu;

			auto* disconnectAllAction = menu.addAction("Disconnect all ghost connections");
			menu.addSeparator();
			menu.addAction("Cancel");

			if (auto* action = menu.exec(mapToGlobal(pos)))
			{
				if (action == disconnectAllAction)
				{
				}
			}
		});
}

void TalkerStreamConnectionWidget::updateData()
{
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();

	QString onlineStatus{ "Offline" };
	bool isGhost{ true };

	auto controlledEntity = manager.getControlledEntity(_listenerConnection.entityID);
	if (controlledEntity)
	{
		onlineStatus = avdecc::helper::smartEntityName(*controlledEntity);

		try
		{
			auto const& entityNode = controlledEntity->getEntityNode();

			// Check if entity is connected as well (or if we have a ghost connection)
			auto const& streamNode = controlledEntity->getStreamInputNode(entityNode.dynamicModel->currentConfiguration, _listenerConnection.streamIndex);
			auto const* const dynamicModel = streamNode.dynamicModel;

			if (dynamicModel->connectionInfo.state != la::avdecc::entity::model::StreamInputConnectionInfo::State::NotConnected)
			{
				if (dynamicModel->connectionInfo.talkerStream == _talkerConnection)
				{
					isGhost = false;
				}
			}
		}
		catch (...)
		{
		}
	}

	_entityNameLabel.setText(onlineStatus);
	_entityNameLabel.setProperty("isOnline", !isGhost);
	style()->unpolish(&_entityNameLabel);
	style()->polish(&_entityNameLabel);
	_disconnectButton.setEnabled(isGhost);
}
