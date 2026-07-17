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

#include "networkGraphPane.hpp"

#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <QtMate/graph/graphEdgeItem.hpp>
#include <QtMate/graph/graphNodeItem.hpp>
#include <QtMate/graph/treeLayout.hpp>

#include <QFontMetricsF>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

namespace
{
using TopologyNode = hive::modelsLibrary::NetworkTopologyModel::Node;

// Node dimensions and layout constants (sized to fit an EntityID string plus the GM tag)
constexpr auto EntityNodeSize = QSizeF{ 150.0, 56.0 };
constexpr auto BridgeNodeSize = QSizeF{ 150.0, 44.0 };
constexpr auto HorizontalSpacing = 40.0;
constexpr auto VerticalSpacing = 70.0;
// The dashed border is the only cue telling a bridge from an entity, it deserves to be slightly thicker than a default one
constexpr auto DefaultBorderWidth = 1.0;
constexpr auto BridgeBorderWidth = 1.5;

// Colors (fixed, readable on both light and dark application themes since nodes are self contained boxes)
auto const EntityFillColor = QColor{ 0xFAFAFA };
auto const BridgeFillColor = QColor{ 0xECEFF1 };
auto const BorderColor = QColor{ 0x616161 };
auto const SelectedBorderColor = QColor{ 0x1E88E5 };
auto const TextColor = QColor{ 0x212121 };
auto const SecondaryTextColor = QColor{ 0x757575 };
auto const GrandmasterColor = QColor{ 0xFFC107 };
auto const ErrorColor = QColor{ 0xD32F2F };
auto const ClockLockedColor = QColor{ 0x4CAF50 };
auto const ClockUnlockedColor = QColor{ 0xF44336 };
auto const ClockUnknownColor = QColor{ 0x9E9E9E };
auto const EdgeColor = QColor{ 0x90A4AE };
auto const ActiveEdgeColor = QColor{ 0x1E88E5 };
auto const HighlightEdgeColor = QColor{ 0xFB8C00 };
auto const StreamingBorderColor = QColor{ 0x37474F };
constexpr auto DimmedOpacity = 0.25;

// Returns the rounded rect on which the border of a node must be stroked, so the whole pen width lies inside the box.
// The nodes are opaque boxes drawn over the application background: a border centered on the box edge has half of its
// width blending into that background, which makes it nearly invisible on a dark theme (and, for the bridges, makes the
// dashes indistinguishable from the plain border of an entity). The outer edge of the stroke, hence the visible size of
// the box, is left unchanged.
QRectF borderRect(QSizeF const& nodeSize, QPen const& pen)
{
	auto const halfWidth = pen.widthF() / 2.0;
	return QRectF{ QPointF{ 0.0, 0.0 }, nodeSize }.adjusted(halfWidth, halfWidth, -halfWidth, -halfWidth);
}

QString formatPropagationDelay(std::uint32_t const delayNsec)
{
	if (delayNsec >= 1000u)
	{
		return QString::number(delayNsec / 1000.0, 'f', 2) + QString::fromUtf8(" \xC2\xB5s");
	}
	return QString::number(delayNsec) + " ns";
}

QString formatBandwidth(std::uint64_t const bitsPerSecond)
{
	if (bitsPerSecond >= 1000000u)
	{
		return QString::number(bitsPerSecond / 1000000.0, 'f', 1) + " Mb/s";
	}
	return QString::number(bitsPerSecond / 1000.0, 'f', 1) + " kb/s";
}

QColor clockLockStateColor(hive::modelsLibrary::NetworkTopologyModel::ClockLockState const clockLockState)
{
	switch (clockLockState)
	{
		case hive::modelsLibrary::NetworkTopologyModel::ClockLockState::Locked:
			return ClockLockedColor;
		case hive::modelsLibrary::NetworkTopologyModel::ClockLockState::Unlocked:
			return ClockUnlockedColor;
		default:
			return ClockUnknownColor;
	}
}

QString clockLockStateText(hive::modelsLibrary::NetworkTopologyModel::ClockLockState const clockLockState)
{
	// The state text is always displayed with the color of the dot shown on the tile
	switch (clockLockState)
	{
		case hive::modelsLibrary::NetworkTopologyModel::ClockLockState::Locked:
			return QStringLiteral("<font color=\"#4CAF50\">Locked</font> (green dot)");
		case hive::modelsLibrary::NetworkTopologyModel::ClockLockState::Unlocked:
			return QStringLiteral("<font color=\"#F44336\">Not locked</font> (red dot)");
		default:
			return QStringLiteral("<font color=\"#9E9E9E\">Not reported by the entity</font> (gray dot)");
	}
}

// Draws the 'GM' tag in the top-right corner of grandmaster nodes and the error badge in the bottom-left corner
// (both corners are kept free of text by the nodes paint implementations)
void paintNodeBadges(QPainter* painter, QSizeF const& nodeSize, bool const isGrandmaster, std::uint64_t const errorCounter)
{
	if (isGrandmaster)
	{
		auto const tagRect = QRectF{ nodeSize.width() - 34.0, 4.0, 30.0, 15.0 };
		painter->setPen(Qt::NoPen);
		painter->setBrush(GrandmasterColor);
		painter->drawRoundedRect(tagRect, 3.0, 3.0);
		auto font = painter->font();
		font.setPointSizeF(8.0);
		font.setBold(true);
		painter->setFont(font);
		painter->setPen(TextColor);
		painter->drawText(tagRect, Qt::AlignCenter, "GM");
	}

	if (errorCounter > 0u)
	{
		auto const text = errorCounter > 99u ? QStringLiteral("99+") : QString::number(errorCounter);
		auto const badgeRect = QRectF{ 4.0, nodeSize.height() - 19.0, 26.0, 15.0 };
		painter->setPen(Qt::NoPen);
		painter->setBrush(ErrorColor);
		painter->drawRoundedRect(badgeRect, 7.0, 7.0);
		auto font = painter->font();
		font.setPointSizeF(8.0);
		font.setBold(true);
		painter->setFont(font);
		painter->setPen(Qt::white);
		painter->drawText(badgeRect, Qt::AlignCenter, text);
	}
}

// Base of the topology node items, allowing in-place refresh of the displayed information when the
// topology structure didn't change (avoids a full scene rebuild on counters/status updates)
class TopologyNodeItem : public qtMate::graph::GraphNodeItem
{
public:
	TopologyNodeItem(TopologyNode const& node, QSizeF const& size)
		: GraphNodeItem{ node.name, size }
		, _node{ node }
	{
	}

