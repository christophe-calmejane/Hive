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
#include <QBrush>

namespace qt
{
namespace toolkit
{
namespace material
{
namespace color
{
// Name + Shade combinations as described here : https://material.io/design/color/the-color-system.html

// Color name definition
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

	NameCount
};

// Color shade definition
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

	ShadeCount
};

// Color luminance definition
enum class Luminance
{
	Dark,
	Light
};

// Default color shade
static auto constexpr DefaultColor = Name::DeepPurple;
static auto constexpr DefaultShade = Shade::Shade500;

// Return the color value for a given name + shade
// May throw invalid_argument for non existing combinations
QColor value(Name const name, Shade const shade = DefaultShade);

// Return the foreground color value for given name + shade
// May throw invalid_argument for non existing combinations
// Foreground value is linked to the color luminance
// Dark: white
// Light: black
QColor foregroundValue(Name const name, Shade const shade = DefaultShade);

// Return the complementary color value for given name + shade
// May throw invalid_argument for non existing combinations
QColor complementaryValue(Name const name, Shade const shade = DefaultShade);

// Return the foreground complementary color value for given name + shade
// May throw invalid_argument for non existing combinations
QColor foregroundComplementaryValue(Name const name, Shade const shade = DefaultShade);

// Return the luminance for a given name + shade
// May throw invalid_argument for non existing combinations
Luminance luminance(Name const name, Shade const shade = DefaultShade);

// Return a brush that represents a given name + shade
QBrush brush(Name const name, Shade const shade = DefaultShade) noexcept;

} // namespace color
} // namespace material
} // namespace toolkit
} // namespace qt
