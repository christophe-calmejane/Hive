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

#include <hive/modelsLibrary/networkTopologyModel.hpp>
#include <QtMate/graph/graphView.hpp>

#include <QGraphicsScene>
#include <QPen>
#include <QWidget>

#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace qtMate::graph
{
class GraphNodeItem;
class GraphEdgeItem;
} // namespace qtMate::graph

/**
* @brief Graph of one network (one AVB interface index), displayed in one tab of the Network Graph window.
* @details Renders a topology snapshot as a forest of trees (grandmasters on top), using the generic
*          qtMate::graph rendering classes. Provides entity selection synchronization and stream path highlighting.
*/
class NetworkGraphPane : public QWidget
{
	Q_OBJECT
public:
	NetworkGraphPane(QWidget* parent = nullptr);

	/** Sets the topology to display (copied) and rebuilds the scene. */
	void setTopology(hive::modelsLibrary::NetworkTopologyModel::Topology const& topology);

	/**
	* @brief Selects the node(s) of the given entity (all its interfaces) and makes them visible.
	* @details Used to synchronize the graph with the application wide entity selection. Does nothing if the
	*          entity is not part of the topology. Does not re-emit entitySelectionChanged().
	* @param[in] entityID Entity to select, an invalid identifier clears the graph selection.
	*/
	void selectEntity(la::avdecc::UniqueIdentifier const entityID);

	/** Emitted when the user selects an entity node in the graph. */
	Q_SIGNAL void entitySelectionChanged(la::avdecc::UniqueIdentifier const entityID);

	/** Recomputes the layout of the graph. */
	void relayout();

	/** Zooms and centers the view so the whole graph is visible. */
	void fitToContents();

	/** Highlights the path of all the streams transiting through the given edge (dims the rest of the graph). */
	void highlightStreamsOfEdge(std::size_t const edgeIndex);

	/** Highlights the path of a single stream (dims the rest of the graph). */
	void highlightStream(std::size_t const streamIndex);

	/** Clears the stream path highlight. */
	void clearHighlight();

	/** Shows/hides the stream bandwidth and latency labels on the edges (tooltips remain available). */
	void setShowStreamInfo(bool const show);

	/** Shows the context menu of an edge (stream highlight actions). */
	void showEdgeContextMenu(std::size_t const edgeIndex, QPoint const& screenPos);

	/** Gets a short description of the graph content (entities and bridges count). */
	QString statsText() const;

	/** A highlighted stream connection is identified by its endpoints, so the highlight survives topology rebuilds. */
	using StreamKey = std::tuple<la::avdecc::UniqueIdentifier, la::avdecc::entity::model::StreamIndex, la::avdecc::UniqueIdentifier, la::avdecc::entity::model::StreamIndex>;

protected:
	virtual void showEvent(QShowEvent* event) override;

private:
	void rebuildScene();
	void refreshDecorations();
	void applyEdgeDecorations(std::size_t const edgeIndex);
	void updateStatsText();
	void applySelectionToScene();
	void applyHighlightToScene();

	hive::modelsLibrary::NetworkTopologyModel::Topology _topology{};
	QGraphicsScene* _scene{ nullptr };
	qtMate::graph::GraphView* _graphView{ nullptr };
	QString _statsText{};

	// Scene items, aligned with the topology snapshot vectors
	std::vector<qtMate::graph::GraphNodeItem*> _nodeItems{};
	std::vector<qtMate::graph::GraphEdgeItem*> _edgeItems{};
	std::vector<QPen> _edgeBasePens{};

	// Selection synchronization state
	std::unordered_map<la::avdecc::UniqueIdentifier, std::vector<QGraphicsItem*>, la::avdecc::UniqueIdentifier::hash> _itemsForEntity{};
	std::unordered_map<QGraphicsItem*, la::avdecc::UniqueIdentifier> _entityForItem{};
	la::avdecc::UniqueIdentifier _selectedEntityID{};
	bool _changingSelection{ false };
	bool _pendingFit{ false }; // Fit deferred until the pane becomes visible (fitInView is a no-op on a hidden viewport)
	bool _showStreamInfo{ true };

	// Stream path highlight state
	std::set<StreamKey> _highlightedStreamKeys{};
};
