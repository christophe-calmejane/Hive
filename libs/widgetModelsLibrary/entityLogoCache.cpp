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

#include "hive/widgetModelsLibrary/entityLogoCache.hpp"

#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <la/avdecc/utils.hpp>

#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QtGlobal>
#include <QThread>

namespace hive
{
namespace widgetModelsLibrary
{
inline auto qHash(EntityLogoCache::Type const type, uint seed = 0)
{
	return ::qHash(la::avdecc::utils::to_integral(type), seed);
}

class EntityLogoCacheImpl : public EntityLogoCache
{
public:
	EntityLogoCacheImpl()
	{
		qRegisterMetaType<hive::widgetModelsLibrary::EntityLogoCache::Type>("hive::widgetModelsLibrary::EntityLogoCache::Type");
	}

	virtual QImage getImage(la::avdecc::UniqueIdentifier const entityID, Type const type, bool const downloadIfNotInCache) noexcept override
	{
		Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "EntityLogoCache", "getImage must be called in the GUI thread.");

		auto const key{ makeKey(entityID) };

		// Check if we have something in the memory cache for this key (entityID-entityModelID)
		auto imagesIt = _cache.find(key);
		if (imagesIt != _cache.end())
		{
			// Check if we have the specified image type in the memory cache
			auto imageIt = imagesIt->find(type);
			if (imageIt != imagesIt->end())
			{
				// Return the cached image (might be an empty QImage if the download is in progress)
				return imageIt.value();
			}
		}

		QImage image;

		// Try to load from the disk
		QFileInfo fileInfo{ imagePath(entityID, type) };
		if (fileInfo.exists())
		{
			// Found the image on the disk, add it to the memory cache
			image = QImage{ fileInfo.filePath() };
			_cache[key][type] = image;
		}
		else if (downloadIfNotInCache)
		{
			// Add a temporary empty QImage in the memory cache so we won't start another download while this one is pending
			_cache[key][type] = image;
			downloadImage(entityID, type);
		}

		return image;
	}

	virtual bool isImageInCache(la::avdecc::UniqueIdentifier const entityID, Type const type) const noexcept override
	{
		Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "EntityLogoCache", "isImageInCache must be called in the GUI thread.");

		auto const key{ makeKey(entityID) };
		auto imagesIt = _cache.find(key);
		if (imagesIt != _cache.end())
		{
			// Check if we have the specified image type in the memory cache
			auto imageIt = imagesIt->find(type);
			if (imageIt != imagesIt->end())
			{
				// The image has been found in the memory cache (either a real one, or a temporary one)
				return true;
			}
		}

		return false;
	}

	virtual void clear() noexcept override
	{
		Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "EntityLogoCache", "clear must be called in the GUI thread.");

		QDir dir(imageDir());
		dir.removeRecursively();

		auto const keys{ _cache.keys() };
		_cache.clear();

		for (auto const& key : keys)
		{
			emit imageChanged(la::avdecc::UniqueIdentifier{ key.first }, Type::Entity);
			emit imageChanged(la::avdecc::UniqueIdentifier{ key.first }, Type::Manufacturer);
		}
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

	using Key = QPair<la::avdecc::UniqueIdentifier::value_type, la::avdecc::UniqueIdentifier::value_type>;

	Key makeKey(la::avdecc::UniqueIdentifier const entityID) const noexcept
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);
		if (controlledEntity)
		{
			auto const entityModelID = controlledEntity->getEntity().getEntityModelID();
			return qMakePair(entityID.getValue(), entityModelID.getValue());
		}
		return {};
	}

	QString fileName(la::avdecc::UniqueIdentifier const entityID, Type const type) const noexcept
	{
		auto const key{ makeKey(entityID) };
		return QString{ typeToString(type) + '-' + hive::modelsLibrary::helper::uniqueIdentifierToString(la::avdecc::UniqueIdentifier{ key.first }) + '-' + hive::modelsLibrary::helper::uniqueIdentifierToString(la::avdecc::UniqueIdentifier{ key.second }) };
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
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);

		if (!controlledEntity)
		{
			return;
		}

		try
		{
			auto const& configurationNode{ controlledEntity->getCurrentConfigurationNode() };

			for (auto const& it : configurationNode.memoryObjects)
			{
				auto const& obj{ it.second };
				auto const& model{ obj.staticModel };

				if ((type == Type::Entity && model.memoryObjectType == la::avdecc::entity::model::MemoryObjectType::PngEntity) || (type == Type::Manufacturer && model.memoryObjectType == la::avdecc::entity::model::MemoryObjectType::PngManufacturer))
				{
					auto const& dynamicModel{ obj.dynamicModel };
					manager.readDeviceMemory(entityID, model.startAddress, dynamicModel.length, nullptr,
						[this, entityID, type](la::avdecc::controller::ControlledEntity const* const /*entity*/, la::avdecc::entity::ControllerEntity::AaCommandStatus const status, la::avdecc::controller::Controller::DeviceMemoryBuffer const& memoryBuffer)
						{
							auto image = QImage::fromData(memoryBuffer.data(), static_cast<int>(memoryBuffer.size()));

							// Be sure to run this code in the UI thread so we don't have to lock the cache
							QMetaObject::invokeMethod(this,
								[this, entityID, type, status, image = std::move(image)]()
								{
									auto const key = makeKey(entityID);
									if (!!status && !image.isNull())
									{
										QFileInfo fileInfo{ imagePath(entityID, type) };

										// Make sure this directory exists & save the image to the disk
										QDir().mkpath(fileInfo.absoluteDir().absolutePath());
										image.save(fileInfo.filePath());

										// Save the image to the cache
										_cache[key][type] = image;
										emit imageChanged(entityID, type);
									}
									else
									{
										// Error downloading the image, remove the temporary one from the cache
										auto imagesIt = _cache.find(key);
										if (imagesIt != _cache.end())
										{
											imagesIt->remove(type);
										}
									}
								});
						});
				}
			}
		}
		catch (...)
		{
			AVDECC_ASSERT(false, "Failed to find logo descriptor information in AEM");
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

} // namespace widgetModelsLibrary
} // namespace hive