	void setNode(TopologyNode const& node)
	{
		_node = node;
		setLabel(node.name);
		setToolTip(buildTooltip());
		update();
	}

	// Emphasizes the node as the talker of at least one highlighted stream (strong visual cue to spot the stream sources)
	void setTalkerHighlighted(bool const talkerHighlighted)
	{
		if (talkerHighlighted != _talkerHighlighted)
		{
			_talkerHighlighted = talkerHighlighted;
			update();
		}
	}

protected:
	virtual QString buildTooltip() const = 0;

	TopologyNode _node{};
	bool _talkerHighlighted{ false };
};

// Graph node displaying a discovered entity (one node per AVB interface)
class EntityGraphNodeItem final : public TopologyNodeItem
{
public:
	EntityGraphNodeItem(TopologyNode const& node)
		: TopologyNodeItem{ node, EntityNodeSize }
	{
		setToolTip(buildTooltip());
	}

	virtual QString buildTooltip() const override
	{
		auto const& node = _node;
		auto tooltip = QString{ "<b>%1</b>" }.arg(node.name.toHtmlEscaped());
		if (node.isInterconnected)
		{
			tooltip += QString{ "<br><font color=\"#D32F2F\"><b>%1</b></font>" }.arg(node.interconnectionError.toHtmlEscaped());
		}
		tooltip += "<br>Entity ID: " + hive::modelsLibrary::helper::uniqueIdentifierToString(node.entityID);
		if (node.isTalker || node.isListener)
		{
			auto roles = QStringList{};
			if (node.isTalker)
			{
				roles += node.isStreaming ? "Talker (streaming)" : "Talker";
			}
			if (node.isListener)
			{
				roles += "Listener";
			}
			tooltip += "<br>Role: " + roles.join(" + ");
		}
		if (node.isMultiInterface || !node.avbInterfaceName.isEmpty())
		{
			tooltip += QString{ "<br>AVB Interface: %1 (index %2)" }.arg(node.avbInterfaceName.toHtmlEscaped()).arg(node.avbInterfaceIndex);
		}
		tooltip += "<br>Media Clock: " + clockLockStateText(node.clockLockState);
		tooltip += "<br>Clock Identity: " + hive::modelsLibrary::helper::uniqueIdentifierToString(node.clockIdentity);
		tooltip += "<br>Grandmaster ID: " + hive::modelsLibrary::helper::uniqueIdentifierToString(node.gptpGrandmasterID);
		if (node.gptpDomainNumber)
		{
			tooltip += QString{ "<br>gPTP Domain: %1" }.arg(*node.gptpDomainNumber);
		}
		if (node.propagationDelay)
		{
			tooltip += QString{ "<br>Propagation Delay: %1" }.arg(formatPropagationDelay(*node.propagationDelay));
		}
		if (!node.hasAsPath)
		{
			tooltip += "<br><i>Entity does not expose its AsPath, physical path is unknown</i>";
		}
		if (node.errorCounter > 0u)
		{
			tooltip += QString{ "<br><font color=\"#D32F2F\">Errors: %1</font>" }.arg(node.errorCounter);
		}
		return tooltip;
	}

	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* /*option*/, QWidget* /*widget*/) override
	{
		auto const rect = QRectF{ QPointF{ 0.0, 0.0 }, size() };
		painter->setRenderHint(QPainter::Antialiasing);
		// Border precedence: interconnection error > talker of a highlighted stream > selection > streaming talker > default
		auto borderPen = QPen{ BorderColor, DefaultBorderWidth };
		if (_node.isInterconnected)
		{
			borderPen = QPen{ ErrorColor, 2.5 };
		}
		else if (_talkerHighlighted)
		{
			// Same color than the highlighted stream paths, so the stream sources are immediately identifiable
			borderPen = QPen{ HighlightEdgeColor, 3.0 };
		}
		else if (isSelected())
		{
			borderPen = QPen{ SelectedBorderColor, 2.0 };
		}
		else if (_node.isStreaming)
		{
			borderPen = QPen{ StreamingBorderColor, 2.2 };
		}
		painter->setPen(borderPen);
		painter->setBrush(EntityFillColor);
		painter->drawRoundedRect(borderRect(size(), borderPen), 6.0, 6.0);

		auto const textWidth = rect.width() - 16.0;
		// Keep the top-right corner free of text when the GM tag is displayed
		auto const nameWidth = _node.isGrandmaster ? textWidth - 34.0 : textWidth;

		// Entity name (bold)
		auto nameFont = painter->font();
		nameFont.setBold(true);
		painter->setFont(nameFont);
		painter->setPen(TextColor);
		painter->drawText(QRectF{ 8.0, 4.0, nameWidth, 18.0 }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF{ nameFont }.elidedText(label(), Qt::ElideMiddle, nameWidth));

		// Entity ID (the bottom strip below it is reserved for the error badge, left, and the link status dot, right)
		auto smallFont = painter->font();
		smallFont.setBold(false);
		smallFont.setPointSizeF(smallFont.pointSizeF() * 0.85);
		painter->setFont(smallFont);
		painter->setPen(SecondaryTextColor);
		painter->drawText(QRectF{ 8.0, 22.0, textWidth, 14.0 }, Qt::AlignLeft | Qt::AlignVCenter, hive::modelsLibrary::helper::uniqueIdentifierToString(_node.entityID));

		// Media clock lock state dot
		painter->setPen(Qt::NoPen);
		painter->setBrush(clockLockStateColor(_node.clockLockState));
		painter->drawEllipse(QRectF{ rect.width() - 14.0, rect.height() - 14.0, 8.0, 8.0 });

		paintNodeBadges(painter, size(), _node.isGrandmaster, _node.errorCounter);
	}

protected:
	virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override
	{
		auto menu = QMenu{};
		auto* const identifyAction = menu.addAction("Identify Entity (10 sec)");
		if (auto* const action = menu.exec(event->screenPos()); action == identifyAction)
		{
			hive::modelsLibrary::ControllerManager::getInstance().identifyEntity(_node.entityID, std::chrono::seconds{ 10 });
		}
		event->accept();
	}
};

