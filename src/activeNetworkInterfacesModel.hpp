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

#include <QSortFilterProxyModel>
#include <QScopedPointer>

// Model that wraps, sorts and filters an underlying NetworkInterfacesModel filtering interfaces according to the settings
class ActiveNetworkInterfacesModelPrivate;
class ActiveNetworkInterfacesModel : public QSortFilterProxyModel
{
	using QSortFilterProxyModel::setSourceModel;

public:
	ActiveNetworkInterfacesModel(QObject* parent = nullptr);
	virtual ~ActiveNetworkInterfacesModel();

	bool isEnabled(QString const& id) const noexcept;

private:
	virtual bool filterAcceptsRow(int sourceRow, QModelIndex const& sourceParent) const override;

private:
	QScopedPointer<ActiveNetworkInterfacesModelPrivate> const d_ptr;
	Q_DECLARE_PRIVATE(ActiveNetworkInterfacesModel);
};
