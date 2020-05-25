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

#include <QAbstractListModel>
#include <QListView>
#include "avdecc/controllerManager.hpp"
#include "avdecc/mcDomainManager.hpp"

class UnassignedListModelPrivate;

//**************************************************************
//class MediaClockManagementTableModel
//**************************************************************
/**
* @brief	Implements a table model for domains and their sub entities.
* [@author  Marius Erlen]
* [@date    2018-09-21]
*
* Holds a list audio cluster nodes to display in a QTableView. Supports editing of the data.
* The data is not directly written to the controller, but stored in a map first.
* These changes can be gathered with the getChanges method.
*/
class UnassignedListModel : public QAbstractListModel
{
	Q_OBJECT
public:
	UnassignedListModel(QObject* parent = nullptr);
	~UnassignedListModel();

	QStringList mimeTypes() const override;
	QMimeData* mimeData(QModelIndexList const& indexes) const override;

	QVariant data(QModelIndex const& index, int role) const override;
	Qt::ItemFlags flags(QModelIndex const& index) const override;
	int rowCount(QModelIndex const& parent = QModelIndex()) const override;
	Qt::DropActions supportedDropActions() const override;
	bool canDropMimeData(QMimeData const* data, Qt::DropAction action, int row, int column, QModelIndex const& parent) const override;
	bool dropMimeData(QMimeData const* data, Qt::DropAction action, int row, int column, QModelIndex const& parent) override;
	bool removeRows(int row, int count, QModelIndex const& parent = QModelIndex()) override;

	void setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping const& domains);

	void removeEntity(la::avdecc::UniqueIdentifier const& entityId);
	void addEntity(la::avdecc::UniqueIdentifier const& entityId);
	QList<la::avdecc::UniqueIdentifier> getSelectedItems(QItemSelection const& itemSelection) const;
	QList<la::avdecc::UniqueIdentifier> getAllItems() const;

	// Signals
	Q_SIGNAL void domainSetupChanged();

private:
	UnassignedListModelPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(UnassignedListModel)
};
