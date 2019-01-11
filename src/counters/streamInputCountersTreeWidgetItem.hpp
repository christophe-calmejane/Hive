/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#pragma once

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>

#include "avdecc/helper.hpp"
#include "avdecc/controllerManager.hpp"

#include <map>

#include <QObject>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QListWidget>

class StreamInputCountersTreeWidgetItem : public QObject, public QTreeWidgetItem
{
public:
	StreamInputCountersTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::StreamInputCounters const& counters, QTreeWidget* parent = nullptr);

private:
	void updateCounters(la::avdecc::controller::model::StreamInputCounters const& counters);

	la::avdecc::UniqueIdentifier const _entityID{};
	la::avdecc::entity::model::StreamIndex const _streamIndex{ 0u };

	// Counters
	std::map<la::avdecc::entity::StreamInputCounterValidFlag, QTreeWidgetItem*> _counters{};
};
