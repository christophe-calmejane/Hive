#pragma once

#include <QPainter>

namespace painterHelper
{
void drawCentered(QPainter* painter, QRect const& rect, QImage const& image);
void drawCentered(QPainter* painter, QRect const& rect, QPixmap const& pixmap);

} // namespace painterHelper
