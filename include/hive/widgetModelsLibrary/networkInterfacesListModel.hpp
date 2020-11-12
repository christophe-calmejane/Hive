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

#pragma once

#include <hive/modelsLibrary/networkInterfacesModel.hpp>

#include <QIcon>

namespace hive
{
namespace widgetModelsLibrary
{
class NetworkInterfacesListModel : public hive::modelsLibrary::NetworkInterfacesAbstractListModel
{
public:
	bool isEnabled(QString const& id) const noexcept;
	la::avdecc::networkInterface::Interface::Type getInterfaceType(QModelIndex const& index) const noexcept;

	static QIcon interfaceTypeIcon(la::avdecc::networkInterface::Interface::Type const type) noexcept;

private:
	// hive::modelsLibrary::NetworkInterfacesAbstractListModel overrides
	virtual void nameChanged(std::size_t const index, std::string const& name) noexcept override;
	virtual void enabledStateChanged(std::size_t const index, bool const isEnabled) noexcept override;
	virtual void connectedStateChanged(std::size_t const index, bool const isConnected) noexcept override;

	// QAbstractListModel overrides
	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual QVariant data(QModelIndex const& index, int role) const override;
	virtual Qt::ItemFlags flags(QModelIndex const& index) const override;

	// Private members
	hive::modelsLibrary::NetworkInterfacesModel _model{ this };
};
} // namespace widgetModelsLibrary
} // namespace hive
