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

#include <QImage>
#include <QFont>
#include <QColor>
#include <QSize>
#include <QString>
#include <QSvgRenderer>

#include <memory>
#include <optional>

namespace qtMate::image
{
class LogoGenerator
{
public:
	struct LabelInfo
	{
		QFont font; // leave point/pixel size to default (-1) to allow auto scaling
		QColor color;
		QString text;
		qreal topMargin{ 0.f }; // Top margin for the text
		bool horizontalMirror{ false }; // Horizontal mirroring for the text
	};

	struct IconInfo
	{
		QString path{}; // Path to the SVG icon file
		std::optional<QColor> color = std::nullopt; // Patching color for the SVG icon
		std::optional<QSize> size = std::nullopt; // Size of the icon (if not provided auto-scaled to 1/3 of the logo size minus top margin)
		qreal topMargin = 0.f; // Margin top the top of the icon
		qreal leftMargin = 0.f; // Margin top the left of the icon
	};

	/**
	 * @brief Generate a compatibility logo image with a specific color.
	 * @param logoSize The size of the logo image.
	 * @param mainTextInfo Main text drawn to the center row (font, color, text).
	 * @param iconInfo SVG Icon decorator drawn to top left of the logo (optional).
	 * @param topRightTextInfo Top right text of the logo (optional).
	 * @param bottomTextInfo Add a mirrored version of the main text right below (optional).
	 * @return The generated logo image as QImage.
	 * @throws std::runtime_error if the SVG file is invalid or cannot be loaded.
	 */
	static QImage generateCompatibilityLogo(QSize const& logoSize, LabelInfo const& mainTextInfo, std::optional<IconInfo> const& iconInfo = std::nullopt, std::optional<LabelInfo> const& topRightTextInfo = std::nullopt, std::optional<LabelInfo> const& bottomTextInfo = std::nullopt);

	/**
	 * @brief Generate a recentred image from the given image.
	 * @param image The image to be recentred.
	 * @param logoSize The size of the logo image.
	 * @return The recentred image as QImage.
	 * @note This function tries to find the smallest rectangle that contains non-transparent pixels to recenter the image.
	 */
	static QImage generateRecentredImage(QImage& image, QSize const& logoSize);

private:
	// Private constructor to prevent instantiation
	LogoGenerator() = default;
	// Private destructor
	~LogoGenerator() = default;

	// Disable copy and move constructors and assignment operators
	LogoGenerator(const LogoGenerator&) = delete;
	LogoGenerator& operator=(const LogoGenerator&) = delete;
	LogoGenerator(LogoGenerator&&) = delete;
	LogoGenerator& operator=(LogoGenerator&&) = delete;

	static std::optional<QRectF> drawIcon(QPainter& painter, QSvgRenderer* iconSvgRenderer, qreal x, qreal y, qreal height, qreal width, qreal topMargin, qreal leftMargin);
	static std::optional<QRectF> drawLabel(QPainter& painter, LabelInfo const& labelInfo, QTextOption const& options, qreal x, qreal y, qreal height, qreal width);
	static QFontMetrics fitFontToWidth(QFont& font, QString const& text, qreal width);
	static QFontMetrics fitFontToHeight(QFont& font, QString const& text, qreal height);
};
} // namespace qtMate::image
