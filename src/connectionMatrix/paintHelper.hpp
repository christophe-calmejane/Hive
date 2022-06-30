/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "connectionMatrix/model.hpp"

#include <QRect>
#include <QPainter>
#include <QPainterPath>

namespace connectionMatrix
{
namespace paintHelper
{
QPainterPath buildHeaderArrowPath(QRect const& rect, Qt::Orientation const orientation, bool const isTransposed, bool const alwaysShowArrowTip, bool const alwaysShowArrowEnd, int const arrowOffset, int const arrowSize, int const width);
void drawCapabilities(QPainter* painter, QRect const& rect, Model::IntersectionData::Type const type, Model::IntersectionData::State const state, Model::IntersectionData::Flags const& flags, bool const drawMediaLockedDot, bool const drawCRFAudioConnections);

} // namespace paintHelper
} // namespace connectionMatrix
