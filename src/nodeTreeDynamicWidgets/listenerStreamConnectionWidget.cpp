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

#include "listenerStreamConnectionWidget.hpp"

#include <QMenu>
#include <QStyle>

ListenerStreamConnectionWidget::ListenerStreamConnectionWidget(la::avdecc::entity::model::StreamConnectionState const& state, QWidget* parent)
	: QWidget(parent)
	, _state(state)
{
	auto const margin = style()->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, this) + 1;
	_layout.setContentsMargins(margin, 0, margin, 0);

	_layout.addWidget(&_streamConnectionLabel, 1);
	_layout.addWidget(&_entityNameLabel, 2);
	_layout.addWidget(&_disconnectButton);

	_entityNameLabel.setObjectName("EntityNameLabel");
	_disconnectButton.setObjectName("DisconnectButton");

	// Update info right now
	updateData();

	// Connect ControllerManager signals
	auto const& manager = avdecc::ControllerManager::getInstance();

	// Listen for Connection changed signals
	connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamConnectionChanged, this,
		[this](la::avdecc::entity::model::StreamConnectionState const& state)
		{
			if (state.listenerStream == _state.listenerStream)
			{
				// Update state
				_state = state;
				// Update data based on the new state
				updateData();
			}
		});

	// EntityOnline
	connect(&manager, &avdecc::ControllerManager::entityOnline, this,
		[this](la::avdecc::UniqueIdentifier const entityID)
		{
			if (entityID == _state.talkerStream.entityID)
				updateData();
		});

	// EntityOffline
	connect(&manager, &avdecc::ControllerManager::entityOffline, this,
		[this](la::avdecc::UniqueIdentifier const entityID)
		{
			if (entityID == _state.talkerStream.entityID)
				updateData();
		});

	// Connect Widget signals
	// Disconnect button
	connect(&_disconnectButton, &QPushButton::clicked, this,
		[this]()
		{
			avdecc::ControllerManager::getInstance().disconnectStream(_state.talkerStream.entityID, _state.talkerStream.streamIndex, _state.listenerStream.entityID, _state.listenerStream.streamIndex);
		});
}

void ListenerStreamConnectionWidget::updateData()
{
	auto& manager = avdecc::ControllerManager::getInstance();

	QString stateText{ "Unknown" };
	auto haveTalker{ false };
	auto listenerEntity = manager.getControlledEntity(_state.listenerStream.entityID);

	switch (_state.state)
	{
		case la::avdecc::entity::model::StreamConnectionState::State::NotConnected:
			if (listenerEntity && listenerEntity->getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
			{
				stateText = "Unbound";
			}
			else
			{
				stateText = "Not Connected";
			}
			haveTalker = false;
			break;
		case la::avdecc::entity::model::StreamConnectionState::State::FastConnecting:
			stateText = "Fast Connecting to ";
			haveTalker = true;
			break;
		case la::avdecc::entity::model::StreamConnectionState::State::Connected:
			if (listenerEntity && listenerEntity->getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
			{
				stateText = "Bound to ";
			}
			else
			{
				stateText = "Connected to ";
			}
			haveTalker = true;
			break;
		default:
			AVDECC_ASSERT(false, "Unhandled case");
			break;
	}

	if (!haveTalker)
	{
		_streamConnectionLabel.setText(stateText);
		_entityNameLabel.setText("");
		_disconnectButton.hide();
	}
	else
	{
		_streamConnectionLabel.setText(stateText + avdecc::helper::uniqueIdentifierToString(_state.talkerStream.entityID) + ":" + QString::number(_state.talkerStream.streamIndex));

		QString onlineStatus{ "Offline" };
		bool isGhost{ true };

		auto controlledEntity = manager.getControlledEntity(_state.talkerStream.entityID);
		// If talker is online, get it's name
		if (controlledEntity)
		{
			isGhost = false;
			onlineStatus = avdecc::helper::smartEntityName(*controlledEntity);
		}

		_entityNameLabel.setText(onlineStatus);
		_entityNameLabel.setProperty("isOnline", !isGhost);
		style()->unpolish(&_entityNameLabel);
		style()->polish(&_entityNameLabel);
		_disconnectButton.show();
		_disconnectButton.setEnabled(isGhost);
	}
}
