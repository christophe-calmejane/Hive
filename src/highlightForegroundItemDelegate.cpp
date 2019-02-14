#include "highlightForegroundItemDelegate.hpp"

void HighlightForegroundItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	auto opt{ option };

	auto const data = index.data(Qt::ForegroundRole);
	if (data.isValid())
	{
		opt.palette.setColor(QPalette::HighlightedText, data.value<QColor>());
	}

	QStyledItemDelegate::paint(painter, opt, index);
}
