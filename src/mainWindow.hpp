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
#include <QSettings>
#include <QLabel>
#include <memory>
#include "avdecc/controllerModel.hpp"
#include "toolkit/dynamicHeaderView.hpp"
#include "toolkit/comboBox.hpp"

class DynamicHeaderView;

class MainWindow : public QMainWindow, private Ui::MainWindow
{
	Q_OBJECT
public:
	MainWindow(QWidget* parent = nullptr);

	Q_SLOT void currentControllerChanged();
	Q_SLOT void currentControlledEntityChanged(QModelIndex const& index);

private:
	void registerMetaTypes();

	void createViewMenu();
	void createMainToolBar();

	void createControllerView();

	void populateProtocolComboBox();
	void populateInterfaceComboBox();

	void loadSettings();

	void connectSignals();

private:
	void showChangeLog(QString const title, QString const versionString);
	void showEvent(QShowEvent* event) override;
	void closeEvent(QCloseEvent* event) override;

private:
	qt::toolkit::ComboBox _protocolComboBox{ this };
	qt::toolkit::ComboBox _interfaceComboBox{ this };
	QLabel _controllerEntityIDLabel{ this };
	avdecc::ControllerModel* _controllerModel{ nullptr };
	qt::toolkit::DynamicHeaderView _controllerDynamicHeaderView{ Qt::Horizontal, this };
};
