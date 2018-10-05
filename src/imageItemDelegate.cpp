#include "imageItemDelegate.hpp"
#include "painterHelper.hpp"

void ImageItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	QStyledItemDelegate::paint(painter, option, index);

	auto const userData{ index.data(Qt::UserRole) };
	if (!userData.canConvert<QImage>())
	{
		return;
	}

	auto const image = userData.value<QImage>();
	painterHelper::drawCentered(painter, option.rect, image);
}
