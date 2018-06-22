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

#pragma once

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>

#include "avdecc/helper.hpp"
#include "avdecc/controllerManager.hpp"

#include <QObject>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QListWidget>

class StreamDynamicTreeWidgetItem : public QObject, public QTreeWidgetItem
{
public:
	StreamDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamNodeStaticModel const* const staticModel, la::avdecc::controller::model::StreamInputNodeDynamicModel const* const inputDynamicModel, la::avdecc::controller::model::StreamOutputNodeDynamicModel const* const outputDynamicModel, QTreeWidget *parent = nullptr);

private:
	void updateStreamInfo(la::avdecc::entity::model::StreamInfo const& streamInfo);
	void updateConnections(la::avdecc::controller::model::StreamConnections const& connections);
	void updateConnectionState(la::avdecc::controller::model::StreamConnectionState const& connectionState);

	la::avdecc::UniqueIdentifier const _entityID{};
	la::avdecc::entity::model::DescriptorType const _streamType{ la::avdecc::entity::model::DescriptorType::Entity };
	la::avdecc::entity::model::StreamIndex const _streamIndex{ 0u };

	// StreamInfo
	QTreeWidgetItem* _streamFormat{ nullptr };
	QTreeWidgetItem* _streamFlags{ nullptr };
	QTreeWidgetItem* _streamDestMac{ nullptr };
	QTreeWidgetItem* _streamID{ nullptr };
	QTreeWidgetItem* _streamVlanID{ nullptr };
	QTreeWidgetItem* _msrpAccumulatedLatency{ nullptr };
	QTreeWidgetItem* _msrpFailureCode{ nullptr };
	QTreeWidgetItem* _msrpFailureBridgeID{ nullptr };

	// Connections
	QListWidget* _connections{ nullptr };

	// ConnectionState
	QTreeWidgetItem* _connectionState{ nullptr };
};
