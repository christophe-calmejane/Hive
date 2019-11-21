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

#include <QDialog>

class MediaClockManagementDialogImpl;

// **************************************************************
// class MediaClockManagementDialog
// **************************************************************
/**
* @brief    The ui of the dialog to edit media clock domains.
* [@author  Marius Erlen]
* [@date    2018-10-22]
*/
class MediaClockManagementDialog : public QDialog
{
	Q_OBJECT
public:
	MediaClockManagementDialog(QWidget* parent = nullptr);
	virtual ~MediaClockManagementDialog() noexcept;

	// Deleted compiler auto-generated methods
	MediaClockManagementDialog(MediaClockManagementDialog&&) = delete;
	MediaClockManagementDialog(MediaClockManagementDialog const&) = delete;
	MediaClockManagementDialog& operator=(MediaClockManagementDialog const&) = delete;
	MediaClockManagementDialog& operator=(MediaClockManagementDialog&&) = delete;

	void reject() override;

private:
	MediaClockManagementDialogImpl* _pImpl{ nullptr };
};
