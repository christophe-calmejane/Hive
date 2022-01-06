/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QObject>
#include <QModelIndex>

#include "discoveredEntities/view.hpp"
#include "connectionMatrix/model.hpp"

// Mini controller linking discoveredEntities::View selection to connectionMatrix::Model::SelectedEntityRole
class ListViewSelectionToMatrixModelController final : public QObject
{
public:
	// Constructor
	ListViewSelectionToMatrixModelController(discoveredEntities::View* listView, connectionMatrix::Model* matrixModel, QObject* parent = nullptr);

private:
	// Private members
	QModelIndex _selectedIndex{};
};
