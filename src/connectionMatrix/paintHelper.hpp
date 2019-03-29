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

#pragma once

#include <QPainter>

namespace connectionMatrix
{

void drawNothing(QPainter* painter, QRect const& rect);
void drawSquare(QPainter* painter, QRect const& rect);
void drawLozenge(QPainter* painter, QRect const& rect);
void drawCircle(QPainter* painter, QRect const& rect);

void drawConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawWrongDomainConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawWrongFormatConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawWrongDomainFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawWrongFormatFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawConnectedInterfaceDownStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawNotConnectedInterfaceDownStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawWrongDomainNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawWrongFormatNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
void drawErrorNotConnectedRedundantNode(QPainter* painter, QRect const& rect);
void drawErrorConnectedRedundantNode(QPainter* painter, QRect const& rect);
void drawPartiallyConnectedRedundantNode(QPainter* painter, QRect const& rect);
void drawEntityConnection(QPainter* painter, QRect const& rect);
void drawEntityNoConnection(QPainter* painter, QRect const& rect);
void drawNotApplicable(QPainter* painter, QRect const& rect);

} // namespace connectionMatrix
