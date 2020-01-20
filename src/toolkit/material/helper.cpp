/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "helper.hpp"
#include <QPainter>

namespace qt
{
namespace toolkit
{
namespace material
{
namespace helper
{
QIcon generateIcon(QString const& what, QColor const& color)
{
	auto const generatePixmap = [&color, &what](int size, int dpr)
	{
		QPixmap pixmap{ size * dpr, size * dpr };
		pixmap.setDevicePixelRatio(dpr);
		pixmap.fill(Qt::transparent);

		QPainter painter{ &pixmap };
		painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::HighQualityAntialiasing);

		QFont font{ "Material Icons" };
		font.setStyleStrategy(QFont::PreferQuality);
		font.setPointSize(size - 8);

		painter.setFont(font);

		painter.setPen(color);
		painter.drawText(QRect{ 0, 0, size, size }, what, QTextOption{ Qt::AlignCenter });

		return pixmap;
	};

	QIcon icon;

	for (auto const size : { 16, 32, 64, 128 })
	{
		icon.addPixmap(generatePixmap(size, 1));
		icon.addPixmap(generatePixmap(size, 2));
	}

	return icon;
}

} // namespace helper
} // namespace material
} // namespace toolkit
} // namespace qt