// Graph node displaying a bridge inferred from the discovery protocol (not an ATDECC entity)
class BridgeGraphNodeItem final : public TopologyNodeItem
{
public:
	BridgeGraphNodeItem(TopologyNode const& node)
		: TopologyNodeItem{ node, BridgeNodeSize }
	{
		setToolTip(buildTooltip());
	}

protected:
	virtual QString buildTooltip() const override
	{
		auto const& node = _node;
		auto tooltip = QStringLiteral("<b>Network Bridge</b> (inferred from gPTP, not an ATDECC entity)");
		if (node.isInterconnected)
		{
			tooltip += QString{ "<br><font color=\"#D32F2F\"><b>%1</b></font>" }.arg(node.interconnectionError.toHtmlEscaped());
		}
		if (!node.name.isEmpty())
		{
			tooltip += "<br>Vendor: " + node.name.toHtmlEscaped();
		}
		tooltip += "<br>Clock Identity: " + hive::modelsLibrary::helper::uniqueIdentifierToString(node.clockIdentity);
		return tooltip;
	}

public:
	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* /*option*/, QWidget* /*widget*/) override
	{
		auto const rect = QRectF{ QPointF{ 0.0, 0.0 }, size() };
		painter->setRenderHint(QPainter::Antialiasing);
		// An interconnection error takes visual precedence over the selection
		auto borderPen = _node.isInterconnected ? QPen{ ErrorColor, 2.5 } : QPen{ isSelected() ? SelectedBorderColor : BorderColor, isSelected() ? 2.0 : BridgeBorderWidth };
		borderPen.setStyle(Qt::DashLine);
		painter->setPen(borderPen);
		painter->setBrush(BridgeFillColor);
		painter->drawRoundedRect(borderRect(size(), borderPen), 6.0, 6.0);

		auto const textWidth = rect.width() - 16.0;
		// Keep the top-right corner free of text when the GM tag is displayed
		auto const titleWidth = _node.isGrandmaster ? textWidth - 34.0 : textWidth;

		// Title: the vendor name when known, the dashed border being enough to tell it's a bridge
		auto titleFont = painter->font();
		titleFont.setItalic(true);
		painter->setFont(titleFont);
		painter->setPen(TextColor);
		auto const title = _node.name.isEmpty() ? QStringLiteral("Bridge") : _node.name;
		painter->drawText(QRectF{ 8.0, 6.0, titleWidth, 18.0 }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF{ titleFont }.elidedText(title, Qt::ElideRight, titleWidth));

		// Clock identity
		auto smallFont = painter->font();
		smallFont.setItalic(false);
		smallFont.setPointSizeF(smallFont.pointSizeF() * 0.85);
		painter->setFont(smallFont);
		painter->setPen(SecondaryTextColor);
		painter->drawText(QRectF{ 8.0, 24.0, textWidth, 16.0 }, Qt::AlignLeft | Qt::AlignVCenter, hive::modelsLibrary::helper::uniqueIdentifierToString(_node.clockIdentity));

		paintNodeBadges(painter, size(), _node.isGrandmaster, 0u);
	}
};

