/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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
#include "discoveredEntities/view.hpp"

#include "visibilitySettings.hpp"

#include <QtMate/widgets/flatIconButton.hpp>

#include <QWidget>
#include <QLineEdit>
#include <QCheckBox>

class DiscoveredEntitiesView : public QWidget
{
	Q_OBJECT
public:
	DiscoveredEntitiesView(QWidget* parent = nullptr);

	void setupView(hive::VisibilityDefaults const& defaults) noexcept;
	discoveredEntities::View* entitiesTableView() noexcept;
	void setInspectorGeometry(QByteArray const& geometry) noexcept;
	QString filterText() const noexcept;
	bool isFilterLinked() const noexcept;

	// Public signals
	Q_SIGNAL void filterChanged(QString const& filter);
	Q_SIGNAL void filterLinkStateChanged(bool const isLinked, QString const& filter);

private:
	discoveredEntities::View _entitiesView{ this };
	QLineEdit _searchLineEdit{ this };
	QSortFilterProxyModel _searchFilterProxyModel{ this };
	QCheckBox _filterLinkedCheckbox{ "Link with Matrix Filter", this };
	qtMate::widgets::FlatIconButton _removeAllConnectionsButton{ "Hive", "remove_connections", this };
	qtMate::widgets::FlatIconButton _clearAllErrorsButton{ "Hive", "clear_errors", this };
	QByteArray _inspectorGeometry{};
};
