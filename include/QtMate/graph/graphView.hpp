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

namespace qtMate::graph
{
/**
* @brief QGraphicsView with mouse driven zoom and pan, suitable to display any graph.
* @details Mouse wheel zooms in/out around the cursor position.
*          Left-dragging an empty area pans the view, items remain movable/selectable.
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
	virtual void mouseReleaseEvent(QMouseEvent* event) override;

private:
	qreal _currentZoom{ 1.0 };
};

} // namespace qtMate::graph
