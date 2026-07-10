/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "QtMate/graph/graphView.hpp"

#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace qtMate::graph
{
static constexpr auto MinZoom = 0.1;
static constexpr auto MaxZoom = 4.0;

GraphView::GraphView(QWidget* parent)
	: QGraphicsView{ parent }
{
	setRenderHint(QPainter::Antialiasing);
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	setDragMode(QGraphicsView::NoDrag);
}

GraphView::~GraphView() = default;

void GraphView::fitToContents()
{
	if (!scene())
	{
		return;
	}

	auto const contentRect = scene()->itemsBoundingRect().adjusted(-20.0, -20.0, 20.0, 20.0);
	if (contentRect.isEmpty())
	{
		return;
	}

	fitInView(contentRect, Qt::KeepAspectRatio);

	// Never zoom in past 100%, a small graph should be displayed at its natural size
	auto const appliedZoom = transform().m11();
	if (appliedZoom > 1.0)
	{
		scale(1.0 / appliedZoom, 1.0 / appliedZoom);
	}
	_currentZoom = std::min(appliedZoom, 1.0);
}

void GraphView::wheelEvent(QWheelEvent* event)
{
	auto const factor = std::pow(1.0015, event->angleDelta().y());
	auto const newZoom = std::clamp(_currentZoom * factor, MinZoom, MaxZoom);
	auto const appliedFactor = newZoom / _currentZoom;

	scale(appliedFactor, appliedFactor);
	_currentZoom = newZoom;

	event->accept();
}

void GraphView::mousePressEvent(QMouseEvent* event)
{
	// Pan when left-pressing an empty area, otherwise let items handle the event (move/select)
	if (event->button() == Qt::LeftButton && !itemAt(event->pos()))
	{
		setDragMode(QGraphicsView::ScrollHandDrag);
	}
	QGraphicsView::mousePressEvent(event);
}

void GraphView::mouseReleaseEvent(QMouseEvent* event)
{
	QGraphicsView::mouseReleaseEvent(event);
	if (dragMode() == QGraphicsView::ScrollHandDrag)
	{
		setDragMode(QGraphicsView::NoDrag);
	}
}

} // namespace qtMate::graph
