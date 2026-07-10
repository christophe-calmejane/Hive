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

#include <QPointF>
#include <QSizeF>

#include <vector>

namespace qtMate::graph
{
/** Input of computeTreeLayout(): one item of the forest to lay out. */
struct TreeLayoutItem
{
	QSizeF size{}; /**< Size of the item */
	int parentIndex{ -1 }; /**< Index of the parent item in the input vector, -1 for a root */
};

/**
* @brief Computes a layered layout for a forest of trees.
* @details Roots are at the top, children are centered below their parent,
*          each depth level forms a row whose height is the maximum item height of the level.
*          The different trees of the forest are laid out side by side.
*          Invalid parent indices and parent cycles are gracefully handled by treating the offending items as roots.
* @param[in] items Items to lay out, parent relationship expressed by index in this vector.
* @param[in] horizontalSpacing Minimum horizontal gap between two items.
* @param[in] verticalSpacing Vertical gap between two depth levels.
* @return The computed top-left position of each item, in the same order than the input vector.
*/
std::vector<QPointF> computeTreeLayout(std::vector<TreeLayoutItem> const& items, qreal const horizontalSpacing, qreal const verticalSpacing);

} // namespace qtMate::graph
