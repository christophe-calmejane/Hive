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

#include "painterHelper.hpp"

namespace painterHelper
{
void drawCentered(QPainter* painter, QRect const& rect, QImage const& image)
{
	drawCentered(painter, rect, QPixmap::fromImage(image));
}

void drawCentered(QPainter* painter, QRect const& rect, QPixmap const& pixmap)
{
	if (!painter || !rect.isValid() || pixmap.isNull())
	{
		return;
	}

	auto const devicePixelRatio = painter->device()->devicePixelRatioF();

	auto scaledPixmap = pixmap.scaled(rect.width() * devicePixelRatio, rect.height() * devicePixelRatio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	scaledPixmap.setDevicePixelRatio(devicePixelRatio);

	auto const x = rect.x() + (rect.width() - scaledPixmap.width() / devicePixelRatio) / 2;
	auto const y = rect.y() + (rect.height() - scaledPixmap.height() / devicePixelRatio) / 2;

	painter->drawPixmap(x, y, scaledPixmap);
}

} // namespace painterHelper
