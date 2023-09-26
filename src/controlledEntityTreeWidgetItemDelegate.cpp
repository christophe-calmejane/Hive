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

#include "controlledEntityTreeWidgetItemDelegate.hpp"

#include <hive/widgetModelsLibrary/qtUserRoles.hpp>

#include <QPainter>
#include <QAbstractItemView>
#include <QComboBox>

ControlledEntityTreeWidgetItemDelegate::ControlledEntityTreeWidgetItemDelegate(qtMate::material::color::Name const themeColorName, QObject* parent) noexcept
	: QStyledItemDelegate(parent)
{
	setThemeColorName(themeColorName);
}

void ControlledEntityTreeWidgetItemDelegate::setThemeColorName(qtMate::material::color::Name const themeColorName)
{
	_themeColorName = themeColorName;
	_isDark = qtMate::material::color::luminance(_themeColorName) == qtMate::material::color::Luminance::Dark;
	_errorItemDelegate.setThemeColorName(themeColorName);
}

void ControlledEntityTreeWidgetItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Override default options according to the model current state
	auto basePainterOption = option;


	// Base painter
	{
		painter->save();

		// Check if the item has the active role
		auto const isActive = index.data(la::avdecc::utils::to_integral(hive::widgetModelsLibrary::QtUserRoles::ActiveRole)).toBool();
		// If active, use a bold font
		if (isActive)
		{
			basePainterOption.font.setBold(true);
		}

		// Check if the item has the error role
		auto const isError = index.data(la::avdecc::utils::to_integral(hive::widgetModelsLibrary::QtUserRoles::ErrorRole)).toBool();
		// If error, change the text color
		if (isError)
		{
			// Change foreground color
			basePainterOption.palette.setColor(QPalette::Text, qtMate::material::color::foregroundErrorColorValue(qtMate::material::color::backgroundColorName(), qtMate::material::color::colorSchemeShade()));
		}

		QStyledItemDelegate::paint(painter, basePainterOption, index);

		painter->restore();
	}

	// Error painter
	{
		static_cast<QStyledItemDelegate const&>(_errorItemDelegate).paint(painter, option, index);
	}
}
