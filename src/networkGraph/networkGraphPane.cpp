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
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <functional>
#include <numeric>
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
// Switch group node (aggregated layout) dimensions: a header sized like a bridge node, then one compact row
// per aggregated entity (wider than the other nodes so the indented daisy chained entity names remain readable)
constexpr auto GroupNodeWidth = 190.0;
constexpr auto GroupHeaderHeight = 40.0;
constexpr auto GroupRowHeight = 34.0; /**< Two lines: entity name, then the link information of its uplink */
constexpr auto GroupRowSpacing = 4.0;
constexpr auto GroupPadding = 6.0;
constexpr auto GroupRowsLeftMargin = 14.0; /**< Left margin of the rows, larger than the padding to leave room for the tree guides */
constexpr auto GroupRowIndent = 14.0; /**< Horizontal shift of a row per daisy chain depth level */
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

// Builds the tooltip of an Entity node (shared by the detailed entity nodes and the aggregated rows)
QString buildEntityNodeTooltip(TopologyNode const& node)
{
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
		// Escaped because of the '<' of the too-short distance display
		tooltip += QString{ "<br>Propagation Delay: %1" }.arg(hive::modelsLibrary::helper::propagationDelayWithDistanceToString(*node.propagationDelay).toHtmlEscaped());
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

// Builds the tooltip of an InferredBridge node (shared by the detailed bridge nodes and the switch group headers)
QString buildBridgeNodeTooltip(TopologyNode const& node)
{
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

// Shows the context menu of an entity (shared by the detailed entity nodes and the aggregated rows)
void showEntityContextMenu(la::avdecc::UniqueIdentifier const entityID, QPoint const& screenPos)
{
	auto menu = QMenu{};
	auto* const identifyAction = menu.addAction("Identify Entity (10 sec)");
	if (auto* const action = menu.exec(screenPos); action == identifyAction)
	{
		hive::modelsLibrary::ControllerManager::getInstance().identifyEntity(entityID, std::chrono::seconds{ 10 });
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

	virtual void setNode(TopologyNode const& node)
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
		return buildEntityNodeTooltip(_node);
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
		showEntityContextMenu(_node.entityID, event->screenPos());
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
		return buildBridgeNodeTooltip(_node);
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

/**
* @brief Graph node of the aggregated layout: a bridge and the entities attached to it, as one single item.
* @details The bridge is displayed as a header (painted like a BridgeGraphNodeItem) and each aggregated entity
*          as a compact row below it, indented by its daisy chain depth with tree guides showing the chains.
*          The rows support the same interactions than the detailed entity nodes (selection, tooltip, context
*          menu, stream highlight emphasis), handled per row since a QGraphicsScene only knows the whole item.
*          The connector zone at the left of a row plate plays the role of the edge going to the entity
*          (which has no scene item inside a group): clicking it highlights the streams of the uplink.
*/
class SwitchGroupNodeItem final : public TopologyNodeItem
{
public:
	struct Row
	{
		TopologyNode node{};
		std::size_t nodeIndex{ 0u }; /**< Index of the displayed node in the topology (needed by the pane for the uplink interactions) */
		int depth{ 0 }; /**< 0 for entities directly attached to the bridge, +1 per daisy chain hop */
		std::optional<std::size_t> parentRow{}; /**< Row of the upstream entity for daisy chained entities */
		// Link information of the uplink of the entity, driven by the pane (empty when hidden or not available)
		QString linkText{};
		QString linkTooltip{};
		// Interaction states, driven by the pane
		bool selected{ false };
		bool dimmed{ false };
		bool talkerHighlighted{ false };
	};

	static QSizeF sizeForRowCount(std::size_t const count)
	{
		auto height = GroupHeaderHeight + static_cast<qreal>(count) * (GroupRowHeight + GroupRowSpacing) + GroupPadding;
		if (count > 0u)
		{
			height -= GroupRowSpacing;
		}
		return QSizeF{ GroupNodeWidth, height };
	}

	SwitchGroupNodeItem(NetworkGraphPane* pane, TopologyNode const& node, std::vector<Row>&& rows)
		: TopologyNodeItem{ node, sizeForRowCount(rows.size()) }
		, _pane{ pane }
		, _rows{ std::move(rows) }
	{
		// Tooltips are per row, they have to be tracked by hand (a QGraphicsItem has a single tooltip)
		setAcceptHoverEvents(true);
		setToolTip(buildTooltip());
	}

	virtual void setNode(TopologyNode const& node) override
	{
		TopologyNodeItem::setNode(node);
		// The base implementation put back the header tooltip, restore the row one if a row is being hovered
		refreshTooltip();
	}

	void setRowNode(std::size_t const row, TopologyNode const& node)
	{
		_rows[row].node = node;
		if (_hoveredRow == row)
		{
			refreshTooltip();
		}
		update();
	}

	void setRowLinkInfo(std::size_t const row, QString const& linkText, QString const& linkTooltip)
	{
		if (linkText != _rows[row].linkText || linkTooltip != _rows[row].linkTooltip)
		{
			_rows[row].linkText = linkText;
			_rows[row].linkTooltip = linkTooltip;
			if (_hoveredRow == row)
			{
				refreshTooltip();
			}
			update();
		}
	}

	void setRowSelected(std::size_t const row, bool const selected)
	{
		if (selected != _rows[row].selected)
		{
			_rows[row].selected = selected;
			update();
		}
	}

	void clearRowSelections()
	{
		for (auto rowIndex = std::size_t{ 0u }; rowIndex < _rows.size(); ++rowIndex)
		{
			setRowSelected(rowIndex, false);
		}
	}

	void setRowEmphasis(std::size_t const row, bool const dimmed, bool const talkerHighlighted)
	{
		if (dimmed != _rows[row].dimmed || talkerHighlighted != _rows[row].talkerHighlighted)
		{
			_rows[row].dimmed = dimmed;
			_rows[row].talkerHighlighted = talkerHighlighted;
			update();
		}
	}

	// Dims the bridge header when it is not part of the highlighted stream paths (the item opacity cannot be
	// used for that: it would also dim the rows, whose entities may be part of the highlighted paths)
	void setHeaderDimmed(bool const dimmed)
	{
		if (dimmed != _headerDimmed)
		{
			_headerDimmed = dimmed;
			update();
		}
	}

	QRectF rowSceneRect(std::size_t const row) const
	{
		return mapToScene(rowRect(row)).boundingRect();
	}

	virtual QString buildTooltip() const override
	{
		auto tooltip = buildBridgeNodeTooltip(_node);
		if (!_rows.empty())
		{
			tooltip += QString{ "<br><i>%1 aggregated %2</i>" }.arg(_rows.size()).arg(_rows.size() > 1u ? "entities" : "entity");
		}
		return tooltip;
	}

	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* /*option*/, QWidget* /*widget*/) override
	{
		auto const rect = QRectF{ QPointF{ 0.0, 0.0 }, size() };
		auto const baseFont = painter->font();
		painter->setRenderHint(QPainter::Antialiasing);

		// Container box, painted like a bridge node (dashed border): an interconnection error takes visual precedence over the selection
		auto borderPen = _node.isInterconnected ? QPen{ ErrorColor, 2.5 } : QPen{ isSelected() ? SelectedBorderColor : BorderColor, isSelected() ? 2.0 : BridgeBorderWidth };
		borderPen.setStyle(Qt::DashLine);
		painter->setPen(borderPen);
		painter->setBrush(BridgeFillColor);
		painter->drawRoundedRect(borderRect(size(), borderPen), 6.0, 6.0);

		// Header: same content than a bridge node (vendor name, clock identity, GM tag)
		painter->setOpacity(_headerDimmed ? DimmedOpacity : 1.0);
		auto const textWidth = rect.width() - 16.0;
		// Keep the top-right corner free of text when the GM tag is displayed
		auto const titleWidth = _node.isGrandmaster ? textWidth - 34.0 : textWidth;
		auto titleFont = painter->font();
		titleFont.setItalic(true);
		painter->setFont(titleFont);
		painter->setPen(TextColor);
		auto const title = _node.name.isEmpty() ? QStringLiteral("Bridge") : _node.name;
		painter->drawText(QRectF{ 8.0, 3.0, titleWidth, 18.0 }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF{ titleFont }.elidedText(title, Qt::ElideRight, titleWidth));
		auto smallFont = painter->font();
		smallFont.setItalic(false);
		smallFont.setPointSizeF(smallFont.pointSizeF() * 0.85);
		painter->setFont(smallFont);
		painter->setPen(SecondaryTextColor);
		painter->drawText(QRectF{ 8.0, 21.0, textWidth, 14.0 }, Qt::AlignLeft | Qt::AlignVCenter, hive::modelsLibrary::helper::uniqueIdentifierToString(_node.clockIdentity));
		paintNodeBadges(painter, size(), _node.isGrandmaster, 0u);

		// Entity rows
		for (auto rowIndex = std::size_t{ 0u }; rowIndex < _rows.size(); ++rowIndex)
		{
			paintRow(painter, rowIndex, baseFont);
		}
		painter->setOpacity(1.0);
	}

protected:
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override
	{
		// A click on a row plate selects its entity application wide. A click on the connector zone at the left
		// of a plate (where the tree guide lies) highlights the streams of the entity uplink, same interaction
		// than clicking an edge (the uplink of an aggregated entity has no edge item). The base handler then
		// also performs the usual scene selection of the whole item and allows dragging the group.
		if (event->button() == Qt::LeftButton)
		{
			if (auto const row = rowAt(event->pos()))
			{
				_pane->notifyEntityItemClicked(_rows[*row].node.entityID);
			}
			else if (auto const linkRow = linkZoneRowAt(event->pos()))
			{
				_pane->highlightRowUplink(_rows[*linkRow].nodeIndex);
			}
		}
		TopologyNodeItem::mousePressEvent(event);
	}

	virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override
	{
		auto row = rowAt(event->pos());
		if (!row)
		{
			row = linkZoneRowAt(event->pos());
		}
		if (row)
		{
			_pane->showRowContextMenu(_rows[*row].nodeIndex, event->screenPos());
			event->accept();
			return;
		}
		TopologyNodeItem::contextMenuEvent(event);
	}

	virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
	{
		auto const row = rowAt(event->pos());
		auto const linkRow = row ? std::optional<std::size_t>{} : linkZoneRowAt(event->pos());
		if (row != _hoveredRow || linkRow != _hoveredLinkRow)
		{
			_hoveredRow = row;
			_hoveredLinkRow = linkRow;
			refreshTooltip();
			// The connector zone is a discreet target: a pointing hand cursor shows it is clickable (only
			// when its uplink actually carries streams, ie. when it has a link tooltip)
			if (_hoveredLinkRow && !_rows[*_hoveredLinkRow].linkTooltip.isEmpty())
			{
				setCursor(Qt::PointingHandCursor);
			}
			else
			{
				unsetCursor();
			}
		}
		TopologyNodeItem::hoverMoveEvent(event);
	}

	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
	{
		if (_hoveredRow || _hoveredLinkRow)
		{
			_hoveredRow = std::nullopt;
			_hoveredLinkRow = std::nullopt;
			refreshTooltip();
			unsetCursor();
		}
		TopologyNodeItem::hoverLeaveEvent(event);
	}

private:
	void refreshTooltip()
	{
		if (_hoveredRow)
		{
			auto const& row = _rows[*_hoveredRow];
			auto tooltip = buildEntityNodeTooltip(row.node);
			if (!row.linkTooltip.isEmpty())
			{
				tooltip += "<br>" + row.linkTooltip;
			}
			setToolTip(tooltip);
		}
		else if (_hoveredLinkRow)
		{
			// Over the connector zone, only the link information is relevant
			setToolTip(_rows[*_hoveredLinkRow].linkTooltip);
		}
		else
		{
			setToolTip(buildTooltip());
		}
	}

	QRectF rowRect(std::size_t const row) const
	{
		auto const left = GroupRowsLeftMargin + static_cast<qreal>(_rows[row].depth) * GroupRowIndent;
		auto const top = GroupHeaderHeight + static_cast<qreal>(row) * (GroupRowHeight + GroupRowSpacing);
		return QRectF{ left, top, size().width() - GroupPadding - left, GroupRowHeight };
	}

	std::optional<std::size_t> rowAt(QPointF const& pos) const
	{
		for (auto rowIndex = std::size_t{ 0u }; rowIndex < _rows.size(); ++rowIndex)
		{
			if (rowRect(rowIndex).contains(pos))
			{
				return rowIndex;
			}
		}
		return std::nullopt;
	}

	// Connector zone of a row: the whole band between the group border and the row plate (where the tree
	// guide lies), acting as the clickable uplink of the entity
	QRectF linkZoneRect(std::size_t const row) const
	{
		auto const plate = rowRect(row);
		return QRectF{ 2.0, plate.top(), plate.left() - 2.0, plate.height() };
	}

	std::optional<std::size_t> linkZoneRowAt(QPointF const& pos) const
	{
		for (auto rowIndex = std::size_t{ 0u }; rowIndex < _rows.size(); ++rowIndex)
		{
			if (linkZoneRect(rowIndex).contains(pos))
			{
				return rowIndex;
			}
		}
		return std::nullopt;
	}

	void paintRow(QPainter* painter, std::size_t const rowIndex, QFont const& baseFont) const
	{
		auto const& row = _rows[rowIndex];
		auto const& node = row.node;
		auto const plate = rowRect(rowIndex);
		painter->setOpacity(row.dimmed ? DimmedOpacity : 1.0);

		// Tree guide connecting the row to its parent (the bridge header, or the upstream entity for daisy chains)
		auto const guideX = plate.left() - 8.0;
		auto const guideTop = row.parentRow ? rowRect(*row.parentRow).bottom() : GroupHeaderHeight - 2.0;
		painter->setPen(QPen{ SecondaryTextColor, 1.0 });
		painter->setBrush(Qt::NoBrush);
		painter->drawLine(QPointF{ guideX, guideTop }, QPointF{ guideX, plate.center().y() });
		painter->drawLine(QPointF{ guideX, plate.center().y() }, QPointF{ plate.left(), plate.center().y() });

		// Row plate: border precedence like the detailed entity nodes (interconnection error > talker of a highlighted stream > selection > streaming talker > default)
		auto borderPen = QPen{ BorderColor, 1.0 };
		if (node.isInterconnected)
		{
			borderPen = QPen{ ErrorColor, 2.0 };
		}
		else if (row.talkerHighlighted)
		{
			borderPen = QPen{ HighlightEdgeColor, 2.0 };
		}
		else if (row.selected)
		{
			borderPen = QPen{ SelectedBorderColor, 1.5 };
		}
		else if (node.isStreaming)
		{
			borderPen = QPen{ StreamingBorderColor, 1.5 };
		}
		painter->setPen(borderPen);
		painter->setBrush(EntityFillColor);
		auto const halfWidth = borderPen.widthF() / 2.0;
		painter->drawRoundedRect(plate.adjusted(halfWidth, halfWidth, -halfWidth, -halfWidth), 4.0, 4.0);

		// Right side decorations: media clock lock state dot, then optional GM tag and error badge
		auto const centerY = plate.center().y();
		auto rightEdge = plate.right() - 4.0;
		painter->setPen(Qt::NoPen);
		painter->setBrush(clockLockStateColor(node.clockLockState));
		painter->drawEllipse(QRectF{ rightEdge - 7.0, centerY - 3.5, 7.0, 7.0 });
		rightEdge -= 11.0;

		auto badgeFont = baseFont;
		badgeFont.setPointSizeF(7.0);
		badgeFont.setBold(true);
		if (node.isGrandmaster)
		{
			auto const tagRect = QRectF{ rightEdge - 22.0, centerY - 7.0, 22.0, 14.0 };
			painter->setPen(Qt::NoPen);
			painter->setBrush(GrandmasterColor);
			painter->drawRoundedRect(tagRect, 3.0, 3.0);
			painter->setFont(badgeFont);
			painter->setPen(TextColor);
			painter->drawText(tagRect, Qt::AlignCenter, "GM");
			rightEdge -= 26.0;
		}
		if (node.errorCounter > 0u)
		{
			auto const text = node.errorCounter > 99u ? QStringLiteral("99+") : QString::number(node.errorCounter);
			auto const badgeRect = QRectF{ rightEdge - 24.0, centerY - 7.0, 24.0, 14.0 };
			painter->setPen(Qt::NoPen);
			painter->setBrush(ErrorColor);
			painter->drawRoundedRect(badgeRect, 7.0, 7.0);
			painter->setFont(badgeFont);
			painter->setPen(Qt::white);
			painter->drawText(badgeRect, Qt::AlignCenter, text);
			rightEdge -= 28.0;
		}

		// Entity name, with the link information of its uplink below it (name vertically centered when there is none)
		auto nameFont = baseFont;
		nameFont.setPointSizeF(baseFont.pointSizeF() * 0.9);
		painter->setFont(nameFont);
		painter->setPen(TextColor);
		auto const textLeft = plate.left() + 6.0;
		auto const textWidth = rightEdge - textLeft;
		auto const nameRect = row.linkText.isEmpty() ? QRectF{ textLeft, plate.top(), textWidth, plate.height() } : QRectF{ textLeft, plate.top() + 2.0, textWidth, 16.0 };
		painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF{ nameFont }.elidedText(node.name, Qt::ElideMiddle, textWidth));
		if (!row.linkText.isEmpty())
		{
			auto linkFont = baseFont;
			linkFont.setPointSizeF(baseFont.pointSizeF() * 0.75);
			painter->setFont(linkFont);
			painter->setPen(SecondaryTextColor);
			painter->drawText(QRectF{ textLeft, plate.top() + 18.0, textWidth, 13.0 }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF{ linkFont }.elidedText(row.linkText, Qt::ElideRight, textWidth));
		}
	}

	NetworkGraphPane* _pane{ nullptr };
	std::vector<Row> _rows{};
	bool _headerDimmed{ false };
	std::optional<std::size_t> _hoveredRow{};
	std::optional<std::size_t> _hoveredLinkRow{};
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

// Ordering key of a topology node: the rendering must be fully deterministic, it cannot depend on the
// discovery order of the devices (which changes on every launch)
std::uint64_t nodeSortKey(TopologyNode const& node)
{
	return node.type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity ? node.entityID.getValue() : node.clockIdentity.getValue();
}

// Comparator of two topology node indices, by node ordering key (index as tie breaker for full determinism)
bool isNodeSortedBefore(hive::modelsLibrary::NetworkTopologyModel::Topology const& topology, std::size_t const lhs, std::size_t const rhs)
{
	auto const lhsKey = nodeSortKey(topology.nodes[lhs]);
	auto const rhsKey = nodeSortKey(topology.nodes[rhs]);
	return lhsKey != rhsKey ? lhsKey < rhsKey : lhs < rhs;
}

// Aggregation of a topology for the AggregatedBySwitch layout: each InferredBridge node collects the entities
// attached to it, directly or through a daisy chain of entities, as rows of its switch group node
struct Aggregation
{
	struct Group
	{
		std::size_t bridgeNodeIndex{ 0u };
		std::vector<SwitchGroupNodeItem::Row> rows{};
		std::vector<std::size_t> rowNodeIndices{}; /**< Topology node displayed by each row, aligned with rows */
	};
	std::vector<Group> groups{}; /**< One group per InferredBridge node, in topology nodes order */
	std::vector<std::optional<std::pair<std::size_t, std::size_t>>> placementForNode{}; /**< Group index and row index of each aggregated entity node (not set for the other nodes) */
};

Aggregation computeAggregation(hive::modelsLibrary::NetworkTopologyModel::Topology const& topology)
{
	auto result = Aggregation{};
	result.placementForNode.resize(topology.nodes.size());

	// Children of each node through GptpPath edges only: a GptpGrandmasterOnly edge doesn't denote physical
	// attachment, entities discovered that way are never aggregated (they stay standalone with their dashed edge)
	auto childrenOf = std::vector<std::vector<std::size_t>>(topology.nodes.size());
	for (auto const& edge : topology.edges)
	{
		if (edge.kind == hive::modelsLibrary::NetworkTopologyModel::EdgeKind::GptpPath)
		{
			childrenOf[edge.upstreamNodeIndex].push_back(edge.downstreamNodeIndex);
		}
	}
	// Deterministic rows order, whatever the discovery order of the devices
	for (auto& children : childrenOf)
	{
		std::sort(children.begin(), children.end(),
			[&topology](std::size_t const lhs, std::size_t const rhs)
			{
				return isNodeSortedBefore(topology, lhs, rhs);
			});
	}

	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < topology.nodes.size(); ++nodeIndex)
	{
		if (topology.nodes[nodeIndex].type != hive::modelsLibrary::NetworkTopologyModel::NodeType::InferredBridge)
		{
			continue;
		}
		auto const groupIndex = result.groups.size();
		auto group = Aggregation::Group{ nodeIndex, {}, {} };
		// Collect the entity subtree of the bridge in preorder, an entity child of an aggregated entity being a
		// daisy chain. Nested bridges are not entered: they get their own group, linked by a regular edge.
		// The placement check makes the recursion robust to a malformed topology (it never visits a node twice).
		std::function<void(std::size_t, int, std::optional<std::size_t>)> const collectRows = [&](std::size_t const parentNodeIndex, int const depth, std::optional<std::size_t> const parentRow)
		{
			for (auto const childIndex : childrenOf[parentNodeIndex])
			{
				if (topology.nodes[childIndex].type != hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity || result.placementForNode[childIndex])
				{
					continue;
				}
				auto const rowIndex = group.rows.size();
				group.rows.push_back(SwitchGroupNodeItem::Row{ topology.nodes[childIndex], childIndex, depth, parentRow });
				group.rowNodeIndices.push_back(childIndex);
				result.placementForNode[childIndex] = std::make_pair(groupIndex, rowIndex);
				collectRows(childIndex, depth + 1, rowIndex);
			}
		};
		collectRows(nodeIndex, 0, std::nullopt);
		result.groups.push_back(std::move(group));
	}
	return result;
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
						// The previous selection may have been a row of a switch group (rows are not part of the scene selection)
						for (auto* const groupItem : _switchGroupItems)
						{
							static_cast<SwitchGroupNodeItem*>(groupItem)->clearRowSelections();
						}
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
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < _nodeRepresentations.size(); ++nodeIndex)
	{
		auto const& representation = _nodeRepresentations[nodeIndex];
		if (representation.row)
		{
			static_cast<SwitchGroupNodeItem*>(representation.item)->setRowNode(*representation.row, _topology.nodes[nodeIndex]);
			applyRowLinkDecorations(nodeIndex);
		}
		else
		{
			static_cast<TopologyNodeItem*>(representation.item)->setNode(_topology.nodes[nodeIndex]);
		}
	}
	for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < _edgeItems.size(); ++edgeIndex)
	{
		applyEdgeDecorations(edgeIndex);
	}
	applyHighlightToScene();
}

