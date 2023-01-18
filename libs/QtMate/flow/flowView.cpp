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

#include "QtMate/flow/flowView.hpp"
#include "QtMate/flow/flowScene.hpp"
#include "QtMate/flow/flowNode.hpp"
#include "QtMate/flow/flowInput.hpp"
#include "QtMate/flow/flowOutput.hpp"
#include "QtMate/flow/flowConnection.hpp"
#include "QtMate/flow/flowStyle.hpp"

#include <QMouseEvent>
#include <QScrollBar>

namespace qtMate::flow
{
FlowView::FlowView(FlowScene* scene, QWidget* parent)
	: QGraphicsView{ scene, parent }
	, _scene{ scene }
{
	setRenderHint(QPainter::Antialiasing);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setDragMode(DragMode::NoDrag);
	setRenderHint(QPainter::Antialiasing);
	setScene(_scene);

	_elapsedTimer.start();
	_animationTimerId = startTimer(std::chrono::milliseconds{ 16 }, Qt::TimerType::CoarseTimer);

	// configure animations
	_centerOnAnimation = new QVariantAnimation{ this };
	_centerOnAnimation->setDuration(800);
	_centerOnAnimation->setEasingCurve(QEasingCurve::Type::OutQuart);
	connect(_centerOnAnimation, &QVariantAnimation::valueChanged, this,
		[this](QVariant const& value)
		{
			centerOn(value.toPointF());
		});
}

FlowView::~FlowView() = default;

void FlowView::animatedCenterOn(QPointF const& scenePos)
{
	auto const width = viewport()->width();
	auto const height = viewport()->height();
	auto const startValue = mapToScene(QPoint{ width / 2, height / 2 });
	auto const endValue = scenePos;

	_centerOnAnimation->stop();
	_centerOnAnimation->setStartValue(startValue);
	_centerOnAnimation->setEndValue(endValue);
	_centerOnAnimation->start(QAbstractAnimation::DeletionPolicy::KeepWhenStopped);
}

void FlowView::mousePressEvent(QMouseEvent* event)
{
	if (!handleMousePressEvent(event))
	{
		QGraphicsView::mousePressEvent(event);
	}
}

void FlowView::mouseMoveEvent(QMouseEvent* event)
{
	if (!handleMouseMoveEvent(event))
	{
		QGraphicsView::mouseMoveEvent(event);
	}
}

void FlowView::mouseReleaseEvent(QMouseEvent* event)
{
	if (!handleMouseReleaseEvent(event))
	{
		QGraphicsView::mouseReleaseEvent(event);
	}
}

void FlowView::wheelEvent(QWheelEvent* event)
{
	if (!handleWheelEvent(event))
	{
		QGraphicsView::wheelEvent(event);
	}
}

void FlowView::timerEvent(QTimerEvent* event)
{
	if (!handleTimerEvent(event))
	{
		QGraphicsView::timerEvent(event);
	}
}

bool FlowView::handleMousePressEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton)
	{
		return false;
	}

	auto const scenePos = mapToScene(event->pos());
	if (auto* socket = socketAt(scenePos))
	{
		// save current socket type, in order to apply the appropriate color to the links
		_tmpSocketType = socket->descriptor().type;

		// check whether we want to create or move existing connections (only applies to outputs)
		auto const isOutputSocket = socket->type() == FlowOutput::Type;
		auto const hasMoveModifier = event->modifiers().testFlag(Qt::ControlModifier);
		auto const skipChange = isOutputSocket && !hasMoveModifier;

		if (socket->isConnected() && !skipChange)
		{
			if (socket->type() == FlowInput::Type)
			{
				_mode = ConnectionMode::ChangeInput;
				auto* input = qgraphicsitem_cast<FlowInput*>(socket);
				if (auto* connection = input->connection())
				{
					createLinkFromConnection(connection);
					_slots.insert(connection->output()->slot());
				}
			}
			else
			{
				_mode = ConnectionMode::ChangeOutput;
				auto* output = qgraphicsitem_cast<FlowOutput*>(socket);
				for (auto* connection : output->connections())
				{
					createLinkFromConnection(connection);
					_slots.insert(connection->input()->slot());
				}
			}
		}
		else
		{
			_tmpConnection.reset(new FlowConnection);
			_slots.insert(socket->slot());

			if (socket->type() == FlowInput::Type)
			{
				_mode = ConnectionMode::ConnectToOutput;
				_tmpConnection->setInput(qgraphicsitem_cast<FlowInput*>(socket));
			}
			else
			{
				_mode = ConnectionMode::ConnectToInput;
				_tmpConnection->setOutput(qgraphicsitem_cast<FlowOutput*>(socket));
			}

			auto* link = createLinkFromConnection(nullptr);
			auto const pos = socket->hotSpotSceneCenter();
			link->setStart(pos);
			link->setStop(pos);
		}

		return true;
	}

	return false;
}

