/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QScrollBar>

/**
* @brief Scroll bar that automatically sticks to its maximum position.
* @details While the scroll bar is at its maximum position (ie. the view is scrolled to the bottom), it automatically
*          follows range changes so newly appended rows stay visible. Scrolling away from the bottom disables the
*          automatic behavior until the user scrolls back to the bottom.
*/
class AutoScrollBar : public QScrollBar
{
public:
	AutoScrollBar(QWidget* parent)
		: AutoScrollBar(Qt::Vertical, parent)
	{
	}

	AutoScrollBar(Qt::Orientation orientation, QWidget* parent)
		: QScrollBar(orientation, parent)
	{
		_bufferedMaximum = maximum();

		connect(this, &QScrollBar::rangeChanged, this,
			[this](int, int max)
			{
				if (value() == _bufferedMaximum)
				{
					setValue(max);
				}

				_bufferedMaximum = max;
			});
	}

private:
	int _bufferedMaximum{ 0 };
};
