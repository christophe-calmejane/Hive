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

#include <QColor>
#include <QAbstractListModel>

namespace qt
{
namespace toolkit
{
namespace materialPalette
{
	
/* Palette color name definition */
enum class Name
{
	Red,
	Pink,
	Purple,
	DeepPurple,
	Indigo,
	Blue,
	LightBlue,
	Cyan,
	Teal,
	Green,
	LightGreen,
	Lime,
	Yellow,
	Amber,
	Orange,
	DeepOrange,
	Brown,
	Gray,
	BlueGray,

	Count
};

/** Palette color shade definition */
enum class Shade
{
	Shade50,
	Shade100,
	Shade200,
	Shade300,
	Shade400,
	Shade500,
	Shade600,
	Shade700,
	Shade800,
	Shade900,
	ShadeA100,
	ShadeA200,
	ShadeA400,
	ShadeA700,

	Count,
};

/** Palette color luminance definition */
enum class Luminance
{
	Dark,
	Light
};

/** Default color shade */
static auto constexpr DefaultShade = Shade::Shade500;

/** Returns the color value for a given name + shade */
QColor color(Name const name, Shade const shade = DefaultShade);

/** Returns the color luminance for a given name + shade */
Luminance luminance(Name const name, Shade const shade = DefaultShade);

/** Abstract model that exposes all the available colors */
class Model : public QAbstractListModel
{
public:
	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual QVariant data(QModelIndex const &index, int role = Qt::DisplayRole) const override;
};

} // namespace materialPalette
} // namespace toolkit
} // namespace qt
