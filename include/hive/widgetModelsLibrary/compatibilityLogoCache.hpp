/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <hive/modelsLibrary/discoveredEntitiesModel.hpp>

namespace hive::widgetModelsLibrary
{
class CompatibilityLogoCache
{
public:
	enum class Theme
	{
		Light,
		Dark,
	};

	static CompatibilityLogoCache& getInstance() noexcept;

	/**
	 * @brief Get the logo image for a specific compatibility and theme.
	 * @param compatibility The compatibility type.
	 * @param theme The theme (light or dark).
	 * @return The logo image.
	 * @details This function will generate the logo image if it is not already in the cache.
	 * @note The function is thread-safe and should be called from the GUI thread.
	 */
	virtual QImage getImage(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility const compatibility, la::avdecc::entity::model::MilanVersion milanVersion, bool isRedundant, Theme const theme) noexcept = 0;
	/**
	 * @brief Check if the logo image is already in the cache.
	 * @param compatibility The compatibility type.
	 * @param theme The theme (light or dark).
	 * @return True if the image is in the cache, false otherwise.
	 */
	virtual bool isImageInCache(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility const compatibility, la::avdecc::entity::model::MilanVersion milanVersion, bool isRedundant, Theme const theme) const noexcept = 0;

	virtual ~CompatibilityLogoCache() = default;

protected:
	CompatibilityLogoCache() = default;
};

} // namespace hive::widgetModelsLibrary
