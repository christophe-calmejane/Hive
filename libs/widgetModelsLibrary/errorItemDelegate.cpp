/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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
#include "hive/widgetModelsLibrary/qtUserRoles.hpp"

#include <QPainter>

namespace hive
{
namespace widgetModelsLibrary
{
ErrorItemDelegate::ErrorItemDelegate(bool const paintBaseDelegate, qtMate::material::color::Name const themeColorName, QObject* parent) noexcept
	: QStyledItemDelegate(parent)
	, _paintBaseDelegate{ paintBaseDelegate }
	, _themeColorName{ themeColorName }
{
}

void ErrorItemDelegate::setThemeColorName(qtMate::material::color::Name const themeColorName)
{
	_themeColorName = themeColorName;
}

void ErrorItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Only paint parent if requested
	if (_paintBaseDelegate)
	{
		QStyledItemDelegate::paint(painter, option, index);
	}

	if (index.data(la::avdecc::utils::to_integral(QtUserRoles::ErrorRole)).toBool())
	{
		auto const colorName = (option.state & QStyle::StateFlag::State_Selected) ? _themeColorName : qtMate::material::color::backgroundColorName();
		auto const shade = (option.state & QStyle::StateFlag::State_Selected) ? qtMate::material::color::DefaultLightShade : qtMate::material::color::colorSchemeShade();
		painter->setPen(qtMate::material::color::foregroundErrorColorValue(colorName, shade));
		painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
	}
}

} // namespace widgetModelsLibrary
} // namespace hive
