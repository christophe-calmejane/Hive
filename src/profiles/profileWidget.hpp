/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QGridLayout>
#include <QFrame>
#include <QLabel>

namespace profiles
{
class ProfileWidget : public QFrame
{
	Q_OBJECT
public:
	ProfileWidget(QString const& title, QString const& description, QString const& icon, QWidget* parent = nullptr);

signals:
	void clicked();

protected:
	virtual void mouseReleaseEvent(QMouseEvent* event) override;

private:
	QGridLayout _layout{ this };
	QLabel _title{ this };
	QLabel _description{ this };
	QLabel _icon{ this };
};
} // namespace profiles