// Edge item with stream path highlight interactions (left-click highlights, right-click opens the streams menu)
class StreamEdgeItem final : public qtMate::graph::GraphEdgeItem
{
public:
	StreamEdgeItem(NetworkGraphPane* pane, std::size_t const edgeIndex, qtMate::graph::GraphNodeItem* upstreamNode, qtMate::graph::GraphNodeItem* downstreamNode)
		: GraphEdgeItem{ upstreamNode, downstreamNode }
		, _pane{ pane }
		, _edgeIndex{ edgeIndex }
	{
	}

protected:
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override
	{
		if (event->button() == Qt::LeftButton)
		{
			_pane->highlightStreamsOfEdge(_edgeIndex);
			event->accept();
			return;
		}
		GraphEdgeItem::mousePressEvent(event);
	}

	virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override
	{
		_pane->showEdgeContextMenu(_edgeIndex, event->screenPos());
		event->accept();
	}

private:
	NetworkGraphPane* _pane{ nullptr };
	std::size_t _edgeIndex{ 0u };
};

NetworkGraphPane::StreamKey makeStreamKey(hive::modelsLibrary::NetworkTopologyModel::Stream const& stream)
{
	return { stream.talkerEntityID, stream.talkerStreamIndex };
}
} // namespace

NetworkGraphPane::NetworkGraphPane(QWidget* parent)
	: QWidget{ parent }
{
	_scene = new QGraphicsScene{ this };
	_graphView = new qtMate::graph::GraphView{ this };
	_graphView->setScene(_scene);

	auto* const layout = new QVBoxLayout{ this };
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_graphView);

	connect(_scene, &QGraphicsScene::selectionChanged, this,
		[this]()
		{
			if (_changingSelection)
			{
				return;
			}
			// Only react when an entity node gets selected (clicking an empty area or a bridge keeps the application wide selection)
			for (auto* const item : _scene->selectedItems())
			{
				if (auto const it = _entityForItem.find(item); it != _entityForItem.end())
				{
					if (it->second != _selectedEntityID)
					{
						_selectedEntityID = it->second;
						emit entitySelectionChanged(_selectedEntityID);
					}
					break;
				}
			}
		});
}

