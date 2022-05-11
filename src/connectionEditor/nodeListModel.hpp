#pragma once

#include <QStandardItemModel>
#include <QtMate/flow/flowdefs.hpp>

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
