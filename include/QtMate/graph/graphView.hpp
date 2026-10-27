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

#pragma once

#include <QGraphicsView>

#include <memory>

namespace qtMate::graph
{
/**
* @brief QGraphicsView with mouse driven zoom and pan, suitable to display any graph.
* @details Mouse wheel zooms in/out around the cursor position.
*          Left-dragging anywhere but on a node (empty area, edges and their labels) pans the view, nodes
*          remain movable/selectable. A left press outside of a node is held back from the scene until it
*          turns out to be a drag (panning the view) or a click (then delivered to the scene on release), so
*          the items other than the nodes must react to clicks, not to drags.
*/
class GraphView : public QGraphicsView
{
	Q_OBJECT
public:
	GraphView(QWidget* parent = nullptr);
	virtual ~GraphView() override;

	/** Zooms and centers the view so the whole scene content is visible (never zooms in past 100%). */
	void fitToContents();

protected:
	virtual void wheelEvent(QWheelEvent* event) override;
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void mouseReleaseEvent(QMouseEvent* event) override;

private:
	bool isNodeAt(QPoint const& pos) const;
	void endPanGesture();

	qreal _currentZoom{ 1.0 };
	std::unique_ptr<QMouseEvent> _pendingPress{}; /**< Left press outside of a node, held back from the scene while it may still turn into a pan */
	QPoint _lastPanPos{};
	bool _isPanning{ false };
};

} // namespace qtMate::graph
