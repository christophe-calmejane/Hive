/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "QtMate/material/color.hpp"

#include <QStyleHints>
#include <QGuiApplication>
#include <QApplication>

#include <unordered_map>
#include <stdexcept>
#include <mutex>

namespace qtMate
{
namespace material
{
namespace color
{
struct ColorData
{
	QColor value; // The actual color value
	Luminance luminance{ Luminance::Dark }; // The associated luminance
};

using ShadeMap = std::unordered_map<Shade, ColorData>;
using ColorMap = std::unordered_map<Name, ShadeMap>;

static ColorMap g_colorMap = {
	{ Name::Red,
		{
			{ Shade::Shade50, { 0xFFEBEE } },
			{ Shade::Shade100, { 0xFFCDD2 } },
			{ Shade::Shade200, { 0xEF9A9A } },
			{ Shade::Shade300, { 0xE57373 } },
			{ Shade::Shade400, { 0xEF5350 } },
			{ Shade::Shade500, { 0xF44336 } },
			{ Shade::Shade600, { 0xE53935 } },
			{ Shade::Shade700, { 0xD32F2F } },
			{ Shade::Shade800, { 0xC62828 } },
			{ Shade::Shade900, { 0xB71C1C } },
			{ Shade::ShadeA100, { 0xFF8A80 } },
			{ Shade::ShadeA200, { 0xFF5252 } },
			{ Shade::ShadeA400, { 0xFF1744 } },
			{ Shade::ShadeA700, { 0xD50000 } },
		} },
	{ Name::Pink,
		{
			{ Shade::Shade50, { 0xFCE4EC } },
			{ Shade::Shade100, { 0xF8BBD0 } },
			{ Shade::Shade200, { 0xF48FB1 } },
			{ Shade::Shade300, { 0xF06292 } },
			{ Shade::Shade400, { 0xEC407A } },
			{ Shade::Shade500, { 0xE91E63 } },
			{ Shade::Shade600, { 0xD81B60 } },
			{ Shade::Shade700, { 0xC2185B } },
			{ Shade::Shade800, { 0xAD1457 } },
			{ Shade::Shade900, { 0x880E4F } },
			{ Shade::ShadeA100, { 0xFF80AB } },
			{ Shade::ShadeA200, { 0xFF4081 } },
			{ Shade::ShadeA400, { 0xF50057 } },
			{ Shade::ShadeA700, { 0xC51162 } },
		} },
	{ Name::Purple,
		{
			{ Shade::Shade50, { 0xF3E5F5 } },
			{ Shade::Shade100, { 0xE1BEE7 } },
			{ Shade::Shade200, { 0xCE93D8 } },
			{ Shade::Shade300, { 0xBA68C8 } },
			{ Shade::Shade400, { 0xAB47BC } },
			{ Shade::Shade500, { 0x9C27B0 } },
			{ Shade::Shade600, { 0x8E24AA } },
			{ Shade::Shade700, { 0x7B1FA2 } },
			{ Shade::Shade800, { 0x6A1B9A } },
			{ Shade::Shade900, { 0x4A148C } },
			{ Shade::ShadeA100, { 0xEA80FC } },
			{ Shade::ShadeA200, { 0xE040FB } },
			{ Shade::ShadeA400, { 0xD500F9 } },
			{ Shade::ShadeA700, { 0xAA00FF } },
		} },
	{ Name::DeepPurple,
		{
			{ Shade::Shade50, { 0xEDE7F6 } },
			{ Shade::Shade100, { 0xD1C4E9 } },
			{ Shade::Shade200, { 0xB39DDB } },
			{ Shade::Shade300, { 0x9575CD } },
			{ Shade::Shade400, { 0x7E57C2 } },
			{ Shade::Shade500, { 0x673AB7 } },
			{ Shade::Shade600, { 0x5E35B1 } },
			{ Shade::Shade700, { 0x512DA8 } },
			{ Shade::Shade800, { 0x4527A0 } },
			{ Shade::Shade900, { 0x311B92 } },
			{ Shade::ShadeA100, { 0xB388FF } },
			{ Shade::ShadeA200, { 0x7C4DFF } },
			{ Shade::ShadeA400, { 0x651FFF } },
			{ Shade::ShadeA700, { 0x6200EA } },
		} },
	{ Name::Indigo,
		{
			{ Shade::Shade50, { 0xE8EAF6 } },
			{ Shade::Shade100, { 0xC5CAE9 } },
			{ Shade::Shade200, { 0x9FA8DA } },
			{ Shade::Shade300, { 0x7986CB } },
			{ Shade::Shade400, { 0x5C6BC0 } },
			{ Shade::Shade500, { 0x3F51B5 } },
			{ Shade::Shade600, { 0x3949AB } },
			{ Shade::Shade700, { 0x303F9F } },
			{ Shade::Shade800, { 0x283593 } },
			{ Shade::Shade900, { 0x1A237E } },
			{ Shade::ShadeA100, { 0x8C9EFF } },
			{ Shade::ShadeA200, { 0x536DFE } },
			{ Shade::ShadeA400, { 0x3D5AFE } },
			{ Shade::ShadeA700, { 0x304FFE } },
		} },
	{ Name::Blue,
		{
			{ Shade::Shade50, { 0xE3F2FD } },
			{ Shade::Shade100, { 0xBBDEFB } },
			{ Shade::Shade200, { 0x90CAF9 } },
			{ Shade::Shade300, { 0x64B5F6 } },
			{ Shade::Shade400, { 0x42A5F5 } },
			{ Shade::Shade500, { 0x2196F3 } },
			{ Shade::Shade600, { 0x1E88E5 } },
			{ Shade::Shade700, { 0x1976D2 } },
			{ Shade::Shade800, { 0x1565C0 } },
			{ Shade::Shade900, { 0x0D47A1 } },
			{ Shade::ShadeA100, { 0x82B1FF } },
			{ Shade::ShadeA200, { 0x448AFF } },
			{ Shade::ShadeA400, { 0x2979FF } },
			{ Shade::ShadeA700, { 0x2962FF } },
		} },
	{ Name::LightBlue,
		{
			{ Shade::Shade50, { 0xE1F5FE } },
			{ Shade::Shade100, { 0xB3E5FC } },
			{ Shade::Shade200, { 0x81D4FA } },
			{ Shade::Shade300, { 0x4FC3F7 } },
			{ Shade::Shade400, { 0x29B6F6 } },
			{ Shade::Shade500, { 0x03A9F4 } },
			{ Shade::Shade600, { 0x039BE5 } },
			{ Shade::Shade700, { 0x0288D1 } },
			{ Shade::Shade800, { 0x0277BD } },
			{ Shade::Shade900, { 0x01579B } },
			{ Shade::ShadeA100, { 0x80D8FF } },
			{ Shade::ShadeA200, { 0x40C4FF } },
			{ Shade::ShadeA400, { 0x00B0FF } },
			{ Shade::ShadeA700, { 0x0091EA } },
		} },
	{ Name::Cyan,
		{
			{ Shade::Shade50, { 0xE0F7FA } },
			{ Shade::Shade100, { 0xB2EBF2 } },
			{ Shade::Shade200, { 0x80DEEA } },
			{ Shade::Shade300, { 0x4DD0E1 } },
			{ Shade::Shade400, { 0x26C6DA } },
			{ Shade::Shade500, { 0x00BCD4 } },
			{ Shade::Shade600, { 0x00ACC1 } },
			{ Shade::Shade700, { 0x0097A7 } },
			{ Shade::Shade800, { 0x00838F } },
			{ Shade::Shade900, { 0x006064 } },
			{ Shade::ShadeA100, { 0x84FFFF } },
			{ Shade::ShadeA200, { 0x18FFFF } },
			{ Shade::ShadeA400, { 0x00E5FF } },
			{ Shade::ShadeA700, { 0x00B8D4 } },
		} },
	{ Name::Teal,
		{
			{ Shade::Shade50, { 0xE0F2F1 } },
			{ Shade::Shade100, { 0xB2DFDB } },
			{ Shade::Shade200, { 0x80CBC4 } },
			{ Shade::Shade300, { 0x4DB6AC } },
			{ Shade::Shade400, { 0x26A69A } },
			{ Shade::Shade500, { 0x009688 } },
			{ Shade::Shade600, { 0x00897B } },
			{ Shade::Shade700, { 0x00796B } },
			{ Shade::Shade800, { 0x00695C } },
			{ Shade::Shade900, { 0x004D40 } },
			{ Shade::ShadeA100, { 0xA7FFEB } },
			{ Shade::ShadeA200, { 0x64FFDA } },
			{ Shade::ShadeA400, { 0x1DE9B6 } },
			{ Shade::ShadeA700, { 0x00BFA5 } },
		} },
	{ Name::Green,
		{
			{ Shade::Shade50, { 0xE8F5E9 } },
			{ Shade::Shade100, { 0xC8E6C9 } },
			{ Shade::Shade200, { 0xA5D6A7 } },
			{ Shade::Shade300, { 0x81C784 } },
			{ Shade::Shade400, { 0x66BB6A } },
			{ Shade::Shade500, { 0x4CAF50 } },
			{ Shade::Shade600, { 0x43A047 } },
			{ Shade::Shade700, { 0x388E3C } },
			{ Shade::Shade800, { 0x2E7D32 } },
			{ Shade::Shade900, { 0x1B5E20 } },
			{ Shade::ShadeA100, { 0xB9F6CA } },
			{ Shade::ShadeA200, { 0x69F0AE } },
			{ Shade::ShadeA400, { 0x00E676 } },
			{ Shade::ShadeA700, { 0x00C853 } },
		} },
	{ Name::LightGreen,
		{
			{ Shade::Shade50, { 0xF1F8E9 } },
			{ Shade::Shade100, { 0xDCEDC8 } },
			{ Shade::Shade200, { 0xC5E1A5 } },
			{ Shade::Shade300, { 0xAED581 } },
			{ Shade::Shade400, { 0x9CCC65 } },
			{ Shade::Shade500, { 0x8BC34A } },
			{ Shade::Shade600, { 0x7CB342 } },
			{ Shade::Shade700, { 0x689F38 } },
			{ Shade::Shade800, { 0x558B2F } },
			{ Shade::Shade900, { 0xC33691E } },
			{ Shade::ShadeA100, { 0xCCFF90 } },
			{ Shade::ShadeA200, { 0xB2FF59 } },
			{ Shade::ShadeA400, { 0x76FF03 } },
			{ Shade::ShadeA700, { 0x64DD17 } },
		} },
	{ Name::Lime,
		{
			{ Shade::Shade50, { 0xF9FBE7 } },
			{ Shade::Shade100, { 0xF0F4C3 } },
			{ Shade::Shade200, { 0xE6EE9C } },
			{ Shade::Shade300, { 0xDCE775 } },
			{ Shade::Shade400, { 0xD4E157 } },
			{ Shade::Shade500, { 0xCDDC39 } },
			{ Shade::Shade600, { 0xC0CA33 } },
			{ Shade::Shade700, { 0xAFB42B } },
			{ Shade::Shade800, { 0x9E9D24 } },
			{ Shade::Shade900, { 0x827717 } },
			{ Shade::ShadeA100, { 0xF4FF81 } },
			{ Shade::ShadeA200, { 0xEEFF41 } },
			{ Shade::ShadeA400, { 0xC6FF00 } },
			{ Shade::ShadeA700, { 0xAEEA00 } },
		} },
	{ Name::Yellow,
		{
			{ Shade::Shade50, { 0xFFFDE7 } },
			{ Shade::Shade100, { 0xFFF9C4 } },
			{ Shade::Shade200, { 0xFFF59D } },
			{ Shade::Shade300, { 0xFFF176 } },
			{ Shade::Shade400, { 0xFFEE58 } },
			{ Shade::Shade500, { 0xFFEB3B } },
			{ Shade::Shade600, { 0xFDD835 } },
			{ Shade::Shade700, { 0xFBC02D } },
			{ Shade::Shade800, { 0xF9A825 } },
			{ Shade::Shade900, { 0xF57F17 } },
			{ Shade::ShadeA100, { 0xFFFF8D } },
			{ Shade::ShadeA200, { 0xFFFF00 } },
			{ Shade::ShadeA400, { 0xFFEA00 } },
			{ Shade::ShadeA700, { 0xFFD600 } },
		} },
	{ Name::Amber,
		{
			{ Shade::Shade50, { 0xFFF8E1 } },
			{ Shade::Shade100, { 0xFFECB3 } },
			{ Shade::Shade200, { 0xFFE082 } },
			{ Shade::Shade300, { 0xFFD54F } },
			{ Shade::Shade400, { 0xFFCA28 } },
			{ Shade::Shade500, { 0xFFC107 } },
			{ Shade::Shade600, { 0xFFB300 } },
			{ Shade::Shade700, { 0xFFA000 } },
			{ Shade::Shade800, { 0xFF8F00 } },
			{ Shade::Shade900, { 0xFF6F00 } },
			{ Shade::ShadeA100, { 0xFFE57F } },
			{ Shade::ShadeA200, { 0xFFD740 } },
			{ Shade::ShadeA400, { 0xFFC400 } },
			{ Shade::ShadeA700, { 0xFFAB00 } },
		} },
	{ Name::Orange,
		{
			{ Shade::Shade50, { 0xFFF3E0 } },
			{ Shade::Shade100, { 0xFFE0B2 } },
			{ Shade::Shade200, { 0xFFCC80 } },
			{ Shade::Shade300, { 0xFFB74D } },
			{ Shade::Shade400, { 0xFFA726 } },
			{ Shade::Shade500, { 0xFF9800 } },
			{ Shade::Shade600, { 0xFB8C00 } },
			{ Shade::Shade700, { 0xF57C00 } },
			{ Shade::Shade800, { 0xEF6C00 } },
			{ Shade::Shade900, { 0xE65100 } },
			{ Shade::ShadeA100, { 0xFFD180 } },
			{ Shade::ShadeA200, { 0xFFAB40 } },
			{ Shade::ShadeA400, { 0xFF9100 } },
			{ Shade::ShadeA700, { 0xFF6D00 } },
		} },
	{ Name::DeepOrange,
		{
			{ Shade::Shade50, { 0xFBE9E7 } },
			{ Shade::Shade100, { 0xFFCCBC } },
			{ Shade::Shade200, { 0xFFAB91 } },
			{ Shade::Shade300, { 0xFF8A65 } },
			{ Shade::Shade400, { 0xFF7043 } },
			{ Shade::Shade500, { 0xFF5722 } },
			{ Shade::Shade600, { 0xF4511E } },
			{ Shade::Shade700, { 0xE64A19 } },
			{ Shade::Shade800, { 0xD84315 } },
			{ Shade::Shade900, { 0xBF360C } },
			{ Shade::ShadeA100, { 0xFF9E80 } },
			{ Shade::ShadeA200, { 0xFF6E40 } },
			{ Shade::ShadeA400, { 0xFF3D00 } },
			{ Shade::ShadeA700, { 0xDD2C00 } },
		} },
	{ Name::Brown,
		{
			{ Shade::Shade50, { 0xEFEBE9 } },
			{ Shade::Shade100, { 0xD7CCC8 } },
			{ Shade::Shade200, { 0xBCAAA4 } },
			{ Shade::Shade300, { 0xA1887F } },
			{ Shade::Shade400, { 0x8D6E63 } },
			{ Shade::Shade500, { 0x795548 } },
			{ Shade::Shade600, { 0x6D4C41 } },
			{ Shade::Shade700, { 0x5D4037 } },
			{ Shade::Shade800, { 0x4E342E } },
			{ Shade::Shade900, { 0x3E2723 } },
			{ Shade::ShadeA100, { 0xA1887F } },
			{ Shade::ShadeA200, { 0x795548 } },
			{ Shade::ShadeA400, { 0x5D4037 } },
			{ Shade::ShadeA700, { 0x3E2723 } },
		} },
	{ Name::Gray,
		{
			{ Shade::Shade50, { 0xFAFAFA } },
			{ Shade::Shade100, { 0xF5F5F5 } },
			{ Shade::Shade200, { 0xEEEEEE } },
			{ Shade::Shade300, { 0xE0E0E0 } },
			{ Shade::Shade400, { 0xBDBDBD } },
			{ Shade::Shade500, { 0x9E9E9E } },
			{ Shade::Shade600, { 0x757575 } },
			{ Shade::Shade700, { 0x616161 } },
			{ Shade::Shade800, { 0x424242 } },
			{ Shade::Shade900, { 0x212121 } },
			{ Shade::ShadeA100, { 0xE0E0E0 } },
			{ Shade::ShadeA200, { 0x9E9E9E } },
			{ Shade::ShadeA400, { 0x616161 } },
			{ Shade::ShadeA700, { 0x212121 } },
		} },
	{ Name::BlueGray,
		{
			{ Shade::Shade50, { 0xECEFF1 } },
			{ Shade::Shade100, { 0xCFD8DC } },
			{ Shade::Shade200, { 0xB0BEC5 } },
			{ Shade::Shade300, { 0x90A4AE } },
			{ Shade::Shade400, { 0x78909C } },
			{ Shade::Shade500, { 0x607D8B } },
			{ Shade::Shade600, { 0x546E7A } },
			{ Shade::Shade700, { 0x455A64 } },
			{ Shade::Shade800, { 0x37474F } },
			{ Shade::Shade900, { 0x263238 } },
			{ Shade::ShadeA100, { 0x90A4AE } },
			{ Shade::ShadeA200, { 0x607D8B } },
			{ Shade::ShadeA400, { 0x455A64 } },
			{ Shade::ShadeA700, { 0x263238 } },
		} },
	{ Name::Black,
		{
			{ Shade::Shade50, { "black" } },
			{ Shade::Shade100, { "black" } },
			{ Shade::Shade200, { "black" } },
			{ Shade::Shade300, { "black" } },
			{ Shade::Shade400, { "black" } },
			{ Shade::Shade500, { "black" } },
			{ Shade::Shade600, { "black" } },
			{ Shade::Shade700, { "black" } },
			{ Shade::Shade800, { "black" } },
			{ Shade::Shade900, { "black" } },
			{ Shade::ShadeA100, { "black" } },
			{ Shade::ShadeA200, { "black" } },
			{ Shade::ShadeA400, { "black" } },
			{ Shade::ShadeA700, { "black" } },
		} },
	{ Name::White,
		{
			{ Shade::Shade50, { "white" } },
			{ Shade::Shade100, { "white" } },
			{ Shade::Shade200, { "white" } },
			{ Shade::Shade300, { "white" } },
			{ Shade::Shade400, { "white" } },
			{ Shade::Shade500, { "white" } },
			{ Shade::Shade600, { "white" } },
			{ Shade::Shade700, { "white" } },
			{ Shade::Shade800, { "white" } },
			{ Shade::Shade900, { "white" } },
			{ Shade::ShadeA100, { "white" } },
			{ Shade::ShadeA200, { "white" } },
			{ Shade::ShadeA400, { "white" } },
			{ Shade::ShadeA700, { "white" } },
		} },
};

using ShadeColorMap = std::unordered_map<Shade, QColor>;
using NameShadeColorMap = std::unordered_map<Name, ShadeColorMap>;
static const NameShadeColorMap g_errorMap = {
	{ Name::Red,
		{
			{ Shade::Shade300, 0x000000 },
			{ Shade::Shade600, 0x000000 },
			{ Shade::Shade800, 0x000000 },
			{ Shade::Shade900, 0x000000 },
		} },
	{ Name::Pink,
		{
			{ Shade::Shade300, 0x000000 },
			{ Shade::Shade600, 0x000000 },
			{ Shade::Shade800, 0x000000 },
			{ Shade::Shade900, 0x000000 },
		} },
	{ Name::Purple,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xE57373 },
			{ Shade::Shade900, 0xE57373 },
		} },
	{ Name::DeepPurple,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xE57373 },
			{ Shade::Shade900, 0xE57373 },
		} },
	{ Name::Indigo,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xEF5350 },
			{ Shade::Shade900, 0xEF5350 },
		} },
	{ Name::Blue,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xE57373 },
			{ Shade::Shade900, 0xE57373 },
		} },
	{ Name::LightBlue,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xE57373 },
			{ Shade::Shade900, 0xE57373 },
		} },
	{ Name::Cyan,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xE57373 },
			{ Shade::Shade900, 0xE57373 },
		} },
	{ Name::Teal,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xEF5350 },
			{ Shade::Shade900, 0xEF5350 },
		} },
	{ Name::Green,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xEF5350 },
			{ Shade::Shade900, 0xEF5350 },
		} },
	{ Name::LightGreen,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xEF5350 },
			{ Shade::Shade900, 0xEF5350 },
		} },
	{ Name::Lime,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xB71C1C },
			{ Shade::Shade900, 0xB71C1C },
		} },
	{ Name::Yellow,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xD50000 },
			{ Shade::Shade900, 0xD50000 },
		} },
	{ Name::Amber,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xD50000 },
			{ Shade::Shade900, 0xD50000 },
		} },
	{ Name::Orange,
		{
			{ Shade::Shade300, 0x000000 },
			{ Shade::Shade600, 0x000000 },
			{ Shade::Shade800, 0x000000 },
			{ Shade::Shade900, 0x000000 },
		} },
	{ Name::DeepOrange,
		{
			{ Shade::Shade300, 0x000000 },
			{ Shade::Shade600, 0x000000 },
			{ Shade::Shade800, 0x000000 },
			{ Shade::Shade900, 0x000000 },
		} },
	{ Name::Brown,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade800, 0xEF5350 },
			{ Shade::Shade900, 0xEF5350 },
		} },
	{ Name::Gray,
		{
			{ Shade::Shade300, 0xFF5252 },
			{ Shade::Shade600, 0xFF5252 },
			{ Shade::Shade800, 0xFF5252 },
			{ Shade::Shade900, 0xFF5252 },
		} },
	{ Name::BlueGray,
		{
			{ Shade::Shade300, 0xFF5252 },
			{ Shade::Shade600, 0xFF5252 },
			{ Shade::Shade800, 0xFF5252 },
			{ Shade::Shade900, 0xFF5252 },
		} },
};

