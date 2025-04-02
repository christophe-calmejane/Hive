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
		QFont font;
		QColor color;
		QString text;
	};

	struct RedundantOptions
	{
		double spacingRatio = 0.05f; // Default spacing ratio for redundant text
		QColor color = QColor(0, 0, 0, 255); // Default color for redundant text
	};

	struct IconInfo
	{
		QString path{}; // Path to the SVG icon file
		std::optional<QColor> color = std::nullopt; // Don't override color if not specified
	};

	/**
	 * @brief Generate a compatibility logo image with a specific color.
	 * @param logoSize The size of the logo image.
	 * @param mainTextInfo Main text drawn to the center row (font, color, text).
	 * @param iconInfo SVG Icon decorator drawn to top left of the logo (optional).
	 * @param additionalTextInfo Additional text decorator drawn to top right of the logo (optional).
	 * @param redundantOptions Add a mirrored version of the main text right below (optional).
	 * @return The generated logo image as QImage.
	 * @throws std::runtime_error if the SVG file is invalid or cannot be loaded.
	 */
	static QImage generateCompatibilityLogo(QSize const& logoSize, LabelInfo const& mainTextInfo, std::optional<IconInfo> const& iconInfo = std::nullopt, std::optional<LabelInfo> const& additionalTextInfo = std::nullopt, std::optional<RedundantOptions> const& redundantOptions = std::nullopt);

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

	static void drawIcon(QPainter& painter, QSvgRenderer* iconSvgRenderer, int rowHeight, int x, int y);
	static void drawAdditionalText(QPainter& painter, LabelInfo const& textInfo, int rowHeight, int xEnd, int y);
	static QRect drawMainText(QPainter& painter, LabelInfo const& textInfo, int rowHeight, int width);
	static void drawRedundantText(QPainter& painter, LabelInfo const& textInfo, RedundantOptions const& redundantOptions, int rowHeight, int width, int y);
	static QFontMetrics fitFontToWidth(QFont& font, QString const& text, int width);
};
} // namespace qtMate::image
