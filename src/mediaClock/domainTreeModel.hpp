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

#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <optional>
#include "avdecc/controllerManager.hpp"
#include "avdecc/mcDomainManager.hpp"

class DomainTreeModel;
class DomainTreeModelPrivate;

//**************************************************************
//enum MediaClockManagementTableModelColumn
//**************************************************************
/**
* @brief	All columns that can be displayed.
* [@author  Marius Erlen]
* [@date    2018-10-11]
*/
enum class DomainTreeModelColumn
{
	Domain,
	MediaClockMaster
};

//**************************************************************
//class MCMasterSelectionDelegate
//**************************************************************
/**
* @brief	Implements a delegate for the mc master column.
* [@author  Marius Erlen]
* [@date    2018-10-16]
*
* Shows a label and combobox for domains and two labels for entities.
*/
class SampleRateDomainDelegate final : public QStyledItemDelegate
{
	Q_OBJECT

public:
	SampleRateDomainDelegate(QTreeView* parent = 0);
	QWidget* createEditor(QWidget* parent, QStyleOptionViewItem const& option, QModelIndex const& index) const;
	void setModelData(QWidget* editor, QAbstractItemModel* model, QModelIndex const& index) const;
	void updateEditorGeometry(QWidget* editor, QStyleOptionViewItem const& option, QModelIndex const& index) const;
	void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const;
	QSize sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const;

private:
	QTreeView* _treeView;
};

//**************************************************************
//class MCMasterSelectionDelegate
//**************************************************************
/**
* @brief	Implements a delegate for the mc master column.
* [@author  Marius Erlen]
* [@date    2018-10-16]
*
* Shows a radio button that is checked if the entity in the row is
* the mc master.
*/
class MCMasterSelectionDelegate final : public QStyledItemDelegate
{
	Q_OBJECT

public:
	MCMasterSelectionDelegate(QTreeView* parent = 0);
	void updateEditorGeometry(QWidget* editor, QStyleOptionViewItem const& option, QModelIndex const& index) const;
	void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const;
	QSize sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const;

private:
	QTreeView* _treeView;
};

//**************************************************************
//class DomainTreeModel
//**************************************************************
/**
* @brief	Implements a tree model for domains and their assigned entities.
* [@author  Marius Erlen]
* [@date    2018-10-16]
*
* Holds a tree of domains and entities. This model is assembled from a MediaClockDomains object.
* And can be converted back to by calling createMediaClockMappings.
*/
class DomainTreeModel : public QAbstractItemModel
{
	Q_OBJECT
public:
	DomainTreeModel(QObject* parent = nullptr);
	~DomainTreeModel();

	QVariant data(QModelIndex const& index, int role) const override;
	bool setData(QModelIndex const& index, QVariant const& value, int role = Qt::EditRole) override;
	Qt::ItemFlags flags(QModelIndex const& index) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column, QModelIndex const& parent = QModelIndex()) const override;
	QModelIndex parent(QModelIndex const& index) const override;
	bool removeRows(int row, int count, QModelIndex const& parent) override;
	int rowCount(QModelIndex const& parent = QModelIndex()) const override;
	int columnCount(QModelIndex const& parent) const;
	QStringList mimeTypes() const override;
	QMimeData* mimeData(const QModelIndexList& indexes) const override;
	Qt::DropActions supportedDropActions() const override;
	bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
	bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

	bool addEntityToSelection(QModelIndex const& currentIndex, la::avdecc::UniqueIdentifier const& entityId);
	bool addEntityToDomain(avdecc::mediaClock::DomainIndex const domainIndex, la::avdecc::UniqueIdentifier const& entityId);
	std::optional<avdecc::mediaClock::DomainIndex> getSelectedDomain(QModelIndex const& currentIndex) const;
	QList<QPair<avdecc::mediaClock::DomainIndex, la::avdecc::UniqueIdentifier>> getSelectedEntityItems(QItemSelection const& itemSelection) const;
	QList<avdecc::mediaClock::DomainIndex> getSelectedDomainItems(QItemSelection const& itemSelection) const;
	QPair<std::optional<avdecc::mediaClock::DomainIndex>, la::avdecc::UniqueIdentifier> getSelectedEntity(QModelIndex const& currentIndex) const;
	void removeEntity(avdecc::mediaClock::DomainIndex const domainIndex, la::avdecc::UniqueIdentifier const& entityId);
	void removeEntity(la::avdecc::UniqueIdentifier const& entityId);
	avdecc::mediaClock::DomainIndex addNewDomain();
	QList<la::avdecc::UniqueIdentifier> removeSelectedDomain(QModelIndex const& currentIndex);
	QList<la::avdecc::UniqueIdentifier> removeDomain(avdecc::mediaClock::DomainIndex domainIndex);
	QList<la::avdecc::UniqueIdentifier> removeAllDomains();
	bool isEntityDoubled(la::avdecc::UniqueIdentifier const& entityId) const;

	void setMediaClockDomainModel(avdecc::mediaClock::MCEntityDomainMapping const& domains);
	avdecc::mediaClock::MCEntityDomainMapping createMediaClockMappings();

	QModelIndex getDomainModelIndex(avdecc::mediaClock::DomainIndex domainIndex) const;

	// Slots
	void handleClick(QModelIndex const& current, QModelIndex const& previous);

	// Signals
	Q_SIGNAL void domainSetupChanged();
	Q_SIGNAL void expandDomain(QModelIndex const& domainModelIndex);
	Q_SIGNAL void triggerResizeColumns();
	Q_SIGNAL void deselectAll();

protected:
private:
	DomainTreeModelPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(DomainTreeModel)
};
