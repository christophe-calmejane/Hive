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
#include <QtMate/widgets/flatIconButton.hpp>

#include <QGraphicsScene>
#include <QLabel>
#include <QWidget>

#include <unordered_map>
#include <vector>

/**
* @brief Dockable view displaying the network topology graph.
* @details Renders the topology exposed by hive::modelsLibrary::NetworkTopologyModel as a forest of trees
*          (grandmasters on top), using the generic qtMate::graph rendering classes.
*          Entity nodes display gPTP and error information, inferred bridges are displayed with their
*          vendor name (deduced from the OUI of their clock identity).
*/
class NetworkGraphView : public QWidget
{
	Q_OBJECT
public:
	NetworkGraphView(QWidget* parent = nullptr);

	/**
	* @brief Selects the node(s) of the given entity in the graph (all its interfaces) and makes them visible.
	* @details Used to synchronize the graph with the application wide entity selection. Does nothing if the
	*          entity is not part of the topology. Does not re-emit entitySelectionChanged().
	* @param[in] entityID Entity to select, an invalid identifier clears the graph selection.
	*/
	void selectEntity(la::avdecc::UniqueIdentifier const entityID);

	/** Emitted when the user selects an entity node in the graph. */
	Q_SIGNAL void entitySelectionChanged(la::avdecc::UniqueIdentifier const entityID);

private:
	void rebuildScene();
	void applySelectionToScene();

	hive::modelsLibrary::NetworkTopologyModel _topologyModel{ this };
	QGraphicsScene* _scene{ nullptr };
	qtMate::graph::GraphView* _graphView{ nullptr };
	qtMate::widgets::FlatIconButton _relayoutButton{ "Material Icons", "account_tree", this };
	qtMate::widgets::FlatIconButton _fitButton{ "Material Icons", "zoom_out_map", this };
	QLabel _statsLabel{ this };

	// Selection synchronization state
	std::unordered_map<la::avdecc::UniqueIdentifier, std::vector<QGraphicsItem*>, la::avdecc::UniqueIdentifier::hash> _itemsForEntity{};
	std::unordered_map<QGraphicsItem*, la::avdecc::UniqueIdentifier> _entityForItem{};
	la::avdecc::UniqueIdentifier _selectedEntityID{};
	bool _changingSelection{ false };
};
