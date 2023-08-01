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

#include "hive/widgetModelsLibrary/errorIconItemDelegate.hpp"
#include "hive/widgetModelsLibrary/qtUserRoles.hpp"

#include <QPainter>

namespace hive
{
namespace widgetModelsLibrary
{
ErrorIconItemDelegate::ErrorIconItemDelegate(bool const paintBaseDelegate, qtMate::material::color::Name const themeColorName, QObject* parent) noexcept
	: QStyledItemDelegate(parent)
	, _paintBaseDelegate{ paintBaseDelegate }
	, _themeColorName{ themeColorName }
{
}

void ErrorIconItemDelegate::setThemeColorName(qtMate::material::color::Name const themeColorName)
{
	_themeColorName = themeColorName;
}

static QRect getCenteredSquare(QRect const& rect, int const size)
{
	auto const x = rect.x() + (rect.width() - size) / 2;
	auto const y = rect.y() + (rect.height() - size) / 2;
	return QRect{ x, y, size, size };
}

void ErrorIconItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Only paint parent if requested
	if (_paintBaseDelegate)
	{
		QStyledItemDelegate::paint(painter, option, index);
	}

	painter->save();
	if (index.data(la::avdecc::utils::to_integral(QtUserRoles::ErrorRole)).toBool())
	{
		auto color = QColor{};
		//if (option.state & QStyle::StateFlag::State_Selected)
		//{
		//	color = qtMate::material::color::complementaryValue(_themeColorName, qtMate::material::color::Shade::Shade600);
		//}
		//else
		{
			color = qtMate::material::color::foregroundErrorColorValue(qtMate::material::color::DefaultColor, qtMate::material::color::Shade::ShadeA700); // Right now, always use default value, as we draw on white background
		}
		auto const brush = QBrush{ color, Qt::SolidPattern };
		painter->setBrush(brush);
		painter->drawEllipse(getCenteredSquare(option.rect, 10));
	}
	painter->restore();
}

} // namespace widgetModelsLibrary
} // namespace hive
