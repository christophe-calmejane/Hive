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
#include <functional>

namespace qtMate::graph
{
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

	// Compute depth of each item and the height of each depth level
	auto depths = std::vector<size_t>(count, 0u);
	auto levelHeights = std::vector<qreal>{};
	{
		std::function<void(size_t, size_t)> const computeDepth = [&](size_t const idx, size_t const depth)
		{
			depths[idx] = depth;
			if (levelHeights.size() <= depth)
			{
				levelHeights.resize(depth + 1, 0.0);
			}
			levelHeights[depth] = std::max(levelHeights[depth], items[idx].size.height());
			for (auto const child : children[idx])
			{
				computeDepth(child, depth + 1);
			}
		};
		for (auto const root : roots)
		{
			computeDepth(root, 0u);
		}
	}

	// Compute the y position of each depth level
	auto levelY = std::vector<qreal>(levelHeights.size(), 0.0);
	for (auto level = size_t{ 1u }; level < levelHeights.size(); ++level)
	{
		levelY[level] = levelY[level - 1] + levelHeights[level - 1] + verticalSpacing;
	}

	// Compute the width required by each subtree
	auto subtreeWidths = std::vector<qreal>(count, 0.0);
	{
		std::function<qreal(size_t)> const computeSubtreeWidth = [&](size_t const idx)
		{
			auto childrenWidth = qreal{ 0.0 };
			for (auto const child : children[idx])
			{
				if (childrenWidth > 0.0)
				{
					childrenWidth += horizontalSpacing;
				}
				childrenWidth += computeSubtreeWidth(child);
			}
			subtreeWidths[idx] = std::max(items[idx].size.width(), childrenWidth);
			return subtreeWidths[idx];
		};
		for (auto const root : roots)
		{
			computeSubtreeWidth(root);
		}
	}

	// Assign positions: each subtree is given a horizontal span, the item is centered in it and its children share it
	{
		std::function<void(size_t, qreal)> const placeSubtree = [&](size_t const idx, qreal const left)
		{
			positions[idx] = QPointF{ left + (subtreeWidths[idx] - items[idx].size.width()) / 2.0, levelY[depths[idx]] };

			// Center the children group within the subtree span
			auto childrenWidth = qreal{ 0.0 };
			for (auto const child : children[idx])
			{
				if (childrenWidth > 0.0)
				{
					childrenWidth += horizontalSpacing;
				}
				childrenWidth += subtreeWidths[child];
			}
			auto childLeft = left + (subtreeWidths[idx] - childrenWidth) / 2.0;
			for (auto const child : children[idx])
			{
				placeSubtree(child, childLeft);
				childLeft += subtreeWidths[child] + horizontalSpacing;
			}
		};

		// Lay out each tree of the forest side by side
		auto treeLeft = qreal{ 0.0 };
		for (auto const root : roots)
		{
			placeSubtree(root, treeLeft);
			treeLeft += subtreeWidths[root] + horizontalSpacing * 2.0;
		}
	}

	return positions;
}

} // namespace qtMate::graph