bool FlowView::handleMouseMoveEvent(QMouseEvent* event)
{
	if (_mode == ConnectionMode::Undefined)
	{
		return false;
	}

	auto const scenePos = mapToScene(event->pos());
	switch (_mode)
	{
		case ConnectionMode::ChangeInput:
		case ConnectionMode::ConnectToInput:
			for (auto* link : _links)
			{
				link->setStop(scenePos);
			}
			break;
		case ConnectionMode::ChangeOutput:
		case ConnectionMode::ConnectToOutput:
			for (auto* link : _links)
			{
				link->setStart(scenePos);
			}
			break;
		default:
			break;
	}

	if (auto* socket = socketAt(scenePos))
	{
		if (canConnect(socket))
		{
			setCursor(Qt::CursorShape::DragMoveCursor);
		}
		else
		{
			setCursor(Qt::CursorShape::ForbiddenCursor);
		}
	}
	else
	{
		setCursor(Qt::CursorShape::ArrowCursor);
	}

	// if the mouse is close to an edge of the viewport, try to scroll
	auto const margin = 20;
	auto const step = 5;

	// build an adjusted rect from the viewport
	auto const pos = event->pos();
	auto const rect = viewport()->geometry().adjusted(margin, margin, -2 * margin, -2 * margin);

	auto dx = 0;
	auto dy = 0;

	// check if the mouse is close to the viewport borders
	if (pos.x() <= rect.left())
	{
		dx -= step;
	}
	else if (pos.x() >= rect.right())
	{
		dx += step;
	}

	if (pos.y() <= rect.top())
	{
		dy -= step;
	}
	else if (pos.y() >= rect.bottom())
	{
		dy += step;
	}

	// apply scroll delta if any
	horizontalScrollBar()->setValue(horizontalScrollBar()->value() + dx);
	verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);

	return true;
}

bool FlowView::handleMouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton)
	{
		return false;
	}

	if (_mode == ConnectionMode::Undefined)
	{
		return false;
	}

	// clear tmp connection
	_tmpSocketType = 0;
	_tmpConnection.reset();

	// gather the list of connections that needs to be created
	auto descriptors = FlowConnectionDescriptors{};
	auto maybeNodeUid = std::optional<FlowNodeUid>{};

	auto const scenePos = mapToScene(event->pos());
	if (auto* socket = socketAt(scenePos))
	{
		maybeNodeUid = socket->node()->uid();
		switch (_mode)
		{
			case ConnectionMode::ChangeInput:
			case ConnectionMode::ConnectToInput:
				if (socket->type() == FlowInput::Type)
				{
					auto* input = qgraphicsitem_cast<FlowInput*>(socket);
					for (auto const& slot : _slots)
					{
						descriptors.insert({ slot, input->slot() });
					}
				}
				break;
			case ConnectionMode::ChangeOutput:
			case ConnectionMode::ConnectToOutput:
				if (socket->type() == FlowOutput::Type)
				{
					auto* output = qgraphicsitem_cast<FlowOutput*>(socket);
					for (auto const& slot : _slots)
					{
						descriptors.insert({ output->slot(), slot });
					}
				}
				break;
			default:
				break;
		}
	}

	// sanity check, all connections must be valid, otherwise do nothing and restore previous state
	auto const valid = std::all_of(std::begin(descriptors), std::end(descriptors),
		[this](FlowConnectionDescriptor const& descriptor)
		{
			return _scene->canConnect(descriptor);
		});

	if (valid)
	{
		// avoid re-creating existing connections
		for (auto* connection : _connections)
		{
			auto const descriptor = connection->descriptor();
			// spotted already existing connection
			if (descriptors.contains(descriptor))
			{
				// remove it from the list of connections to create
				descriptors.remove(descriptor);
				// reset opacity
				connection->setOpacity(1.f);
			}
			else
			{
				_scene->destroyConnection(descriptor);
			}
		}
		_connections.clear();

		// create new connections
		for (auto const& descriptor : descriptors)
		{
			_scene->createConnection(descriptor);
		}
	}
	else
	{
		// reset connection opacity
		for (auto* connection : _connections)
		{
			connection->setOpacity(1.f);
		}
		_connections.clear();

		// if over a node, shake it up to notify the user of failure
		if (maybeNodeUid)
		{
			auto* animation = new QVariantAnimation{ this };
			animation->setDuration(800);
			animation->setEasingCurve(QEasingCurve::Type::Linear);
			animation->setKeyValueAt(0.1, QPoint{ +4, 0 });
			animation->setKeyValueAt(0.2, QPoint{ -4, 0 });
			animation->setKeyValueAt(0.3, QPoint{ +4, 0 });
			animation->setKeyValueAt(0.4, QPoint{ -4, 0 });
			animation->setKeyValueAt(0.5, QPoint{ +4, 0 });
			animation->setKeyValueAt(0.6, QPoint{ -4, 0 });
			animation->setKeyValueAt(0.7, QPoint{ +4, 0 });
			animation->setKeyValueAt(0.8, QPoint{ -4, 0 });
			animation->setKeyValueAt(0.9, QPoint{ +4, 0 });
			animation->setKeyValueAt(1.0, QPoint{ -4, 0 });

			// caution, pass the node uid and retrieve it in the lambda cause the node may have been deleted
			connect(animation, &QVariantAnimation::valueChanged, this,
				[this, uid = *maybeNodeUid](QVariant const& value)
				{
					if (auto* n = _scene->node(uid))
					{
						n->setPos(n->pos() + value.toPointF());
					}
				});

			animation->start(QAbstractAnimation::DeletionPolicy::DeleteWhenStopped);
		}
	}

	// clear state
	qDeleteAll(_links);
	_links.clear();

	_slots.clear();

	_mode = ConnectionMode::Undefined;

	// reset cursor
	setCursor(Qt::CursorShape::ArrowCursor);

	return true;
}