void NetworkGraphPane::setLayoutMode(LayoutMode const mode)
{
	if (mode != _layoutMode)
	{
		_layoutMode = mode;
		// Same visibility deferral than setTopology: don't rebuild a graph nobody sees
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

void NetworkGraphPane::notifyEntityItemClicked(la::avdecc::UniqueIdentifier const entityID)
{
	if (entityID != _selectedEntityID)
	{
		_selectedEntityID = entityID;
		applySelectionToScene();
		emit entitySelectionChanged(entityID);
	}
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
	auto const hadHighlight = !_highlightedStreamKeys.empty();
	_highlightedStreamKeys.clear();
	for (auto const streamIndex : _topology.edges[edgeIndex].streamIndices)
	{
		_highlightedStreamKeys.insert(makeStreamKey(_topology.streams[streamIndex]));
	}
	applyHighlightToScene();
	if (!hadHighlight)
	{
		emit highlightChanged(true);
	}
}

void NetworkGraphPane::highlightStream(std::size_t const streamIndex)
{
	if (streamIndex >= _topology.streams.size())
	{
		return;
	}
	auto const hadHighlight = !_highlightedStreamKeys.empty();
	_highlightedStreamKeys.clear();
	_highlightedStreamKeys.insert(makeStreamKey(_topology.streams[streamIndex]));
	applyHighlightToScene();
	if (!hadHighlight)
	{
		emit highlightChanged(true);
	}
}

void NetworkGraphPane::clearHighlight()
{
	if (!_highlightedStreamKeys.empty())
	{
		_highlightedStreamKeys.clear();
		applyHighlightToScene();
		emit highlightChanged(false);
	}
}

bool NetworkGraphPane::hasHighlight() const noexcept
{
	return !_highlightedStreamKeys.empty();
}

void NetworkGraphPane::highlightRowUplink(std::size_t const nodeIndex)
{
	if (nodeIndex < _uplinkEdgeForNode.size() && _uplinkEdgeForNode[nodeIndex])
	{
		highlightStreamsOfEdge(*_uplinkEdgeForNode[nodeIndex]);
	}
}

void NetworkGraphPane::setShowStreamInfo(bool const show)
{
	if (show != _showStreamInfo)
	{
		_showStreamInfo = show;
		refreshLinkDecorations();
	}
}

void NetworkGraphPane::setShowDelayAsDistance(bool const show)
{
	if (show != _showDelayAsDistance)
	{
		_showDelayAsDistance = show;
		refreshLinkDecorations();
	}
}

void NetworkGraphPane::refreshLinkDecorations()
{
	// Don't touch the items when a scene rebuild is pending: they may not match the already updated
	// topology anymore, and the rebuild will directly create them with the new display modes
	if (_pendingSceneRebuild)
	{
		return;
	}
	for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < _edgeItems.size(); ++edgeIndex)
	{
		applyEdgeDecorations(edgeIndex);
	}
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < _nodeRepresentations.size(); ++nodeIndex)
	{
		applyRowLinkDecorations(nodeIndex);
	}
	applyHighlightToScene();
}

