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

#pragma once

#include <QAbstractListModel>
#include <QScopedPointer>
#include <la/networkInterfaceHelper/networkInterfaceHelper.hpp>

// Model for Network Interface Types, load and update the associated settings
class NetworkInterfaceTypeModelPrivate;
class NetworkInterfaceTypeModel final : public QAbstractListModel
{
public:
	NetworkInterfaceTypeModel(QObject* parent = nullptr);
	virtual ~NetworkInterfaceTypeModel();

	void setActive(la::networkInterface::Interface::Type const interfaceType, bool const active);
	bool isActive(la::networkInterface::Interface::Type const interfaceType) const;

private:
	// QAbstractListModel overrides
	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual QVariant data(QModelIndex const& index, int role) const override;
	virtual bool setData(QModelIndex const& index, QVariant const& value, int role = Qt::EditRole) override;
	virtual Qt::ItemFlags flags(QModelIndex const& index) const override;

private:
	QScopedPointer<NetworkInterfaceTypeModelPrivate> const d_ptr;
	Q_DECLARE_PRIVATE(NetworkInterfaceTypeModel);
};
