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

#include "QtMate/material/colorPalette.hpp"

#include <QIcon>
#include <QPainter>
#include <QPixmap>

#include <unordered_map>
#include <cassert>

namespace qtMate
{
namespace material
{
namespace color
{
QPixmap buildPixmap(Name const name, Shade const shade)
{
	QPixmap pixmap{ 14, 14 };
	pixmap.fill(Qt::transparent);

	QPainter painter{ &pixmap };
	auto const pixmapRect = pixmap.rect();

	// Background color
	painter.fillRect(pixmapRect, brush(name, shade));

	// Border
	painter.setPen(Qt::darkGray);
	painter.drawRect(pixmapRect.adjusted(0, 0, -1, -1));

	return pixmap;
}

QPixmap pixmap(Name const name, Shade const shade)
{
	using Shades = std::unordered_map<Shade, QPixmap>;
	using Colors = std::unordered_map<Name, Shades>;

	static Colors s_colors;

	auto& shades = s_colors[name];
	auto const it = shades.find(shade);
	if (it == std::end(shades))
	{
		shades[shade] = buildPixmap(name, shade);
	}

	return shades[shade];
}

int Palette::nameCount()
{
	return static_cast<int>(Name::NameCount);
}

int Palette::shadeCount()
{
	return static_cast<int>(Shade::ShadeCount);
}

Name Palette::name(int const index)
{
	assert(index >= 0 && index < nameCount());
	return static_cast<Name>(index);
}

Shade Palette::shade(int const index)
{
	assert(index >= 0 && index < shadeCount());
	return static_cast<Shade>(index);
}

QString Palette::nameToString(Name const name)
{
	switch (name)
	{
		case Name::Red:
			return "Red";
		case Name::Pink:
			return "Pink";
		case Name::Purple:
			return "Purple";
		case Name::DeepPurple:
			return "Deep Purple";
		case Name::Indigo:
			return "Indigo";
		case Name::Blue:
			return "Blue";
		case Name::LightBlue:
			return "Light Blue";
		case Name::Cyan:
			return "Cyan";
		case Name::Teal:
			return "Teal";
		case Name::Green:
			return "Green";
		case Name::LightGreen:
			return "Light Green";
		case Name::Lime:
			return "Lime";
		case Name::Yellow:
			return "Yellow";
		case Name::Amber:
			return "Amber";
		case Name::Orange:
			return "Orange";
		case Name::DeepOrange:
			return "Deep Orange";
		case Name::Brown:
			return "Brown";
		case Name::Gray:
			return "Gray";
		case Name::BlueGray:
			return "Blue Gray";
		default:
			assert(false);
			return "Undefined";
	}
}

QString Palette::shadeToString(Shade const shade)
{
	switch (shade)
	{
		case Shade::Shade50:
			return "50";
		case Shade::Shade100:
			return "100";
		case Shade::Shade200:
			return "200";
		case Shade::Shade300:
			return "300";
		case Shade::Shade400:
			return "400";
		case Shade::Shade500:
			return "500";
		case Shade::Shade600:
			return "600";
		case Shade::Shade700:
			return "700";
		case Shade::Shade800:
			return "800";
		case Shade::Shade900:
			return "900";
		case Shade::ShadeA100:
			return "A100";
		case Shade::ShadeA200:
			return "A200";
		case Shade::ShadeA400:
			return "A400";
		case Shade::ShadeA700:
			return "A700";
		default:
			assert(false);
			return "Undefined";
	}
}

int Palette::index(Name const name)
{
	if (name == Name::NameCount)
	{
		return -1;
	}
	return static_cast<int>(name);
}

int Palette::index(Shade const shade)
{
	if (shade == Shade::ShadeCount)
	{
		return -1;
	}
	return static_cast<int>(shade);
}

int Palette::rowCount(QModelIndex const& parent) const
{
	Q_UNUSED(parent);
	return nameCount();
}

int Palette::columnCount(QModelIndex const& parent) const
{
	Q_UNUSED(parent);
	return shadeCount();
}

QVariant Palette::data(QModelIndex const& index, int role) const
{
	if (index.row() < 0 || index.row() >= rowCount())
	{
		return {};
	}

	auto const name = static_cast<Name>(index.row());
	auto const shade = static_cast<Shade>(index.column());

	if (role == Qt::DisplayRole)
	{
		return nameToString(name);
	}
	else if (role == Qt::DecorationRole)
	{
		return pixmap(name, shade);
	}

	return {};
}

} // namespace color
} // namespace material
} // namespace qtMate
