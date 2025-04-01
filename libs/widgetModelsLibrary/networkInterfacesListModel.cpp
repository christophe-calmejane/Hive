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

#include "hive/widgetModelsLibrary/networkInterfacesListModel.hpp"
#include "hive/widgetModelsLibrary/errorItemDelegate.hpp"
#include "hive/widgetModelsLibrary/qtUserRoles.hpp"

#include <QtMate/material/color.hpp>
#include <QtMate/material/helper.hpp>

#include <QStyleHints>
#include <QGuiApplication>

#include <unordered_map>

namespace hive
{
namespace widgetModelsLibrary
{
static std::unordered_map<la::networkInterface::Interface::Type, QIcon> s_cachedIcons{};

NetworkInterfacesListModel::NetworkInterfacesListModel(bool const addOfflineInterface) noexcept
	: _model{ hive::modelsLibrary::NetworkInterfacesModel{ this, addOfflineInterface } }
{
	connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
		[](Qt::ColorScheme /*scheme*/)
		{
			s_cachedIcons.clear();
		});
}

bool NetworkInterfacesListModel::isEnabled(QString const& id) const noexcept
{
	auto const optInterface = _model.networkInterface(id.toStdString());

	if (optInterface)
	{
		auto const& intfc = (*optInterface).get();
		return intfc.isEnabled;
	}

	return false;
}

la::networkInterface::Interface::Type NetworkInterfacesListModel::getInterfaceType(QModelIndex const& index) const noexcept
{
	if (auto const optInterface = _model.networkInterface(static_cast<std::size_t>(index.row())))
	{
		auto const& intfc = (*optInterface).get();
		return intfc.interfaceType;
	}

	return la::networkInterface::Interface::Type::None;
}

QIcon NetworkInterfacesListModel::interfaceTypeIcon(la::networkInterface::Interface::Type const type) noexcept
{
	auto const it = s_cachedIcons.find(type);
	if (it == std::end(s_cachedIcons))
	{
		auto what = QString{};

		switch (type)
		{
			case la::networkInterface::Interface::Type::Loopback:
				what = "flight";
				break;
			case la::networkInterface::Interface::Type::Ethernet:
				what = "settings_ethernet";
				break;
			case la::networkInterface::Interface::Type::WiFi:
				what = "wifi";
				break;
			default:
				AVDECC_ASSERT(false, "Unhandled type");
				what = "error_outline";
				break;
		}

		s_cachedIcons[type] = qtMate::material::helper::generateIcon(what, qtMate::material::color::foregroundColor());
	}

	return s_cachedIcons[type];
}

// hive::modelsLibrary::NetworkInterfacesAbstractListModel overrides
void NetworkInterfacesListModel::nameChanged(std::size_t const index, std::string const& /*name*/) noexcept
{
	auto const modelIndex = createIndex(static_cast<int>(index), 0);
	emit dataChanged(modelIndex, modelIndex, { Qt::DisplayRole });
}
void NetworkInterfacesListModel::enabledStateChanged(std::size_t const index, bool const /*isEnabled*/) noexcept
{
	auto const modelIndex = createIndex(static_cast<int>(index), 0);
	emit dataChanged(modelIndex, modelIndex, { Qt::DisplayRole });
}

void NetworkInterfacesListModel::connectedStateChanged(std::size_t const index, bool const /*isConnected*/) noexcept
{
	auto const modelIndex = createIndex(static_cast<int>(index), 0);
	emit dataChanged(modelIndex, modelIndex, { Qt::DisplayRole });
}

// QAbstractListModel overrides
int NetworkInterfacesListModel::rowCount(QModelIndex const& /*parent*/) const
{
	return static_cast<int>(_model.networkInterfacesCount());
}

QVariant NetworkInterfacesListModel::data(QModelIndex const& index, int role) const
{
	switch (role)
	{
		case Qt::DisplayRole:
		{
			if (auto const optInterface = _model.networkInterface(static_cast<std::size_t>(index.row())))
			{
				auto const& intfc = (*optInterface).get();
				return QString::fromStdString(intfc.name);
			}
			break;
		}
		case la::avdecc::utils::to_integral(QtUserRoles::ErrorRole):
		{
			if (auto const optInterface = _model.networkInterface(static_cast<std::size_t>(index.row())))
			{
				auto const& intfc = (*optInterface).get();
				return intfc.isEnabled && !intfc.isConnected;
			}
			break;
		}
		case Qt::ForegroundRole:
		{
			if (auto const optInterface = _model.networkInterface(static_cast<std::size_t>(index.row())))
			{
				// TODO: This should be moved to the errorItemDelegate, with isEnabled/isConnected as roles (so that we can change the color when an item is selected and in error state)
				auto const& intfc = (*optInterface).get();
				if (!intfc.isEnabled)
				{
					return qtMate::material::color::disabledForegroundColor();
				}
				else if (!intfc.isConnected)
				{
					return qtMate::material::color::foregroundErrorColorValue(qtMate::material::color::backgroundColorName(), qtMate::material::color::colorSchemeShade());
				}
			}
			break;
		}
		case Qt::UserRole:
		{
			if (auto const optInterface = _model.networkInterface(static_cast<std::size_t>(index.row())))
			{
				auto const& intfc = (*optInterface).get();
				return QString::fromStdString(intfc.id);
			}
			break;
		}
		case Qt::WhatsThisRole:
		{
			if (auto const optInterface = _model.networkInterface(static_cast<std::size_t>(index.row())))
			{
				auto const& intfc = (*optInterface).get();
				return QString{ "%1#%2" }.arg(la::avdecc::utils::to_integral(intfc.interfaceType)).arg(intfc.id.c_str());
			}
			break;
		}
		case Qt::DecorationRole:
		{
			if (auto const optInterface = _model.networkInterface(static_cast<std::size_t>(index.row())))
			{
				auto const& intfc = (*optInterface).get();
				return interfaceTypeIcon(intfc.interfaceType);
			}
			break;
		}
		default:
			break;
	}

	return {};
}

Qt::ItemFlags NetworkInterfacesListModel::flags(QModelIndex const& index) const
{
	if (auto const optInterface = _model.networkInterface(static_cast<std::size_t>(index.row())))
	{
		auto const& intfc = (*optInterface).get();
		if (intfc.isEnabled)
		{
			return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
		}
	}

	return {};
}

} // namespace widgetModelsLibrary
} // namespace hive
