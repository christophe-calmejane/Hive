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
QImage LogoGenerator::generateCompatibilityLogo(QSize const& logoSize, LabelInfo const& mainTextInfo, std::optional<IconInfo> const& iconInfo, std::optional<LabelInfo> const& topRightTextInfo, std::optional<LabelInfo> const& bottomTextInfo)
{
	auto image = QImage(logoSize, QImage::Format_ARGB32);
	image.fill(Qt::transparent); // Fill with transparency

	auto painter = QPainter(&image);
	painter.setRenderHint(QPainter::Antialiasing);

	auto labelOptions = QTextOption();
	labelOptions.setTextDirection(Qt::LeftToRight);

	// Calculate row height (split into 3 equal rows)
	auto rowHeight = logoSize.height() / 3;

	auto topRightLabelRect = std::optional<QRectF>{};
	auto iconRect = std::optional<QRectF>{};
	auto mainTextRect = std::optional<QRectF>{};
	auto bottomTextRect = std::optional<QRectF>{};

	// Draw the top right label if provided
	if (topRightTextInfo)
	{
		labelOptions.setAlignment(Qt::AlignRight | Qt::AlignTop);
		topRightLabelRect = drawLabel(painter, topRightTextInfo.value(), labelOptions, 0, 0, rowHeight, logoSize.width());
	}

	// Load the icon SVG and draw it after Top right label last to overlap the top right text if needed
	if (iconInfo && !iconInfo->path.isEmpty())
	{
		auto* iconSvgRenderer = svgUtils::loadSVGImage(iconInfo->path, iconInfo->color);
		auto topMargin = iconInfo->topMargin;
		auto leftMargin = iconInfo->leftMargin;
		auto iconSize = iconInfo->size.value_or(QSize(logoSize.width() / 3, logoSize.height() / 3));
		iconRect = drawIcon(painter, iconSvgRenderer, 0, 0, iconSize.height(), iconSize.width(), topMargin, leftMargin);
		delete iconSvgRenderer; // Clean up the SVG renderer
	}

	// Draw the main text in the middle row
	{
		labelOptions.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
		// Compute the Y position based on the icon and top right label
		auto mainTextY = iconRect ? iconRect->bottom() : topRightLabelRect ? topRightLabelRect->bottom() : 0;
		mainTextRect = drawLabel(painter, mainTextInfo, labelOptions, 0, mainTextY, rowHeight, logoSize.width());
	}

	// Draw the bottom text if provided
	if (bottomTextInfo)
	{
		labelOptions.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
		// Compute Y position based on the previous elements
		auto bottomTextY = mainTextRect ? mainTextRect->bottom() : iconRect ? iconRect->bottom() : topRightLabelRect ? topRightLabelRect->bottom() : 0;
		bottomTextRect = drawLabel(painter, bottomTextInfo.value(), labelOptions, 0, bottomTextY, rowHeight, logoSize.width());
	}

	painter.end();

	// Return a recentred image based on the bounding rectangle of non-transparent pixels
	return generateRecentredImage(image, logoSize);
}

QImage LogoGenerator::generateRecentredImage(QImage& image, QSize const& size)
{
	// Step 1: Find the bounding rectangle of non-transparent pixels
	QRect boundingRect;
	for (int y = 0; y < image.height(); ++y)
	{
		for (int x = 0; x < image.width(); ++x)
		{
			if (image.pixelColor(x, y).alpha() > 0)
			{ // Non-transparent pixel
				if (boundingRect.isNull())
				{
					boundingRect = QRect(x, y, 1, 1);
				}
				else
				{
					boundingRect = boundingRect.united(QRect(x, y, 1, 1));
				}
			}
		}
	}

	// Step 2: Create a new image with the target size and recenter the cropped image
	QImage centeredImage(size, QImage::Format_ARGB32);
	centeredImage.setDevicePixelRatio(image.devicePixelRatio()); // Preserve DPI scaling
	centeredImage.fill(Qt::transparent); // Fill with transparent background

	QPainter painter(&centeredImage);
	painter.setRenderHint(QPainter::Antialiasing);
	QPoint centeringOffset((size.width() - boundingRect.width()) / 2, (size.height() - boundingRect.height()) / 2);
	painter.drawImage(centeringOffset, image.copy(boundingRect));
	painter.end();

	return centeredImage;
}

