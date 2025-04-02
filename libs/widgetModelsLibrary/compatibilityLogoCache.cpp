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

#include "hive/widgetModelsLibrary/compatibilityLogoCache.hpp"

#include <QtMate/image/logoGenerator.hpp>
#include <QtMate/image/svgUtils.hpp>

#include <la/avdecc/utils.hpp>

#include <QApplication>
#include <QtGlobal>
#include <QThread>

namespace
{
static const auto s_CompatibilityLogoSize = QSize{ 64, 64 };
static constexpr auto s_warningIcon = ":/Warning.svg";
static const auto s_warningIconInfo = qtMate::image::LogoGenerator::IconInfo{ s_warningIcon };

// Milan static definitions
static constexpr auto s_milanMainLabel = "MILAN";
static constexpr auto s_milanCertifiedIconPath = ":/Cocarde.svg";
static const auto s_milanMainColorMap = std::unordered_map<hive::widgetModelsLibrary::CompatibilityLogoCache::Theme, QColor>{ { hive::widgetModelsLibrary::CompatibilityLogoCache::Theme::Light, QColor(83, 79, 155) }, { hive::widgetModelsLibrary::CompatibilityLogoCache::Theme::Dark, QColor(255, 255, 255) } };
static const auto s_milanRedundantMainColorMap = std::unordered_map<hive::widgetModelsLibrary::CompatibilityLogoCache::Theme, QColor>{ { hive::widgetModelsLibrary::CompatibilityLogoCache::Theme::Light, QColor(60, 56, 94) }, { hive::widgetModelsLibrary::CompatibilityLogoCache::Theme::Dark, QColor(144, 144, 144) } };
} // namespace