static const NameShadeColorMap g_warnMap = {
	{ Name::Red,
		{
			{ Shade::Shade800, 0xFF9100 },
		} },
	{ Name::Pink,
		{
			{ Shade::Shade800, 0xFF9100 },
		} },
	{ Name::Purple,
		{
			{ Shade::Shade800, 0xFF9100 },
		} },
	{ Name::DeepPurple,
		{
			{ Shade::Shade800, 0xFF9100 },
		} },
	{ Name::Indigo,
		{
			{ Shade::Shade800, 0xFF9100 },
		} },
	{ Name::Blue,
		{
			{ Shade::Shade800, 0xFF9100 },
		} },
	{ Name::LightBlue,
		{
			{ Shade::Shade800, 0xFFA726 },
		} },
	{ Name::Cyan,
		{
			{ Shade::Shade800, 0xFFA726 },
		} },
	{ Name::Teal,
		{
			{ Shade::Shade800, 0xFF9100 },
		} },
	{ Name::Green,
		{
			{ Shade::Shade800, 0xFFA726 },
		} },
	{ Name::LightGreen,
		{
			{ Shade::Shade800, 0xFFA726 },
		} },
	{ Name::Lime,
		{
			{ Shade::Shade800, 0xFFB74D },
		} },
	{ Name::Yellow,
		{
			{ Shade::Shade800, 0xE65100 },
		} },
	{ Name::Amber,
		{
			{ Shade::Shade800, 0xE65100 },
		} },
	{ Name::Orange,
		{
			{ Shade::Shade800, 0xFFB74D },
		} },
	{ Name::DeepOrange,
		{
			{ Shade::Shade800, 0xFFB74D },
		} },
	{ Name::Brown,
		{
			{ Shade::Shade800, 0xFF9100 },
		} },
	{ Name::Gray,
		{
			{ Shade::Shade800, 0xFF9100 },
		} },
	{ Name::BlueGray,
		{
			{ Shade::Shade800, 0xFF9100 },
		} },
};

