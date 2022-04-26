/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QMainWindow>

class DynamicHeaderView;

class MainWindowImpl;
class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	// Constructor
	MainWindow(QWidget* parent = nullptr);
	virtual ~MainWindow() noexcept;

	// Deleted compiler auto-generated methods
	MainWindow(MainWindow const&) = delete;
	MainWindow(MainWindow&&) = delete;
	MainWindow& operator=(MainWindow const&) = delete;
	MainWindow& operator=(MainWindow&&) = delete;

private:
	// QMainWindow overrides
	virtual void showEvent(QShowEvent* event) override;
	virtual void closeEvent(QCloseEvent* event) override;
	virtual void dragEnterEvent(QDragEnterEvent* event) override;
	virtual void dropEvent(QDropEvent* event) override;

	// Private variables
	MainWindowImpl* _pImpl{ nullptr };
};
