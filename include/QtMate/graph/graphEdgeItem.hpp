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

#include <QGraphicsPathItem>
#include <QPen>

namespace qtMate::graph
{
class GraphNodeItem;
class GraphEdgeLabelItem;

/**
* @brief Edge connecting two GraphNodeItem in a QGraphicsScene.
* @details Draws a vertical cubic curve from the bottom anchor of the upstream node
*          to the top anchor of the downstream node, with an optional label at mid-path.
*          The label is a separate scene item drawn above all the edge lines (so it is never covered
*          by nearby edges), using the palette text color over a translucent background plate, which
*          keeps it readable in both light and dark themes.
*          The path is automatically updated when either node moves.
*/
class GraphEdgeItem : public QGraphicsPathItem
{
public:
	enum
	{
		Type = UserType + 101
	};

	/**
	* @brief Constructs an edge between two nodes.
	* @param[in] upstreamNode Node the edge starts from (bottom anchor).
	* @param[in] downstreamNode Node the edge ends to (top anchor).
	* @param[in] parent Optional parent item.
	* @note The edge registers itself on both nodes to follow their movements. Both nodes must outlive the edge, or be destroyed by the same QGraphicsScene.
	*/
	GraphEdgeItem(GraphNodeItem* upstreamNode, GraphNodeItem* downstreamNode, QGraphicsItem* parent = nullptr);
	virtual ~GraphEdgeItem() override;

	/** Sets the label displayed at mid-path, may contain multiple lines separated by '\n' (empty to hide). */
	void setLabel(QString const& label);

	/** Sets the pen used to draw the edge line. */
	void setLinePen(QPen const& pen);

	/** Recomputes the path from the current nodes positions. */
	void updatePath();

	/** Called by GraphNodeItem destructor to prevent dangling pointers. */
	void detachNode(GraphNodeItem* node);

	virtual int type() const override;
	virtual QPainterPath shape() const override;
	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget = nullptr) override;

protected:
	virtual QVariant itemChange(GraphicsItemChange change, QVariant const& value) override;

private:
	friend class GraphEdgeLabelItem;

	GraphNodeItem* _upstreamNode{ nullptr };
	GraphNodeItem* _downstreamNode{ nullptr };
	GraphEdgeLabelItem* _labelItem{ nullptr };
};

} // namespace qtMate::graph
