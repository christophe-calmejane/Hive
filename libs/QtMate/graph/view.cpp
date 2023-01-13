/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "QtMate/graph/view.hpp"
#include "QtMate/graph/inputSocket.hpp"
#include "QtMate/graph/outputSocket.hpp"
#include "QtMate/graph/connection.hpp"
#include "QtMate/graph/type.hpp"

#include <QMouseEvent>
#include <QScrollBar>

namespace qtMate
{
namespace graph
{
Qt::Edges computeCloseEdges(QPoint const& p, QRect const& r, int const margin = 20)
{
	auto edges = Qt::Edges{};

	auto const left = r.left() + margin;
	auto const top = r.top() + margin;
	auto const right = r.right() - margin;
	auto const bottom = r.bottom() - margin;

	if (p.x() <= left)
	{
		edges.setFlag(Qt::Edge::LeftEdge);
	}
	if (p.y() <= top)
	{
		edges.setFlag(Qt::Edge::TopEdge);
	}
	if (p.x() >= right)
	{
		edges.setFlag(Qt::Edge::RightEdge);
	}
	if (p.y() >= bottom)
	{
		edges.setFlag(Qt::Edge::BottomEdge);
	}

	return edges;
}

GraphicsView::GraphicsView(QWidget* parent)
	: QGraphicsView(parent)
{
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setResizeAnchor(NoAnchor);
	setTransformationAnchor(AnchorUnderMouse);
	setDragMode(QGraphicsView::ScrollHandDrag);
	setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
}

void GraphicsView::mousePressEvent(QMouseEvent* event)
{
	auto* item = socketAt(event->pos());

	if (item)
	{
		auto const scenePos = mapToScene(event->pos());
		auto const itemPos = item->mapFromScene(scenePos);

		if (event->button() == Qt::LeftButton)
		{
			viewport()->setCursor(Qt::ClosedHandCursor);

			switch (item->type())
			{
				case ItemType::Input:
				{
					auto* socket = static_cast<InputSocketItem*>(item);
					if (socket->isOver(itemPos))
					{
						_connectionDragEvent = std::make_unique<ConnectionDragEvent>();
						startAutoScroll();

						if (socket->isConnected())
						{
							auto connection = socket->connection();
							emit connectionDeleted(connection);
							_connectionDragEvent->mode = ConnectionDragMode::MoveToInput;
							_connectionDragEvent->connections.insert(connection);
							connection->setStop(scenePos);
							connection->disconnectInput();
						}
						else
						{
							_connectionDragEvent->mode = ConnectionDragMode::ConnectToOutput;
							auto connection = new ConnectionItem;
							_connectionDragEvent->connections.insert(connection);
							connection->setStart(scenePos);
							connection->setStop(scenePos);
							connection->connectInput(socket);
							scene()->addItem(connection);
						}
						event->ignore();
					}
					return;
				}
				case ItemType::Output:
				{
					auto* socket = static_cast<OutputSocketItem*>(item);
					if (socket->isOver(itemPos))
					{
						_connectionDragEvent = std::make_unique<ConnectionDragEvent>();
						startAutoScroll();

						if (socket->isConnected() && event->modifiers().testFlag(Qt::ControlModifier))
						{
							_connectionDragEvent->mode = ConnectionDragMode::MoveToOutput;
							_connectionDragEvent->connections = socket->connections();
							for (auto& connection : _connectionDragEvent->connections)
							{
								emit connectionDeleted(connection);

								connection->setStart(scenePos);
								connection->disconnectOutput();
							}
						}
						else
						{
							_connectionDragEvent->mode = ConnectionDragMode::ConnectToInput;
							auto connection = new ConnectionItem;
							_connectionDragEvent->connections.insert(connection);
							connection->setStart(scenePos);
							connection->setStop(scenePos);
							connection->connectOutput(socket);
							scene()->addItem(connection);
						}
						event->ignore();
					}
					return;
				}
				default:
					break;
			}
		}
	}

	QGraphicsView::mousePressEvent(event);
}

void GraphicsView::mouseMoveEvent(QMouseEvent* event)
{
	auto const scenePos = mapToScene(event->pos());

	if (_connectionDragEvent && event->buttons().testFlag(Qt::LeftButton))
	{
		switch (_connectionDragEvent->mode)
		{
			case ConnectionDragMode::ConnectToInput:
			case ConnectionDragMode::MoveToInput:
			{
				for (auto& connection : _connectionDragEvent->connections)
				{
					connection->setStop(scenePos);
				}
				break;
			}
			case ConnectionDragMode::ConnectToOutput:
			case ConnectionDragMode::MoveToOutput:
			{
				for (auto& connection : _connectionDragEvent->connections)
				{
					connection->setStart(scenePos);
				}
				break;
			}
			default:
				break;
		}

		auto* item = socketAt(event->pos());
		if (item)
		{
			if (acceptableConnection(item))
			{
				viewport()->setCursor(Qt::DragMoveCursor);
			}
			else
			{
				viewport()->setCursor(Qt::ForbiddenCursor);
			}
		}
		else
		{
			viewport()->setCursor(Qt::ClosedHandCursor);
		}
	}

	QGraphicsView::mouseMoveEvent(event);
}

void GraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
	viewport()->setCursor(Qt::ArrowCursor);