static const NameShadeColorMap g_infoMap = {
	{ Name::Red,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::Pink,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::Purple,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::DeepPurple,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::Indigo,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::Blue,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::LightBlue,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::Cyan,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::Teal,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::Green,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::LightGreen,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::Lime,
		{
			{ Shade::Shade800, 0xBBDEFB },
		} },
	{ Name::Yellow,
		{
			{ Shade::Shade800, 0x2979FF },
		} },
	{ Name::Amber,
		{
			{ Shade::Shade800, 0x2979FF },
		} },
	{ Name::Orange,
		{
			{ Shade::Shade800, 0xBBDEFB },
		} },
	{ Name::DeepOrange,
		{
			{ Shade::Shade800, 0xBBDEFB },
		} },
	{ Name::Brown,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::Gray,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
	{ Name::BlueGray,
		{
			{ Shade::Shade800, 0x90CAF9 },
		} },
};

QColor complementary(QColor const& baseColor)
{
	auto const r = 255 - baseColor.red();
	auto const g = 255 - baseColor.green();
	auto const b = 255 - baseColor.blue();

	return QColor::fromRgb(r, g, b);
}

static void buildLuminanceMap()
{
	for (auto& [name, shadeMap] : g_colorMap)
	{
		for (auto& [shade, colData] : shadeMap)
		{
			auto const hsl = colData.value.toHsl();
			auto const light = hsl.lightness();
			auto const lum = light >= 127 ? Luminance::Light : Luminance::Dark;
			colData.luminance = lum;
		}
	}
}

static ShadeMap const& shadeMap(Name const name)
{
	// First time we're called, build the luminance map
	static std::once_flag once;
	std::call_once(once,
		[]()
		{
			buildLuminanceMap();
		});

	auto const it = g_colorMap.find(name);
	if (it == std::end(g_colorMap))
	{
		throw std::invalid_argument("Unknown Name");
	}
	return it->second;
}

static ColorData const& colorData(Name const name, Shade const shade)
{
	auto const& map = shadeMap(name);
	auto const jt = map.find(shade);
	if (jt == std::end(map))
	{
		throw std::invalid_argument("Invalid Shade");
	}
	return jt->second;
}

QColor value(Name const name, Shade const shade)
{
	return colorData(name, shade).value;
}

QColor foregroundColor()
{
	return isDarkColorScheme() ? Qt::white : Qt::black;
}

QColor backgroundColor()
{
	return isDarkColorScheme() ? Qt::black : Qt::white;
}

Name backgroundColorName(Luminance const luminance)
{
	return luminance == Luminance::Light ? Name::White : Name::Black;
}

Name backgroundColorName()
{
	return isDarkColorScheme() ? Name::Black : Name::White;
}

QColor foregroundValue(Name const name, Shade const shade)
{
	auto const l = luminance(name, shade);
	return QColor{ l == Luminance::Light ? Qt::black : Qt::white };
}

QColor complementaryValue(Name const name, Shade const shade)
{
	auto const baseColor = value(name, shade);
	return complementary(baseColor);
}

QColor foregroundComplementaryValue(Name const name, Shade const shade)
{
	auto const baseColor = foregroundValue(name, shade);
	return complementary(baseColor);
}

QColor foregroundErrorColorValue(Name const name, Shade const shade)
{
	auto errorColor = value(Name::Red, qtMate::material::color::Shade::Shade400);

	if (auto const nameMapIt = g_errorMap.find(name); nameMapIt != g_errorMap.end())
	{
		auto const& map = nameMapIt->second;
		if (auto const shadeMapIt = map.find(shade); shadeMapIt != map.end())
		{
			errorColor = shadeMapIt->second;
		}
	}

	return errorColor;
}

QColor foregroundWarningColorValue(Name const name, Shade const shade)
{
	auto errorColor = value(Name::Orange, qtMate::material::color::Shade::ShadeA700);

	if (auto const nameMapIt = g_warnMap.find(name); nameMapIt != g_warnMap.end())
	{
		auto const& map = nameMapIt->second;
		if (auto const shadeMapIt = map.find(shade); shadeMapIt != map.end())
		{
			errorColor = shadeMapIt->second;
		}
	}

	return errorColor;
}

QColor foregroundInformationColorValue(Name const name, Shade const shade)
{
	auto errorColor = value(Name::Blue, qtMate::material::color::Shade::ShadeA400);

	if (auto const nameMapIt = g_infoMap.find(name); nameMapIt != g_infoMap.end())
	{
		auto const& map = nameMapIt->second;
		if (auto const shadeMapIt = map.find(shade); shadeMapIt != map.end())
		{
			errorColor = shadeMapIt->second;
		}
	}

	return errorColor;
}

Luminance luminance(Name const name, Shade const shade)
{
	return colorData(name, shade).luminance;
}

QBrush buildBrush(Name const name, Shade const shade) noexcept
{
	auto const& shades = shadeMap(name);
	auto const it = shades.find(shade);
	if (it != std::end(shades))
	{
		return QBrush{ it->second.value };
	}

	return QBrush{ Qt::darkGray, Qt::BDiagPattern };
}

QBrush brush(Name const name, Shade const shade) noexcept
{
	using Shades = std::unordered_map<Shade, QBrush>;
	using Colors = std::unordered_map<Name, Shades>;

	static Colors s_colors;

	auto& shades = s_colors[name];
	auto const it = shades.find(shade);
	if (it == std::end(shades))
	{
		shades[shade] = buildBrush(name, shade);
	}

	return shades[shade];
}

Shade colorSchemeShade()
{
	return isDarkColorScheme() ? DefaultDarkShade : DefaultLightShade;
}


bool isDarkColorScheme()
{
	// First check for an application property
	auto const prop = qApp->property("isDarkColorScheme");
	if (prop.isValid())
	{
		return prop.toBool();
	}

	// If no application property, check the style hints
	return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

QColor disabledForegroundColor()
{
	return value(Name::Gray, isDarkColorScheme() ? Shade::ShadeA200 : Shade::Shade500);
}

} // namespace color
} // namespace material
} // namespace qtMate
