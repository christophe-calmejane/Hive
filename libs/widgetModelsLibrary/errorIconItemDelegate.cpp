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
	auto const errorType = index.data(la::avdecc::utils::to_integral(QtUserRoles::ErrorRole)).value<ErrorType>();
	auto const colorName = (option.state & QStyle::StateFlag::State_Selected) ? _themeColorName : qtMate::material::color::backgroundColorName(qtMate::material::color::DefaultBackgroundLuminance); // For now, hardcode the background luminance to "Light"
	switch (errorType)
	{
		case ErrorType::Error:
		{
			auto font = QFont{ "Hive" };
			font.setStyleStrategy(QFont::PreferQuality);
			font.setPointSize(14);
			painter->setPen(qtMate::material::color::foregroundErrorColorValue(colorName, qtMate::material::color::DefaultShade));
			painter->setFont(font);
			painter->drawText(option.rect, Qt::AlignCenter, "error_fill");
			break;
		}
		case ErrorType::Warning:
		{
			auto font = QFont{ "Hive" };
			font.setStyleStrategy(QFont::PreferQuality);
			font.setPointSize(14);
			painter->setPen(qtMate::material::color::foregroundWarningColorValue(colorName, qtMate::material::color::DefaultShade));
			painter->setFont(font);
			painter->drawText(option.rect, Qt::AlignCenter, "warning_fill");
			break;
		}
		case ErrorType::Information:
		{
			auto font = QFont{ "Hive" };
			font.setStyleStrategy(QFont::PreferQuality);
			font.setPointSize(14);
			painter->setPen(qtMate::material::color::foregroundInformationColorValue(colorName, qtMate::material::color::DefaultShade));
			painter->setFont(font);
			painter->drawText(option.rect, Qt::AlignCenter, "information_fill");
			break;
		}
		default:
			break;
	}
	painter->restore();
} // namespace widgetModelsLibrary

} // namespace widgetModelsLibrary
} // namespace hive
