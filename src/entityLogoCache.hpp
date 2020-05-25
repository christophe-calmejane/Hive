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

#pragma once

#include <QObject>
#include <QImage>
#include <QHash>

#include "avdecc/controllerManager.hpp"

class EntityLogoCache : public QObject
{
	Q_OBJECT
public:
	enum class Type
	{
		None,
		Entity,
		Manufacturer
	};

	static EntityLogoCache& getInstance() noexcept;

	virtual QImage getImage(la::avdecc::UniqueIdentifier const entityID, Type const type, bool const downloadIfNotInCache = false) noexcept = 0;
	virtual bool isImageInCache(la::avdecc::UniqueIdentifier const entityID, Type const type) const noexcept = 0;

	virtual void clear() noexcept = 0;

	Q_SIGNAL void imageChanged(la::avdecc::UniqueIdentifier const entityID, EntityLogoCache::Type const type);

protected:
	EntityLogoCache() = default;
};
