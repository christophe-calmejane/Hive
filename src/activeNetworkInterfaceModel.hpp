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

#pragma once

#include <QSortFilterProxyModel>
#include <QScopedPointer>

// Model that wrap and filter an underlying NetworkInterfaceModel filtering interfaces according to the settings
class ActiveNetworkInterfaceModelPrivate;
class ActiveNetworkInterfaceModel : public QSortFilterProxyModel
{
	using QSortFilterProxyModel::setSourceModel;

public:
	ActiveNetworkInterfaceModel(QObject* parent = nullptr);
	virtual ~ActiveNetworkInterfaceModel();

	bool isEnabled(QString const& id) const noexcept;

private:
	virtual bool filterAcceptsRow(int sourceRow, QModelIndex const& sourceParent) const override;

private:
	QScopedPointer<ActiveNetworkInterfaceModelPrivate> const d_ptr;
	Q_DECLARE_PRIVATE(ActiveNetworkInterfaceModel);
};