	if (event->button() == Qt::LeftButton)
	{
		if (_connectionDragEvent)
		{
			auto item = socketAt(event->pos());
			if (!item || !acceptableConnection(item))
			{
				// Invalid, delete all connections
				for (auto& connection : _connectionDragEvent->connections)
				{
					delete connection;
				}
			}
			else
			{
				switch (_connectionDragEvent->mode)
				{
					case ConnectionDragMode::ConnectToInput:
					case ConnectionDragMode::MoveToInput:
					{
						auto* socket = static_cast<InputSocketItem*>(item);

						if (auto* connection = socket->connection())
						{
							emit connectionDeleted(connection);
							delete connection;
						}

						for (auto& connection : _connectionDragEvent->connections)
						{
							connection->connectInput(socket);
							emit connectionCreated(connection);
						}
						break;
					}
					case ConnectionDragMode::ConnectToOutput:
					case ConnectionDragMode::MoveToOutput:
					{
						auto* socket = static_cast<OutputSocketItem*>(item);

						for (auto& connection : _connectionDragEvent->connections)
						{
							connection->connectOutput(socket);
							emit connectionCreated(connection);
						}
						break;
					}
					default:
						break;
				}
			}

			stopAutoScroll();
			_connectionDragEvent.reset();
			event->ignore();
			return;
		}
	}

	QGraphicsView::mouseReleaseEvent(event);
}

void GraphicsView::timerEvent(QTimerEvent* event)
{
	auto const pos = mapFromGlobal(QCursor::pos());
	auto const viewportGeometry = viewport()->geometry();

	// Check if the mouse is close to an edge of the viewport
	auto const edges = computeCloseEdges(pos, viewportGeometry, 20);

	if (edges != 0)
	{
		// If so, scroll the contant by the distance in pixels between the mouse and the matching edge
		auto dx = 0;
		auto dy = 0;

		if (edges.testFlag(Qt::LeftEdge))
		{
			dx -= std::abs(pos.x() - viewportGeometry.left());
		}

		if (edges.testFlag(Qt::RightEdge))
		{
			dx += pos.x() - viewportGeometry.right();
		}

		if (edges.testFlag(Qt::TopEdge))
		{
			dy -= std::abs(pos.y() - viewportGeometry.top());
		}

		if (edges.testFlag(Qt::BottomEdge))
		{
			dy += std::abs(pos.y() - viewportGeometry.bottom());
		}

		if (dx)
		{
			horizontalScrollBar()->setValue(horizontalScrollBar()->value() + dx);
		}

		if (dy)
		{
			verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);
		}
	}
}


bool GraphicsView::acceptableConnection(SocketItem* item) const
{
	if (!item || !_connectionDragEvent)
	{
		return false;
	}

	switch (_connectionDragEvent->mode)
	{
		case ConnectionDragMode::ConnectToInput:
		case ConnectionDragMode::MoveToInput:
			return item->type() == ItemType::Input;
		case ConnectionDragMode::ConnectToOutput:
		case ConnectionDragMode::MoveToOutput:
			return item->type() == ItemType::Output;
		default:
			return false;
	}
}

SocketItem* GraphicsView::socketAt(QPoint const& pos) const
{
	for (auto const& item : items(pos))
	{
		switch (item->type())
		{
			case ItemType::Input:
			case ItemType::Output:
				return static_cast<SocketItem*>(item);
			default:
				break;
		}
	}

	return nullptr;
}

void GraphicsView::startAutoScroll()
{
	_autoScrollTimer = startTimer(std::chrono::milliseconds{ 50 });
}

void GraphicsView::stopAutoScroll()
{
	killTimer(_autoScrollTimer);
}

} // namespace graph
} // namespace qtMate
