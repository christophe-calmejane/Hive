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

#include <QSettings>
#include <QLabel>
#include <memory>

class DynamicHeaderView;

// Model for Network Interfaces
class NetworkInterfaceModel final : public QAbstractListModel, public la::avdecc::networkInterface::NetworkInterfaceObserver
{
	struct Data
	{
		std::string id{};
		std::string name{};
		bool isEnabled{ false };
		bool isConnected{ false };
	};

public:
	NetworkInterfaceModel(QObject* parent = nullptr)
		: QAbstractListModel(parent)
	{
		la::avdecc::networkInterface::registerObserver(this);
	}

	QModelIndex indexOf(std::string const& id) const noexcept
	{
		auto const it = std::find_if(_interfaces.begin(), _interfaces.end(),
			[id](auto const& i)
			{
				return i.id == id;
			});
		if (it == _interfaces.end())
		{
			return {};
		}
		return createIndex(static_cast<int>(std::distance(_interfaces.begin(), it)), 0);
	}

	bool isEnabled(QModelIndex const& index) const noexcept
	{
		return (flags(index) & Qt::ItemIsEnabled) == Qt::ItemIsEnabled;
	}

private:
	// QAbstractListModel overrides
	virtual int rowCount(QModelIndex const& parent = QModelIndex()) const override
	{
		return static_cast<int>(_interfaces.size());
	}

	virtual QVariant data(QModelIndex const& index, int role) const override
	{
		auto const idx = index.row();
		if (idx >= 0 && idx < rowCount())
		{
			switch (role)
			{
				case Qt::DisplayRole:
					return QString::fromStdString(_interfaces.at(idx).name);
				case Qt::ForegroundRole:
				{
					auto const& intfc = _interfaces.at(idx);
					if (!intfc.isEnabled)
					{
						return qt::toolkit::material::color::value(qt::toolkit::material::color::Name::Gray);
					}
					else if (!intfc.isConnected)
					{
						return qt::toolkit::material::color::value(qt::toolkit::material::color::Name::Red);
					}
					else
					{
						return qt::toolkit::material::color::value(qt::toolkit::material::color::Name::Black);
					}
				}
				case Qt::UserRole:
				{
					return QString::fromStdString(_interfaces.at(idx).id);
				}
			}
		}
		return {};
	}

	virtual Qt::ItemFlags flags(QModelIndex const& index) const override
	{
		auto const idx = index.row();
		if (idx >= 0 && idx < rowCount())
		{
			if (_interfaces.at(idx).isEnabled)
			{
				return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
			}
		}
		return {};
	}

	// la::avdecc::networkInterface::NetworkInterfaceObserver overrides
	void onInterfaceAdded(la::avdecc::networkInterface::Interface const& intfc) noexcept
	{
		QMetaObject::invokeMethod(this,
			[this, intfc]()
			{
				// Only use non-virtual, enabled, Ethernet interfaces
				if (intfc.type == la::avdecc::networkInterface::Interface::Type::Ethernet && !intfc.isVirtual)
				{
					auto const count = rowCount();
					emit beginInsertRows({}, count, count);
					_interfaces.push_back(Data{ intfc.id, intfc.alias, intfc.isEnabled, intfc.isConnected });
					emit endInsertRows();
				}
			});
	}
	void onInterfaceRemoved(la::avdecc::networkInterface::Interface const& intfc) noexcept
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id]()
			{
				// Search the element
				auto const index = indexOf(id);
				if (index.isValid())
				{
					// Remove it
					auto const idx = index.row();
					emit beginRemoveRows({}, idx, idx);
					_interfaces.remove(idx);
					emit endRemoveRows();
				}
			});
	}
	void onInterfaceEnabledStateChanged(la::avdecc::networkInterface::Interface const& intfc, bool const isEnabled) noexcept
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id, isEnabled]()
			{
				// Search the element
				auto const index = indexOf(id);
				if (index.isValid())
				{
					// Change data and emit signal
					auto const idx = index.row();
					_interfaces[idx].isEnabled = isEnabled;
					auto const index = createIndex(idx, 0);
					emit dataChanged(index, index);
				}
			},
			Qt::QueuedConnection);
	}
	void onInterfaceConnectedStateChanged(la::avdecc::networkInterface::Interface const& intfc, bool const isConnected) noexcept
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id, isConnected]()
			{
				// Search the element
				auto const index = indexOf(id);
				if (index.isValid())
				{
					// Change data and emit signal
					auto const idx = index.row();
					_interfaces[idx].isConnected = isConnected;
					auto const index = createIndex(idx, 0);
					emit dataChanged(index, index);
				}
			},
			Qt::QueuedConnection);
	}

	// Private Members
	QVector<Data> _interfaces{};
	DECLARE_AVDECC_OBSERVER_GUARD(NetworkInterfaceModel);
};

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
	void populateProtocolComboBox();
	void initInterfaceComboBox();
	void loadSettings();
	void connectSignals();
	void showChangeLog(QString const title, QString const versionString);
	virtual void showEvent(QShowEvent* event) override;
	virtual void closeEvent(QCloseEvent* event) override;
	virtual void dragEnterEvent(QDragEnterEvent* event) override;
	virtual void dropEvent(QDropEvent* event) override;
	void updateStyleSheet(qt::toolkit::material::color::Name const colorName);

	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

	// Private memberes
	qt::toolkit::ComboBox _protocolComboBox{ this };
	qt::toolkit::ComboBox _interfaceComboBox{ this };
	NetworkInterfaceModel _networkInterfaceModel{ this };
	QSortFilterProxyModel _networkInterfaceModelProxy{ this };
	qt::toolkit::material::Button _refreshControllerButton{ "refresh", this };
	QLabel _controllerEntityIDLabel{ this };
	avdecc::ControllerModel* _controllerModel{ nullptr };
	qt::toolkit::DynamicHeaderView _controllerDynamicHeaderView{ Qt::Horizontal, this };
};
