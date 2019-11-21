/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "errorItemDelegate.hpp"
#include "toolkit/material/color.hpp"

#include <QPainter>

ErrorItemDelegate::ErrorItemDelegate(QObject* parent) noexcept
	: QStyledItemDelegate(parent)
{
	// Configure settings observers
	auto& settings = settings::SettingsManager::getInstance();
	settings.registerSettingObserver(settings::General_ThemeColorIndex.name, this);
}

ErrorItemDelegate::~ErrorItemDelegate() noexcept
{
	// Remove settings observers
	auto& settings = settings::SettingsManager::getInstance();
	settings.unregisterSettingObserver(settings::General_ThemeColorIndex.name, this);
}

void ErrorItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	QStyledItemDelegate::paint(painter, option, index);

	if (index.data(ErrorRole).toBool())
	{
		if (option.state & QStyle::StateFlag::State_Selected)
		{
			painter->setPen(QPen{ qt::toolkit::material::color::complementaryValue(_colorName, qt::toolkit::material::color::Shade::Shade600), 2 });
			painter->drawRect(option.rect.adjusted(1, 1, -1, -1));
		}
		else
		{
			//painter->setPen(qt::toolkit::material::color::foregroundErrorColorValue(_colorName, qt::toolkit::material::color::Shade::ShadeA700));
			painter->setPen(qt::toolkit::material::color::foregroundErrorColorValue(qt::toolkit::material::color::DefaultColor, qt::toolkit::material::color::Shade::ShadeA700)); // Right now, always use default value, as we draw on white background
			painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
		}
	}
}

void ErrorItemDelegate::onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept
{
	if (name == settings::General_ThemeColorIndex.name)
	{
		_colorName = qt::toolkit::material::color::Palette::name(value.toInt());
	}
}
