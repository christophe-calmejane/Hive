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

#include <QPushButton>
#include <QDialog>
#include <QGridLayout>

#include <utility>
#include <vector>
#include <string>
#include "toolkit/graph/view.hpp"
#include "toolkit/graph/node.hpp"
#include "toolkit/graph/inputSocket.hpp"
#include "toolkit/graph/outputSocket.hpp"

namespace mappingMatrix
{
struct Node
{
	std::string name{};
	std::vector<std::string> sockets;
};

using Nodes = std::vector<Node>;
using Outputs = Nodes;
using Inputs = Nodes;
using SlotID = std::pair<size_t, size_t>; // Pair of "Node Index", "Socket Index"
using Connection = std::pair<SlotID, SlotID>; // Pair of "Output SlotID", "Input SlotID"
using Connections = std::vector<Connection>;

/*

	 Node 0                   Node 0
------------            ------------
| Socket 0 | ---------- | Socket 0 |
| Socket 1 |     \----- | Socket 1 |
| Socket 2 |            ------------
| Socket 3 |
------------                Node 1
												------------
	 Node 1          ---- | Socket 0 |
------------      /      ------------
| Socket 0 |     /
| Socket 1 | ---/
| Socket 2 |
| Socket 3 |
------------

 Connections:
	- <0,0> -> <0,0>
	- <0,0> -> <0,1>
	- <1,1> -> <1,0>

*/

class MappingMatrix : public graph::GraphicsView
{
public:
	MappingMatrix(const Outputs& outputs, const Inputs& inputs, const Connections& connections, QWidget* parent = nullptr)
		: graph::GraphicsView(parent)
		, _connections{ connections }
	{
		setScene(&_scene);

		auto const paddingX{ 150.f };
		auto const paddingY{ 5.f };

		auto outputNodeX{ 0.f };
		auto outputNodeY{ 0.f };

		auto inputNodeX{ 0.f };
		auto inputNodeY{ 0.f };

		for (auto const& item : outputs)
		{
			auto* node = new graph::NodeItem{ static_cast<int>(_outputs.size()), QString::fromStdString(item.name) };
			for (auto const& socket : item.sockets)
			{
				node->addOutput(QString::fromStdString(socket));
			}

			_outputs.emplace_back(node);
			_scene.addItem(node);

			node->setPos(outputNodeX, outputNodeY);
			outputNodeY += node->boundingRect().height() + paddingY;

			inputNodeX = std::max(inputNodeX, static_cast<float>(node->boundingRect().width()) + paddingX);
		}

		for (auto const& item : inputs)
		{
			auto* node = new graph::NodeItem{ static_cast<int>(_inputs.size()), QString::fromStdString(item.name) };
			for (auto const& socket : item.sockets)
			{
				node->addInput(QString::fromStdString(socket));
			}

			_inputs.emplace_back(node);
			_scene.addItem(node);

			node->setPos(inputNodeX, inputNodeY);
			inputNodeY += node->boundingRect().height() + paddingY;
		}

		for (auto const& connection : _connections)
		{
			auto* item = new graph::ConnectionItem;

			auto const& outputSlot{ connection.first };
			auto const& inputSlot{ connection.second };

			try
			{
				item->connectOutput(_outputs.at(outputSlot.first)->outputAt(static_cast<int>(outputSlot.second)));
				item->connectInput(_inputs.at(inputSlot.first)->inputAt(static_cast<int>(inputSlot.second)));

				_scene.addItem(item);
			}
			catch (...)
			{
				// FIXME
				delete item;
			}
		}

		connect(this, &graph::GraphicsView::connectionCreated, this,
			[this](graph::ConnectionItem* connection)
			{
				_connections.emplace_back(std::make_pair(connection->output()->nodeId(), connection->output()->index()), std::make_pair(connection->input()->nodeId(), connection->input()->index()));
			});

		connect(this, &graph::GraphicsView::connectionDeleted, this,
			[this](graph::ConnectionItem* connection)
			{
				_connections.erase(std::remove_if(_connections.begin(), _connections.end(),
														 [connection](Connection const& item)
														 {
															 return (item.first.first == connection->output()->nodeId() && item.first.second == connection->output()->index()) && (item.second.first == connection->input()->nodeId() && item.second.second == connection->input()->index());
														 }),
					_connections.end());
			});

		auto const scenePadding{ 80 };
		_scene.setSceneRect(_scene.sceneRect().adjusted(-scenePadding, -scenePadding, scenePadding, scenePadding));
	}

	Connections const& connections() const
	{
		return _connections;
	}

private:
	QGraphicsScene _scene{ this };
	std::vector<graph::NodeItem*> _outputs;
	std::vector<graph::NodeItem*> _inputs;
	Connections _connections;
};

class MappingMatrixDialog : public QDialog
{
public:
	MappingMatrixDialog(const Outputs& outputs, const Inputs& inputs, const Connections& connections, QWidget* parent = nullptr)
#ifdef Q_OS_WIN32
		: QDialog(parent, Qt::Dialog | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint) // Because Qt::Tool is ugly on windows and '?' needs to be hidden (currently not supported)
#else
		: QDialog(parent, Qt::Tool | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
#endif
		, _mappingMatrix{ outputs, inputs, connections, this }
	{
		auto* layout = new QGridLayout{ this };

		layout->addWidget(&_mappingMatrix, 0, 0, 1, 2);
		layout->addWidget(&_applyButton, 1, 0);
		layout->addWidget(&_cancelButton, 1, 1);

		connect(&_applyButton, &QPushButton::clicked, this, &QDialog::accept);
		connect(&_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	}

	Connections const& connections() const
	{
		return _mappingMatrix.connections();
	}

private:
	MappingMatrix _mappingMatrix;
	QPushButton _applyButton{ "Apply", this };
	QPushButton _cancelButton{ "Cancel", this };
};

} // namespace mappingMatrix