namespace
{
// Returns true when both topologies have the same graph structure (same nodes identity and same edges),
// meaning the scene items can be refreshed in place instead of being rebuilt and re-laid out
bool hasSameStructure(hive::modelsLibrary::NetworkTopologyModel::Topology const& lhs, hive::modelsLibrary::NetworkTopologyModel::Topology const& rhs)
{
	if (lhs.nodes.size() != rhs.nodes.size() || lhs.edges.size() != rhs.edges.size())
	{
		return false;
	}
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < lhs.nodes.size(); ++nodeIndex)
	{
		auto const& lhsNode = lhs.nodes[nodeIndex];
		auto const& rhsNode = rhs.nodes[nodeIndex];
		if (lhsNode.type != rhsNode.type || lhsNode.clockIdentity != rhsNode.clockIdentity || lhsNode.entityID != rhsNode.entityID || lhsNode.avbInterfaceIndex != rhsNode.avbInterfaceIndex)
		{
			return false;
		}
	}
	for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < lhs.edges.size(); ++edgeIndex)
	{
		auto const& lhsEdge = lhs.edges[edgeIndex];
		auto const& rhsEdge = rhs.edges[edgeIndex];
		if (lhsEdge.upstreamNodeIndex != rhsEdge.upstreamNodeIndex || lhsEdge.downstreamNodeIndex != rhsEdge.downstreamNodeIndex || lhsEdge.kind != rhsEdge.kind)
		{
			return false;
		}
	}
	return true;
}
} // namespace

void NetworkGraphPane::setTopology(hive::modelsLibrary::NetworkTopologyModel::Topology const& topology)
{
	// When the structure didn't change (only counters, statuses, names or stream traffic did), refresh the
	// existing items in place: much cheaper than a full rebuild, and it preserves the view zoom/pan and
	// any manual node placement (frequent updates occur continuously on large networks)
	auto const structureUnchanged = !_pendingSceneRebuild && hasSameStructure(_topology, topology);
	_topology = topology;
	updateStatsText();

	// A hidden pane (non current tab, or hidden window) defers all the scene work until it becomes visible:
	// laying out (or even redecorating) a graph nobody sees is wasted work, especially on large networks
	if (structureUnchanged)
	{
		if (isVisible())
		{
			refreshDecorations();
		}
		else
		{
			_pendingDecorationRefresh = true;
		}
	}
	else
	{
		if (isVisible())
		{
			rebuildScene();
		}
		else
		{
			_pendingSceneRebuild = true;
		}
	}
}

void NetworkGraphPane::refreshDecorations()
{
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < _nodeItems.size(); ++nodeIndex)
	{
		static_cast<TopologyNodeItem*>(_nodeItems[nodeIndex])->setNode(_topology.nodes[nodeIndex]);
	}
	for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < _edgeItems.size(); ++edgeIndex)
	{
		applyEdgeDecorations(edgeIndex);
	}
	applyHighlightToScene();
}

void NetworkGraphPane::relayout()
{
	rebuildScene();
}

void NetworkGraphPane::fitToContents()
{
	_graphView->fitToContents();
}

QString NetworkGraphPane::statsText() const
{
	return _statsText;
}

void NetworkGraphPane::highlightStreamsOfEdge(std::size_t const edgeIndex)
{
	if (edgeIndex >= _topology.edges.size() || _topology.edges[edgeIndex].streamIndices.empty())
	{
		return;
	}
	_highlightedStreamKeys.clear();
	for (auto const streamIndex : _topology.edges[edgeIndex].streamIndices)
	{
		_highlightedStreamKeys.insert(makeStreamKey(_topology.streams[streamIndex]));
	}
	applyHighlightToScene();
}