std::optional<QRectF> LogoGenerator::drawIcon(QPainter& painter, QSvgRenderer* iconSvgRenderer, qreal x, qreal y, qreal height, qreal width, qreal topMargin, qreal leftMargin)
{
	if (iconSvgRenderer == nullptr)
	{
		return {};
	}

	// Get the original size of the SVG
	auto iconOriginalSize = iconSvgRenderer->defaultSize();

	// Calculate the target width and height for the icon flattening by the scale factor
	auto iconTargetHeight = height - topMargin;
	auto iconTargetWidth = (iconOriginalSize.width() * iconTargetHeight) / iconOriginalSize.height();

	// Render the SVG image using the SVG renderer
	auto iconRect = QRectF(x + leftMargin, y + topMargin, iconTargetWidth, iconTargetHeight);
	iconSvgRenderer->render(&painter, iconRect);
	return iconRect;
}

std::optional<QRectF> LogoGenerator::drawLabel(QPainter& painter, LabelInfo const& labelInfo, QTextOption const& options, qreal x, qreal y, qreal height, qreal width)
{
	if (labelInfo.text.isEmpty())
	{
		return {};
	}

	auto font = labelInfo.font;
	font.setPixelSize(width); // Start with the maximum font size
	// If font size is not enforced auto scale it to fit the row width
	auto fontMetric = QFontMetrics(font);
	if (labelInfo.font.pixelSize() == -1 || labelInfo.font.pointSize() == -1)
	{
		// Auto scale the font to fit the row width and height if provided font do not enforce a size
		fitFontToWidth(font, labelInfo.text, width);
		fontMetric = fitFontToHeight(font, labelInfo.text, height);
	}

	painter.setFont(font);
	painter.setPen(labelInfo.color);
	auto labelRect = QRectF(x, y + labelInfo.topMargin, width, height);

	if (labelInfo.horizontalMirror)
	{
		qreal xTranslation = labelRect.left() + labelRect.width() / 2;
		qreal yTranslation = labelRect.top() + labelRect.height() / 2;

		QTransform horizontalMirrorTransform;
		horizontalMirrorTransform.translate(xTranslation, yTranslation); // Translate to center
		horizontalMirrorTransform.scale(1, -1); // Mirror Vertically
		horizontalMirrorTransform.translate(-xTranslation, -yTranslation); // Translate back
		painter.setTransform(horizontalMirrorTransform, true);
	}

	painter.drawText(labelRect, labelInfo.text, options);
	painter.resetTransform(); // Reset the eventual transformation

	auto const labelTightRect = fontMetric.tightBoundingRect(labelInfo.text);
	auto const labelTightWidth = static_cast<qreal>(labelTightRect.width());
	auto const labelTightHeight = static_cast<qreal>(labelTightRect.height());

	return QRectF{ labelRect.left(), labelRect.top(), labelTightWidth, labelTightHeight }; // Return the rectangle of drawn label
}

QFontMetrics LogoGenerator::fitFontToWidth(QFont& font, QString const& text, qreal width)
{
	font.setPixelSize(width); // Start with the maximum font size
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

QFontMetrics LogoGenerator::fitFontToHeight(QFont& font, QString const& text, qreal height)
{
	font.setPixelSize(height); // Start with the maximum font size
	auto fontMetrics = QFontMetrics(font);
	int textHeight = 0;
	do
	{
		textHeight = fontMetrics.height();
		font.setPixelSize(font.pixelSize() - 1);
		fontMetrics = QFontMetrics(font);
	} while (textHeight > height && font.pixelSize() > 1);

	return fontMetrics;
}
} // namespace qtMate::image
