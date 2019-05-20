/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

QPixmap generatePixmap(QString const& what, QColor const& color)
{
	QPixmap pixmap{ 16, 16 };
	pixmap.fill(Qt::transparent);
	
	QPainter painter{ &pixmap };
	painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::HighQualityAntialiasing);

	QFont font{ "Material Icons" };
	font.setStyleStrategy(QFont::PreferQuality);
	painter.setFont(font);

	painter.setPen(color);
	painter.drawText(pixmap.rect(), what, QTextOption{ Qt::AlignCenter });

	return pixmap;
}

} // namespace helper
} // namespace material
} // namespace toolkit
} // namespace qt
