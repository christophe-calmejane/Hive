/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <QTableView>
#include <QSortFilterProxyModel>
#include "settingsManager/settings.hpp"
#include "toolkit/transposeProxyModel.hpp"

namespace connectionMatrix
{
class Model;
class HeaderView;
class ItemDelegate;
class Legend;

class Filter : public QSortFilterProxyModel
{
protected:
	virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
		return sourceModel()->headerData(sourceRow, Qt::Vertical, Qt::DisplayRole).toString().contains(filterRegExp());
	}
	virtual bool filterAcceptsColumn(int sourceColumn, const QModelIndex &sourceParent) const override {
		return sourceModel()->headerData(sourceColumn, Qt::Horizontal, Qt::DisplayRole).toString().contains(filterRegExp());
	}
};

class View final : public QTableView, private settings::SettingsManager::Observer
{
	using QTableView::setModel;
	using QTableView::setVerticalHeader;
	using QTableView::setHorizontalHeader;

public:
	View(QWidget* parent = nullptr);
	virtual ~View();

protected:
	// QTableView overrides
	virtual void mouseMoveEvent(QMouseEvent* event) override;

	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

	Q_SLOT void onClicked(QModelIndex const& index);
	Q_SLOT void onCustomContextMenuRequested(QPoint const& pos);
	Q_SLOT void onHeaderCustomContextMenuRequested(QPoint const& pos);
	Q_SLOT void onLegendGeometryChanged();

	QVariant talkerData(QModelIndex const& index, int role) const;
	QVariant listenerData(QModelIndex const& index, int role) const;

private:
	std::unique_ptr<Model> _model;
	std::unique_ptr<HeaderView> _verticalHeaderView;
	std::unique_ptr<HeaderView> _horizontalHeaderView;
	std::unique_ptr<ItemDelegate> _itemDelegate;
	std::unique_ptr<Legend> _legend;

	qt::toolkit::TransposeProxyModel _proxy;
	Filter _filterProxy;
	bool _isTransposed{ false };
};

} // namespace connectionMatrix
