/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "hive/widgetModelsLibrary/errorItemDelegate.hpp"

#include <QtMate/material/color.hpp>

#include <QPainter>

namespace hive
{
namespace widgetModelsLibrary
{
ErrorItemDelegate::ErrorItemDelegate(qtMate::material::color::Name const themeColorName, QObject* parent) noexcept
	: QStyledItemDelegate(parent)
	, _themeColorName{ themeColorName }
{
}

void ErrorItemDelegate::setThemeColorName(qtMate::material::color::Name const themeColorName)
{
	_themeColorName = themeColorName;
}

void ErrorItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	QStyledItemDelegate::paint(painter, option, index);

	if (index.data(ErrorRole).toBool())
	{
		if (option.state & QStyle::StateFlag::State_Selected)
		{
			painter->setPen(QPen{ qtMate::material::color::complementaryValue(_themeColorName, qtMate::material::color::Shade::Shade600), 2 });
			painter->drawRect(option.rect.adjusted(1, 1, -1, -1));
		}
		else
		{
			//painter->setPen(qtMate::material::color::foregroundErrorColorValue(_colorName, qtMate::material::color::Shade::ShadeA700));
			painter->setPen(qtMate::material::color::foregroundErrorColorValue(qtMate::material::color::DefaultColor, qtMate::material::color::Shade::ShadeA700)); // Right now, always use default value, as we draw on white background
			painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
		}
	}
}

} // namespace widgetModelsLibrary
} // namespace hive
