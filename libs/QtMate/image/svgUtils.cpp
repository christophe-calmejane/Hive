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

#include "QtMate/image/svgUtils.hpp"

#include <QFile>
#include <QRegularExpression>

#include <stdexcept>

namespace qtMate::image::svgUtils
{
bool validateSVG(QDomDocument const& doc)
{
	// Get the root element of the XML document
	QDomElement root = doc.documentElement();

	// Check if the root element is <svg>
	if (root.tagName() != "svg" || root.namespaceURI() != "http://www.w3.org/2000/svg")
	{
		return false;
	}
	return true;
}

void patchSVGColor(QDomDocument& doc, std::optional<QColor> const& fill, std::optional<QColor> const& stroke)
{
	// Modify the color attributes in the SVG XML
	QDomNodeList elements = doc.elementsByTagName("*");
	for (int i = 0; i < elements.size(); ++i)
	{
		QDomElement element = elements.at(i).toElement();
		if (fill && element.hasAttribute("fill"))
		{
			element.setAttribute("fill", fill->name());
		}
		if (stroke && element.hasAttribute("stroke"))
		{
			element.setAttribute("stroke", stroke->name());
		}
	}

	// Handle CSS styles within <defs>
	QDomNodeList defsList = doc.elementsByTagName("defs");
	for (int i = 0; i < defsList.size(); ++i)
	{
		QDomElement defs = defsList.at(i).toElement();
		QDomNodeList styles = defs.elementsByTagName("style");
		for (int j = 0; j < styles.size(); ++j)
		{
			QDomElement styleElement = styles.at(j).toElement();
			QString styleContent = styleElement.text();
			if (fill)
			{
				styleContent.replace(QRegularExpression("fill:[^;]+"), QString("fill:%1").arg(fill->name()));
			}
			if (stroke)
			{
				styleContent.replace(QRegularExpression("stroke:[^;]+"), QString("stroke:%1").arg(stroke->name()));
			}
			styleElement.firstChild().setNodeValue(styleContent);
		}
	}
}

QSvgRenderer* loadSVGImage(QString const& path, std::optional<QColor> const& fill, std::optional<QColor> const& stroke)
{
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		throw std::runtime_error("Failed to open SVG file.");
	}

	auto doc = QDomDocument{};
	if (!doc.setContent(&file) && !validateSVG(doc))
	{
		file.close();
		throw std::runtime_error("Invalid SVG file.");
	}
	file.close();

	// Patch the color of the SVG
	patchSVGColor(doc, fill, stroke);

	// Convert the modified SVG XML back to a string
	auto modifiedSvg = doc.toString();

	// Render the modified SVG using QSvgRenderer
	auto svgRenderer = new QSvgRenderer(modifiedSvg.toUtf8());
	if (svgRenderer && !svgRenderer->isValid())
	{
		throw std::runtime_error("Failed to render modified SVG.");
	}

	return svgRenderer;
}
} // namespace qtMate::image::svgUtils
