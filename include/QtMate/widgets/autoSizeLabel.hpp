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

#include <QLabel>
#include <QFont>

namespace qtMate
{
namespace widgets
{
class AutoSizeLabel : public QLabel
{
public:
	explicit AutoSizeLabel(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	explicit AutoSizeLabel(QString const& text, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	void setText(QString const& text);
	void setFont(QFont const& font);

	virtual void resizeEvent(QResizeEvent* event) override;

private:
	void adjustFontSize();
	QFont _font{};
};

} // namespace widgets
} // namespace qtMate
