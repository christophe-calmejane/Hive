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

#include <QAbstractTableModel>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QTableView>

#include <hive/modelsLibrary/controllerManager.hpp>

class DeviceDetailsStreamFormatTableModelPrivate;

//**************************************************************
//enum DeviceDetailsStreamFormatTableModelColumn
//**************************************************************
/**
* @brief	All columns that can be displayed.
* [@author  Christian Ahrens]
* [@date    2021-05-27]
*/
enum class DeviceDetailsStreamFormatTableModelColumn
{
	StreamName,
	StreamFormat,

};

//**************************************************************
//class StreamFormatTableRowEntry
//**************************************************************
/**
* @brief	Helper struct. Holds all data needed to to display a table row.
* [@author  Christian Ahrens]
* [@date    2021-05-27]
*/
struct StreamFormatTableRowEntry
{
	/**
	* Default Constructor.
	*/
	StreamFormatTableRowEntry()
	{
		this->streamType = la::avdecc::entity::model::DescriptorType{};
		this->streamIndex = la::avdecc::entity::model::StreamIndex{};
		this->streamFormat = la::avdecc::entity::model::StreamFormat{};
	}

	/**
	* Constructor.
	*/
	StreamFormatTableRowEntry(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::DescriptorType const streamType, la::avdecc::entity::model::StreamFormat const streamFormat)
	{
		this->streamType = streamType;
		this->streamIndex = streamIndex;
		this->streamFormat = streamFormat;
	}

	static constexpr size_t size()
	{
		return sizeof(streamIndex) + sizeof(streamType) + sizeof(streamFormat);
	}
	constexpr friend bool operator==(StreamFormatTableRowEntry const& lhs, StreamFormatTableRowEntry const& rhs) noexcept
	{
		return (lhs.streamIndex == rhs.streamIndex) && (lhs.streamType == rhs.streamType) && (lhs.streamFormat == rhs.streamFormat);
	}
	constexpr friend bool operator!=(StreamFormatTableRowEntry const& lhs, StreamFormatTableRowEntry const& rhs) noexcept
	{
		return !operator==(lhs, rhs);
	}

	la::avdecc::entity::model::StreamIndex streamIndex;
	la::avdecc::entity::model::DescriptorType streamType;
	la::avdecc::entity::model::StreamFormat streamFormat;
};
Q_DECLARE_METATYPE(StreamFormatTableRowEntry)

//**************************************************************
//class StreamFormatItemDelegate
//**************************************************************
/**
* @brief	Implements a delegate to display the stream format
*			dropdown inside the table cell.
* [@author  Christian Ahrens]
* [@date    2021-05-27]
*/
class StreamFormatItemDelegate final : public QStyledItemDelegate
{
	Q_OBJECT

public:
	using QStyledItemDelegate::QStyledItemDelegate;

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void setEditorData(QWidget* editor, const QModelIndex& index) const override;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

private:
};

//**************************************************************
//class DeviceDetailsStreamFormatTableModel
//**************************************************************
/**
* @brief    Implementation of the table model for displaying/modifying
* 			stream formats per device talker/listener stream.
* [@author  Christian Ahrens]
* [@date    2021-05-27]
*
* 
*/
class DeviceDetailsStreamFormatTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	DeviceDetailsStreamFormatTableModel(QObject* parent = nullptr);
	~DeviceDetailsStreamFormatTableModel();

	virtual int rowCount(QModelIndex const& parent = QModelIndex()) const override;
	virtual int columnCount(QModelIndex const& parent = QModelIndex()) const override;
	virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags flags(QModelIndex const& index) const override;

	void setControlledEntityID(la::avdecc::UniqueIdentifier const entityID);
	la::avdecc::UniqueIdentifier getControlledEntityID() const;
	void addNode(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::DescriptorType const streamType, la::avdecc::entity::model::StreamFormat const streamFormat);
	QMap<la::avdecc::entity::model::DescriptorIndex, QMap<DeviceDetailsStreamFormatTableModelColumn, QVariant>*> getChanges() const;
	void resetChangedData();
	void removeAllNodes();

	Q_SIGNAL void dataEdited();

protected:
	StreamFormatTableRowEntry const& tableDataAtRow(int row) const;

private:
	DeviceDetailsStreamFormatTableModelPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(DeviceDetailsStreamFormatTableModel)

	friend class StreamFormatItemDelegate;
};
