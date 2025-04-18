/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "networkInterfaceTypeModel.hpp"
#include "settingsManager/settings.hpp"

#include <hive/widgetModelsLibrary/networkInterfacesListModel.hpp>

#include <QApplication>

class NetworkInterfaceTypeModelPrivate : private settings::SettingsManager::Observer
{
public:
	NetworkInterfaceTypeModelPrivate(NetworkInterfaceTypeModel* q)
		: q_ptr{ q }
	{
		_typeInfo.insert({ la::networkInterface::Interface::Type::Ethernet, { "Ethernet", false } });
		_typeInfo.insert({ la::networkInterface::Interface::Type::WiFi, { "WiFi", false } });
	}

	void setActive(la::networkInterface::Interface::Type const type, bool const active, bool const updateSettings)
	{
		Q_Q(NetworkInterfaceTypeModel);

		auto const it = _typeInfo.find(type);
		if (it != std::end(_typeInfo))
		{
			auto& info = it->second;

			// Update the model
			info.active = active;

			// Update associated setting if requested
			if (updateSettings)
			{
				auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
				switch (type)
				{
					case la::networkInterface::Interface::Type::Ethernet:
						settings->setValue(settings::Network_InterfaceTypeEthernet.name, active);
						break;
					case la::networkInterface::Interface::Type::WiFi:
						settings->setValue(settings::Network_InterfaceTypeWiFi.name, active);
						break;
					default:
						AVDECC_ASSERT(false, "Unhandled interface type");
						break;
				}
			}

			// Notify that the model has changed
			auto const row = std::distance(std::begin(_typeInfo), it);
			auto const index = q->createIndex(row, 0);
			emit q->dataChanged(index, index, { Qt::CheckStateRole });
		}
	}

	bool isActive(la::networkInterface::Interface::Type const type) const
	{
		auto const it = _typeInfo.find(type);
		if (it != std::end(_typeInfo))
		{
			return it->second.active;
		}

		return false;
	}

private:
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override
	{
		if (name == settings::Network_InterfaceTypeEthernet.name)
		{
			setActive(la::networkInterface::Interface::Type::Ethernet, value.toBool(), false);
		}
		else if (name == settings::Network_InterfaceTypeWiFi.name)
		{
			setActive(la::networkInterface::Interface::Type::WiFi, value.toBool(), false);
		}
	}

private:
	NetworkInterfaceTypeModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(NetworkInterfaceTypeModel)

	struct Info
	{
		QString name;
		bool active{ false };
	};

	std::map<la::networkInterface::Interface::Type, Info> _typeInfo;
};

NetworkInterfaceTypeModel::NetworkInterfaceTypeModel(QObject* parent)
	: QAbstractListModel{ parent }
	, d_ptr{ new NetworkInterfaceTypeModelPrivate{ this } }
{
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	settings->registerSettingObserver(settings::Network_InterfaceTypeEthernet.name, d_ptr.get());
	settings->registerSettingObserver(settings::Network_InterfaceTypeWiFi.name, d_ptr.get());
}

NetworkInterfaceTypeModel::~NetworkInterfaceTypeModel()
{
	// Remove settings observers
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	settings->unregisterSettingObserver(settings::Network_InterfaceTypeWiFi.name, d_ptr.get());
	settings->unregisterSettingObserver(settings::Network_InterfaceTypeEthernet.name, d_ptr.get());
}

void NetworkInterfaceTypeModel::setActive(la::networkInterface::Interface::Type const type, bool const active)
{
	Q_D(NetworkInterfaceTypeModel);
	d->setActive(type, active, true);
}

bool NetworkInterfaceTypeModel::isActive(la::networkInterface::Interface::Type const type) const
{
	Q_D(const NetworkInterfaceTypeModel);
	return d->isActive(type);
}

int NetworkInterfaceTypeModel::rowCount(QModelIndex const& /*parent*/) const
{
	Q_D(const NetworkInterfaceTypeModel);
	return static_cast<int>(d->_typeInfo.size());
}

QVariant NetworkInterfaceTypeModel::data(QModelIndex const& index, int role) const
{
	Q_D(const NetworkInterfaceTypeModel);

	auto const it = std::next(std::begin(d->_typeInfo), index.row());
	if (it != std::end(d->_typeInfo))
	{
		auto const& [type, info] = *it;

		switch (role)
		{
			case Qt::DisplayRole:
				return info.name;
			case Qt::CheckStateRole:
				return info.active ? Qt::Checked : Qt::Unchecked;
			case Qt::DecorationRole:
				return hive::widgetModelsLibrary::NetworkInterfacesListModel::interfaceTypeIcon(type);
			default:
				break;
		}
	}

	return {};
}

bool NetworkInterfaceTypeModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	Q_D(NetworkInterfaceTypeModel);

	if (role == Qt::CheckStateRole)
	{
		auto const it = std::next(std::begin(d->_typeInfo), index.row());
		if (it != std::end(d->_typeInfo))
		{
			auto const type = it->first;
			d->setActive(type, value.toBool(), true);
			return true;
		}
	}

	return false;
}

Qt::ItemFlags NetworkInterfaceTypeModel::flags(QModelIndex const&) const
{
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}
