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

#include "connectionMatrix.hpp"
#include "matrixTreeView.hpp"

#include <vector>
#include <cassert>
#include <memory>
#include <functional>
#include <algorithm>
#include <QHeaderView>
#include <QPainter>
#include <QMenu>

namespace qt
{
namespace toolkit
{

class MatrixModel::MatrixModelPrivate
{
public:

	MatrixModelPrivate(MatrixModel* q) : q_ptr(q) {}

	int indexForUserData(std::vector<std::unique_ptr<Node>> const& nodes, QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept;
	std::pair<int, Node const*> indexAndNodeForUserData(std::vector<std::unique_ptr<Node>> const& nodes, QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept;

protected:
	MatrixModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(MatrixModel);

	std::vector<std::unique_ptr<Node>>  _vNodes{}; // Rows
	Node _vRootNode{ nullptr }; // Root vertical node

	std::vector<std::unique_ptr<Node>>  _hNodes{}; // Columns
	Node _hRootNode{ nullptr }; // Root horizontal node
};

int MatrixModel::MatrixModelPrivate::indexForUserData(std::vector<std::unique_ptr<Node>> const& nodes, QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept
{
	auto const result = indexAndNodeForUserData(nodes, userData, comparisonFunction);
	return result.first;
}

std::pair<int, MatrixModel::Node const*> MatrixModel::MatrixModelPrivate::indexAndNodeForUserData(std::vector<std::unique_ptr<Node>> const& nodes, QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept
{
	int index = 0;

	for (auto const& node : nodes)
	{
		if (comparisonFunction(node->userData, userData))
			return std::make_pair(index, node.get());
		++index;
	}
	return std::make_pair(-1, nullptr);
}

MatrixModel::MatrixModel(QObject* parent)
	: QAbstractTableModel(parent)
	, d_ptr(new MatrixModelPrivate(this))
{
}

MatrixModel::~MatrixModel()
{
	delete d_ptr;
}

int MatrixModel::rowCount(QModelIndex const& parent) const
{
	Q_D(const MatrixModel);
	return static_cast<int>(d->_vNodes.size());
}

int MatrixModel::columnCount(QModelIndex const& parent) const
{
	Q_D(const MatrixModel);
	return static_cast<int>(d->_hNodes.size());
}

QVariant MatrixModel::data(QModelIndex const& index, int role) const
{
	switch (role)
	{
		case Qt::BackgroundRole:
			return QColor{Qt::blue};
		default:
			return {};
	}
}

void MatrixModel::beginAppendRows(QModelIndex const& parent, int count)
{
	Q_D(MatrixModel);

	auto const currentCount = d->_vNodes.size();
	beginInsertRows(parent, currentCount, currentCount + count - 1);
}

std::pair<QModelIndex, MatrixModel::Node&> MatrixModel::appendRow(QModelIndex const& parent)
{
	Q_D(MatrixModel);

	AVDECC_ASSERT(parent.column() == -1, "addRow: parent.column must be -1");
	MatrixModel::Node* parentNode{ nullptr };
	if (parent.row() != -1)
	{
		parentNode = nodeAtRow(parent.row());
		AVDECC_ASSERT(parentNode, "Should have a parent at this row");
	}

	auto const row = d->_vNodes.size();
	d->_vNodes.push_back(std::make_unique<MatrixModel::Node>(parentNode));
	auto& node = d->_vNodes.back();

	if (parentNode)
	{
		parentNode->children.push_back(node.get());
	}
	else
	{
		d->_vRootNode.children.push_back(node.get());
	}

	return std::pair<QModelIndex, MatrixModel::Node&>{createIndex(row, -1, nullptr), *node};
}

void MatrixModel::endAppendRows()
{
	endInsertRows();
}

bool MatrixModel::removeRows(int row, int count, QModelIndex const& parent)
{
	Q_D(MatrixModel);

	auto const len = d->_vNodes.size();
	auto const lastIndex = row + count - 1;
	if (static_cast<decltype(len)>(lastIndex) >= len)
		return false;

	beginRemoveRows(parent, row, lastIndex);
	auto const beginIt = d->_vNodes.begin() + row;
	auto const endIt = beginIt + count;
	d->_vNodes.erase(beginIt, endIt);
	endRemoveRows();

	return true;
}

void MatrixModel::beginAppendColumns(QModelIndex const& parent, int count)
{
	Q_D(MatrixModel);

	auto const currentCount = d->_hNodes.size();
	beginInsertColumns(parent, currentCount, currentCount + count - 1);
}

std::pair<QModelIndex, MatrixModel::Node&> MatrixModel::appendColumn(QModelIndex const& parent)
{
	Q_D(MatrixModel);

	AVDECC_ASSERT(parent.row() == -1, "addColumn: parent.row must be -1");
	MatrixModel::Node* parentNode{ nullptr };
	if (parent.column() != -1)
	{
		parentNode = nodeAtColumn(parent.column());
		AVDECC_ASSERT(parentNode, "Should have a parent at this column");
	}

	auto const column = d->_hNodes.size();
	d->_hNodes.push_back(std::make_unique<MatrixModel::Node>(parentNode));
	auto& node = d->_hNodes.back();

	if (parentNode)
	{
		parentNode->children.push_back(node.get());
	}
	else
	{
		d->_hRootNode.children.push_back(node.get());
	}

	return std::pair<QModelIndex, MatrixModel::Node&>{createIndex(-1, column, nullptr), *node};
}

void MatrixModel::endAppendColumns()
{
	endInsertColumns();
}

bool MatrixModel::removeColumns(int column, int count, QModelIndex const& parent)
{
	Q_D(MatrixModel);

	auto const len = d->_hNodes.size();
	auto const lastIndex = column + count - 1;
	if (static_cast<decltype(len)>(lastIndex) >= len)
		return false;

	beginRemoveRows(parent, column, lastIndex);
	auto const beginIt = d->_hNodes.begin() + column;
	auto const endIt = beginIt + count;
	d->_hNodes.erase(beginIt, endIt);
	endRemoveColumns();

	return true;
}

void MatrixModel::clearModel() noexcept
{
	Q_D(MatrixModel);

	beginResetModel();

	d->_vNodes.clear();
	d->_vRootNode = nullptr;

	d->_hNodes.clear();
	d->_hRootNode = nullptr;

	endResetModel();
}

MatrixModel::Node* MatrixModel::nodeAtRow(int const row) const noexcept
{
	Q_D(const MatrixModel);

	try
	{
		return d->_vNodes.at(row).get();
	}
	catch (...)
	{
		return nullptr;
	}
}

MatrixModel::Node* MatrixModel::nodeAtColumn(int const column) const noexcept
{
	Q_D(const MatrixModel);

	try
	{
		return d->_hNodes.at(column).get();
	}
	catch (...)
	{
		return nullptr;
	}
}

int MatrixModel::rowForUserData(QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept
{
	Q_D(const MatrixModel);

	return d->indexForUserData(d->_vNodes, userData, comparisonFunction);
}

int MatrixModel::columnForUserData(QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept
{
	Q_D(const MatrixModel);

	return d->indexForUserData(d->_hNodes, userData, comparisonFunction);
}

std::pair<int, MatrixModel::Node const*> MatrixModel::rowAndNodeForUserData(QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept
{
	Q_D(const MatrixModel);

	return d->indexAndNodeForUserData(d->_vNodes, userData, comparisonFunction);
}

std::pair<int, MatrixModel::Node const*> MatrixModel::columnAndNodeForUserData(QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept
{
	Q_D(const MatrixModel);

	return d->indexAndNodeForUserData(d->_hNodes, userData, comparisonFunction);
}

int MatrixModel::countChildren(Node const* node) const noexcept
{
	std::function<int(MatrixModel::Node const*, int)> countRecurse;
	countRecurse = [&countRecurse](MatrixModel::Node const* const node, int const children)
	{
		auto count = children;

		for (auto const* n : node->children)
		{
			count = countRecurse(n, count + 1);
		}
		return count;
	};

	return countRecurse(node, 0);
}

/* ************************************************************ */
/*                                                              */
/* ************************************************************ */

class MatrixHeaderView : public QHeaderView
{
public:
	MatrixHeaderView(Qt::Orientation orientation, QWidget* parent = nullptr)
		: QHeaderView(orientation, parent)
	{
		setDefaultSectionSize(20);
		setSectionResizeMode(QHeaderView::Fixed);
		setSectionsClickable(true);

		connect(this, &QHeaderView::sectionClicked, this, [this](int const logicalIndex)
		{
			MatrixModel::Node* node{ nullptr };

			if (this->orientation() == Qt::Vertical)
			{
				node = model()->nodeAtRow(logicalIndex);
			}
			else if (this->orientation() == Qt::Horizontal)
			{
				node = model()->nodeAtColumn(logicalIndex);
			}
			AVDECC_ASSERT(node, "Node should be valid");
			if (!node)
				return;

			node->isExpanded = !node->isExpanded;
			updateSectionVisibility(logicalIndex);
		});
	}

	virtual void mouseDoubleClickEvent(QMouseEvent* e) override
	{
		mousePressEvent(e);
	}

	void updateSectionVisibility(int const logicalIndex)
	{
		MatrixModel::Node* node{ nullptr };

		if (this->orientation() == Qt::Vertical)
		{
			node = model()->nodeAtRow(logicalIndex);
		}
		else if (this->orientation() == Qt::Horizontal)
		{
			node = model()->nodeAtColumn(logicalIndex);
		}
		AVDECC_ASSERT(node, "Node should be valid");
		if (!node)
			return;

		if (!node->children.empty())
		{
			auto const subSectionCount = model()->countChildren(node);
			for (auto i = 0; i < subSectionCount; ++i)
			{
				auto const section = logicalIndex + i + 1;
				if (node->isExpanded)
				{
					showSection(section);
				}
				else
				{
					hideSection(section);
				}
			}
		}
	}

	MatrixModel* model() const
	{
		return static_cast<MatrixModel*>(QHeaderView::model());
	}

	virtual void paintSection(QPainter* painter, QRect const& rect, int logicalIndex) const override
	{
		painter->setRenderHint(QPainter::Antialiasing);

		auto* m = model();
		MatrixModel::Node* node{ nullptr };

		if (orientation() == Qt::Vertical)
		{
			node = m->nodeAtRow(logicalIndex);
		}
		else if (orientation() == Qt::Horizontal)
		{
			node = m->nodeAtColumn(logicalIndex);
		}
		AVDECC_ASSERT(node, "Node should be valid");
		if (!node)
			return;

		std::function<int(MatrixModel::Node*, int)> countDepth;
		countDepth = [&countDepth](MatrixModel::Node* node, int const depth)
		{
			if (node->parent == nullptr)
				return depth;
			return countDepth(node->parent, depth + 1);
		};

		QBrush backgroundBrush{};
		auto const arrowSize{ 10 };
		auto const depth = countDepth(node, 0);
		auto const arrowOffset{ 25 * depth };
		switch (depth)
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

		auto highlighted{false};

		QPainterPath path;
		if (orientation() == Qt::Horizontal)
		{
			path.moveTo(rect.topLeft());
			path.lineTo(rect.bottomLeft() - QPoint{ 0, arrowSize + arrowOffset });
			path.lineTo(rect.center() + QPoint{ 0, rect.height() / 2 - arrowOffset });
			path.lineTo(rect.bottomRight() - QPoint{ 0, arrowSize + arrowOffset });
			path.lineTo(rect.topRight());

			highlighted = selectionModel()->isColumnSelected(logicalIndex, {});
		}
		else
		{
			path.moveTo(rect.topLeft());
			path.lineTo(rect.topRight() - QPoint{ arrowSize + arrowOffset, 0 });
			path.lineTo(rect.center() + QPoint{ rect.width() / 2 - arrowOffset, 0 });
			path.lineTo(rect.bottomRight() - QPoint{ arrowSize + arrowOffset, 0 });
			path.lineTo(rect.bottomLeft());

			highlighted = selectionModel()->isRowSelected(logicalIndex, {});
		}

		if (highlighted)
		{
			backgroundBrush = QColor{ "#007ACC" };
		}

		painter->fillPath(path, backgroundBrush);

		painter->save();
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

		auto const text = m->headerData(logicalIndex, orientation()).toString();
		auto const elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, textRect.width());

		auto const isStreamingWait = m->headerData(logicalIndex, orientation(), Qt::UserRole).toBool();
		if (isStreamingWait)
		{
			painter->setPen(Qt::red);
		}
		else
		{
			painter->setPen(Qt::white);
		}

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

	void sectionInserted(QModelIndex const& parent, int const first, int const last)
	{
		for (auto i = first; i <= last; ++i)
		{
			updateSectionVisibility(i);
		}
	}
};

/* ************************************************************ */
/*                                                              */
/* ************************************************************ */

MatrixTreeView::MatrixTreeView(QWidget* parent)
	: QTableView(parent)
{
	setVerticalHeader(new MatrixHeaderView(Qt::Vertical, this));
	setHorizontalHeader(new MatrixHeaderView(Qt::Horizontal, this));
	setSelectionMode(QAbstractItemView::NoSelection);
}

void MatrixTreeView::setModel(MatrixModel* model)
{
	if (auto* previousModel = QTableView::model())
	{
		previousModel->disconnect(verticalHeader());
		previousModel->disconnect(horizontalHeader());
	}

	QTableView::setModel(model);
	
	if (model)
	{
		connect(model, &MatrixModel::rowsInserted, static_cast<MatrixHeaderView*>(verticalHeader()), &MatrixHeaderView::sectionInserted);
		connect(model, &MatrixModel::columnsInserted, static_cast<MatrixHeaderView*>(horizontalHeader()), &MatrixHeaderView::sectionInserted);
	}
}

} // namespace toolkit
} // namespace qt
