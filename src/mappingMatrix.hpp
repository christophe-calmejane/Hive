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

#pragma once

#include <QWidget>
#include <QPushButton>
#include <QDialog>

#include <vector>
#include <string>
#include <any>

namespace mappingMatrix
{
struct Node
{
	std::string name{};
	std::vector<std::string> sockets{};
	std::any userData{};
};

using Nodes = std::vector<Node>;
using Outputs = Nodes;
using Inputs = Nodes;
using SlotID = std::pair<quint32, quint32>; // Pair of "Node Index", "Socket Index"
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
 
 
 CAUTION: Input and Output nodes can share the same ID as it is the index of the node in the provided list

*/

class MappingMatrix;

class MappingMatrixDialog : public QDialog
{
public:
	// Construct the MappingMatrix editor dialog
	MappingMatrixDialog(QString const& title, Outputs const& outputs, Inputs const& inputs, Connections const& connections, QWidget* parent = nullptr);

	// Return the list of active connections,
	// Note: it is ment to be retrived after the dialog has been closed
	Connections connections() const;

private:
	MappingMatrix* _matrix{};
};

} // namespace mappingMatrix
