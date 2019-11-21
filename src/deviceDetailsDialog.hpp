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

#include "avdecc/controllerManager.hpp"

class DeviceDetailsDialogImpl;

// **************************************************************
// class DeviceDetailsDialog
// **************************************************************
/**
* @brief	Implements the dialog to view and edit device details.
* [@author  Marius Erlen]
* [@date    2018-09-21]
*
* Provides a view with four tabs. Each tab displays a different set of data.
*/
class DeviceDetailsDialog : public QDialog
{
	Q_OBJECT
public:
	DeviceDetailsDialog(QWidget* parent = nullptr);
	virtual ~DeviceDetailsDialog() noexcept;

	// Delete compiler auto-generated methods
	DeviceDetailsDialog(DeviceDetailsDialog&&) = delete;
	DeviceDetailsDialog(DeviceDetailsDialog const&) = delete;
	DeviceDetailsDialog& operator=(DeviceDetailsDialog const&) = delete;
	DeviceDetailsDialog& operator=(DeviceDetailsDialog&&) = delete;

	void setControlledEntityID(la::avdecc::UniqueIdentifier const entityID);

private:
	DeviceDetailsDialogImpl* _pImpl{ nullptr };
	la::avdecc::UniqueIdentifier _controlledEntityID;
};
