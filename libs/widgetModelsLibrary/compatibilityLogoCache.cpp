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
static const auto s_milanFont = QFont{ "Futura LT", -1, QFont::ExtraBold, false };
static const auto s_milanVersionFont = QFont{ "Futura LT", 50, QFont::Normal, false }; // Disable logo generator auto scale of the font by forcing it to 8px
static constexpr auto s_milanCertifiedIconPath = ":/Cocarde.svg";
static const auto s_milanMainColorMap = std::unordered_map<hive::widgetModelsLibrary::CompatibilityLogoCache::Theme, QColor>{ { hive::widgetModelsLibrary::CompatibilityLogoCache::Theme::Light, QColor(81, 77, 149) }, { hive::widgetModelsLibrary::CompatibilityLogoCache::Theme::Dark, QColor(255, 255, 255) } };
static const auto s_milanRedundantMainColorMap = std::unordered_map<hive::widgetModelsLibrary::CompatibilityLogoCache::Theme, QColor>{ { hive::widgetModelsLibrary::CompatibilityLogoCache::Theme::Light, QColor(64, 62, 100) }, { hive::widgetModelsLibrary::CompatibilityLogoCache::Theme::Dark, QColor(144, 144, 144) } };
} // namespace

namespace hive::widgetModelsLibrary
{
inline auto qHash(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility const compatibility, uint seed = 0)
{
	return ::qHash(la::avdecc::utils::to_integral(compatibility), seed);
}

inline auto qHash(CompatibilityLogoCache::Theme const theme, uint seed = 0)
{
	return ::qHash(la::avdecc::utils::to_integral(theme), seed);
}

inline auto qHash(la::avdecc::entity::model::MilanVersion const version, uint seed = 0)
{
	return ::qHash(version.getValue(), seed);
}

inline auto qHash(bool const value, uint seed = 0)
{
	return ::qHash(static_cast<uint>(value), seed);
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

	virtual QImage getImage(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility const compatibility, la::avdecc::entity::model::MilanVersion milanVersion, bool isRedundant, Theme const theme) noexcept override
	{
		Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "CompatibilityLogoCache", "getImage must be called in the GUI thread.");

		auto const key = makeKey(compatibility, milanVersion, isRedundant, theme);

		// Check if requested compatibility logo is in cache
		auto imageIt = _cache.find(key);
		if (imageIt != _cache.end())
		{
			// Return the cached image
			return imageIt.value();
		}
		buildImage(compatibility, milanVersion, isRedundant, theme);
		return _cache[key];
	}

	virtual bool isImageInCache(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility const compatibility, la::avdecc::entity::model::MilanVersion milanVersion, bool isRedundant, Theme const theme) const noexcept override
	{
		Q_ASSERT_X(QThread::currentThread() == qApp->thread(), "CompatibilityLogoCache", "isImageInCache must be called in the GUI thread.");

		auto const key = makeKey(compatibility, milanVersion, isRedundant, theme);
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
	using Key = std::tuple<modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility, la::avdecc::entity::model::MilanVersion, bool, Theme>;

	Key makeKey(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility compatibility, la::avdecc::entity::model::MilanVersion milanVersion, bool isRedundant, Theme theme) const noexcept
	{
		return std::make_tuple(compatibility, milanVersion, isRedundant, theme);
	}

	void buildImage(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility compatibility, la::avdecc::entity::model::MilanVersion milanVersion, bool isRedundant, Theme theme) noexcept
	{
		auto const key = makeKey(compatibility, milanVersion, isRedundant, theme);
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
					[[fallthrough]];
				case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanCertified:
					[[fallthrough]];
				case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanWarning:
				{
					auto mainLabel = getMilanMainLabelInfo(s_milanMainColorMap.at(theme));
					auto iconInfo = getMilanIconInfo(compatibility, s_milanMainColorMap.at(theme));
					auto versionLabel = getMilanVersionLabelInfo(milanVersion, s_milanMainColorMap.at(theme));
					auto redundantOptions = getMilanRedundantOptions(isRedundant, s_milanRedundantMainColorMap.at(theme));
					_cache[key] = qtMate::image::LogoGenerator::generateCompatibilityLogo(s_CompatibilityLogoSize, mainLabel, iconInfo, versionLabel, redundantOptions);
					break;
				}
				case modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::IEEEWarning:
					_cache[key] = QImage{ ":/ieee_Warning.png" };
					break;
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
		return { s_milanFont, color, s_milanMainLabel };
	}

	static std::optional<qtMate::image::LogoGenerator::LabelInfo> getMilanVersionLabelInfo(la::avdecc::entity::model::MilanVersion version, QColor color) noexcept
	{
		if (version.getValue() == 0u)
		{
			return std::nullopt;
		}
		auto const versionString = QString::fromStdString(version.to_string(2)); // 2 digit string is used: MAJOR.MINOR
		return qtMate::image::LogoGenerator::LabelInfo{ s_milanVersionFont, color, versionString };
	}

	static std::optional<qtMate::image::LogoGenerator::LabelInfo> getMilanRedundantOptions(bool isRedundant, QColor color) noexcept
	{
		if (!isRedundant)
		{
			return std::nullopt;
		}
		auto labelInfo = getMilanMainLabelInfo(color);
		labelInfo.horizontalMirror = true;
		labelInfo.topMargin = -1.f; // negative Margin due to the font definition in order to get the text sticking to the bottom of main label
		return labelInfo;
	}

	static std::optional<qtMate::image::LogoGenerator::IconInfo> getMilanIconInfo(modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility compatibility, QColor color) noexcept
	{
		if (compatibility == modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanCertified)
		{
			return qtMate::image::LogoGenerator::IconInfo{ s_milanCertifiedIconPath, color, std::nullopt, 0.f, 2.f };
		}
		else if (compatibility == modelsLibrary::DiscoveredEntitiesModel::ProtocolCompatibility::MilanWarning)
		{
			return qtMate::image::LogoGenerator::IconInfo{ s_warningIcon, color, std::nullopt, 0.f, 2.f };
		}
		return std::nullopt; // No icon for Milan compatible uncertified device
	}

	QHash<Key, QImage> _cache;
}; // namespace hive::widgetModelsLibrary

CompatibilityLogoCache& CompatibilityLogoCache::getInstance() noexcept
{
	static CompatibilityLogoCacheImpl s_LogoCache{};

	return s_LogoCache;
}
} // namespace hive::widgetModelsLibrary
