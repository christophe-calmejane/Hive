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

#include <QStandardItemModel>
#include <QtMate/flow/flowDefs.hpp>

class NodeListModel : public QStandardItemModel
{
public:
	using QStandardItemModel::QStandardItemModel;

	// add a node to the list
	void createItem(qtMate::flow::FlowNodeUid const& uid, qtMate::flow::FlowNodeDescriptor const& descriptor, QPixmap const& pixmap);

	// disabled means not draggable and grayed-out
	void setEnabled(qtMate::flow::FlowNodeUid const& uid, bool enabled);

	// return the index associated with a node
	QModelIndex nodeIndex(qtMate::flow::FlowNodeUid const& uid) const;

	// return a pointer to the node descriptor associated with a node
	qtMate::flow::FlowNodeDescriptor const* descriptor(qtMate::flow::FlowNodeUid const& uid) const;

	// QStandardItemModel overrides
	virtual QMimeData* mimeData(QModelIndexList const& indexes) const override;
	virtual QStringList mimeTypes() const override;

private:
	QHash<qtMate::flow::FlowNodeUid, QStandardItem*> _items{};
	QHash<qtMate::flow::FlowNodeUid, qtMate::flow::FlowNodeDescriptor> _descriptors{};
	QHash<qtMate::flow::FlowNodeUid, QPixmap> _pixmaps{};
};