void NetworkGraphPane::highlightStream(std::size_t const streamIndex)
{
	if (streamIndex >= _topology.streams.size())
	{
		return;
	}
	_highlightedStreamKeys.clear();
	_highlightedStreamKeys.insert(makeStreamKey(_topology.streams[streamIndex]));
	applyHighlightToScene();
}

void NetworkGraphPane::clearHighlight()
{
	if (!_highlightedStreamKeys.empty())
	{
		_highlightedStreamKeys.clear();
		applyHighlightToScene();
	}
}

void NetworkGraphPane::setShowStreamInfo(bool const show)
{
	if (show != _showStreamInfo)
	{
		_showStreamInfo = show;
		for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < _edgeItems.size(); ++edgeIndex)
		{
			applyEdgeDecorations(edgeIndex);
		}
		applyHighlightToScene();
	}
}

void NetworkGraphPane::showEdgeContextMenu(std::size_t const edgeIndex, QPoint const& screenPos)
{
	if (edgeIndex >= _topology.edges.size())
	{
		return;
	}
	auto const& edge = _topology.edges[edgeIndex];
	if (edge.streamIndices.empty() && _highlightedStreamKeys.empty())
	{
		return;
	}

	auto menu = QMenu{};
	auto* highlightAllAction = static_cast<QAction*>(nullptr);
	auto streamActions = std::unordered_map<QAction*, std::size_t>{};

	if (edge.streamIndices.size() > 1)
	{
		highlightAllAction = menu.addAction(QString{ "Highlight all streams (%1)" }.arg(edge.streamIndices.size()));
	}
	for (auto const streamIndex : edge.streamIndices)
	{
		auto* const action = menu.addAction(QString{ "Highlight %1" }.arg(_topology.streams[streamIndex].description));
		streamActions.emplace(action, streamIndex);
	}

	auto* clearAction = static_cast<QAction*>(nullptr);
	if (!_highlightedStreamKeys.empty())
	{
		menu.addSeparator();
		clearAction = menu.addAction("Clear highlight");
	}

	if (auto* const action = menu.exec(screenPos); action != nullptr)
	{
		if (action == highlightAllAction)
		{
			highlightStreamsOfEdge(edgeIndex);
		}
		else if (action == clearAction)
		{
			clearHighlight();
		}
		else if (auto const it = streamActions.find(action); it != streamActions.end())
		{
			highlightStream(it->second);
		}
	}
}

void NetworkGraphPane::applyEdgeDecorations(std::size_t const edgeIndex)
{
	auto const& edge = _topology.edges[edgeIndex];
	auto* const edgeItem = _edgeItems[edgeIndex];
	auto const streamCount = edge.streamIndices.size();
	auto const hasStreams = streamCount > 0u;
	auto pen = QPen{ hasStreams ? ActiveEdgeColor : EdgeColor, hasStreams ? 2.5 : 1.5 };
	auto labelParts = QStringList{};
	auto tooltip = QString{};

	if (edge.kind == hive::modelsLibrary::NetworkTopologyModel::EdgeKind::GptpGrandmasterOnly)
	{
		pen.setStyle(Qt::DashLine);
		tooltip = "Physical path unknown (entity does not expose its AsPath), attached to its grandmaster";
	}
	else
	{
		// Show the propagation delay of the downstream entity on its upstream link
		auto const& downstreamNode = _topology.nodes[edge.downstreamNodeIndex];
		if (downstreamNode.type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity && downstreamNode.propagationDelay && *downstreamNode.propagationDelay > 0u)
		{
			labelParts += formatPropagationDelay(*downstreamNode.propagationDelay);
		}
	}

	if (hasStreams)
	{
		// Accumulate the reserved bandwidth of the running streams transiting through this edge
		auto reservedBandwidth = std::uint64_t{ 0u };
		for (auto const streamIndex : edge.streamIndices)
		{
			auto const& stream = _topology.streams[streamIndex];
			if (stream.isRunning)
			{
				reservedBandwidth += stream.reservedBandwidth;
			}
		}

		auto streamsText = QString{ "%1 strm" }.arg(streamCount);
		if (reservedBandwidth > 0u)
		{
			streamsText += QString{ " \xC2\xB7 %1" }.arg(formatBandwidth(reservedBandwidth));
		}
		labelParts += streamsText;

		// List the transiting stream connections in the tooltip (capped to keep it readable)
		constexpr auto MaxTooltipStreams = std::size_t{ 15u };
		auto streamList = QStringList{};
		for (auto const streamIndex : edge.streamIndices)
		{
			if (static_cast<std::size_t>(streamList.size()) >= MaxTooltipStreams)
			{
				streamList += QString{ "... and %1 more" }.arg(streamCount - MaxTooltipStreams);
				break;
			}
			auto const& stream = _topology.streams[streamIndex];
			auto description = stream.description.toHtmlEscaped();
			if (!stream.isRunning)
			{
				description += " (stopped)";
			}
			streamList += description;
		}
		if (!tooltip.isEmpty())
		{
			tooltip += "<br>";
		}
		tooltip += QString{ "<b>%1 (estimated reserved bandwidth, transport overhead included)</b><br>%2<br><i>Left-click to highlight the stream paths, right-click for options</i>" }.arg(streamsText.toHtmlEscaped(), streamList.join("<br>"));
	}

	// Labels can be hidden to unclutter the graph, the tooltips remain available.
	// Latency on the first line, stream count and bandwidth on the second one (better readability when edges are close to each other)
	edgeItem->setLabel(_showStreamInfo ? labelParts.join('\n') : QString{});
	edgeItem->setToolTip(tooltip);
	edgeItem->setLinePen(pen);
	_edgeBasePens[edgeIndex] = pen;
}

