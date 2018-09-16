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

#include <QAbstractProxyModel>

namespace qt
{
namespace toolkit
{

class TransposeProxyModel: public QAbstractProxyModel
{
	using QAbstractProxyModel::setSourceModel;

public:
	TransposeProxyModel(QObject* parent = nullptr)
		: QAbstractProxyModel{parent}
	{
	}

	Qt::Orientation mapToSource(Qt::Orientation const orientation) const
	{
		return orientation == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal;
	}

	Qt::Orientation mapFromSource(Qt::Orientation const orientation) const
	{
		return mapToSource(orientation);
	}

	virtual QModelIndex mapFromSource(QModelIndex const& sourceIndex) const override
	{
		return index(sourceIndex.column(), sourceIndex.row());
	}

	virtual QModelIndex mapToSource(QModelIndex const& proxyIndex) const override
	{
		return sourceModel()->index(proxyIndex.column(), proxyIndex.row());
	}

	virtual QModelIndex index(int row, int column, QModelIndex const& = {}) const override
	{
		return createIndex(row, column);
	}

	virtual QModelIndex parent(QModelIndex const&) const override
	{
		return QModelIndex();
	}

	virtual int rowCount(QModelIndex const&) const override
	{
		return sourceModel()->columnCount();
	}

	virtual int columnCount(QModelIndex const&) const override
	{
		return sourceModel()->rowCount();
	}

	virtual QVariant data(QModelIndex const& index, int role) const override
	{
		return sourceModel()->data(mapToSource(index), role);
	}

	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override
	{
		return sourceModel()->headerData(section, orientation == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal, role);
	}

	void connectToModel(QAbstractItemModel* model)
	{
		setSourceModel(model);

		connect(model, &QAbstractItemModel::dataChanged, this, &TransposeProxyModel::_dataChanged);
		connect(model, &QAbstractItemModel::headerDataChanged, this, &TransposeProxyModel::_headerDataChanged);
		connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, &TransposeProxyModel::_rowsAboutToBeInserted);
		connect(model, &QAbstractItemModel::rowsInserted, this, &TransposeProxyModel::_rowsInserted);
		connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &TransposeProxyModel::_rowsAboutToBeRemoved);
		connect(model, &QAbstractItemModel::rowsRemoved, this, &TransposeProxyModel::_rowsRemoved);
		connect(model, &QAbstractItemModel::columnsAboutToBeInserted, this, &TransposeProxyModel::_columnsAboutToBeInserted);
		connect(model, &QAbstractItemModel::columnsInserted, this, &TransposeProxyModel::_columnsInserted);
		connect(model, &QAbstractItemModel::columnsAboutToBeRemoved, this, &TransposeProxyModel::_columnsAboutToBeRemoved);
		connect(model, &QAbstractItemModel::columnsRemoved, this, &TransposeProxyModel::_columnsRemoved);
		connect(model, &QAbstractItemModel::modelReset, this, &TransposeProxyModel::_modelReset);
		connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, &TransposeProxyModel::_layoutAboutToBeChanged);
		connect(model, &QAbstractItemModel::layoutChanged, this, &TransposeProxyModel::_layoutChanged);
	}

	void disconnectFromModel(QAbstractItemModel* model)
	{
		disconnect(model, &QAbstractItemModel::dataChanged, this, &TransposeProxyModel::_dataChanged);
		disconnect(model, &QAbstractItemModel::headerDataChanged, this, &TransposeProxyModel::_headerDataChanged);
		disconnect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, &TransposeProxyModel::_rowsAboutToBeInserted);
		disconnect(model, &QAbstractItemModel::rowsInserted, this, &TransposeProxyModel::_rowsInserted);
		disconnect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &TransposeProxyModel::_rowsAboutToBeRemoved);
		disconnect(model, &QAbstractItemModel::rowsRemoved, this, &TransposeProxyModel::_rowsRemoved);
		disconnect(model, &QAbstractItemModel::columnsAboutToBeInserted, this, &TransposeProxyModel::_columnsAboutToBeInserted);
		disconnect(model, &QAbstractItemModel::columnsInserted, this, &TransposeProxyModel::_columnsInserted);
		disconnect(model, &QAbstractItemModel::columnsAboutToBeRemoved, this, &TransposeProxyModel::_columnsAboutToBeRemoved);
		disconnect(model, &QAbstractItemModel::columnsRemoved, this, &TransposeProxyModel::_columnsRemoved);
		disconnect(model, &QAbstractItemModel::modelReset, this, &TransposeProxyModel::_modelReset);
		disconnect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, &TransposeProxyModel::_layoutAboutToBeChanged);
		disconnect(model, &QAbstractItemModel::layoutChanged, this, &TransposeProxyModel::_layoutChanged);
	}

