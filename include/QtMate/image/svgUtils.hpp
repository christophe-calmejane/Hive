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

#include <QColor>
#include <QDomDocument>
#include <QSvgRenderer>
#include <QString>

#include <memory>

namespace qtMate::image::svgUtils
{
/**
 * @brief Validate if the loaded XML is an actual SVG file.
 * @param doc The QDomDocument containing the loaded XML.
 * @throws std::runtime_error if the XML is not a valid SVG file.
 */
bool validateSVG(QDomDocument const& doc);

/**
 * @brief Patch the color of an SVG file loaded as XML.
 * @details This function modifies the color attributes of the SVG elements in the provided QDomDocument but does not validate provided XML to be valid SVG.
 * @param doc The QDomDocument containing the loaded SVG XML.
 * @param fill The fill color to set for the SVG elements.
 * @param stroke The stroke color to set for the SVG elements.
 * @throws std::runtime_error if the QDomDocument is not valid.
 */
void patchSVGColor(QDomDocument& doc, std::optional<QColor> const& fill, std::optional<QColor> const& stroke = std::nullopt);

/**
 * @brief Construct an SVG Renderer from a file path and patch color.
 * @param path Path to the SVG file.
 * @param color Color to override the SVG color (default is black).
 * @return QSvgRenderer object containing the loaded SVG image.
 * @throws std::runtime_error if the SVG file is invalid or cannot be loaded.
 */
QSvgRenderer* loadSVGImage(QString const& path, std::optional<QColor> const& fill, std::optional<QColor> const& stroke = std::nullopt);
} // namespace qtMate::image::svgUtils
