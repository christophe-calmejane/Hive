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

#include <QTableView>
#include <QRegularExpression>
#include "settingsManager/settings.hpp"
#include "avdecc/channelConnectionManager.hpp"

namespace connectionMatrix
{
class Model;
class HeaderView;
class ItemDelegate;
class CornerWidget;

class View final : public QTableView, private settings::SettingsManager::Observer
{
	Q_OBJECT

	using QTableView::setModel;
	using QTableView::setHorizontalHeader;
	using QTableView::setVerticalHeader;

public:
	View(QWidget* parent = nullptr);
	virtual ~View();

	// Returns the index that refers to the given entityID
	QModelIndex findEntityModelIndex(la::avdecc::UniqueIdentifier const& entityID) const;
	QLineEdit* talkerFilterLineEdit() noexcept;
	QLineEdit* listenerFilterLineEdit() noexcept;

private:
	void onIntersectionClicked(QModelIndex const& index);
	void onCustomContextMenuRequested(QPoint const& pos);
	void onFilterChanged(QString const& filter);
	void applyFilterPattern(QRegularExpression const& pattern);
	void forceFilter();

	// QTableView overrides
	virtual void mouseMoveEvent(QMouseEvent* event) override;

	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

	// Slots
	void handleCreateChannelConnectionsFinished(avdecc::CreateConnectionsInfo const& info);

private:
	std::unique_ptr<Model> _model;
	std::unique_ptr<HeaderView> _horizontalHeaderView;
	std::unique_ptr<HeaderView> _verticalHeaderView;
	std::unique_ptr<ItemDelegate> _itemDelegate;
	std::unique_ptr<CornerWidget> _cornerWidget;
};

} // namespace connectionMatrix
