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

#include "QtMate/image/logoGenerator.hpp"
#include "QtMate/image/svgUtils.hpp"

#include <QPainter>
#include <QImage>
#include <QFontMetrics>
#include <QLinearGradient>

namespace qtMate::image
{
QImage LogoGenerator::generateCompatibilityLogoFromSvgRenderer(const QSize& logoSize, const LabelInfo& textInfo, QSvgRenderer* iconSvgRenderer, const std::optional<LabelInfo>& additionalTextInfo, std::optional<RedundantOptions> redundantOptions)
{
	// Create the final image with the provided size
	auto finalImage = QImage(logoSize, QImage::Format_ARGB32);
	finalImage.fill(Qt::transparent); // Fill with transparency

	auto painter = QPainter(&finalImage);
	painter.setRenderHint(QPainter::Antialiasing);

	// Calculate row height (split into 3 equal rows)
	auto rowHeight = logoSize.height() / 3;

	// Draw the main text in the middle row
	auto mainTextRect = drawMainText(painter, textInfo, rowHeight, logoSize.width());
	auto targetAdditionalTextXEnd = mainTextRect.right(); // Get the right edge of the main text rectangle
	auto targetIconX = mainTextRect.left(); // Icon is drawn to the left of the main text
	auto targetRedundantTextY = mainTextRect.bottom(); // Redundant text is drawn below the main text

	// Draw the redundant text in the bottom row if enabled
	if (redundantOptions)
	{
		drawRedundantText(painter, textInfo, redundantOptions.value(), rowHeight, logoSize.width(), targetRedundantTextY);
	}

	// Then draw the icon and additional text according to the main text position if any
	if (additionalTextInfo)
	{
		drawAdditionalText(painter, additionalTextInfo.value(), rowHeight, targetAdditionalTextXEnd, 0);
	}
	drawIcon(painter, iconSvgRenderer, rowHeight, targetIconX, 0);

	painter.end();
	return finalImage;
}

QImage LogoGenerator::generateCompatibilityLogo(const QSize& logoSize, const LabelInfo& mainTextInfo, std::optional<IconInfo> iconInfo, const std::optional<LabelInfo>& additionalTextInfo, std::optional<RedundantOptions> redundantOptions)
{
	auto* iconSvgRenderer = iconInfo && !iconInfo->path.isEmpty() ? svgUtils::loadSVGImage(iconInfo->path, iconInfo->color) : nullptr;
	QImage image = qtMate::image::LogoGenerator::generateCompatibilityLogoFromSvgRenderer(logoSize, mainTextInfo, iconSvgRenderer, additionalTextInfo, redundantOptions);
	delete iconSvgRenderer; // Clean up the SVG renderer
	return image;
}

void LogoGenerator::drawIcon(QPainter& painter, QSvgRenderer* iconSvgRenderer, int rowHeight, int x, int y)
{
	// Draw the top row (topImage and topText on the same row)
	if (iconSvgRenderer != nullptr)
	{
		// Get the original size of the SVG
		auto iconOriginalSize = iconSvgRenderer->defaultSize();

		// Calculate the target width while maintaining the aspect ratio to fit the row height
		auto iconTargetHeight = rowHeight;
		auto iconTargetWidth = (iconOriginalSize.width() * iconTargetHeight) / iconOriginalSize.height();

		// Render the SVG image using the SVG renderer
		auto iconRect = QRect(x, y, iconTargetWidth, iconTargetHeight);
		iconSvgRenderer->render(&painter, iconRect);
	}
}
void LogoGenerator::drawAdditionalText(QPainter& painter, const LabelInfo& textInfo, int rowHeight, int xEnd, int y)
{
	if (!textInfo.text.isEmpty())
	{
		auto topFont = textInfo.font;
		topFont.setPixelSize(rowHeight); // Match font size to row height

		// Adjust the font size to fit the text within the row
		auto topTextFontMetrics = fitFontToWidth(topFont, textInfo.text, xEnd);
		int topTextWidth = topTextFontMetrics.horizontalAdvance(textInfo.text);
		int topTextHeight = topTextFontMetrics.height();

		painter.setFont(topFont);
		painter.setPen(textInfo.color);

		//draw top text aligned to the right of the main text
		auto targetX = xEnd - topTextWidth; // Compute the target X position (origin is top left)
		auto topTextRect = QRect(targetX, y, topTextWidth, topTextHeight);
		painter.drawText(topTextRect, Qt::AlignHCenter | Qt::AlignVCenter, textInfo.text);
	}
}

QRect LogoGenerator::drawMainText(QPainter& painter, const LabelInfo& textInfo, int rowHeight, int width)
{
	if (textInfo.text.isEmpty())
	{
		return QRect(0, 0, width, rowHeight); // No main text to draw
	}

	auto mainFont = textInfo.font;
	mainFont.setPixelSize(rowHeight); // Start with the maximum font size

	// Adjust the font size to fit the text within the row
	auto mainFontMetric = fitFontToWidth(mainFont, textInfo.text, width);

	painter.setFont(mainFont);
	painter.setPen(textInfo.color);
	auto mainTextRect = QRect(0, rowHeight, width, rowHeight);
	painter.drawText(mainTextRect, Qt::AlignHCenter | Qt::AlignTop, textInfo.text);

	// Get actual main text rectangle size
	auto actualMainTextRect = mainFontMetric.tightBoundingRect(textInfo.text);
	auto actualTextWidth = actualMainTextRect.width();
	auto actualTextX = (width - actualTextWidth) / 2;
	// Return the rectangle of the main text for further use
	return QRect(actualTextX, rowHeight, actualTextWidth, rowHeight);
}

void LogoGenerator::drawRedundantText(QPainter& painter, const LabelInfo& textInfo, RedundantOptions const& redundantOptions, int rowHeight, int width, int y)
{
	if (textInfo.text.isEmpty())
	{
		return; // No redundant text to draw
	}

	// Draw the text mirrored horizontally with a darker color
	auto redundantFont = textInfo.font;
	redundantFont.setPixelSize(rowHeight); // Match font size to row height

	// Adjust the font size to fit the text within the row
	fitFontToWidth(redundantFont, textInfo.text, width);

	painter.setFont(redundantFont);
	painter.setPen(redundantOptions.color);
	auto redundantTextFontMetrics = QFontMetrics(redundantFont);
	int redundantTextWidth = redundantTextFontMetrics.horizontalAdvance(textInfo.text);
	int redundantTextHeight = redundantTextFontMetrics.height();
	auto targetX = (width - redundantTextWidth) / 2; // Center the redundant text
	auto overlappingOffset = redundantTextFontMetrics.height() * redundantOptions.spacingRatio;
	auto targetY = y + rowHeight - redundantTextHeight - overlappingOffset; // Adjust Y to stick to the bottom of the main text
	auto redundantTextRect = QRect(targetX, targetY, redundantTextWidth, redundantTextHeight);

	// Draw the mirrored redundant text
	QTransform redundantTextTransform;
	redundantTextTransform.translate(targetX + redundantTextWidth / 2, targetY + redundantTextHeight / 2); // Translate to center
	redundantTextTransform.scale(1, -1); // Mirror Vertically
	redundantTextTransform.translate(-(targetX + redundantTextWidth / 2), -(targetY + redundantTextHeight / 2)); // Translate back
	painter.setTransform(redundantTextTransform, true);
	painter.drawText(redundantTextRect, Qt::AlignHCenter | Qt::AlignTop, textInfo.text);
	painter.resetTransform(); // Reset the transformation
}

QFontMetrics LogoGenerator::fitFontToWidth(QFont& font, const QString& text, int width)
{
	auto fontMetrics = QFontMetrics(font);
	int textWidth = 0;
	do
	{
		textWidth = fontMetrics.horizontalAdvance(text);
		font.setPixelSize(font.pixelSize() - 1);
		fontMetrics = QFontMetrics(font);
	} while (textWidth > width && font.pixelSize() > 1);

	return fontMetrics;
}
} // namespace qtMate::image
