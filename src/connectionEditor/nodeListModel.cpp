/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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
#include "nodeListModel.hpp"

#include <QMimeData>
#include <QByteArray>
#include <QIODevice>
#include <QPalette>

void NodeListModel::createItem(qtMate::flow::FlowNodeUid const& uid, qtMate::flow::FlowNodeDescriptor const& descriptor, QPixmap const& pixmap)
{
	auto* item = new QStandardItem{};
	item->setData(descriptor.name, Qt::DisplayRole);
	item->setData(uid, Qt::UserRole);
	item->setEditable(false);
	item->setDragEnabled(true);
	item->setEnabled(true);
	_items.insert(uid, item);
	_descriptors.insert(uid, descriptor);
	_pixmaps.insert(uid, pixmap);
	appendRow(item);
}

void NodeListModel::setEnabled(qtMate::flow::FlowNodeUid const& uid, bool enabled)
{
	if (auto* item = _items.value(uid))
	{
		item->setDragEnabled(enabled);

		auto const group = enabled ? QPalette::Normal : QPalette::Disabled;
		auto const color = QPalette().color(group, QPalette::WindowText);
		item->setData(color, Qt::ForegroundRole);
	}
}

QModelIndex NodeListModel::nodeIndex(qtMate::flow::FlowNodeUid const& uid) const
{
	if (auto* item = _items.value(uid))
	{
		return createIndex(item->row(), 0);
	}
	return {};
}

qtMate::flow::FlowNodeDescriptor const* NodeListModel::descriptor(qtMate::flow::FlowNodeUid const& uid) const
{
	auto const it = _descriptors.find(uid);
	if (it != std::end(_descriptors))
	{
		return &(*it);
	}
	return nullptr;
}

QMimeData* NodeListModel::mimeData(QModelIndexList const& indexes) const
{
	// only handle 1 valid node
	if (indexes.empty() || !indexes.first().isValid())
	{
		return nullptr;
	}

	auto encodedData = QByteArray{};
	auto stream = QDataStream{ &encodedData, QIODevice::WriteOnly };

	auto const index = indexes.first();
	auto const uid = index.data(Qt::UserRole).value<qtMate::flow::FlowNodeUid>();
	stream << uid;

	auto* mimeData = new QMimeData{};
	mimeData->setData("application/x-node", encodedData);
	mimeData->setImageData(_pixmaps[uid].toImage());

	return mimeData;
}

QStringList NodeListModel::mimeTypes() const
{
	return { "application/x-node" };
}
