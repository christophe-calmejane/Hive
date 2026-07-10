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

private:
	void rebuildScene();

	hive::modelsLibrary::NetworkTopologyModel _topologyModel{ this };
	QGraphicsScene* _scene{ nullptr };
	qtMate::graph::GraphView* _graphView{ nullptr };
	qtMate::widgets::FlatIconButton _relayoutButton{ "Material Icons", "account_tree", this };
	qtMate::widgets::FlatIconButton _fitButton{ "Material Icons", "zoom_out_map", this };
	QLabel _statsLabel{ this };
};
