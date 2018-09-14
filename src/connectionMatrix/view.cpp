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

#include "connectionMatrix/view.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/headerView.hpp"
#include "connectionMatrix/itemDelegate.hpp"

#include <QMouseEvent>

namespace connectionMatrix
{

View::View(QWidget* parent)
	: QTableView{parent}
	, _model{std::make_unique<Model>()}
	, _verticalHeaderView{std::make_unique<HeaderView>(Qt::Vertical, this)}
	, _horizontalHeaderView{std::make_unique<HeaderView>(Qt::Horizontal, this)}
	, _itemDelegate{std::make_unique<ItemDelegate>()}
{
	setModel(_model.get());
	setVerticalHeader(_verticalHeaderView.get());
	setHorizontalHeader(_horizontalHeaderView.get());
	setItemDelegate(_itemDelegate.get());
	
	setSelectionMode(QAbstractItemView::NoSelection);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setCornerButtonEnabled(false);
	setMouseTracking(true);
	
	// Configure highlight color
	auto p = palette();
	p.setColor(QPalette::Highlight, 0xf3e5f5);
	setPalette(p);
}

void View::mouseMoveEvent(QMouseEvent* event)
{
	auto const index = indexAt(static_cast<QMouseEvent*>(event)->pos());
	selectionModel()->select(index, QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows|QItemSelectionModel::Columns);
	
	QTableView::mouseMoveEvent(event);
}

} // namespace connectionMatrix