bool FlowView::handleWheelEvent(QWheelEvent* event)
{
	if (event->modifiers().testFlag(Qt::KeyboardModifier::ControlModifier))
	{
		auto const anchor = transformationAnchor();
		setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
		auto const angle = event->angleDelta().y();
		auto const factor = qPow(1.0015, angle);
		scale(factor, factor);
		setTransformationAnchor(anchor);
		return true;
	}

	return false;
}

bool FlowView::handleTimerEvent(QTimerEvent* event)
{
	if (event->timerId() != _animationTimerId)
	{
		return false;
	}

	auto const offset = _elapsedTimer.elapsed() / 100.f;

	auto pen = NODE_CONNECTION_PEN;
	pen.setDashOffset(-offset);

	// default color for volatile connections
	pen.setColor(_scene->socketTypeColor(_tmpSocketType));
	for (auto* link : _links)
	{
		link->setPen(pen);
	}

	for (auto* connection : _scene->connections())
	{
		pen.setColor(_scene->socketTypeColor(connection->output()->descriptor().type));
		connection->setPen(pen);
	}

	return true;
}

FlowSocket* FlowView::socketAt(QPointF const& scenePos) const
{
	for (auto* item : _scene->items(scenePos))
	{
		if (auto* input = qgraphicsitem_cast<FlowInput*>(item); input && input->hit(scenePos))
		{
			return input;
		}
		else if (auto* output = qgraphicsitem_cast<FlowOutput*>(item); output && output->hit(scenePos))
		{
			return output;
		}
	}
	return nullptr;
}

FlowLink* FlowView::createLinkFromConnection(FlowConnection* connection)
{
	auto* link = new FlowLink{};

	if (connection)
	{
		link->setStart(connection->output()->hotSpotSceneCenter());
		link->setStop(connection->input()->hotSpotSceneCenter());

		_connections.insert(connection);
		connection->setOpacity(0.2f);
	}

	_links.insert(link);
	_scene->addItem(link);

	return link;
}

bool FlowView::canConnect(FlowSocket* socket) const
{
	switch (_mode)
	{
		case ConnectionMode::ConnectToInput:
			if (socket->type() == FlowInput::Type)
			{
				auto* input = qgraphicsitem_cast<FlowInput*>(socket);
				return _scene->canConnect(_tmpConnection->output(), input);
			}
			break;
		case ConnectionMode::ConnectToOutput:
			if (socket->type() == FlowOutput::Type)
			{
				auto* output = qgraphicsitem_cast<FlowOutput*>(socket);
				return _scene->canConnect(output, _tmpConnection->input());
			}
			break;
		case ConnectionMode::ChangeInput:
			if (socket->type() == FlowInput::Type)
			{
				auto* input = qgraphicsitem_cast<FlowInput*>(socket);
				return std::all_of(std::begin(_connections), std::end(_connections),
					[this, input](FlowConnection* connection)
					{
						return _scene->canConnect(connection->output(), input);
					});
			}
			break;
		case ConnectionMode::ChangeOutput:
			if (socket->type() == FlowOutput::Type)
			{
				auto* output = qgraphicsitem_cast<FlowOutput*>(socket);
				return std::all_of(std::begin(_connections), std::end(_connections),
					[this, output](FlowConnection* connection)
					{
						return _scene->canConnect(output, connection->input());
					});
			}
			break;
		default:
			break;
	}

	return false;
}

} // namespace qtMate::flow
