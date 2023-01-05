/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "ui_loggerView.h"
#include "avdecc/loggerModel.hpp"

#include <QtMate/widgets/dynamicHeaderView.hpp>
#include <QtMate/widgets/tickableMenu.hpp>

#include <QSortFilterProxyModel>

class LoggerView : public QWidget, private Ui::LoggerView
{
	Q_OBJECT
public:
	LoggerView(QWidget* parent = nullptr);
	qtMate::widgets::DynamicHeaderView* header() const;

private:
	void createLayerFilterButton();
	void createLevelFilterButton();

private:
	avdecc::LoggerModel _loggerModel{ this };
	QSortFilterProxyModel _layerFilterProxyModel{ this };
	QSortFilterProxyModel _levelFilterProxyModel{ this };
	QSortFilterProxyModel _searchFilterProxyModel{ this };
	qtMate::widgets::DynamicHeaderView _dynamicHeaderView{ Qt::Horizontal, this };
	qtMate::widgets::TickableMenu _layerFilterMenu{ this };
	qtMate::widgets::TickableMenu _levelFilterMenu{ this };
};
