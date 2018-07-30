#include "painterHelper.hpp"

namespace painterHelper
{

void drawCentered(QPainter* painter, QRect const& rect, QImage const& image)
{
	drawCentered(painter, rect, QPixmap::fromImage(image));
}

void drawCentered(QPainter* painter, QRect const& rect, QPixmap const& pixmap)
{
	if (!painter || !rect.isValid() || pixmap.isNull())
	{
		return;
	}
	
	auto const devicePixelRatio = painter->device()->devicePixelRatioF();
	
	auto const shouldScale = pixmap.height() > rect.height() * devicePixelRatio;
	
	QPixmap scaledPixmap = shouldScale ? pixmap.scaledToHeight(rect.height() * devicePixelRatio, Qt::SmoothTransformation) : pixmap;
	scaledPixmap.setDevicePixelRatio(devicePixelRatio);
	
	auto const x = rect.x() + (rect.width() - scaledPixmap.width() / devicePixelRatio) / 2;
	auto const y = rect.y() + (rect.height() - scaledPixmap.height() / devicePixelRatio) / 2;

	painter->drawPixmap(x, y, scaledPixmap);
}

} // namespace painterHelper
