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
#include "connectionWorkspace.hpp"

#include <QtMate/flow/flowScene.hpp>
#include <QtMate/flow/flowSocket.hpp>
#include <QtMate/flow/flowNode.hpp>

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

#include <QMimeData>

namespace
{
bool hasValidMimeData(QMimeData const* mimeData)
{
	return mimeData->hasFormat("application/x-node");
}

} // namespace

ConnectionWorkspace::ConnectionWorkspace(qtMate::flow::FlowScene* scene, QWidget* parent)
	: FlowView{ scene, parent }
{
	setMinimumSize(960, 720);
	setAcceptDrops(true);
}

ConnectionWorkspace::~ConnectionWorkspace() = default;

void ConnectionWorkspace::dragEnterEvent(QDragEnterEvent* event)
{
	if (hasValidMimeData(event->mimeData()))
	{
		_dragEnterAccepted = true;
		event->acceptProposedAction();
	}
	else
	{
		_dragEnterAccepted = false;
		QGraphicsView::dragEnterEvent(event);
	}
}
void ConnectionWorkspace::dragLeaveEvent(QDragLeaveEvent* event)
{
	if (!_dragEnterAccepted)
	{
		QGraphicsView::dragLeaveEvent(event);
	}
}

void ConnectionWorkspace::dragMoveEvent(QDragMoveEvent* event)
{
	if (hasValidMimeData(event->mimeData()))
	{
		event->acceptProposedAction();
	}
	else
	{
		QGraphicsView::dragMoveEvent(event);
	}
}

void ConnectionWorkspace::dropEvent(QDropEvent* event)
{
	if (hasValidMimeData(event->mimeData()))
	{
		event->acceptProposedAction();
	}
	else
	{
		QGraphicsView::dropEvent(event);
	}
}
