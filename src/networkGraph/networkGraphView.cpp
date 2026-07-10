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

#include "networkGraphView.hpp"

#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <QtMate/graph/graphEdgeItem.hpp>
#include <QtMate/graph/graphNodeItem.hpp>
#include <QtMate/graph/treeLayout.hpp>

#include <QFontMetricsF>
#include <QGraphicsSceneContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QStringList>
#include <QVBoxLayout>

#include <chrono>
#include <unordered_map>

namespace
{
using TopologyNode = hive::modelsLibrary::NetworkTopologyModel::Node;

// Node dimensions and layout constants
constexpr auto EntityNodeSize = QSizeF{ 190.0, 72.0 };
constexpr auto BridgeNodeSize = QSizeF{ 190.0, 54.0 };
constexpr auto HorizontalSpacing = 40.0;
constexpr auto VerticalSpacing = 70.0;

// Colors (fixed, readable on both light and dark application themes since nodes are self contained boxes)
auto const EntityFillColor = QColor{ 0xFAFAFA };
auto const BridgeFillColor = QColor{ 0xECEFF1 };
auto const BorderColor = QColor{ 0x616161 };
auto const SelectedBorderColor = QColor{ 0x1E88E5 };
auto const TextColor = QColor{ 0x212121 };
auto const SecondaryTextColor = QColor{ 0x757575 };
auto const GrandmasterColor = QColor{ 0xFFC107 };
auto const ErrorColor = QColor{ 0xD32F2F };
auto const LinkUpColor = QColor{ 0x4CAF50 };
auto const LinkDownColor = QColor{ 0xF44336 };
auto const LinkUnknownColor = QColor{ 0x9E9E9E };
auto const EdgeColor = QColor{ 0x90A4AE };
auto const ActiveEdgeColor = QColor{ 0x1E88E5 };

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

QColor linkStatusColor(la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus)
{
	switch (linkStatus)
	{
		case la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Up:
			return LinkUpColor;
		case la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down:
			return LinkDownColor;
		default:
			return LinkUnknownColor;
	}
}

// Draws the 'GM' tag in the top-right corner of grandmaster nodes and the error badge in the top-left corner
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
		auto const badgeRect = QRectF{ 4.0, 4.0, 26.0, 15.0 };
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

// Graph node displaying a discovered entity (one node per AVB interface)
class EntityGraphNodeItem final : public qtMate::graph::GraphNodeItem
{
public:
	EntityGraphNodeItem(TopologyNode const& node)
		: GraphNodeItem{ node.name, EntityNodeSize }
		, _node{ node }
	{
		auto tooltip = QString{ "<b>%1</b>" }.arg(node.name.toHtmlEscaped());
		tooltip += "<br>Entity ID: " + hive::modelsLibrary::helper::uniqueIdentifierToString(node.entityID);
		if (node.isMultiInterface || !node.avbInterfaceName.isEmpty())
		{
			tooltip += QString{ "<br>AVB Interface: %1 (index %2)" }.arg(node.avbInterfaceName.toHtmlEscaped()).arg(node.avbInterfaceIndex);
		}
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
		setToolTip(tooltip);
	}

	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* /*option*/, QWidget* /*widget*/) override
	{
		auto const rect = QRectF{ QPointF{ 0.0, 0.0 }, size() };
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(QPen{ isSelected() ? SelectedBorderColor : BorderColor, isSelected() ? 2.0 : 1.0 });
		painter->setBrush(EntityFillColor);
		painter->drawRoundedRect(rect, 6.0, 6.0);

		auto const textWidth = rect.width() - 16.0;

		// Entity name (bold)
		auto nameFont = painter->font();
		nameFont.setBold(true);
		painter->setFont(nameFont);
		painter->setPen(TextColor);
		painter->drawText(QRectF{ 8.0, 6.0, textWidth, 18.0 }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF{ nameFont }.elidedText(label(), Qt::ElideMiddle, textWidth));

		// Entity ID
		auto smallFont = painter->font();
		smallFont.setBold(false);
		smallFont.setPointSizeF(smallFont.pointSizeF() * 0.85);
		painter->setFont(smallFont);
		painter->setPen(SecondaryTextColor);
		painter->drawText(QRectF{ 8.0, 24.0, textWidth, 16.0 }, Qt::AlignLeft | Qt::AlignVCenter, hive::modelsLibrary::helper::uniqueIdentifierToString(_node.entityID));

		// AVB interface (only relevant when the entity has multiple interfaces, ie. multiple nodes)
		if (_node.isMultiInterface)
		{
			auto const interfaceText = QString{ "%1 (index %2)" }.arg(_node.avbInterfaceName).arg(_node.avbInterfaceIndex);
			painter->drawText(QRectF{ 8.0, 40.0, textWidth, 16.0 }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF{ smallFont }.elidedText(interfaceText, Qt::ElideRight, textWidth));
		}

		// Link status dot
		painter->setPen(Qt::NoPen);
		painter->setBrush(linkStatusColor(_node.linkStatus));
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

private:
	TopologyNode _node{};
};

// Graph node displaying a bridge inferred from the discovery protocol (not an ATDECC entity)
class BridgeGraphNodeItem final : public qtMate::graph::GraphNodeItem
{
public:
	BridgeGraphNodeItem(TopologyNode const& node)
		: GraphNodeItem{ node.name, BridgeNodeSize }
		, _node{ node }
	{
		auto tooltip = QStringLiteral("<b>Network Bridge</b> (inferred from gPTP, not an ATDECC entity)");
		if (!node.name.isEmpty())
		{
			tooltip += "<br>Vendor: " + node.name.toHtmlEscaped();
		}
		tooltip += "<br>Clock Identity: " + hive::modelsLibrary::helper::uniqueIdentifierToString(node.clockIdentity);
		setToolTip(tooltip);
	}

	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* /*option*/, QWidget* /*widget*/) override
	{
		auto const rect = QRectF{ QPointF{ 0.0, 0.0 }, size() };
		painter->setRenderHint(QPainter::Antialiasing);
		auto borderPen = QPen{ isSelected() ? SelectedBorderColor : BorderColor, isSelected() ? 2.0 : 1.0 };
		borderPen.setStyle(Qt::DashLine);
		painter->setPen(borderPen);
		painter->setBrush(BridgeFillColor);
		painter->drawRoundedRect(rect, 6.0, 6.0);

		auto const textWidth = rect.width() - 16.0;

		// Title: 'Bridge' with the vendor name when known
		auto titleFont = painter->font();
		titleFont.setItalic(true);
		painter->setFont(titleFont);
		painter->setPen(TextColor);
		auto const title = _node.name.isEmpty() ? QStringLiteral("Bridge") : QString{ "Bridge - %1" }.arg(_node.name);
		painter->drawText(QRectF{ 8.0, 6.0, textWidth, 18.0 }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetricsF{ titleFont }.elidedText(title, Qt::ElideRight, textWidth));

		// Clock identity
		auto smallFont = painter->font();
		smallFont.setItalic(false);
		smallFont.setPointSizeF(smallFont.pointSizeF() * 0.85);
		painter->setFont(smallFont);
		painter->setPen(SecondaryTextColor);
		painter->drawText(QRectF{ 8.0, 24.0, textWidth, 16.0 }, Qt::AlignLeft | Qt::AlignVCenter, hive::modelsLibrary::helper::uniqueIdentifierToString(_node.clockIdentity));

		paintNodeBadges(painter, size(), _node.isGrandmaster, 0u);
	}

private:
	TopologyNode _node{};
};
} // namespace

