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

#include "hive/widgetModelsLibrary/networkInterfacesListItemDelegate.hpp"
#include "hive/widgetModelsLibrary/networkInterfacesListModel.hpp"
#include "hive/widgetModelsLibrary/qtUserRoles.hpp"

#include <QPainter>
#include <QAbstractItemView>
#include <QComboBox>

namespace hive
{
namespace widgetModelsLibrary
{
NetworkInterfacesListItemDelegate::NetworkInterfacesListItemDelegate(qtMate::material::color::Name const themeColorName, QObject* parent) noexcept
	: QStyledItemDelegate(parent)
{
	setThemeColorName(themeColorName);
}

void NetworkInterfacesListItemDelegate::setThemeColorName(qtMate::material::color::Name const themeColorName)
{
	_themeColorName = themeColorName;
	_isDark = qtMate::material::color::luminance(_themeColorName) == qtMate::material::color::Luminance::Dark;
	_errorItemDelegate.setThemeColorName(themeColorName);
	_imageItemDelegate.setThemeColorName(themeColorName);
}

void NetworkInterfacesListItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Override default options according to the model current state
	auto basePainterOption = option;

	// Clear focus state if any
	if (basePainterOption.state & QStyle::State_HasFocus)
	{
		basePainterOption.state &= ~QStyle::State_HasFocus;
	}

	// Base painter
	{
		if (auto* view = dynamic_cast<QAbstractItemView const*>(option.widget))
		{
			if (auto* parent = view->parent(); !qstrcmp("QComboBoxPrivateContainer", parent->metaObject()->className()))
			{
				if (auto* widget = dynamic_cast<QComboBox const*>(parent->parent()))
				{
					basePainterOption.font.setBold(index.row() == widget->currentIndex());
				}
			}
		}

		QStyledItemDelegate::paint(painter, basePainterOption, index);
	}

	// Error painter
	{
		static_cast<QStyledItemDelegate const&>(_errorItemDelegate).paint(painter, option, index);
	}
}

} // namespace widgetModelsLibrary
} // namespace hive
