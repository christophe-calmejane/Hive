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

#include <optional>
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
	/** Available layouts of the graph. */
	enum class LayoutMode
	{
		Detailed = 0, /**< One node per device (entity interface or bridge), the full topology is visible */
		AggregatedBySwitch = 1, /**< Entities are aggregated inside the node of the bridge they are attached to (daisy chained entities are indented), much more compact on large networks */
	};

	NetworkGraphPane(QWidget* parent = nullptr);

	/** Sets the topology to display (copied) and rebuilds the scene. */
	void setTopology(hive::modelsLibrary::NetworkTopologyModel::Topology const& topology);

	/** Sets the layout of the graph and rebuilds the scene when it changed. */
	void setLayoutMode(LayoutMode const mode);

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

	/** Gets whether a stream highlight is currently active (ie. clearHighlight() would have an effect). */
	bool hasHighlight() const noexcept;

	/** Emitted when the stream highlight switches between active and inactive. */
	Q_SIGNAL void highlightChanged(bool const hasHighlight);

	/** Shows/hides the stream bandwidth and latency labels on the edges (tooltips remain available). */
	void setShowStreamInfo(bool const show);

	/** Displays the propagation delay labels of the graph (edges and aggregated rows) as an estimated cable length instead of a time (the entity tooltips always show both). */
	void setShowDelayAsDistance(bool const show);

	/** Shows the context menu of an edge (stream highlight actions). */
	void showEdgeContextMenu(std::size_t const edgeIndex, QPoint const& screenPos);

	/** Called by the scene items when the user clicks a graphical element representing an entity (aggregated row of a switch group node), to update the application wide selection. */
	void notifyEntityItemClicked(la::avdecc::UniqueIdentifier const entityID);

	/** Highlights the path of all the streams transiting through the uplink of an aggregated entity (the edge itself has no scene item inside a switch group). */
	void highlightRowUplink(std::size_t const nodeIndex);

	/** Shows the context menu of an aggregated row: entity actions plus the stream highlight actions of its uplink. */
	void showRowContextMenu(std::size_t const nodeIndex, QPoint const& screenPos);

	/** Gets a short description of the graph content (entities and bridges count). */
	QString statsText() const;

	/** A highlighted stream is identified by its talker endpoint (streams are multicast), so the highlight survives topology rebuilds. */
	using StreamKey = std::tuple<la::avdecc::UniqueIdentifier, la::avdecc::entity::model::StreamIndex>;

protected:
	virtual void showEvent(QShowEvent* event) override;

private:
	/**
	* @brief How one topology node is represented in the scene.
	* @details In detailed layout every node has its own item. In aggregated layout the bridges and the
	*          standalone entities have their own item, while an aggregated entity is a row of the switch
	*          group item it belongs to (several nodes then share the same item pointer).
	*/
	struct NodeRepresentation
	{
		qtMate::graph::GraphNodeItem* item{ nullptr };
		std::optional<std::size_t> row{}; /**< Set when the node is an aggregated row of a switch group item */
	};

	/** Displayable information of a link (an edge of the topology): shared by the edge labels and the aggregated rows. */
	struct EdgeLinkInfo
	{
		QStringList labelParts{}; /**< Propagation delay, then stream count and bandwidth (only the available parts) */
		QString tooltip{}; /**< Path discovery note and transiting streams list (without any interaction hint) */
		bool hasStreams{ false };
	};

	void rebuildScene();
	void rebuildDetailedScene();
	void rebuildAggregatedScene();
	void refreshDecorations();
	void refreshLinkDecorations();
	EdgeLinkInfo buildEdgeLinkInfo(std::size_t const edgeIndex) const;
	void applyEdgeDecorations(std::size_t const edgeIndex);
	void applyRowLinkDecorations(std::size_t const nodeIndex);
	void showLinkContextMenu(std::optional<la::avdecc::UniqueIdentifier> const identifyEntityID, std::optional<std::size_t> const edgeIndex, QPoint const& screenPos);
	void updateStatsText();
	void applySelectionToScene();
	void applyHighlightToScene();
	void scheduleFit();

	hive::modelsLibrary::NetworkTopologyModel::Topology _topology{};
	QGraphicsScene* _scene{ nullptr };
	qtMate::graph::GraphView* _graphView{ nullptr };
	QString _statsText{};
	LayoutMode _layoutMode{ LayoutMode::Detailed };

	// Scene representation of each topology node, aligned with the topology nodes vector
	std::vector<NodeRepresentation> _nodeRepresentations{};
	// Edge items, aligned with the topology edges vector (null for the edges internal to a switch group in aggregated layout)
	std::vector<qtMate::graph::GraphEdgeItem*> _edgeItems{};
	std::vector<QPen> _edgeBasePens{};
	// Switch group items of the aggregated layout (subset of the _nodeRepresentations items, for row level operations)
	std::vector<qtMate::graph::GraphNodeItem*> _switchGroupItems{};
	// Edge going to the upstream neighbor of each node (aggregated layout only: source of the link information of the rows)
	std::vector<std::optional<std::size_t>> _uplinkEdgeForNode{};

	// Selection synchronization state
	std::unordered_map<la::avdecc::UniqueIdentifier, std::vector<NodeRepresentation>, la::avdecc::UniqueIdentifier::hash> _itemsForEntity{};
	std::unordered_map<QGraphicsItem*, la::avdecc::UniqueIdentifier> _entityForItem{};
	la::avdecc::UniqueIdentifier _selectedEntityID{};
	bool _changingSelection{ false };
	// Expensive operations are deferred while the pane is hidden (non current tab, or hidden window) and
	// performed once when it becomes visible: scene rebuild + layout, items decoration refresh, and view fit
	// (which also requires the viewport geometry to be final, hence the deferred single-shot in scheduleFit())
	bool _pendingSceneRebuild{ false };
	bool _pendingDecorationRefresh{ false };
	bool _pendingFit{ false };
	bool _showStreamInfo{ true };
	bool _showDelayAsDistance{ false };

	// Stream path highlight state
	std::set<StreamKey> _highlightedStreamKeys{};
};
