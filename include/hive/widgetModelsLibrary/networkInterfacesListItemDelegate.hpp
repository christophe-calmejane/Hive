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

#pragma once

#include "errorItemDelegate.hpp"
#include "imageItemDelegate.hpp"

#include <QStyledItemDelegate>

namespace hive
{
namespace widgetModelsLibrary
{
// This delegate handles all QtUserRoles used by NetworkInterfacesListModel and can be used as global ItemDelegate
class NetworkInterfacesListItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	NetworkInterfacesListItemDelegate(qtMate::material::color::Name const themeColorName = qtMate::material::color::DefaultColor, QObject* parent = nullptr) noexcept;

	Q_SLOT void setThemeColorName(qtMate::material::color::Name const themeColorName);

protected:
	virtual void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override;

private:
	// Private members
	qtMate::material::color::Name _themeColorName{ qtMate::material::color::DefaultColor };
	bool _isDark{ false };
	ErrorItemDelegate _errorItemDelegate{ false };
	ImageItemDelegate _imageItemDelegate{ false };
};

} // namespace widgetModelsLibrary
} // namespace hive
