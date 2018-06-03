/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QApplication>
#include <QDebug>

#include "toolkit/graph/view.hpp"
#include "toolkit/graph/node.hpp"
#include "toolkit/graph/inputSocket.hpp"
#include "toolkit/graph/outputSocket.hpp"

#include "mappingMatrix.hpp"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	mappingMatrix::Outputs outputs{
		{"Output Node 0", {"Socket 0", "Socket 1", "Socket 2", "Socket 2" }},
		{"Output Node 1", {"Socket 0", "Socket 1", "Socket 2", "Socket 2" }},
	};
	mappingMatrix::Inputs inputs{
		{"Input Node 0", {"Socket 0", "Socket 1"}},
		{"Input Node 1", {"Socket 0"}},
	};
	mappingMatrix::Connections connections{
		{{0, 0}, {0, 0}},
		{{0, 0}, {0, 1}},
		{{1, 1}, {1, 0}},
	};

	mappingMatrix::MappingMatrixDialog view(outputs, inputs, connections);
	if (view.exec() == QDialog::Accepted)
	{
		for (auto const& connection : view.connections())
		{
			qDebug() << connection;
		}
	}

	return app.exec();
}
