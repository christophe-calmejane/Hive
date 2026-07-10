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

#include <QGraphicsItem>

#include <unordered_set>

namespace qtMate::graph
{
class GraphEdgeItem;

/**
* @brief Generic movable node of a graph, rendered in a QGraphicsScene.
* @details Base building block to display any kind of node based graph.
*          The default implementation paints a rounded rectangle with a centered label,
*          subclasses may override paint() to fully customize the rendering.
*          Attached GraphEdgeItem are automatically updated when the node is moved.
*/
class GraphNodeItem : public QGraphicsItem
{
public:
	enum
	{
		Type = UserType + 100
	};

	/**
	* @brief Constructs a node.
	* @param[in] label Text displayed by the default paint() implementation.
	* @param[in] size Fixed size of the node, used by boundingRect() and anchor points.
	* @param[in] parent Optional parent item.
	*/
	GraphNodeItem(QString const& label, QSizeF const& size, QGraphicsItem* parent = nullptr);
	virtual ~GraphNodeItem() override;

	/** Gets the label of the node. */
	QString const& label() const noexcept;

	/** Sets the label of the node and triggers a repaint. */
	void setLabel(QString const& label);

	/** Gets the fixed size of the node. */
	QSizeF const& size() const noexcept;

	/** Gets the scene position of the top-center anchor point (used by edges). */
	QPointF topAnchorScenePos() const;

	/** Gets the scene position of the bottom-center anchor point (used by edges). */
	QPointF bottomAnchorScenePos() const;

	virtual int type() const override;
	virtual QRectF boundingRect() const override;
	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget = nullptr) override;

protected:
	virtual QVariant itemChange(GraphicsItemChange change, QVariant const& value) override;

private:
	friend class GraphEdgeItem;
	// Called by GraphEdgeItem to keep the list of edges to be updated when the node moves
	void registerEdge(GraphEdgeItem* edge);
	void unregisterEdge(GraphEdgeItem* edge);

	QString _label{};
	QSizeF _size{};
	std::unordered_set<GraphEdgeItem*> _edges{};
};

} // namespace qtMate::graph