void NetworkGraphPane::showEdgeContextMenu(std::size_t const edgeIndex, QPoint const& screenPos)
{
	if (edgeIndex >= _topology.edges.size())
	{
		return;
	}
	showLinkContextMenu(std::nullopt, edgeIndex, screenPos);
}

void NetworkGraphPane::showRowContextMenu(std::size_t const nodeIndex, QPoint const& screenPos)
{
	auto const uplinkEdge = nodeIndex < _uplinkEdgeForNode.size() ? _uplinkEdgeForNode[nodeIndex] : std::nullopt;
	showLinkContextMenu(_topology.nodes[nodeIndex].entityID, uplinkEdge, screenPos);
}

void NetworkGraphPane::showLinkContextMenu(std::optional<la::avdecc::UniqueIdentifier> const identifyEntityID, std::optional<std::size_t> const edgeIndex, QPoint const& screenPos)
{
	auto const* const edge = edgeIndex ? &_topology.edges[*edgeIndex] : nullptr;
	auto const hasStreams = edge != nullptr && !edge->streamIndices.empty();
	if (!identifyEntityID && !hasStreams && _highlightedStreamKeys.empty())
	{
		return;
	}

	auto menu = QMenu{};
	auto* identifyAction = static_cast<QAction*>(nullptr);
	if (identifyEntityID)
	{
		identifyAction = menu.addAction("Identify Entity (10 sec)");
	}

	auto* highlightAllAction = static_cast<QAction*>(nullptr);
	auto streamActions = std::unordered_map<QAction*, std::size_t>{};
	if (hasStreams)
	{
		if (identifyAction != nullptr)
		{
			menu.addSeparator();
		}
		if (edge->streamIndices.size() > 1)
		{
			highlightAllAction = menu.addAction(QString{ "Highlight all streams (%1)" }.arg(edge->streamIndices.size()));
		}
		for (auto const streamIndex : edge->streamIndices)
		{
			auto* const action = menu.addAction(QString{ "Highlight %1" }.arg(_topology.streams[streamIndex].description));
			streamActions.emplace(action, streamIndex);
		}
	}

	auto* clearAction = static_cast<QAction*>(nullptr);
	if (!_highlightedStreamKeys.empty())
	{
		menu.addSeparator();
		clearAction = menu.addAction("Clear highlight");
	}

	if (auto* const action = menu.exec(screenPos); action != nullptr)
	{
		if (action == identifyAction)
		{
			hive::modelsLibrary::ControllerManager::getInstance().identifyEntity(*identifyEntityID, std::chrono::seconds{ 10 });
		}
		else if (action == highlightAllAction)
		{
			highlightStreamsOfEdge(*edgeIndex);
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

NetworkGraphPane::EdgeLinkInfo NetworkGraphPane::buildEdgeLinkInfo(std::size_t const edgeIndex) const
{
	auto const& edge = _topology.edges[edgeIndex];
	auto info = EdgeLinkInfo{};

	if (edge.kind == hive::modelsLibrary::NetworkTopologyModel::EdgeKind::GptpGrandmasterOnly)
	{
		info.tooltip = "Physical path unknown (entity does not expose its AsPath), attached to its grandmaster";
	}
	else
	{
		// Show the propagation delay of the downstream entity on its upstream link
		auto const& downstreamNode = _topology.nodes[edge.downstreamNodeIndex];
		if (downstreamNode.type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity && downstreamNode.propagationDelay && *downstreamNode.propagationDelay > 0u)
		{
			info.labelParts += hive::modelsLibrary::helper::propagationDelayToString(*downstreamNode.propagationDelay, _showDelayAsDistance);
		}
	}

	auto const streamCount = edge.streamIndices.size();
	if (streamCount > 0u)
	{
		info.hasStreams = true;

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
		info.labelParts += streamsText;

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
		if (!info.tooltip.isEmpty())
		{
			info.tooltip += "<br>";
		}
		info.tooltip += QString{ "<b>%1 (estimated reserved bandwidth, transport overhead included)</b><br>%2" }.arg(streamsText.toHtmlEscaped(), streamList.join("<br>"));
	}

	return info;
}

void NetworkGraphPane::applyEdgeDecorations(std::size_t const edgeIndex)
{
	auto* const edgeItem = _edgeItems[edgeIndex];
	// Edges internal to a switch group have no item in aggregated layout
	if (edgeItem == nullptr)
	{
		return;
	}
	auto const& edge = _topology.edges[edgeIndex];
	auto const info = buildEdgeLinkInfo(edgeIndex);

	auto pen = QPen{ info.hasStreams ? ActiveEdgeColor : EdgeColor, info.hasStreams ? 2.5 : 1.5 };
	if (edge.kind == hive::modelsLibrary::NetworkTopologyModel::EdgeKind::GptpGrandmasterOnly)
	{
		pen.setStyle(Qt::DashLine);
	}
	auto tooltip = info.tooltip;
	if (info.hasStreams)
	{
		tooltip += QStringLiteral("<br><i>Left-click to highlight the stream paths, right-click for options</i>");
	}

	// Labels can be hidden to unclutter the graph, the tooltips remain available.
	// Latency on the first line, stream count and bandwidth on the second one (better readability when edges are close to each other)
	edgeItem->setLabel(_showStreamInfo ? info.labelParts.join('\n') : QString{});
	edgeItem->setToolTip(tooltip);
	edgeItem->setLinePen(pen);
	_edgeBasePens[edgeIndex] = pen;
}

void NetworkGraphPane::applyRowLinkDecorations(std::size_t const nodeIndex)
{
	auto const& representation = _nodeRepresentations[nodeIndex];
	if (!representation.row)
	{
		return;
	}
	auto* const groupItem = static_cast<SwitchGroupNodeItem*>(representation.item);
	auto const uplinkEdge = _uplinkEdgeForNode[nodeIndex];
	if (!uplinkEdge)
	{
		groupItem->setRowLinkInfo(*representation.row, {}, {});
		return;
	}
	// The uplink of an aggregated entity has no edge item: its link information is displayed on the row itself,
	// as a single compact line (same visibility toggle than the edge labels, the tooltip remains available)
	auto const info = buildEdgeLinkInfo(*uplinkEdge);
	auto tooltip = info.tooltip;
	if (info.hasStreams)
	{
		tooltip += QStringLiteral("<br><i>Left-click the connector left of the entity to highlight the stream paths, right-click for options</i>");
	}
	groupItem->setRowLinkInfo(*representation.row, _showStreamInfo ? info.labelParts.join(QString::fromUtf8(" \xC2\xB7 ")) : QString{}, tooltip);
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
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < _nodeRepresentations.size(); ++nodeIndex)
	{
		auto const& representation = _nodeRepresentations[nodeIndex];
		auto const isDimmed = hasHighlight && involvedNodes.count(nodeIndex) == 0;
		auto const isTalker = talkerNodes.count(nodeIndex) > 0;
		if (representation.row)
		{
			static_cast<SwitchGroupNodeItem*>(representation.item)->setRowEmphasis(*representation.row, isDimmed, isTalker);
		}
		else if (_layoutMode == LayoutMode::AggregatedBySwitch && _topology.nodes[nodeIndex].type == hive::modelsLibrary::NetworkTopologyModel::NodeType::InferredBridge)
		{
			// The item opacity cannot be used on a switch group, it would also dim the aggregated rows
			static_cast<SwitchGroupNodeItem*>(representation.item)->setHeaderDimmed(isDimmed);
		}
		else
		{
			representation.item->setOpacity(isDimmed ? DimmedOpacity : 1.0);
			static_cast<TopologyNodeItem*>(representation.item)->setTalkerHighlighted(isTalker);
		}
	}
	for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < _edgeItems.size(); ++edgeIndex)
	{
		auto* const edgeItem = _edgeItems[edgeIndex];
		if (edgeItem == nullptr)
		{
			continue;
		}
		auto const isInvolved = involvedEdges.count(edgeIndex) > 0;
		edgeItem->setOpacity(!hasHighlight || isInvolved ? 1.0 : DimmedOpacity);
		auto pen = _edgeBasePens[edgeIndex];
		if (isInvolved)
		{
			pen.setColor(HighlightEdgeColor);
			pen.setWidthF(3.0);
		}
		edgeItem->setLinePen(pen);
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
	for (auto* const groupItem : _switchGroupItems)
	{
		static_cast<SwitchGroupNodeItem*>(groupItem)->clearRowSelections();
	}
	if (auto const it = _itemsForEntity.find(_selectedEntityID); it != _itemsForEntity.end())
	{
		for (auto const& representation : it->second)
		{
			if (representation.row)
			{
				static_cast<SwitchGroupNodeItem*>(representation.item)->setRowSelected(*representation.row, true);
			}
			else
			{
				representation.item->setSelected(true);
			}
		}
		// Make sure the (first) selected element is visible
		auto const& first = it->second.front();
		if (first.row)
		{
			auto const rowRect = static_cast<SwitchGroupNodeItem*>(first.item)->rowSceneRect(*first.row);
			_graphView->ensureVisible(rowRect, 50, 50);
		}
		else
		{
			_graphView->ensureVisible(first.item, 50, 50);
		}
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
	_nodeRepresentations.clear();
	_edgeItems.clear();
	_edgeBasePens.clear();
	_switchGroupItems.clear();
	_uplinkEdgeForNode.clear();

	if (_layoutMode == LayoutMode::AggregatedBySwitch)
	{
		rebuildAggregatedScene();
	}
	else
	{
		rebuildDetailedScene();
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

void NetworkGraphPane::rebuildDetailedScene()
{
	// Lay out the nodes sorted by their identifier: siblings (and trees of the forest) are placed in
	// increasing layout index order, so the rendering doesn't depend on the discovery order of the devices
	auto nodeForSlot = std::vector<std::size_t>(_topology.nodes.size());
	std::iota(nodeForSlot.begin(), nodeForSlot.end(), std::size_t{ 0u });
	std::sort(nodeForSlot.begin(), nodeForSlot.end(),
		[this](std::size_t const lhs, std::size_t const rhs)
		{
			return isNodeSortedBefore(_topology, lhs, rhs);
		});
	auto slotForNode = std::vector<std::size_t>(_topology.nodes.size());
	for (auto slot = std::size_t{ 0u }; slot < nodeForSlot.size(); ++slot)
	{
		slotForNode[nodeForSlot[slot]] = slot;
	}

	// Compute the layout: parent of a node is its upstream neighbor (towards the grandmaster)
	auto layoutItems = std::vector<qtMate::graph::TreeLayoutItem>(_topology.nodes.size());
	for (auto slot = std::size_t{ 0u }; slot < nodeForSlot.size(); ++slot)
	{
		layoutItems[slot].size = _topology.nodes[nodeForSlot[slot]].type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity ? EntityNodeSize : BridgeNodeSize;
	}
	for (auto const& edge : _topology.edges)
	{
		layoutItems[slotForNode[edge.downstreamNodeIndex]].parentIndex = static_cast<int>(slotForNode[edge.upstreamNodeIndex]);
	}
	auto const positions = qtMate::graph::computeTreeLayout(layoutItems, HorizontalSpacing, VerticalSpacing);

	// Create node items
	_nodeRepresentations.resize(_topology.nodes.size());
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < _topology.nodes.size(); ++nodeIndex)
	{
		auto const& node = _topology.nodes[nodeIndex];
		qtMate::graph::GraphNodeItem* item = nullptr;
		if (node.type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity)
		{
			item = new EntityGraphNodeItem{ node };
			_itemsForEntity[node.entityID].push_back(NodeRepresentation{ item, std::nullopt });
			_entityForItem.emplace(item, node.entityID);
		}
		else
		{
			item = new BridgeGraphNodeItem{ node };
		}
		item->setPos(positions[slotForNode[nodeIndex]]);
		_scene->addItem(item);
		_nodeRepresentations[nodeIndex] = NodeRepresentation{ item, std::nullopt };
	}

	// Create edge items
	_edgeItems.resize(_topology.edges.size());
	_edgeBasePens.resize(_topology.edges.size());
	for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < _topology.edges.size(); ++edgeIndex)
	{
		auto const& edge = _topology.edges[edgeIndex];
		auto* const edgeItem = new StreamEdgeItem{ this, edgeIndex, _nodeRepresentations[edge.upstreamNodeIndex].item, _nodeRepresentations[edge.downstreamNodeIndex].item };
		_scene->addItem(edgeItem);
		_edgeItems[edgeIndex] = edgeItem;
		applyEdgeDecorations(edgeIndex);
	}
}

void NetworkGraphPane::rebuildAggregatedScene()
{
	auto aggregation = computeAggregation(_topology);

	// One layout slot per represented element: the bridges (as switch groups) and the non aggregated entities
	auto groupIndexForNode = std::vector<std::optional<std::size_t>>(_topology.nodes.size());
	for (auto groupIndex = std::size_t{ 0u }; groupIndex < aggregation.groups.size(); ++groupIndex)
	{
		groupIndexForNode[aggregation.groups[groupIndex].bridgeNodeIndex] = groupIndex;
	}
	auto nodeForSlot = std::vector<std::size_t>{};
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < _topology.nodes.size(); ++nodeIndex)
	{
		if (!aggregation.placementForNode[nodeIndex])
		{
			nodeForSlot.push_back(nodeIndex);
		}
	}
	// Deterministic placement of siblings (and trees of the forest), whatever the discovery order of the devices
	std::sort(nodeForSlot.begin(), nodeForSlot.end(),
		[this](std::size_t const lhs, std::size_t const rhs)
		{
			return isNodeSortedBefore(_topology, lhs, rhs);
		});
	auto slotForNode = std::vector<std::optional<std::size_t>>(_topology.nodes.size());
	for (auto slot = std::size_t{ 0u }; slot < nodeForSlot.size(); ++slot)
	{
		slotForNode[nodeForSlot[slot]] = slot;
	}

	// Uplink of each node, source of the link information displayed on the aggregated rows
	_uplinkEdgeForNode.assign(_topology.nodes.size(), std::nullopt);
	for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < _topology.edges.size(); ++edgeIndex)
	{
		_uplinkEdgeForNode[_topology.edges[edgeIndex].downstreamNodeIndex] = edgeIndex;
	}

	// Compute the layout of the aggregated forest (usually way smaller than the detailed one)
	auto layoutItems = std::vector<qtMate::graph::TreeLayoutItem>(nodeForSlot.size());
	for (auto slot = std::size_t{ 0u }; slot < nodeForSlot.size(); ++slot)
	{
		auto const nodeIndex = nodeForSlot[slot];
		layoutItems[slot].size = groupIndexForNode[nodeIndex] ? SwitchGroupNodeItem::sizeForRowCount(aggregation.groups[*groupIndexForNode[nodeIndex]].rows.size()) : EntityNodeSize;
	}
	for (auto const& edge : _topology.edges)
	{
		// Edges ending on an aggregated entity are internal to a switch group, the others hang the downstream
		// element below the item representing the upstream node (the containing group for an aggregated entity)
		if (aggregation.placementForNode[edge.downstreamNodeIndex])
		{
			continue;
		}
		auto const upstreamNodeIndex = aggregation.placementForNode[edge.upstreamNodeIndex] ? aggregation.groups[aggregation.placementForNode[edge.upstreamNodeIndex]->first].bridgeNodeIndex : edge.upstreamNodeIndex;
		layoutItems[*slotForNode[edge.downstreamNodeIndex]].parentIndex = static_cast<int>(*slotForNode[upstreamNodeIndex]);
	}
	auto const positions = qtMate::graph::computeTreeLayout(layoutItems, HorizontalSpacing, VerticalSpacing);

	// Create node items (a switch group per bridge, a regular entity node for the non aggregated entities)
	_nodeRepresentations.resize(_topology.nodes.size());
	for (auto slot = std::size_t{ 0u }; slot < nodeForSlot.size(); ++slot)
	{
		auto const nodeIndex = nodeForSlot[slot];
		auto const& node = _topology.nodes[nodeIndex];
		qtMate::graph::GraphNodeItem* item = nullptr;
		if (auto const groupIndex = groupIndexForNode[nodeIndex])
		{
			auto& group = aggregation.groups[*groupIndex];
			auto* const groupItem = new SwitchGroupNodeItem{ this, node, std::move(group.rows) };
			_switchGroupItems.push_back(groupItem);
			for (auto rowIndex = std::size_t{ 0u }; rowIndex < group.rowNodeIndices.size(); ++rowIndex)
			{
				auto const rowNodeIndex = group.rowNodeIndices[rowIndex];
				auto const representation = NodeRepresentation{ groupItem, rowIndex };
				_nodeRepresentations[rowNodeIndex] = representation;
				_itemsForEntity[_topology.nodes[rowNodeIndex].entityID].push_back(representation);
			}
			item = groupItem;
		}
		else
		{
			item = new EntityGraphNodeItem{ node };
			_itemsForEntity[node.entityID].push_back(NodeRepresentation{ item, std::nullopt });
			_entityForItem.emplace(item, node.entityID);
		}
		item->setPos(positions[slot]);
		_scene->addItem(item);
		_nodeRepresentations[nodeIndex] = NodeRepresentation{ item, std::nullopt };
	}

	// Create edge items (the edges internal to a switch group have no item)
	_edgeItems.assign(_topology.edges.size(), nullptr);
	_edgeBasePens.assign(_topology.edges.size(), QPen{});
	for (auto edgeIndex = std::size_t{ 0u }; edgeIndex < _topology.edges.size(); ++edgeIndex)
	{
		auto const& edge = _topology.edges[edgeIndex];
		if (aggregation.placementForNode[edge.downstreamNodeIndex])
		{
			continue;
		}
		auto* const edgeItem = new StreamEdgeItem{ this, edgeIndex, _nodeRepresentations[edge.upstreamNodeIndex].item, _nodeRepresentations[edge.downstreamNodeIndex].item };
		_scene->addItem(edgeItem);
		_edgeItems[edgeIndex] = edgeItem;
		applyEdgeDecorations(edgeIndex);
	}

	// Display the link information of the aggregated rows (their uplink has no edge item)
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < _nodeRepresentations.size(); ++nodeIndex)
	{
		applyRowLinkDecorations(nodeIndex);
	}
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
