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

#include <hive/modelsLibrary/networkInterfaceModel.hpp>

class NetworkInterfaceListModel : public hive::modelsLibrary::NetworkInterfaceAbstractListModel
{
public:
	bool isEnabled(QString const& id) const noexcept;
	la::avdecc::networkInterface::Interface::Type getInterfaceType(QModelIndex const& index) const noexcept;

private:
	// hive::modelsLibrary::NetworkInterfaceAbstractListModel overrides
	virtual void nameChanged(QModelIndex const& index, std::string const& name) noexcept override;
	virtual void enabledStateChanged(QModelIndex const& index, bool const isEnabled) noexcept override;
	virtual void connectedStateChanged(QModelIndex const& index, bool const isConnected) noexcept override;

	// QAbstractListModel overrides
	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual QVariant data(QModelIndex const& index, int role) const override;
	virtual Qt::ItemFlags flags(QModelIndex const& index) const override;

	// Private members
	hive::modelsLibrary::NetworkInterfaceModel _model{ this };
};
