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

#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QApplication>

inline uint qHash(EntityLogoCache::Type const type, uint seed = 0) {
	return qHash(static_cast<int>(type), seed);
}

class EntityLogoCacheImpl : public EntityLogoCache
{
public:
	virtual void clear() noexcept override
	{
		QDir dir(imageDir());
		dir.removeRecursively();
	
		auto const keys{_cache.keys()};
		_cache.clear();
		
		for (auto const& key : keys)
		{
			emit imageChanged(key.first, Type::Entity);
			emit imageChanged(key.first, Type::Manufacturer);
		}
	}
	
	virtual QImage getImage(la::avdecc::UniqueIdentifier const entityID, Type const type, bool const forceDownload) noexcept override
	{
		QImage image;
		
		auto const key{makeKey(entityID)};
		if (_cache.count(key))
		{
			image = _cache[key][type];
		}
		
		if (!image.isNull())
		{
			return image;
		}
		else
		{
			// Try to load from the disk
			QFileInfo fileInfo{imagePath(entityID, type)};
			if (fileInfo.exists())
			{
				image = QImage{fileInfo.filePath()};
				_cache[key][type] = image;
			}
			else if (forceDownload)
			{
				_cache[key][type] = image; // This will avoid downloading it twice
				downloadImage(entityID, type);
			}
		}
		
		return image;
	}
	
private:
	QString typeToString(Type const type) const noexcept
	{
		switch (type)
		{
			case Type::Entity:
				return "Entity";
			case Type::Manufacturer:
				return "Manufacturer";
			default:
				AVDECC_ASSERT(false, "Unsupported logo type");
				return "Unsupported";
		}
	}
	
	using Key = QPair<quint64, quint64>;
	
	Key makeKey(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);
		auto const entityModelID = controlledEntity->getEntity().getEntityModelID();
		return qMakePair(entityID.getValue(), entityModelID.getValue());
	}
	
	QString fileName(la::avdecc::UniqueIdentifier const entityID, Type const type) const noexcept
	{
		auto const key{makeKey(entityID)};
		return QString{typeToString(type) + '-' + avdecc::helper::uniqueIdentifierToString(key.first) + '-' + avdecc::helper::uniqueIdentifierToString(key.second)};
	}
	
	QString imageDir() const noexcept
	{
		return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + '/' + QCoreApplication::applicationName();
	}
	
	QString imagePath(la::avdecc::UniqueIdentifier const entityID, Type const type) const noexcept
	{
		return imageDir() + '/' + fileName(entityID, type) + ".png";
	}
	
	void downloadImage(la::avdecc::UniqueIdentifier const entityID, Type const type) noexcept
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
						(type == Type::Entity && model->memoryObjectType == la::avdecc::entity::model::MemoryObjectType::PngEntity) ||
						(type == Type::Manufacturer && model->memoryObjectType == la::avdecc::entity::model::MemoryObjectType::PngManufacturer)
						)
				{
					manager.readDeviceMemory(entityID, model->startAddress, model->maximumLength, [this, entityID, type](la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::entity::ControllerEntity::AaCommandStatus const status, la::avdecc::controller::Controller::DeviceMemoryBuffer const& memoryBuffer)
					{
						if (!!status && entity)
						{
							QImage image;
							if (image.loadFromData(memoryBuffer.data(), memoryBuffer.size(), "PNG"))
							{
								QFileInfo fileInfo{imagePath(entityID, type)};
								
								// Make sure this directory exists & save the image to the disk
								QDir().mkpath(fileInfo.absoluteDir().absolutePath());
								image.save(fileInfo.filePath());

								// Save the image to the cache
								_cache[makeKey(entityID)][type] = image;
								emit imageChanged(entityID, type);
							}
						}
					});
				}
			}
		}
		catch(...)
		{
			AVDECC_ASSERT(false, "Failed to download image");
		}
	}
	
private:
	using CacheData = QHash<Type, QImage>;
	QHash<Key, CacheData> _cache;
};

EntityLogoCache& EntityLogoCache::getInstance() noexcept
{
	static EntityLogoCacheImpl s_LogoCache{};
	
	return s_LogoCache;
}
