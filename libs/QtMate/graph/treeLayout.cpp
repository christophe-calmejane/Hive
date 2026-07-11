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

#include "QtMate/graph/treeLayout.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace qtMate::graph
{
namespace
{
// Precomputed placement information of one item and its subtree
struct SubtreeMetrics
{
	std::vector<size_t> leafChildren{}; /**< Children without children of their own, packed in a near-square grid */
	std::vector<size_t> branchChildren{}; /**< Children having children, laid out side by side next to the grid */
	size_t gridColumns{ 0u };
	qreal gridCellWidth{ 0.0 };
	qreal gridCellHeight{ 0.0 };
	qreal gridWidth{ 0.0 };
	qreal gridHeight{ 0.0 };
	qreal childrenWidth{ 0.0 }; /**< Total width of the children block (grid + branches) */
	qreal subtreeWidth{ 0.0 };
};
} // namespace

std::vector<QPointF> computeTreeLayout(std::vector<TreeLayoutItem> const& items, qreal const horizontalSpacing, qreal const verticalSpacing)
{
	auto const count = items.size();
	auto positions = std::vector<QPointF>(count);
	if (count == 0)
	{
		return positions;
	}

	// Sanitize parent indices: out of range indices and cycles are converted to roots
	auto parents = std::vector<int>(count, -1);
	for (auto idx = size_t{ 0u }; idx < count; ++idx)
	{
		auto const parent = items[idx].parentIndex;
		if (parent >= 0 && static_cast<size_t>(parent) < count && static_cast<size_t>(parent) != idx)
		{
			parents[idx] = parent;
		}
	}
	for (auto idx = size_t{ 0u }; idx < count; ++idx)
	{
		// Walk up the ancestors chain, if we come back to the starting item we found a cycle
		auto ancestor = parents[idx];
		while (ancestor != -1)
		{
			if (static_cast<size_t>(ancestor) == idx)
			{
				parents[idx] = -1;
				break;
			}
			ancestor = parents[ancestor];
		}
	}

	// Build children lists and roots list
	auto children = std::vector<std::vector<size_t>>(count);
	auto roots = std::vector<size_t>{};
	for (auto idx = size_t{ 0u }; idx < count; ++idx)
	{
		if (parents[idx] == -1)
		{
			roots.push_back(idx);
		}
		else
		{
			children[static_cast<size_t>(parents[idx])].push_back(idx);
		}
	}

	// Compute the metrics of each subtree, bottom-up.
	// Children without children of their own (leaves) are packed in a near-square grid below their parent
	// instead of a single row, to keep wide fan-outs (many devices on the same bridge) readable.
	auto metrics = std::vector<SubtreeMetrics>(count);
	{
		std::function<void(size_t)> const computeMetrics = [&](size_t const idx)
		{
			auto& m = metrics[idx];
			for (auto const child : children[idx])
			{
				if (children[child].empty())
				{
					m.leafChildren.push_back(child);
					m.gridCellWidth = std::max(m.gridCellWidth, items[child].size.width());
					m.gridCellHeight = std::max(m.gridCellHeight, items[child].size.height());
				}
				else
				{
					computeMetrics(child);
					m.branchChildren.push_back(child);
				}
			}

			if (!m.leafChildren.empty())
			{
				auto const leafCount = m.leafChildren.size();
				m.gridColumns = static_cast<size_t>(std::ceil(std::sqrt(static_cast<double>(leafCount))));
				auto const gridRows = (leafCount + m.gridColumns - 1u) / m.gridColumns;
				m.gridWidth = static_cast<qreal>(m.gridColumns) * m.gridCellWidth + static_cast<qreal>(m.gridColumns - 1u) * horizontalSpacing;
				m.gridHeight = static_cast<qreal>(gridRows) * m.gridCellHeight + static_cast<qreal>(gridRows - 1u) * verticalSpacing;
			}

			m.childrenWidth = m.gridWidth;
			for (auto const branch : m.branchChildren)
			{
				if (m.childrenWidth > 0.0)
				{
					m.childrenWidth += horizontalSpacing;
				}
				m.childrenWidth += metrics[branch].subtreeWidth;
			}
			m.subtreeWidth = std::max(items[idx].size.width(), m.childrenWidth);
		};
		for (auto const root : roots)
		{
			computeMetrics(root);
		}
	}

	// Assign positions, top-down: each subtree is given a horizontal span, the item is centered in it,
	// its leaf children grid and branch children share the span below it
	{
		std::function<void(size_t, qreal, qreal)> const placeSubtree = [&](size_t const idx, qreal const left, qreal const top)
		{
			auto const& m = metrics[idx];
			positions[idx] = QPointF{ left + (m.subtreeWidth - items[idx].size.width()) / 2.0, top };

			auto const childrenTop = top + items[idx].size.height() + verticalSpacing;
			auto childLeft = left + (m.subtreeWidth - m.childrenWidth) / 2.0;

			// Leaf children grid, row-major
			for (auto leafPosition = size_t{ 0u }; leafPosition < m.leafChildren.size(); ++leafPosition)
			{
				auto const column = leafPosition % m.gridColumns;
				auto const row = leafPosition / m.gridColumns;
				positions[m.leafChildren[leafPosition]] = QPointF{ childLeft + static_cast<qreal>(column) * (m.gridCellWidth + horizontalSpacing), childrenTop + static_cast<qreal>(row) * (m.gridCellHeight + verticalSpacing) };
			}
			if (m.gridWidth > 0.0)
			{
				childLeft += m.gridWidth + horizontalSpacing;
			}

			// Branch children, side by side
			for (auto const branch : m.branchChildren)
			{
				placeSubtree(branch, childLeft, childrenTop);
				childLeft += metrics[branch].subtreeWidth + horizontalSpacing;
			}
		};

		// Lay out each tree of the forest side by side
		auto treeLeft = qreal{ 0.0 };
		for (auto const root : roots)
		{
			placeSubtree(root, treeLeft, 0.0);
			treeLeft += metrics[root].subtreeWidth + horizontalSpacing * 2.0;
		}
	}

	return positions;
}

} // namespace qtMate::graph