void NetworkGraphPane::updateStatsText()
{
	auto entityCount = 0;
	auto bridgeCount = 0;
	auto hasInterconnectionError = false;
	for (auto const& node : _topology.nodes)
	{
		if (node.type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity)
		{
			++entityCount;
		}
		else
		{
			++bridgeCount;
		}
		hasInterconnectionError |= node.isInterconnected;
	}

	_statsText = QString{ "%1 %2 - %3 %4" }.arg(entityCount).arg(entityCount > 1 ? "entities" : "entity").arg(bridgeCount).arg(bridgeCount > 1 ? "bridges" : "bridge");
	if (hasInterconnectionError)
	{
		_statsText += QStringLiteral(" - <font color=\"#D32F2F\"><b>INTERCONNECTED NETWORKS</b></font>");
	}
}

void NetworkGraphPane::applyHighlightToScene()
{
	// Resolve the highlighted stream keys against the current topology
	auto involvedNodes = std::unordered_set<std::size_t>{};
	auto involvedEdges = std::unordered_set<std::size_t>{};
	auto talkerNodes = std::unordered_set<std::size_t>{};
	auto hasHighlight = false;
	for (auto const& stream : _topology.streams)
	{
		if (_highlightedStreamKeys.count(makeStreamKey(stream)) > 0)
		{
			hasHighlight = true;
			involvedNodes.insert(stream.nodeIndices.begin(), stream.nodeIndices.end());
			involvedEdges.insert(stream.edgeIndices.begin(), stream.edgeIndices.end());
			talkerNodes.insert(stream.talkerNodeIndex);
		}
	}

	// Dim everything that is not part of the highlighted paths, and emphasize the talkers of the highlighted streams
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < _nodeItems.size(); ++nodeIndex)
	{
		_nodeItems[nodeIndex]->setOpacity(!hasHighlight || involvedNodes.count(nodeIndex) > 0 ? 1.0 : DimmedOpacity);
		static_cast<TopologyNodeItem*>(_nodeItems[nodeIndex])->setTalkerHighlighted(talkerNodes.count(nodeIndex) > 0);
	}
	for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < _edgeItems.size(); ++edgeIndex)
	{
		auto const isInvolved = involvedEdges.count(edgeIndex) > 0;
		_edgeItems[edgeIndex]->setOpacity(!hasHighlight || isInvolved ? 1.0 : DimmedOpacity);
		auto pen = _edgeBasePens[edgeIndex];
		if (isInvolved)
		{
			pen.setColor(HighlightEdgeColor);
			pen.setWidthF(3.0);
		}
		_edgeItems[edgeIndex]->setLinePen(pen);
	}
}

