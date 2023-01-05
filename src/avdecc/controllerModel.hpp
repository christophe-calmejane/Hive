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
#include <la/avdecc/controller/avdeccController.hpp>

namespace avdecc
{
class ControllerModelPrivate;
class ControllerModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	enum class Column
	{
		EntityLogo,
		Compatibility,
		EntityID,
		Name,
		Group,
		AcquireState,
		LockState,
		GrandmasterID,
		GptpDomain,
		InterfaceIndex,
		AssociationID,
		MediaClockMasterID,
		MediaClockMasterName,

		Count
	};

	ControllerModel(QObject* parent = nullptr);
	virtual ~ControllerModel();

	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual int columnCount(QModelIndex const& parent = {}) const override;
	virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	// Helpers
	la::avdecc::UniqueIdentifier getControlledEntityID(QModelIndex const& index) const;

	// Signals
	// Notification for when the entity is going offline, BEFORE any model notifications for removed rows
	Q_SIGNAL void entityOffline(la::avdecc::UniqueIdentifier const entityID);

private:
	QScopedPointer<ControllerModelPrivate> const d_ptr;
	Q_DECLARE_PRIVATE(ControllerModel)
};

} // namespace avdecc
