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

#pragma once

#include <QtMate/flow/flowscene.hpp>
#include <QtMate/flow/flowscenedelegate.hpp>
#include <QtMate/flow/flowview.hpp>
#include <QtMate/flow/flownode.hpp>
#include <QtMate/flow/flowinput.hpp>
#include <QtMate/flow/flowoutput.hpp>

#include <QPushButton>
#include <QDialog>
#include <QGridLayout>

#include <utility>
#include <vector>
#include <string>

namespace mappingMatrix
{
struct Node
{
	std::string name{};
	std::vector<std::string> sockets{};
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

class MappingMatrix : public QWidget
{
public:
	enum SocketType {
		Input,
		Output,
	};
	
	class Delegate : public qtMate::flow::FlowSceneDelegate {
	public:
		using FlowSceneDelegate::FlowSceneDelegate;
		
		virtual bool canConnect(qtMate::flow::FlowOutput* output, qtMate::flow::FlowInput* input) const override {
			return true;
		}
		
		virtual QColor socketTypeColor(qtMate::flow::FlowSocketType type) const {
			return 0x2196F3;
		}
	};


	MappingMatrix(Outputs const& outputs, Inputs const& inputs, Connections const& connections, QWidget* parent = nullptr)
		: QWidget{parent}
	{
		auto* delegate = new Delegate{this};
		auto* scene = new qtMate::flow::FlowScene{delegate, this};
		auto* view = new qtMate::flow::FlowView{scene, this};
		
		auto* layout = new QHBoxLayout{this};
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(view);
		
		// Get notified by the scene when connections are created/destroyed
		connect(scene, &qtMate::flow::FlowScene::connectionCreated, this, [&](auto const& descriptor) {
			_connections.insert(descriptor);
		});
		connect(scene, &qtMate::flow::FlowScene::connectionDestroyed, this, [&](auto const& descriptor) {
			_connections.remove(descriptor);
		});
		
		// Needed to configure each node position in the scene
		auto const paddingX{ 150.f };
		auto const paddingY{ 5.f };

		auto outputNodeX{ 0.f };
		auto outputNodeY{ 0.f };

		auto inputNodeX{ 0.f };
		auto inputNodeY{ 0.f };

		// CAUTION: as the spec tells that outputs and inputs nodes are identified by their index in their own list,
		// which is not compatible with qtMate::flow API which requires a unique identifier for each node,
		// we must generate this unique identifier ourselves and also keep the offset to the first input node
		// in order to be able to translate connections back and forth.
		auto id = 0;
		_offset = outputs.size();
		
		// Create output nodes
		for (auto const& output : outputs)
		{
			auto descriptor = qtMate::flow::FlowNodeDescriptor{};
			descriptor.name = QString::fromStdString(output.name);
			for (auto const& socket : output.sockets)
			{
				descriptor.outputs.push_back({QString::fromStdString(socket), SocketType::Output});
			}
			
			auto* node = scene->createNode(id++, descriptor);
			node->setFlag(QGraphicsItem::ItemIsMovable, false);
			node->setPos(outputNodeX, outputNodeY);
			outputNodeY += node->boundingRect().height() + paddingY;

			inputNodeX = std::max(inputNodeX, static_cast<float>(node->boundingRect().width()) + paddingX);
		}

		// Create input nodes
		for (auto const& input : inputs)
		{
			auto descriptor = qtMate::flow::FlowNodeDescriptor{};
			descriptor.name = QString::fromStdString(input.name);
			for (auto const& socket : input.sockets)
			{
				descriptor.inputs.push_back({QString::fromStdString(socket), SocketType::Input});
			}
			
			auto* node = scene->createNode(id++, descriptor);
			node->setFlag(QGraphicsItem::ItemIsMovable, false);
			node->setPos(inputNodeX, inputNodeY);
			inputNodeY += node->boundingRect().height() + paddingY;
		}

		// Create connections
		for (auto const& connection : connections)
		{
			scene->createConnection(convert(connection));
		}
	}

	Connections connections() const
	{
		auto result = Connections{};
		for (auto const& connection : _connections)
		{
			result.emplace_back(convert(connection));
		}
		return result;
	}
	
	Connection convert(qtMate::flow::FlowConnectionDescriptor const& descriptor) const
	{
		auto const source = SlotID{descriptor.first.first, descriptor.first.second};
		auto const sink = SlotID{descriptor.second.first - _offset, descriptor.second.second};
		return Connection{source, sink};
	}
	
	qtMate::flow::FlowConnectionDescriptor convert(Connection const& connection) const
	{
		auto const source = qtMate::flow::FlowSocketSlot{connection.first.first, connection.first.second};
		auto const sink = qtMate::flow::FlowSocketSlot{connection.second.first + _offset, connection.second.second};
		return qtMate::flow::FlowConnectionDescriptor{source, sink};
	}
	
private:
	int _offset{}; // we need an offset to identify output nodes
	qtMate::flow::FlowConnectionDescriptors _connections{};
};

class MappingMatrixDialog : public QDialog
{
public:
	MappingMatrixDialog(QString const& title, Outputs const& outputs, Inputs const& inputs, Connections const& connections, QWidget* parent = nullptr)
#ifdef Q_OS_WIN32
		: QDialog(parent, Qt::Dialog | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint) // Because Qt::Tool is ugly on windows and '?' needs to be hidden (currently not supported)
#else
		: QDialog(parent, Qt::Tool | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
#endif
		, _mappingMatrix{ outputs, inputs, connections, this }
	{
		setWindowTitle(title);

		auto* layout = new QGridLayout{ this };

		layout->addWidget(&_mappingMatrix, 0, 0, 1, 2);
		layout->addWidget(&_applyButton, 1, 0);
		layout->addWidget(&_cancelButton, 1, 1);

		connect(&_applyButton, &QPushButton::clicked, this, &QDialog::accept);
		connect(&_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	}

	Connections connections() const
	{
		return _mappingMatrix.connections();
	}

private:
	MappingMatrix _mappingMatrix;
	QPushButton _applyButton{ "Apply", this };
	QPushButton _cancelButton{ "Cancel", this };
};

} // namespace mappingMatrix
