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

#pragma once

#include "settingsManager/settings.hpp"

#include <QStyledItemDelegate>

// This custom delegate allows to indicate a cell is on error
class ErrorItemDelegate : public QStyledItemDelegate, private settings::SettingsManager::Observer
{
public:
	static constexpr auto ErrorRole = Qt::UserRole + 1;

	explicit ErrorItemDelegate(QObject* parent = nullptr) noexcept;
	~ErrorItemDelegate() noexcept;

protected:
	virtual void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override;

private:
	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

	// Private members
	qt::toolkit::material::color::Name _colorName{ qt::toolkit::material::color::DefaultColor };
};
