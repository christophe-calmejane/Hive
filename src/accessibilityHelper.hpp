/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

namespace accessibility
{
/**
* @brief Displays a widget in a cell of a tree widget item (see QTreeWidget::setItemWidget).
* @details The widget usually edits or acts on the property whose label is displayed in the first column of the item,
*          a label assistive technologies can't associate with the widget. The widget is named after that label,
*          unless it already has an accessible name.
*/
void setTreeItemWidget(QTreeWidget* const treeWidget, QTreeWidgetItem* const item, int const column, QWidget* const widget);

} // namespace accessibility
