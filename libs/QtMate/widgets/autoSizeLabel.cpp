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

#include "QtMate/widgets/autoSizeLabel.hpp"

#include <QFontMetrics>
#include <QStyle>

namespace qtMate
{
namespace widgets
{
AutoSizeLabel::AutoSizeLabel(QWidget* parent, Qt::WindowFlags f)
	: QLabel{ parent, f }
{
}

AutoSizeLabel::AutoSizeLabel(QString const& text, QWidget* parent, Qt::WindowFlags f)
	: QLabel{ text, parent, f }
{
}

void AutoSizeLabel::setText(QString const& text)
{
	QLabel::setText(text);

	adjustFontSize();
}

void AutoSizeLabel::setFont(QFont const& font)
{
	_font = font;

	adjustFontSize();
}

void AutoSizeLabel::resizeEvent(QResizeEvent* event)
{
	QLabel::resizeEvent(event);

	adjustFontSize();
}

void AutoSizeLabel::adjustFontSize()
{
	auto const maxWidth = QLabel::width();
	auto f = _font;

	auto const getTextWidth = [this](auto& f)
	{
	// FontMetrics doesn't take into account BOLD tags in the text, causing the horizontalAdvance to take 20% less space than actual rendering
	// (tested with Qt 5.15.2, 6.2.2 and 6.3.0-alpha1)
	// So for now, compute the space with an actual rendering
#if 1
		auto l = QLabel{};
		l.setFont(f);
		l.setText(text());
		l.show();
		return static_cast<decltype(maxWidth)>(l.width());
#else
		auto const fm = QFontMetrics{ f };
		return static_cast<decltype(maxWidth)>(fm.horizontalAdvance(text()));
#endif
	};

	for (auto textWidth = getTextWidth(f); textWidth > maxWidth; textWidth = getTextWidth(f))
	{
		auto ps = f.pointSizeF();
		auto const diff = textWidth - maxWidth;
		if (diff > 40)
		{
			ps -= 2;
		}
		else if (diff > 30)
		{
			ps -= 1;
		}
		else
		{
			ps -= 0.5;
		}
		f.setPointSizeF(ps);
	}

	QLabel::setFont(f);
}

} // namespace widgets
} // namespace qtMate
