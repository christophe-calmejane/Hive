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

#include "listViewMatrixViewController.hpp"
#include "connectionMatrix/model.hpp"

ListViewMatrixViewController::ListViewMatrixViewController(discoveredEntities::View* listView, connectionMatrix::View* matrixView, QObject* parent)
	: QObject{ parent }
{
	auto* const matrixModel = static_cast<connectionMatrix::Model*>(matrixView->model());
	_matrixView = matrixView;

	// Connections from listView to matrixModel
	connect(listView, &discoveredEntities::View::selectedControlledEntityChanged, matrixModel,
		[this, matrixModel](la::avdecc::UniqueIdentifier const entityID)
		{
			auto const selectedIndex = matrixModel->indexOf(entityID);

			if (selectedIndex != _selectedIndex)
			{
				matrixModel->setHeaderData(_selectedIndex.row(), Qt::Vertical, false, connectionMatrix::Model::SelectedEntityRole);
				matrixModel->setHeaderData(_selectedIndex.column(), Qt::Horizontal, false, connectionMatrix::Model::SelectedEntityRole);
				_selectedIndex = selectedIndex;
				matrixModel->setHeaderData(_selectedIndex.row(), Qt::Vertical, true, connectionMatrix::Model::SelectedEntityRole);
				matrixModel->setHeaderData(_selectedIndex.column(), Qt::Horizontal, true, connectionMatrix::Model::SelectedEntityRole);
			}
		});

	connect(matrixModel, &connectionMatrix::Model::indexesWillChange, matrixModel,
		[this, matrixModel]()
		{
			// Indexes will change, unselect the currently selected index
			matrixModel->setHeaderData(_selectedIndex.row(), Qt::Vertical, false, connectionMatrix::Model::SelectedEntityRole);
			matrixModel->setHeaderData(_selectedIndex.column(), Qt::Horizontal, false, connectionMatrix::Model::SelectedEntityRole);
		});
	connect(matrixModel, &connectionMatrix::Model::indexesHaveChanged, matrixModel,
		[this, listView, matrixModel]()
		{
			// Indexes will change, re-compute selected index and re-select the currently selected index
			_selectedIndex = matrixModel->indexOf(listView->selectedControlledEntity());
			matrixModel->setHeaderData(_selectedIndex.row(), Qt::Vertical, true, connectionMatrix::Model::SelectedEntityRole);
			matrixModel->setHeaderData(_selectedIndex.column(), Qt::Horizontal, true, connectionMatrix::Model::SelectedEntityRole);
		});

	// Connections from matrixView to listView
	connect(matrixView, &connectionMatrix::View::selectEntityRequested, listView,
		[this, listView](la::avdecc::UniqueIdentifier const entityID)
		{
			listView->selectControlledEntity(entityID);
		});

	// Notify the connectionMatrix View that we have a link to the discoveredEntities View
	matrixView->entitiesListAttached(true);
}

ListViewMatrixViewController::~ListViewMatrixViewController() noexcept
{
	// Notify the connectionMatrix View that we removed the link with the discoveredEntities View
	_matrixView->entitiesListAttached(false);
}
