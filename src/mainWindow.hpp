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
#include "profiles/profiles.hpp"
#include "activeNetworkInterfaceModel.hpp"

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
	// Private Structs
	struct Defaults
	{
		static constexpr int ColumnWidth_UniqueIdentifier = 160;
		static constexpr int ColumnWidth_Logo = 60;
		static constexpr int ColumnWidth_Compatibility = 50;
		static constexpr int ColumnWidth_Name = 180;
		static constexpr int ColumnWidth_ExclusiveAccessState = 80;
		static constexpr int ColumnWidth_Group = 80;
		static constexpr int ColumnWidth_GPTPDomain = 80;
		static constexpr int ColumnWidth_InterfaceIndex = 90;

		// MainWindow widgets
		bool mainWindow_Toolbar_Visible{ true };
		bool mainWindow_Inspector_Visible{ true };
		bool mainWindow_Logger_Visible{ true };

		// Controller Table View
		bool controllerTableView_EntityLogo_Visible{ true };
		bool controllerTableView_Compatibility_Visible{ true };
		bool controllerTableView_Name_Visible{ true };
		bool controllerTableView_Group_Visible{ true };
		bool controllerTableView_AcquireState_Visible{ true };
		bool controllerTableView_LockState_Visible{ true };
		bool controllerTableView_GrandmasterID_Visible{ true };
		bool controllerTableView_GptpDomain_Visible{ true };
		bool controllerTableView_InterfaceIndex_Visible{ true };
		bool controllerTableView_AssociationID_Visible{ true };
		bool controllerTableView_MediaClockMasterID_Visible{ true };
		bool controllerTableView_MediaClockMasterName_Visible{ true };
	};

	// Private Slots
	Q_SLOT void currentControllerChanged();
	Q_SLOT void currentControlledEntityChanged(QModelIndex const& index);

	// Private methods
	void setupAdvancedView(Defaults const& defaults);
	void setupStandardProfile();
	void setupDeveloperProfile();
	void setupProfile();
	void registerMetaTypes();
	void createViewMenu();
	void createMainToolBar();
	void createControllerView();
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
	ActiveNetworkInterfaceModel _activeNetworkInterfaceModel{ this };
	QSortFilterProxyModel _networkInterfaceModelProxy{ this };
	qt::toolkit::material::Button _refreshControllerButton{ "refresh", this };
	qt::toolkit::material::Button _openMcmdDialogButton{ "schedule", this };
	QLabel _controllerEntityIDLabel{ this };
	avdecc::ControllerModel* _controllerModel{ nullptr };
	qt::toolkit::DynamicHeaderView _controllerDynamicHeaderView{ Qt::Horizontal, this };
};
