/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "ui_mainWindow.h"
#include "avdecc/controllerModel.hpp"
#include "toolkit/dynamicHeaderView.hpp"
#include "toolkit/comboBox.hpp"
#include "toolkit/material/button.hpp"
#include "toolkit/material/color.hpp"
#include "networkInterfaceModel.hpp"

#include <QSettings>
#include <QLabel>
#include <memory>

class DynamicHeaderView;

class MainWindow : public QMainWindow, private Ui::MainWindow, private settings::SettingsManager::Observer
{
	Q_OBJECT
public:
	// Constructor
	MainWindow(QWidget* parent = nullptr);

private:
	// Private Slots
	Q_SLOT void currentControllerChanged();
	Q_SLOT void currentControlledEntityChanged(QModelIndex const& index);

	// Private methods
	void registerMetaTypes();
	void createViewMenu();
	void createMainToolBar();
	void createControllerView();
	void initInterfaceComboBox();
	void loadSettings();
	void connectSignals();
	void showChangeLog(QString const title, QString const versionString);
	virtual void showEvent(QShowEvent* event) override;
	virtual void closeEvent(QCloseEvent* event) override;
	virtual void dragEnterEvent(QDragEnterEvent* event) override;
	virtual void dropEvent(QDropEvent* event) override;
	void updateStyleSheet(qt::toolkit::material::color::Name const colorName, QString const& filename);

	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

	// Private memberes
	qt::toolkit::ComboBox _interfaceComboBox{ this };
	NetworkInterfaceModel _networkInterfaceModel{ this };
	QSortFilterProxyModel _networkInterfaceModelProxy{ this };
	qt::toolkit::material::Button _refreshControllerButton{ "refresh", this };
	QLabel _controllerEntityIDLabel{ this };
	avdecc::ControllerModel* _controllerModel{ nullptr };
	qt::toolkit::DynamicHeaderView _controllerDynamicHeaderView{ Qt::Horizontal, this };
};
