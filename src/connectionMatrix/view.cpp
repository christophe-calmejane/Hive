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

#include "connectionMatrix/view.hpp"
#include "connectionMatrix/model.hpp"

#include <QHeaderView>
#include <QPainter>

#include "avdecc/helper.hpp"
#include <QDebug>

#include <QAbstractProxyModel>

namespace connectionMatrix
{

class HeaderView : public QHeaderView
{
public:
	HeaderView(Qt::Orientation orientation, QWidget* parent = nullptr)
		: QHeaderView(orientation, parent)
	{
		setDefaultSectionSize(50);
		setSectionResizeMode(QHeaderView::Fixed);
		setSectionsClickable(true);

		connect(this, &QHeaderView::sectionClicked, this, [this](int const logicalIndex)
		{
			qDebug() << model()->headerData(logicalIndex, this->orientation(), Model::HeaderTypeRole).toInt();
		});
	}

	virtual void paintSection(QPainter* painter, QRect const& rect, int logicalIndex) const override
	{
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);

		auto const arrowSize{ 10 };
		auto const headerType = model()->headerData(logicalIndex, orientation(), Model::HeaderTypeRole).toInt();
		auto const arrowOffset{ 25 * headerType };

		QPainterPath path;
		if (orientation() == Qt::Horizontal)
		{
			path.moveTo(rect.topLeft());
			path.lineTo(rect.bottomLeft() - QPoint{ 0, arrowSize + arrowOffset });
			path.lineTo(rect.center() + QPoint{ 0, rect.height() / 2 - arrowOffset });
			path.lineTo(rect.bottomRight() - QPoint{ 0, arrowSize + arrowOffset });
			path.lineTo(rect.topRight());
		}
		else
		{
			path.moveTo(rect.topLeft());
			path.lineTo(rect.topRight() - QPoint{ arrowSize + arrowOffset, 0 });
			path.lineTo(rect.center() + QPoint{ rect.width() / 2 - arrowOffset, 0 });
			path.lineTo(rect.bottomRight() - QPoint{ arrowSize + arrowOffset, 0 });
			path.lineTo(rect.bottomLeft());
		}

		QBrush backgroundBrush{};
		switch (headerType)
		{
			case 0:
				backgroundBrush = QColor{ "#4A148C" };
				break;
			case 1:
				backgroundBrush = QColor{ "#7B1FA2" };
				break;
			case 2:
				backgroundBrush = QColor{ "#BA68C8" };
				break;
			default:
				backgroundBrush = QColor{ "#808080" };
				break;
		}

		painter->fillPath(path, backgroundBrush);
		painter->translate(rect.topLeft());

		auto r = QRect(0, 0, rect.width(), rect.height());
		if (orientation() == Qt::Horizontal)
		{
			r.setWidth(rect.height());
			r.setHeight(rect.width());

			painter->rotate(-90);
			painter->translate(-r.width(), 0);

			r.translate(arrowSize + arrowOffset, 0);
		}

		auto const padding{ 4 };
		auto textRect = r.adjusted(padding, 0, -(padding + arrowSize + arrowOffset), 0);

		auto const text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
		auto const elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, textRect.width());

		painter->setPen(Qt::white);
		painter->drawText(textRect, Qt::AlignVCenter, elidedText);
		painter->restore();
	}

	virtual QSize sizeHint() const override
	{
		if (orientation() == Qt::Horizontal)
		{
			return QSize(20, 200);
		}
		else
		{
			return QSize(200, 20);
		}
	}
};


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

View::View(QWidget* parent)
	: QTableView{parent}
	, _model{std::make_unique<Model>()} {

	auto* tm = new TransposeProxyModel;
	tm->connectToModel(_model.get());
	setModel(tm);

	setVerticalHeader(new HeaderView{Qt::Vertical, this});
	setHorizontalHeader(new HeaderView{Qt::Horizontal, this});
	setSelectionMode(QAbstractItemView::NoSelection);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
}

} // namespace connectionMatrix
