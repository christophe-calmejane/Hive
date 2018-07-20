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

#include "entityLogoCache.hpp"
#include "avdecc/helper.hpp"

#include <QImage>
#include <QStandardPaths>

#include <QDebug>
#include <QDir>

QString logoTypeToString(EntityLogoCache::LogoType const type)
{
	switch (type)
	{
		case EntityLogoCache::LogoType::Entity:
			return "Entity";
		case EntityLogoCache::LogoType::Manufacturer:
			return "Manufacturer";
		default:
			return "Unkown";
	}
}

EntityLogoCache::EntityLogoCache(QObject* parent)
	: QObject{parent}
{
	auto& manager = avdecc::ControllerManager::getInstance();
	connect(&manager, &avdecc::ControllerManager::entityOnline, this, &EntityLogoCache::entityOnline);
}

void EntityLogoCache::requestEntityLogo(la::avdecc::UniqueIdentifier const entityID, LogoType const type)
{
	auto& manager = avdecc::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(entityID);

	if (!controlledEntity)
	{
		return;
	}

	try
	{
		auto const& configurationNode{controlledEntity->getCurrentConfigurationNode()};
		for (auto const& it : configurationNode.memoryObjects)
		{
			auto const& obj{it.second};
			auto const& model{obj.staticModel};

			if (
				(type == Entity && model->memoryObjectType == la::avdecc::entity::model::MemoryObjectType::PngEntity) ||
				(type == Manufacturer && model->memoryObjectType == la::avdecc::entity::model::MemoryObjectType::PngManufacturer)
				)
			{
				manager.readDeviceMemory(entityID, model->startAddress, model->maximumLength, [this, type, entityID](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AaCommandStatus const status, la::avdecc::controller::Controller::DeviceMemoryBuffer const& memoryBuffer)
				{
					if (!!status && entity)
					{
						QImage image;
						if (image.loadFromData(memoryBuffer.data(), memoryBuffer.size(), "PNG"))
						{
							auto const typeStr{logoTypeToString(type)};
							auto const entityIDStr{avdecc::helper::uniqueIdentifierToString(entityID)};
							auto const modelIDStr{QString{}};

							auto const fileName{QString{"/%1-%2-%3.png"}.arg(typeStr).arg(entityIDStr).arg(modelIDStr)};
							auto const dir{QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)};

							QDir().mkpath(dir);

							auto const path{dir + fileName};
							image.save(path);

							emit logoChanged(image);
						}
					}
				});
			}
		}
	}
	catch(...)
	{
	}
}

void EntityLogoCache::entityOnline(la::avdecc::UniqueIdentifier const entityID)
{
	requestEntityLogo(entityID, LogoType::Entity);
}

