/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QtMate/material/color.hpp>
#include <QtMate/material/helper.hpp>

#include <unordered_map>

namespace hive
{
namespace widgetModelsLibrary
{
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
	static std::unordered_map<la::networkInterface::Interface::Type, QIcon> s_icon;

	auto const it = s_icon.find(type);
	if (it == std::end(s_icon))
	{
		auto what = QString{};

		switch (type)
		{
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

		s_icon[type] = qtMate::material::helper::generateIcon(what);
	}

	return s_icon[type];
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
		case ErrorItemDelegate::ErrorRole: // Make use of this role if ErrorItemDelegate is defined as ItemDelegate
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
				auto const& intfc = (*optInterface).get();
				if (!intfc.isEnabled)
				{
					return qtMate::material::color::value(qtMate::material::color::Name::Gray);
				}
				else if (!intfc.isConnected)
				{
					//return qtMate::material::color::foregroundErrorColorValue(colorName, qtMate::material::color::Shade::ShadeA700);
					return qtMate::material::color::foregroundErrorColorValue(qtMate::material::color::DefaultColor, qtMate::material::color::DefaultShade); // Right now, always use default value, as we draw on white background
				}
				else
				{
					return QColor{ Qt::black };
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
