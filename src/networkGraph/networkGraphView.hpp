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
#include <QtMate/widgets/flatIconButton.hpp>

#include <QLabel>
#include <QTabWidget>
#include <QWidget>

#include <vector>

class NetworkGraphPane;

/**
* @brief Dockable view displaying the network topology graphs.
* @details Displays one tab per network (Primary, Secondary, ... based on the AVB interface index of the
*          entities, like Milan redundancy defines them), each tab rendering the topology of its network
*          through a NetworkGraphPane.
*/
class NetworkGraphView : public QWidget
{
	Q_OBJECT
public:
	NetworkGraphView(QWidget* parent = nullptr);

	/**
	* @brief Selects the given entity in all the network graphs.
	* @details Used to synchronize the graphs with the application wide entity selection.
	* @param[in] entityID Entity to select, an invalid identifier clears the graphs selection.
	*/
	void selectEntity(la::avdecc::UniqueIdentifier const entityID);

	/** Emitted when the user selects an entity node in one of the network graphs. */
	Q_SIGNAL void entitySelectionChanged(la::avdecc::UniqueIdentifier const entityID);

private:
	void rebuildPanes();
	NetworkGraphPane* currentPane() const;
	void refreshStats();
	void refreshClearHighlightButton();

	hive::modelsLibrary::NetworkTopologyModel _topologyModel{ this };
	QTabWidget* _tabWidget{ nullptr };
	std::vector<std::pair<la::avdecc::entity::model::AvbInterfaceIndex, NetworkGraphPane*>> _panes{}; // Aligned with the tab widget pages
	qtMate::widgets::FlatIconButton _detailedLayoutButton{ "Material Icons", "device_hub", this };
	qtMate::widgets::FlatIconButton _aggregatedLayoutButton{ "Material Icons", "dns", this };
	qtMate::widgets::FlatIconButton _relayoutButton{ "Material Icons", "account_tree", this };
	qtMate::widgets::FlatIconButton _fitButton{ "Material Icons", "zoom_out_map", this };
	qtMate::widgets::FlatIconButton _clearHighlightButton{ "Material Icons", "highlight_off", this };
	qtMate::widgets::FlatIconButton _streamInfoButton{ "Material Icons", "label", this };
	qtMate::widgets::FlatIconButton _delayAsDistanceButton{ "Material Icons", "straighten", this };
	QLabel _statsLabel{ this };
	la::avdecc::UniqueIdentifier _selectedEntityID{};
};
