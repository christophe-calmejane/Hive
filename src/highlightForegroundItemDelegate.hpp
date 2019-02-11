#pragma once
#include <QStyledItemDelegate>

// This custom delegate allows to keep the Qt::ForegroundRole visible even when a cell is highlighted
class HighlightForegroundItemDelegate : public QStyledItemDelegate
{
public:
	using QStyledItemDelegate::QStyledItemDelegate;

protected:
	virtual void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override;
};
