/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <QAbstractTableModel>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QTableView>

#include "avdecc/controllerManager.hpp"
#include "avdecc/channelConnectionManager.hpp"

class DeviceDetailsChannelTableModelPrivate;

//**************************************************************
//enum DeviceDetailsChannelTableModelColumn
//**************************************************************
/**
* @brief	All columns that can be displayed.
* [@author  Marius Erlen]
* [@date    2018-10-11]
*/
enum class DeviceDetailsChannelTableModelColumn
{
	//ChannelNumber,
	ChannelName,
	ConnectionStatus,
	Connection,

};

//**************************************************************
//class TableRowEntry
//**************************************************************
/**
* @brief	Helper struct. Holds all data needed to to display a table row.
* [@author  Marius Erlen]
* [@date    2018-10-11]
*/
struct TableRowEntry
{
	/**
	* Constructor.
	*/
	TableRowEntry(avdecc::ConnectionInformation connectionInformation)
	{
		this->connectionInformation = connectionInformation;
	}

	avdecc::ConnectionInformation connectionInformation;
};

//**************************************************************
//class ConnectionStateItemDelegate
//**************************************************************
/**
* @brief	Implements a delegate to display a connection state icon
*			inside the table.
* [@author  Marius Erlen]
* [@date    2018-10-11]
*/
class ConnectionStateItemDelegate final : public QAbstractItemDelegate
{
	Q_OBJECT

public:
private:
	// QAbstractItemDelegate overrides
	virtual void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override;
	virtual QSize sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const override;
};

//**************************************************************
//class DeviceDetailsChannelTableModel
//**************************************************************
/**
* @brief	Implements a table model for both rx and tx channels.
* [@author  Marius Erlen]
* [@date    2018-09-21]
*
* Holds a list audio cluster nodes to display in a QTableView. Supports editing of the data.
* The data is not directly written to the controller, but stored in a map first.
* These changes can be gathered with the getChanges method.
*/
class DeviceDetailsChannelTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DeviceDetailsChannelTableModel(QObject* parent = nullptr);
	~DeviceDetailsChannelTableModel();

	enum class ConnectionStatus
	{
		None = 0,
		WrongDomain = 1u << 0,
		WrongFormat = 1u << 1,
		Connectable = 1u << 2, /**< Stream connectable (might be connected, or not) */
		Connected = 1u << 3, /**< Stream is connected (Mutually exclusive with FastConnecting and PartiallyConnected) */
		FastConnecting = 1u << 4, /**< Stream is fast connecting (Mutually exclusive with Connected and PartiallyConnected) */
		PartiallyConnected = 1u << 5, /**< Some, but not all of a redundant streams tuple, are connected (Mutually exclusive with Connected and FastConnecting) */
	};

	virtual int rowCount(QModelIndex const& parent = QModelIndex()) const override;
	virtual int columnCount(QModelIndex const& parent = QModelIndex()) const override;
	virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags flags(QModelIndex const& index) const override;

	void addNode(avdecc::ConnectionInformation const& connectionInformation);
	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsChannelTableModelColumn, QVariant>*> getChanges() const;
	void resetChangedData();
	void removeAllNodes();
	void channelConnectionsUpdate(const la::avdecc::UniqueIdentifier& entityId);
	void updateAudioClusterName(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& audioClusterName);

	Q_SIGNAL void dataEdited();

protected:
	TableRowEntry const& tableDataAtRow(int row) const;

private:
	DeviceDetailsChannelTableModelPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(DeviceDetailsChannelTableModel)

	friend class ConnectionStateItemDelegate;
};


// Define bitfield enum traits for ConnectionStatus
template<>
struct la::avdecc::utils::enum_traits<DeviceDetailsChannelTableModel::ConnectionStatus>
{
	static constexpr bool is_bitfield = true;
};