void NetworkGraphPane::selectEntity(la::avdecc::UniqueIdentifier const entityID)
{
	if (entityID == _selectedEntityID)
	{
		return;
	}
	_selectedEntityID = entityID;
	applySelectionToScene();
}

void NetworkGraphPane::applySelectionToScene()
{
	_changingSelection = true;
	_scene->clearSelection();
	if (auto const it = _itemsForEntity.find(_selectedEntityID); it != _itemsForEntity.end())
	{
		for (auto* const item : it->second)
		{
			item->setSelected(true);
		}
		// Make sure the (first) selected node is visible
		_graphView->ensureVisible(it->second.front(), 50, 50);
	}
	_changingSelection = false;
}

void NetworkGraphPane::rebuildScene()
{
	_changingSelection = true;
	_scene->clear();
	_changingSelection = false;
	_itemsForEntity.clear();
	_entityForItem.clear();
	_nodeItems.clear();
	_edgeItems.clear();
	_edgeBasePens.clear();

	// Compute the layout: parent of a node is its upstream neighbor (towards the grandmaster)
	auto layoutItems = std::vector<qtMate::graph::TreeLayoutItem>(_topology.nodes.size());
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < _topology.nodes.size(); ++nodeIndex)
	{
		layoutItems[nodeIndex].size = _topology.nodes[nodeIndex].type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity ? EntityNodeSize : BridgeNodeSize;
	}
	for (auto const& edge : _topology.edges)
	{
		layoutItems[edge.downstreamNodeIndex].parentIndex = static_cast<int>(edge.upstreamNodeIndex);
	}
	auto const positions = qtMate::graph::computeTreeLayout(layoutItems, HorizontalSpacing, VerticalSpacing);

	// Create node items
	_nodeItems.resize(_topology.nodes.size());
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < _topology.nodes.size(); ++nodeIndex)
	{
		auto const& node = _topology.nodes[nodeIndex];
		qtMate::graph::GraphNodeItem* item = nullptr;
		if (node.type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity)
		{
			item = new EntityGraphNodeItem{ node };
			_itemsForEntity[node.entityID].push_back(item);
			_entityForItem.emplace(item, node.entityID);
		}
		else
		{
			item = new BridgeGraphNodeItem{ node };
		}
		item->setPos(positions[nodeIndex]);
		_scene->addItem(item);
		_nodeItems[nodeIndex] = item;
	}

	// Create edge items
	_edgeItems.resize(_topology.edges.size());
	_edgeBasePens.resize(_topology.edges.size());
	for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < _topology.edges.size(); ++edgeIndex)
	{
		auto const& edge = _topology.edges[edgeIndex];
		auto* const edgeItem = new StreamEdgeItem{ this, edgeIndex, _nodeItems[edge.upstreamNodeIndex], _nodeItems[edge.downstreamNodeIndex] };
		_scene->addItem(edgeItem);
		_edgeItems[edgeIndex] = edgeItem;
		applyEdgeDecorations(edgeIndex);
	}

	// Restore the application wide entity selection and the stream highlight on the freshly created items
	applySelectionToScene();
	applyHighlightToScene();

	// Never fit synchronously: right after the pane creation (or a tab insertion) the viewport geometry is not
	// final yet, and fitting on a stale geometry computes a wrong zoom/center that is never corrected afterwards
	// (in place refreshes purposely don't re-fit)
	_pendingFit = true;
	scheduleFit();
}

void NetworkGraphPane::scheduleFit()
{
	// Postpone until the pending layout events have been processed, so the viewport geometry is final.
	// Nothing to do on a hidden pane: the fit stays pending and showEvent() will reschedule it.
	QTimer::singleShot(0, this,
		[this]()
		{
			if (_pendingFit && isVisible())
			{
				_pendingFit = false;
				_graphView->fitToContents();
			}
		});
}

void NetworkGraphPane::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	// Perform the work deferred while the pane was hidden
	if (_pendingSceneRebuild)
	{
		_pendingSceneRebuild = false;
		_pendingDecorationRefresh = false;
		rebuildScene(); // Ends by scheduling a fit
	}
	else if (_pendingDecorationRefresh)
	{
		_pendingDecorationRefresh = false;
		refreshDecorations();
	}
	if (_pendingFit)
	{
		scheduleFit();
	}
}
