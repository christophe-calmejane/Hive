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

#include <QObject>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QListWidget>

class StreamPortDynamicTreeWidgetItem : public QObject, public QTreeWidgetItem
{
public:
	StreamPortDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamPortType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::StreamPortNodeStaticModel const* const staticModel, la::avdecc::entity::model::StreamPortNodeDynamicModel const* const dynamicModel, QTreeWidget* parent = nullptr);

private:
	void editMappingsButtonClicked();
	void clearMappingsButtonClicked();
	void updateMappings(la::avdecc::entity::model::AudioMappings const& mappings);

	la::avdecc::UniqueIdentifier const _entityID{};
	la::avdecc::entity::model::DescriptorType const _streamPortType{ la::avdecc::entity::model::DescriptorType::Entity };
	la::avdecc::entity::model::StreamPortIndex const _streamPortIndex{ 0u };
	QListWidget* _mappingsList{ nullptr };
};
