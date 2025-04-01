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
		QColor color = QColor(0, 0, 0, 255); // Default color for the icon
	};

	/**
	 * @brief Generate a compatibility logo image with a specific color.
	 * @param logoSize The size of the logo image.
	 * @param mainTextInfo The properties of the main text (font, color, text).
	 * @param iconInfo Path and color of the Icon (optional).
	 * @param additionalTextInfo The properties of the additional text (optional).
	 * @param redundantOptions Whether to include a redundant text reflection with color and spacing parameters (optional).
	 * @return The generated logo image as QImage.
	 * @throws std::runtime_error if the SVG file is invalid or cannot be loaded.
	 */
	static QImage generateCompatibilityLogo(const QSize& logoSize, const LabelInfo& mainTextInfo, std::optional<IconInfo> iconInfo = std::nullopt, const std::optional<LabelInfo>& additionalTextInfo = std::nullopt, std::optional<RedundantOptions> redundantOptions = std::nullopt);

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

	static QImage generateCompatibilityLogoFromSvgRenderer(const QSize& logoSize, const LabelInfo& mainTextInfo, QSvgRenderer* iconSvgRenderer, const std::optional<LabelInfo>& additionalTextInfo, std::optional<RedundantOptions> redundantOptions);
	static void drawIcon(QPainter& painter, QSvgRenderer* iconSvgRenderer, int rowHeight, int x, int y);
	static void drawAdditionalText(QPainter& painter, const LabelInfo& textInfo, int rowHeight, int xEnd, int y);
	static QRect drawMainText(QPainter& painter, const LabelInfo& textInfo, int rowHeight, int width);
	static void drawRedundantText(QPainter& painter, const LabelInfo& textInfo, RedundantOptions const& redundantOptions, int rowHeight, int width, int y);
	static QFontMetrics fitFontToWidth(QFont& font, const QString& text, int width);
};
} // namespace qtMate::image
