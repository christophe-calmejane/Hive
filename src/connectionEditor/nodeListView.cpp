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
#include "nodelistview.hpp"

#include <QDrag>
#include <QMimeData>

NodeListView::NodeListView(QWidget* parent)
	: QListView{ parent }
{
	setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
	setDragDropMode(QAbstractItemView::DragDropMode::DragOnly);
	setDragEnabled(true);
	setMinimumWidth(250);
}

NodeListView::~NodeListView() = default;

void NodeListView::startDrag(Qt::DropActions supportedActions)
{
	auto* drag = new QDrag{ this };

	auto const indexes = selectedIndexes();
	Q_ASSERT(indexes.count() == 1);

	auto* mimeData = model()->mimeData(indexes);
	drag->setMimeData(mimeData);

	if (mimeData->hasImage())
	{
		auto const image = qvariant_cast<QImage>(mimeData->imageData());
		drag->setPixmap(QPixmap::fromImage(image));
	}

	if (drag->exec(supportedActions) != Qt::DropAction::IgnoreAction)
	{
		emit dropped(indexes.first());
	}
}
