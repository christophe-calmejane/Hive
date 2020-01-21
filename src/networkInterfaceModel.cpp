/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "networkInterfaceModel.hpp"
#include "toolkit/material/color.hpp"
#include "avdecc/helper.hpp"
#include "errorItemDelegate.hpp"

bool NetworkInterfaceListModel::isEnabled(QString const& id) const noexcept
{
	auto const optInterface = _model.networkInterface(id.toStdString());

	if (optInterface)
	{
		auto const& intfc = (*optInterface).get();
		return intfc.isEnabled;
	}

	return false;
}

la::avdecc::networkInterface::Interface::Type NetworkInterfaceListModel::getInterfaceType(QModelIndex const& index) const noexcept
{
	auto const optInterface = _model.networkInterface(index);

	if (optInterface)
	{
		auto const& intfc = (*optInterface).get();
		return intfc.interfaceType;
	}

	return la::avdecc::networkInterface::Interface::Type::None;
}

// hive::modelsLibrary::NetworkInterfaceAbstractListModel overrides
void NetworkInterfaceListModel::nameChanged(QModelIndex const& index, std::string const& name) noexcept
{
	emit dataChanged(index, index, { Qt::DisplayRole });
}
void NetworkInterfaceListModel::enabledStateChanged(QModelIndex const& index, bool const isEnabled) noexcept
{
	emit dataChanged(index, index, { Qt::DisplayRole });
}

void NetworkInterfaceListModel::connectedStateChanged(QModelIndex const& index, bool const isConnected) noexcept
{
	emit dataChanged(index, index, { Qt::DisplayRole });
}

// QAbstractListModel overrides
int NetworkInterfaceListModel::rowCount(QModelIndex const& /*parent*/) const
{
	return _model.rowCount();
}

QVariant NetworkInterfaceListModel::data(QModelIndex const& index, int role) const
{
	switch (role)
	{
		case Qt::DisplayRole:
		{
			auto const optInterface = _model.networkInterface(index);
			if (optInterface)
			{
				auto const& intfc = (*optInterface).get();
				return QString::fromStdString(intfc.name);
			}
		}
		case ErrorItemDelegate::ErrorRole: // TODO -> Define this role in NetworkInterfaceModel directly (probably another name !)
		{
			auto const optInterface = _model.networkInterface(index);
			if (optInterface)
			{
				auto const& intfc = (*optInterface).get();
				return intfc.isEnabled && !intfc.isConnected;
			}
		}
		case Qt::ForegroundRole:
		{
			auto const optInterface = _model.networkInterface(index);
			if (optInterface)
			{
				auto const& intfc = (*optInterface).get();
				if (!intfc.isEnabled)
				{
					return qt::toolkit::material::color::value(qt::toolkit::material::color::Name::Gray);
				}
				else if (!intfc.isConnected)
				{
					//return qt::toolkit::material::color::foregroundErrorColorValue(colorName, qt::toolkit::material::color::Shade::ShadeA700);
					return qt::toolkit::material::color::foregroundErrorColorValue(qt::toolkit::material::color::DefaultColor, qt::toolkit::material::color::DefaultShade); // Right now, always use default value, as we draw on white background
				}
				else
				{
					return QColor{ Qt::black };
				}
			}
		}
		case Qt::UserRole:
		{
			auto const optInterface = _model.networkInterface(index);
			if (optInterface)
			{
				auto const& intfc = (*optInterface).get();
				return QString::fromStdString(intfc.id);
			}
		}
		case Qt::WhatsThisRole:
		{
			auto const optInterface = _model.networkInterface(index);
			if (optInterface)
			{
				auto const& intfc = (*optInterface).get();
				return QString{ "%1#%2" }.arg(la::avdecc::utils::to_integral(intfc.interfaceType)).arg(intfc.id.c_str());
			}
		}
		case Qt::DecorationRole:
		{
			auto const optInterface = _model.networkInterface(index);
			if (optInterface)
			{
				auto const& intfc = (*optInterface).get();
				return avdecc::helper::interfaceTypeIcon(intfc.interfaceType);
			}
		}
	}

	return {};
}

Qt::ItemFlags NetworkInterfaceListModel::flags(QModelIndex const& index) const
{
	auto const optInterface = _model.networkInterface(index);

	if (optInterface)
	{
		auto const& intfc = (*optInterface).get();
		if (intfc.isEnabled)
		{
			return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
		}
	}

	return {};
}
