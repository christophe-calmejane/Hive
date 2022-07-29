/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "hive/widgetModelsLibrary/imageItemDelegate.hpp"
#include "hive/widgetModelsLibrary/qtUserRoles.hpp"
#include "hive/widgetModelsLibrary/painterHelper.hpp"

#include <QPainter>

namespace hive
{
namespace widgetModelsLibrary
{
ImageItemDelegate::ImageItemDelegate(bool const paintBaseDelegate, qtMate::material::color::Name const themeColorName, QObject* parent) noexcept
	: QStyledItemDelegate(parent)
	, _paintBaseDelegate{ paintBaseDelegate }
{
	setThemeColorName(themeColorName);
}

void ImageItemDelegate::setThemeColorName(qtMate::material::color::Name const themeColorName)
{
	_themeColorName = themeColorName;
	_isDark = qtMate::material::color::luminance(_themeColorName) == qtMate::material::color::Luminance::Dark;
}

void ImageItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Only paint parent if requested
	if (_paintBaseDelegate)
	{
		QStyledItemDelegate::paint(painter, option, index);
	}

	auto const role = ((option.state & QStyle::StateFlag::State_Selected) && _isDark) ? la::avdecc::utils::to_integral(QtUserRoles::DarkImageRole) : la::avdecc::utils::to_integral(QtUserRoles::LightImageRole);
	auto const userData = index.data(role);
	if (!userData.canConvert<QImage>())
	{
		return;
	}

	auto const image = userData.value<QImage>();
	painterHelper::drawCentered(painter, option.rect, image);
}

} // namespace widgetModelsLibrary
} // namespace hive
