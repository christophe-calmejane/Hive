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
#include "QtMate/graph/graphNodeItem.hpp"

#include <QApplication>
#include <QMouseEvent>
#include <QScrollBar>
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
	// A left press on a node is handled by the node (move/select). Anywhere else (empty area, edge or edge label, which
	// would otherwise be dead zones) it may be the start of a pan: the press is held back from the scene so that neither
	// an item nor the scene acts on it (eg. edge click action, selection clearing) when the user actually drags the view
	if (event->button() == Qt::LeftButton && !isNodeAt(event->position().toPoint()))
	{
		// Drop a gesture whose release has never been received (should not happen)
		endPanGesture();
		_pendingPress.reset(event->clone());
		_lastPanPos = event->position().toPoint();
		event->accept();
		return;
	}
	QGraphicsView::mousePressEvent(event);
}

void GraphView::mouseMoveEvent(QMouseEvent* event)
{
	// Drop a gesture whose release has never been received (should not happen)
	if (_pendingPress && !(event->buttons() & Qt::LeftButton))
	{
		endPanGesture();
	}
	if (_pendingPress)
	{
		auto const pos = event->position().toPoint();
		if (!_isPanning && (pos - _pendingPress->position().toPoint()).manhattanLength() >= QApplication::startDragDistance())
		{
			_isPanning = true;
			viewport()->setCursor(Qt::ClosedHandCursor);
		}
		if (_isPanning)
		{
			auto const delta = pos - _lastPanPos;
			horizontalScrollBar()->setValue(horizontalScrollBar()->value() + (isRightToLeft() ? delta.x() : -delta.x()));
			verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
			_lastPanPos = pos;
		}
		event->accept();
		return;
	}
	QGraphicsView::mouseMoveEvent(event);
}

void GraphView::mouseReleaseEvent(QMouseEvent* event)
{
	if (_pendingPress && event->button() == Qt::LeftButton)
	{
		auto const press = std::move(_pendingPress);
		auto const wasPanning = _isPanning;
		endPanGesture();
		if (wasPanning)
		{
			event->accept();
			return;
		}
		// Not dragged: it was a click, deliver the held back press to the scene (the release below completes it)
		QGraphicsView::mousePressEvent(press.get());
	}
	QGraphicsView::mouseReleaseEvent(event);
}

bool GraphView::isNodeAt(QPoint const& pos) const
{
	auto const itemsAtPos = items(pos);
	return std::any_of(itemsAtPos.begin(), itemsAtPos.end(),
		[](QGraphicsItem const* const item)
		{
			return dynamic_cast<GraphNodeItem const*>(item) != nullptr;
		});
}

void GraphView::endPanGesture()
{
	_pendingPress.reset();
	if (_isPanning)
	{
		_isPanning = false;
		viewport()->unsetCursor();
	}
}

} // namespace qtMate::graph
