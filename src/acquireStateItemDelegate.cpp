#include "acquireStateItemDelegate.hpp"
#include <QPainter>

AcquireStateItemDelegate::AcquireStateItemDelegate(QObject* parent)
: QStyledItemDelegate{parent}
{
}

void AcquireStateItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	QStyledItemDelegate::paint(painter, option, index);

	auto const state = index.data(Qt::UserRole).toInt();

	auto const devicePixelRatio = painter->device()->devicePixelRatioF();
	
	auto pixmap = _pixmaps[state].scaledToHeight(option.rect.height() * devicePixelRatio, Qt::SmoothTransformation);
	pixmap.setDevicePixelRatio(devicePixelRatio);
	
	auto const x = option.rect.x() + (option.rect.width() - pixmap.width() / devicePixelRatio) / 2;
	auto const y = option.rect.y() + (option.rect.height() - pixmap.height() / devicePixelRatio) / 2;

	painter->drawPixmap(x, y, pixmap);
}