NetworkGraphView::NetworkGraphView(QWidget* parent)
	: QWidget{ parent }
{
	_scene = new QGraphicsScene{ this };
	_graphView = new qtMate::graph::GraphView{ this };
	_graphView->setScene(_scene);

	_relayoutButton.setToolTip("Re-layout the graph");
	_fitButton.setToolTip("Zoom to fit");

	auto* const toolbarLayout = new QHBoxLayout{};
	toolbarLayout->setContentsMargins(2, 2, 2, 2);
	toolbarLayout->addWidget(&_relayoutButton);
	toolbarLayout->addWidget(&_fitButton);
	toolbarLayout->addStretch();
	toolbarLayout->addWidget(&_statsLabel);

	auto* const layout = new QVBoxLayout{ this };
	layout->setContentsMargins(2, 2, 2, 2);
	layout->addLayout(toolbarLayout);
	layout->addWidget(_graphView);

	connect(&_topologyModel, &hive::modelsLibrary::NetworkTopologyModel::topologyChanged, this,
		[this]()
		{
			rebuildScene();
		});
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
	connect(&_relayoutButton, &QPushButton::clicked, this,
		[this]()
		{
			rebuildScene();
		});
	connect(&_fitButton, &QPushButton::clicked, this,
		[this]()
		{
			_graphView->fitToContents();
		});
}

