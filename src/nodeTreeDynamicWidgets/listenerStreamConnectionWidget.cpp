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

#include "listenerStreamConnectionWidget.hpp"

#include <hive/modelsLibrary/helper.hpp>

#include <QMenu>
#include <QStyle>

ListenerStreamConnectionWidget::ListenerStreamConnectionWidget(la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamInputConnectionInfo const& info, QWidget* parent)
	: QWidget(parent)
	, _stream(stream)
	, _info(info)
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
	auto const& manager = hive::modelsLibrary::ControllerManager::getInstance();

	// Listen for Connection changed signals
	connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::streamInputConnectionChanged, this,
		[this](la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamInputConnectionInfo const& info)
		{
			if (stream == _stream)
			{
				// Update state
				_info = info;
				// Update data based on the new state
				updateData();
			}
		});

	// EntityOnline
	connect(&manager, &hive::modelsLibrary::ControllerManager::entityOnline, this,
		[this](la::avdecc::UniqueIdentifier const entityID)
		{
			if (entityID == _info.talkerStream.entityID)
				updateData();
		});

	// EntityOffline
	connect(&manager, &hive::modelsLibrary::ControllerManager::entityOffline, this,
		[this](la::avdecc::UniqueIdentifier const entityID)
		{
			if (entityID == _info.talkerStream.entityID)
				updateData();
		});

	// Connect Widget signals
	// Disconnect button
	connect(&_disconnectButton, &QPushButton::clicked, this,
		[this]()
		{
			hive::modelsLibrary::ControllerManager::getInstance().disconnectStream(_info.talkerStream.entityID, _info.talkerStream.streamIndex, _stream.entityID, _stream.streamIndex);
		});
}

void ListenerStreamConnectionWidget::updateData()
{
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();

	QString stateText{ "Unknown" };
	auto haveTalker{ false };
	auto listenerEntity = manager.getControlledEntity(_stream.entityID);

	switch (_info.state)
	{
		case la::avdecc::entity::model::StreamInputConnectionInfo::State::NotConnected:
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
		case la::avdecc::entity::model::StreamInputConnectionInfo::State::FastConnecting:
			stateText = "Fast Connecting to ";
			haveTalker = true;
			break;
		case la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected:
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
		_streamConnectionLabel.setText(stateText + hive::modelsLibrary::helper::uniqueIdentifierToString(_info.talkerStream.entityID) + ":" + QString::number(_info.talkerStream.streamIndex));

		QString onlineStatus{ "Offline" };
		bool isGhost{ true };

		auto controlledEntity = manager.getControlledEntity(_info.talkerStream.entityID);
		// If talker is online, get it's name
		if (controlledEntity)
		{
			isGhost = false;
			onlineStatus = hive::modelsLibrary::helper::smartEntityName(*controlledEntity);
		}

		_entityNameLabel.setText(onlineStatus);
		_entityNameLabel.setProperty("isOnline", !isGhost);
		style()->unpolish(&_entityNameLabel);
		style()->polish(&_entityNameLabel);
		_disconnectButton.show();
		_disconnectButton.setEnabled(isGhost);
	}
}