namespace hive::widgetModelsLibrary
{
inline auto qHash(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility const type, uint seed = 0)
{
	return ::qHash(la::avdecc::utils::to_integral(type), seed);
}

inline auto qHash(CompatibilityLogoCache::Theme const type, uint seed = 0)
{
	return ::qHash(la::avdecc::utils::to_integral(type), seed);
}

// Helper to combine hashes
inline size_t combineHash(size_t seed, size_t hash)
{
	return seed ^ (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

// Recursive hash computation for std::tuple
template<typename Tuple, std::size_t Index = std::tuple_size<Tuple>::value - 1>
struct TupleHashHelper
{
	static size_t compute(const Tuple& tuple, size_t seed)
	{
		seed = combineHash(seed, qHash(std::get<Index>(tuple)));
		return TupleHashHelper<Tuple, Index - 1>::compute(tuple, seed);
	}
};

// Base case for recursion
template<typename Tuple>
struct TupleHashHelper<Tuple, 0>
{
	static size_t compute(const Tuple& tuple, size_t seed)
	{
		return combineHash(seed, qHash(std::get<0>(tuple)));
	}
};

// qHash specialization for std::tuple
template<typename... Args>
size_t qHash(const std::tuple<Args...>& tuple, size_t seed = 0)
{
	return TupleHashHelper<std::tuple<Args...>>::compute(tuple, seed);
}

class CompatibilityLogoCacheImpl : public CompatibilityLogoCache
{
public:
	CompatibilityLogoCacheImpl() {}

	virtual QImage getImage(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility const compatibility, Theme const theme) noexcept override
	{
		Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "CompatibilityLogoCache", "getImage must be called in the GUI thread.");

		auto const key = makeKey(compatibility, theme);

		// Check if requested compatibility logo is in cache
		auto imageIt = _cache.find(key);
		if (imageIt != _cache.end())
		{
			// Return the cached image
			return imageIt.value();
		}
		buildImage(compatibility, theme);
		return _cache[key];
	}

	virtual bool isImageInCache(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility const compatibility, Theme const theme) const noexcept override
	{
		Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "CompatibilityLogoCache", "isImageInCache must be called in the GUI thread.");

		auto const key = makeKey(compatibility, theme);
		// Check if we have the specified image in the memory cache
		auto imageIt = _cache.find(key);
		if (imageIt != _cache.end())
		{
			// The image has been found in the memory cache
			return true;
		}

		return false;
	}

private:
	using Key = std::tuple<modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility, Theme>;

	Key makeKey(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility compatibility, Theme theme) const noexcept
	{
		return std::make_tuple(compatibility, theme);
	}

	void buildImage(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility compatibility, Theme theme) noexcept
	{
		auto const key = makeKey(compatibility, theme);
		try
		{
			switch (compatibility)
			{
				case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::NotCompliant:
					_cache[key] = QImage{ ":/not_compliant.png" };
					break;
				case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::IEEE:
					_cache[key] = QImage{ ":/ieee.png" };
					break;
				case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::Milan:
				{
					auto mainLabel = getMilanMainLabelInfo(s_milanMainColorMap.at(theme));
					_cache[key] = qtMate::image::LogoGenerator::generateCompatibilityLogo(s_CompatibilityLogoSize, mainLabel);
					break;
				}
				case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanCertified:
				{
					auto mainLabel = getMilanMainLabelInfo(s_milanMainColorMap.at(theme));
					auto iconInfo = getMilanCertifiedIconInfo(s_milanMainColorMap.at(theme));
					_cache[key] = qtMate::image::LogoGenerator::generateCompatibilityLogo(s_CompatibilityLogoSize, mainLabel, iconInfo);
					break;
				}
				case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::IEEEWarning:
					_cache[key] = QImage{ ":/ieee_Warning.png" };
					break;
				case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanWarning:
				{
					auto mainLabel = getMilanMainLabelInfo(s_milanMainColorMap.at(theme));
					_cache[key] = qtMate::image::LogoGenerator::generateCompatibilityLogo(s_CompatibilityLogoSize, mainLabel, s_warningIconInfo);
					break;
				}
				// case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanRedundant:
				// {
				// 	auto mainLabel = getMilanMainLabelInfo(s_milanRedundantMainColorMap.at(theme));
				// 	auto redundantOptions = getMilanRedundantOptions(s_milanRedundantMainColorMap.at(theme));
				// 	_cache[key] = qtMate::image::LogoGenerator::generateCompatibilityLogo(s_CompatibilityLogoSize, mainLabel, std::nullopt, std::nullopt, redundantOptions);
				// 	break;
				// }
				// case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanCertifiedRedundant:
				// {
				// 	auto mainLabel = getMilanMainLabelInfo(s_milanRedundantMainColorMap.at(theme));
				// 	auto iconInfo = getMilanCertifiedIconInfo(s_milanRedundantMainColorMap.at(theme));
				// 	auto redundantOptions = getMilanRedundantOptions(s_milanRedundantMainColorMap.at(theme));
				// 	_cache[key] = qtMate::image::LogoGenerator::generateCompatibilityLogo(s_CompatibilityLogoSize, mainLabel, iconInfo, std::nullopt, redundantOptions);
				// 	break;
				// }
				// case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanWarningRedundant:
				// {
				// 	auto mainLabel = getMilanMainLabelInfo(s_milanRedundantMainColorMap.at(theme));
				// 	auto redundantOptions = getMilanRedundantOptions(s_milanRedundantMainColorMap.at(theme));
				// 	_cache[key] = qtMate::image::LogoGenerator::generateCompatibilityLogo(s_CompatibilityLogoSize, mainLabel, s_warningIconInfo, std::nullopt, redundantOptions);
				// 	break;
				// }
				case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::Misbehaving:
					_cache[key] = QImage{ ":/misbehaving.png" };
					break;
				default:
					AVDECC_ASSERT(false, "Unsupported compatibility type");
					_cache[key] = QImage{}; // Set to an empty image in case of failure
			}
		}
		catch (...)
		{
			AVDECC_ASSERT(false, "Failed to build compatibility logo");
			_cache[key] = QImage{}; // Set to an empty image in case of failure
		}
	}

	static qtMate::image::LogoGenerator::LabelInfo getMilanMainLabelInfo(QColor color) noexcept
	{
		auto font = QFont("Futura LT Book");
		font.setBold(true);
		return { font, color, s_milanMainLabel };
	}

	static qtMate::image::LogoGenerator::LabelInfo getMilanVersionLabelInfo(QString version, QColor color) noexcept
	{
		return { QFont("Futura LT Book"), color, version };
	}

	static qtMate::image::LogoGenerator::RedundantOptions getMilanRedundantOptions(QColor color) noexcept
	{
		return { 0.05f, color };
	}

	static qtMate::image::LogoGenerator::IconInfo getMilanCertifiedIconInfo(QColor color) noexcept
	{
		return { s_milanCertifiedIconPath, color };
	}

private:
	QHash<Key, QImage> _cache;
}; // namespace hive::widgetModelsLibrary

CompatibilityLogoCache& CompatibilityLogoCache::getInstance() noexcept
{
	static CompatibilityLogoCacheImpl s_LogoCache{};

	return s_LogoCache;
}
} // namespace hive::widgetModelsLibrary
