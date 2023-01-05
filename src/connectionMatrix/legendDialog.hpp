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

#pragma once

#include "settingsManager/settings.hpp"

#include <QtMate/material/color.hpp>

#include <QDialog>
#include <QLayout>
#include <QColor>
#include <QPushButton>

namespace connectionMatrix
{
class LegendDialog : public QDialog, private settings::SettingsManager::Observer
{
	Q_OBJECT
public:
	LegendDialog(qtMate::material::color::Name const& colorName, bool const isTransposed, QWidget* parent = nullptr);
	~LegendDialog();

private:
	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

	QVBoxLayout _layout{ this };
	QPushButton _closeButton{ "Close", this };
	bool _drawMediaLockedDot{ false };
	bool _showEntitySummary{ false };
};

} // namespace connectionMatrix