void NetworkGraphView::selectEntity(la::avdecc::UniqueIdentifier const entityID)
{
	if (entityID == _selectedEntityID)
	{
		return;
	}
	_selectedEntityID = entityID;
	applySelectionToScene();
}

void NetworkGraphView::applySelectionToScene()
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

void NetworkGraphView::rebuildScene()
{
	auto const& topology = _topologyModel.topology();

	_changingSelection = true;
	_scene->clear();
	_changingSelection = false;
	_itemsForEntity.clear();
	_entityForItem.clear();

	// Compute the layout: parent of a node is its upstream neighbor (towards the grandmaster)
	auto layoutItems = std::vector<qtMate::graph::TreeLayoutItem>(topology.nodes.size());
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < topology.nodes.size(); ++nodeIndex)
	{
		layoutItems[nodeIndex].size = topology.nodes[nodeIndex].type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity ? EntityNodeSize : BridgeNodeSize;
	}
	for (auto const& edge : topology.edges)
	{
		layoutItems[edge.downstreamNodeIndex].parentIndex = static_cast<int>(edge.upstreamNodeIndex);
	}
	auto const positions = qtMate::graph::computeTreeLayout(layoutItems, HorizontalSpacing, VerticalSpacing);

	// Create node items
	auto nodeItems = std::vector<qtMate::graph::GraphNodeItem*>(topology.nodes.size());
	auto entityCount = 0;
	auto bridgeCount = 0;
	for (auto nodeIndex = std::size_t{ 0u }; nodeIndex < topology.nodes.size(); ++nodeIndex)
	{
		auto const& node = topology.nodes[nodeIndex];
		qtMate::graph::GraphNodeItem* item = nullptr;
		if (node.type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity)
		{
			item = new EntityGraphNodeItem{ node };
			_itemsForEntity[node.entityID].push_back(item);
			_entityForItem.emplace(item, node.entityID);
			++entityCount;
		}
		else
		{
			item = new BridgeGraphNodeItem{ node };
			++bridgeCount;
		}
		item->setPos(positions[nodeIndex]);
		_scene->addItem(item);
		nodeItems[nodeIndex] = item;
	}

	// Create edge items
	for (auto const& edge : topology.edges)
	{
		auto* const edgeItem = new qtMate::graph::GraphEdgeItem{ nodeItems[edge.upstreamNodeIndex], nodeItems[edge.downstreamNodeIndex] };
		auto const hasStreams = edge.streamCount > 0u;
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
			auto const& downstreamNode = topology.nodes[edge.downstreamNodeIndex];
			if (downstreamNode.type == hive::modelsLibrary::NetworkTopologyModel::NodeType::Entity && downstreamNode.propagationDelay && *downstreamNode.propagationDelay > 0u)
			{
				labelParts += formatPropagationDelay(*downstreamNode.propagationDelay);
			}
		}

		if (hasStreams)
		{
			auto streamsText = QString{ "%1 %2" }.arg(edge.streamCount).arg(edge.streamCount > 1 ? "streams" : "stream");
			if (edge.streamPayloadBandwidth > 0u)
			{
				streamsText += QString{ " \xC2\xB7 %1" }.arg(formatBandwidth(edge.streamPayloadBandwidth));
			}
			labelParts += streamsText;

			// List the transiting stream connections in the tooltip (capped to keep it readable)
			constexpr auto MaxTooltipStreams = 15;
			auto streamList = QStringList{};
			for (auto const& description : edge.streamDescriptions)
			{
				if (streamList.size() >= MaxTooltipStreams)
				{
					streamList += QString{ "... and %1 more" }.arg(edge.streamDescriptions.size() - MaxTooltipStreams);
					break;
				}
				streamList += description.toHtmlEscaped();
			}
			if (!tooltip.isEmpty())
			{
				tooltip += "<br>";
			}
			tooltip += QString{ "<b>%1 (payload bitrate, transport overhead excluded)</b><br>%2" }.arg(streamsText.toHtmlEscaped(), streamList.join("<br>"));
		}

		edgeItem->setLabel(labelParts.join(" | "));
		edgeItem->setToolTip(tooltip);
		edgeItem->setLinePen(pen);
		_scene->addItem(edgeItem);
	}

	_statsLabel.setText(QString{ "%1 %2 - %3 %4" }.arg(entityCount).arg(entityCount > 1 ? "entities" : "entity").arg(bridgeCount).arg(bridgeCount > 1 ? "bridges" : "bridge"));

	// Restore the application wide entity selection on the freshly created items
	applySelectionToScene();

	_graphView->fitToContents();
}