private:
	Q_SLOT void _dataChanged(QModelIndex const& topLeft, QModelIndex const& bottomRight, QVector<int> const& roles)
	{
		emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
	}

	Q_SLOT void _headerDataChanged(Qt::Orientation orientation, int first, int last)
	{
		emit headerDataChanged(mapFromSource(orientation), first, last);
	}

	Q_SLOT void _rowsAboutToBeInserted(QModelIndex const& parent, int first, int last)
	{
		emit columnsAboutToBeInserted(mapFromSource(parent), first, last, {});
	}

	Q_SLOT void _rowsInserted(QModelIndex const& parent, int first, int last)
	{
		emit columnsInserted(mapFromSource(parent), first, last, {});
	}

	Q_SLOT void _rowsAboutToBeRemoved(QModelIndex const& parent, int first, int last)
	{
		emit columnsAboutToBeRemoved(mapFromSource(parent), first, last, {});
	}

	Q_SLOT void _rowsRemoved(QModelIndex const& parent, int first, int last)
	{
		emit columnsRemoved(mapFromSource(parent), first, last, {});
	}

	Q_SLOT void _columnsAboutToBeInserted(QModelIndex const& parent, int first, int last)
	{
		emit rowsAboutToBeInserted(mapFromSource(parent), first, last, {});
	}

	Q_SLOT void _columnsInserted(QModelIndex const& parent, int first, int last)
	{
		emit rowsInserted(mapFromSource(parent), first, last, {});
	}

	Q_SLOT void _columnsAboutToBeRemoved(QModelIndex const& parent, int first, int last)
	{
		emit rowsAboutToBeRemoved(mapFromSource(parent), first, last, {});
	}

	Q_SLOT void _columnsRemoved(QModelIndex const& parent, int first, int last)
	{
		emit rowsRemoved(mapFromSource(parent), first, last, {});
	}

	Q_SLOT void _modelAboutToBeReset()
	{
		emit modelAboutToBeReset({});
	}

	Q_SLOT void _modelReset()
	{
		emit modelReset({});
	}

	Q_SLOT void _rowsAboutToBeMoved(QModelIndex const& sourceParent, int sourceStart, int sourceEnd, QModelIndex const& destinationParent, int destinationRow)
	{
		emit columnsAboutToBeMoved(mapFromSource(sourceParent), sourceStart, sourceEnd, mapFromSource(destinationParent), destinationRow, {});
	}

	Q_SLOT void _rowsMoved(QModelIndex const& parent, int start, int end, QModelIndex const& destination, int row)
	{
		emit columnsMoved(mapFromSource(parent), start, end, mapFromSource(destination), row, {});
	}

	Q_SLOT void _columnsAboutToBeMoved(QModelIndex const& sourceParent, int sourceStart, int sourceEnd, QModelIndex const& destinationParent, int destinationColumn)
	{
		emit rowsAboutToBeMoved(mapFromSource(sourceParent), sourceStart, sourceEnd, mapFromSource(destinationParent), destinationColumn, {});
	}

	Q_SLOT void _columnsMoved(QModelIndex const& parent, int start, int end, QModelIndex const& destination, int column)
	{
		emit rowsMoved(mapFromSource(parent), start, end, mapFromSource(destination), column, {});
	}

	Q_SLOT void _layoutChanged(const QList<QPersistentModelIndex>& parents, QAbstractItemModel::LayoutChangeHint hint)
	{
		QList<QPersistentModelIndex> proxyParents;
		for (auto const& parent : parents)
		{
			proxyParents << mapFromSource(parent);
		}

		emit layoutChanged(proxyParents, hint);
	}

	Q_SLOT void _layoutAboutToBeChanged(QList<QPersistentModelIndex> const& parents, QAbstractItemModel::LayoutChangeHint hint)
	{
		QList<QPersistentModelIndex> proxyParents;
		for (auto const& parent : parents)
		{
			proxyParents << mapFromSource(parent);
		}

		emit layoutAboutToBeChanged(proxyParents, hint);
	}
};

} // namespace toolkit
} // namespace qt

