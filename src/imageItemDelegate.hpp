#pragma once
#include <QStyledItemDelegate>

class ImageItemDelegate : public QStyledItemDelegate
{
public:
	using QStyledItemDelegate::QStyledItemDelegate;
	
protected:
	virtual void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override;
};
