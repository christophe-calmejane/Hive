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

#include "activeNetworkInterfacesModel.hpp"
#include "settingsManager/settings.hpp"

#include <hive/widgetModelsLibrary/networkInterfacesListModel.hpp>

#include <QApplication>

#include <unordered_set>

class ActiveNetworkInterfacesModelPrivate : private settings::SettingsManager::Observer
{
public:
	ActiveNetworkInterfacesModelPrivate(ActiveNetworkInterfacesModel* q)
		: q_ptr{ q }
	{
	}

private:
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override
	{
		auto type = la::networkInterface::Interface::Type::None;

		if (name == settings::Network_InterfaceTypeEthernet.name)
		{
			type = la::networkInterface::Interface::Type::Ethernet;
		}
		else if (name == settings::Network_InterfaceTypeWiFi.name)
		{
			type = la::networkInterface::Interface::Type::WiFi;
		}

		if (!AVDECC_ASSERT_WITH_RET(type != la::networkInterface::Interface::Type::None, "Invalid type"))
		{
			return;
		}

		if (value.toBool())
		{
			_allowedInterfaceTypes.insert(type);
		}
		else
		{
			_allowedInterfaceTypes.erase(type);
		}

		Q_Q(ActiveNetworkInterfacesModel);
		q->invalidateFilter();
	}

private:
	ActiveNetworkInterfacesModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(ActiveNetworkInterfacesModel)

	hive::widgetModelsLibrary::NetworkInterfacesListModel _model{ true };
	std::unordered_set<la::networkInterface::Interface::Type> _allowedInterfaceTypes{ la::networkInterface::Interface::Type::Loopback };
};

ActiveNetworkInterfacesModel::ActiveNetworkInterfacesModel(QObject* parent)
	: QSortFilterProxyModel{ parent }
	, d_ptr{ new ActiveNetworkInterfacesModelPrivate{ this } }
{
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	settings->registerSettingObserver(settings::Network_InterfaceTypeEthernet.name, d_ptr.get());
	settings->registerSettingObserver(settings::Network_InterfaceTypeWiFi.name, d_ptr.get());

	setSourceModel(&d_ptr->_model);

	setSortRole(Qt::WhatsThisRole);
	sort(0, Qt::AscendingOrder);
}

ActiveNetworkInterfacesModel::~ActiveNetworkInterfacesModel()
{
	// Remove settings observers
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	settings->unregisterSettingObserver(settings::Network_InterfaceTypeWiFi.name, d_ptr.get());
	settings->unregisterSettingObserver(settings::Network_InterfaceTypeEthernet.name, d_ptr.get());
}

bool ActiveNetworkInterfacesModel::isEnabled(QString const& id) const noexcept
{
	Q_D(const ActiveNetworkInterfacesModel);
	return d->_model.isEnabled(id);
}

bool ActiveNetworkInterfacesModel::filterAcceptsRow(int sourceRow, QModelIndex const& /*sourceParent*/) const
{
	Q_D(const ActiveNetworkInterfacesModel);
	auto const index = d->_model.index(sourceRow);
	auto const interfaceType = d->_model.getInterfaceType(index);
	return d->_allowedInterfaceTypes.count(interfaceType) == 1;
}
