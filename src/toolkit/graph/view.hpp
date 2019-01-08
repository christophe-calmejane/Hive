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

#include <QGraphicsView>
#include "connection.hpp"
#include <memory>

namespace graph
{
enum class ConnectionDragMode
{
	Undefined,
	ConnectToInput,
	ConnectToOutput,
	MoveToInput,
	MoveToOutput
};

class ConnectionDragEvent
{
public:
	ConnectionItems connections;
	ConnectionDragMode mode{ ConnectionDragMode::Undefined };
};

class SocketItem;

class GraphicsView : public QGraphicsView
{
	Q_OBJECT
public:
	GraphicsView(QWidget* parent = nullptr);

	Q_SIGNAL void connectionCreated(ConnectionItem* connection);
	Q_SIGNAL void connectionDeleted(ConnectionItem* connection);

private:
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void mouseReleaseEvent(QMouseEvent* event) override;

private:
	bool acceptableConnection(SocketItem* item) const;
	SocketItem* socketAt(QPoint const& pos) const;

private:
	std::unique_ptr<ConnectionDragEvent> _connectionDragEvent{};
};

} // namespace graph
