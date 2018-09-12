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
#include <QTableView>
#include <utility>

namespace qt
{
namespace toolkit
{

class MatrixModel : public QAbstractTableModel
{
public:
	struct Node
	{
		Node* parent{ nullptr };
		std::vector<Node*> children{};
		bool isExpanded{ true };
		QVariant userData{};

		Node(Node* parent) : parent(parent) {}
	};

	MatrixModel(QObject* parent = nullptr);
	virtual ~MatrixModel();

	virtual int rowCount(QModelIndex const& parent) const override;
	virtual int columnCount(QModelIndex const& parent) const override;
	virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;


	void beginAppendRows(QModelIndex const& parent, int count);
	std::pair<QModelIndex, Node&> appendRow(QModelIndex const& parent = QModelIndex());
	void endAppendRows();
	virtual bool removeRows(int row, int count, QModelIndex const& parent = QModelIndex()) override;

	void beginAppendColumns(QModelIndex const& parent, int count);
	std::pair<QModelIndex, Node&> appendColumn(QModelIndex const& parent = QModelIndex());
	void endAppendColumns();
	virtual bool removeColumns(int column, int count, QModelIndex const& parent = QModelIndex()) override;

	void clearModel() noexcept;

	Node* nodeAtRow(int const row) const noexcept;
	Node* nodeAtColumn(int const column) const noexcept;
	int rowForUserData(QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept;
	int columnForUserData(QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept;
	std::pair<int, Node const*> rowAndNodeForUserData(QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept;
	std::pair<int, Node const*> columnAndNodeForUserData(QVariant const& userData, std::function<bool(QVariant const& lhs, QVariant const& rhs)> const& comparisonFunction) const noexcept;

	int countChildren(Node const* node) const noexcept;

private:
	class MatrixModelPrivate;
	MatrixModelPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(MatrixModel);
};

class MatrixTreeView : public QTableView
{
	using QTableView::setModel;

public:
	MatrixTreeView(QWidget* parent = nullptr);

	void setModel(MatrixModel* model);
};

} // namespace toolkit
} // namespace qt
