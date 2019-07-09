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

#include "activeNetworkInterfaceModel.hpp"
#include "networkInterfaceModel.hpp"
#include "settingsManager/settings.hpp"
#include <unordered_set>

class ActiveNetworkInterfaceModelPrivate : private settings::SettingsManager::Observer
{
public:
	ActiveNetworkInterfaceModelPrivate(ActiveNetworkInterfaceModel* q)
		: q_ptr{ q }
	{
	}

private:
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override
	{
		auto type = la::avdecc::networkInterface::Interface::Type::None;

		if (name == settings::InterfaceTypeEthernet.name)
		{
			type = la::avdecc::networkInterface::Interface::Type::Ethernet;
		}
		else if (name == settings::InterfaceTypeWiFi.name)
		{
			type = la::avdecc::networkInterface::Interface::Type::WiFi;
		}

		if (!AVDECC_ASSERT_WITH_RET(type != la::avdecc::networkInterface::Interface::Type::None, "Invalid type"))
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

		Q_Q(ActiveNetworkInterfaceModel);
		q->invalidateFilter();
	}

private:
	ActiveNetworkInterfaceModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(ActiveNetworkInterfaceModel);

	NetworkInterfaceModel _model{};
	std::unordered_set<la::avdecc::networkInterface::Interface::Type> _allowedInterfaceTypes{};
};

ActiveNetworkInterfaceModel::ActiveNetworkInterfaceModel(QObject* parent)
	: QSortFilterProxyModel{ parent }
	, d_ptr{ new ActiveNetworkInterfaceModelPrivate{ this } }
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.registerSettingObserver(settings::InterfaceTypeEthernet.name, d_ptr.get());
	settings.registerSettingObserver(settings::InterfaceTypeWiFi.name, d_ptr.get());

	setSourceModel(&d_ptr->_model);

	setSortRole(Qt::WhatsThisRole);
	sort(0, Qt::AscendingOrder);
}

ActiveNetworkInterfaceModel::~ActiveNetworkInterfaceModel()
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.unregisterSettingObserver(settings::InterfaceTypeWiFi.name, d_ptr.get());
	settings.unregisterSettingObserver(settings::InterfaceTypeEthernet.name, d_ptr.get());
}

bool ActiveNetworkInterfaceModel::isEnabled(QString const& id) const noexcept
{
	Q_D(const ActiveNetworkInterfaceModel);
	return d->_model.isEnabled(id);
}

bool ActiveNetworkInterfaceModel::filterAcceptsRow(int sourceRow, QModelIndex const& /*sourceParent*/) const
{
	Q_D(const ActiveNetworkInterfaceModel);
	auto const index = d->_model.index(sourceRow);
	auto const interfaceType = d->_model.interfaceType(index);
	return d->_allowedInterfaceTypes.count(interfaceType) == 1;
}
