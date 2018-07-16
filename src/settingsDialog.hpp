/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <QDialog>

class SettingsDialogImpl;
class SettingsDialog : public QDialog
{
	Q_OBJECT
public:
	SettingsDialog(QWidget* parent = nullptr);
	virtual ~SettingsDialog() noexcept;

	// Deleted compiler auto-generated methods
	SettingsDialog(SettingsDialog&&) = delete;
	SettingsDialog(SettingsDialog const&) = delete;
	SettingsDialog& operator=(SettingsDialog const&) = delete;
	SettingsDialog& operator=(SettingsDialog&&) = delete;

private:
	Q_SLOT void on_enableAEMCacheCheckBox_toggled(bool checked);

	SettingsDialogImpl* _pImpl{ nullptr };
};
