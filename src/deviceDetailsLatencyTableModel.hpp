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

#include "latencyItemDelegate.hpp"

#include <QAbstractTableModel>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QTableView>

#include <chrono>
#include <string>

#include <hive/modelsLibrary/controllerManager.hpp>

class DeviceDetailsLatencyTableModelPrivate;

//**************************************************************
//enum DeviceDetailsLatencyTableModelColumn
//**************************************************************
/**
* @brief	All columns that can be displayed.
*/
enum class DeviceDetailsLatencyTableModelColumn
{
	StreamName,
	Latency,

};

//**************************************************************
//class DeviceDetailsLatencyTableModel
//**************************************************************
/**
* @brief    Implementation of the table model for displaying/modifying
* 			latency per device talker stream.
*/
class DeviceDetailsLatencyTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DeviceDetailsLatencyTableModel(QObject* parent = nullptr);
	virtual ~DeviceDetailsLatencyTableModel() override;

	virtual int rowCount(QModelIndex const& parent = QModelIndex()) const override;
	virtual int columnCount(QModelIndex const& parent = QModelIndex()) const override;
	virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags flags(QModelIndex const& index) const override;

	void setControlledEntityID(la::avdecc::UniqueIdentifier const entityID);
	la::avdecc::UniqueIdentifier getControlledEntityID() const;
	void addNode(la::avdecc::entity::model::StreamIndex const streamIndex, std::chrono::nanoseconds const latency);
	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsLatencyTableModelColumn, QVariant>*> getChanges() const;
	void resetChangedData();
	void removeAllNodes();

	Q_SIGNAL void dataEdited();

protected:
	LatencyTableRowEntry const& tableDataAtRow(int row) const;

private:
	DeviceDetailsLatencyTableModelPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(DeviceDetailsLatencyTableModel)

	friend class LatencyItemDelegate;
};
