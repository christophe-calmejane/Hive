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

#include <unordered_map>
#include <stdexcept>

namespace qtMate
{
namespace material
{
namespace color
{
struct ColorData
{
	QColor value; // The actual color value
	Luminance luminance; // The associated luminance
};

using ShadeMap = std::unordered_map<Shade, ColorData>;
using ColorMap = std::unordered_map<Name, ShadeMap>;

static const ColorMap g_colorMap = {
	{ Name::Red,
		{
			{ Shade::Shade50, { 0xFFEBEE, Luminance::Light } },
			{ Shade::Shade100, { 0xFFCDD2, Luminance::Light } },
			{ Shade::Shade200, { 0xEF9A9A, Luminance::Light } },
			{ Shade::Shade300, { 0xE57373, Luminance::Light } },
			{ Shade::Shade400, { 0xEF5350, Luminance::Dark } },
			{ Shade::Shade500, { 0xF44336, Luminance::Dark } },
			{ Shade::Shade600, { 0xE53935, Luminance::Dark } },
			{ Shade::Shade700, { 0xD32F2F, Luminance::Dark } },
			{ Shade::Shade800, { 0xC62828, Luminance::Dark } },
			{ Shade::Shade900, { 0xB71C1C, Luminance::Dark } },
			{ Shade::ShadeA100, { 0xFF8A80, Luminance::Light } },
			{ Shade::ShadeA200, { 0xFF5252, Luminance::Dark } },
			{ Shade::ShadeA400, { 0xFF1744, Luminance::Dark } },
			{ Shade::ShadeA700, { 0xD50000, Luminance::Dark } },
		} },
	{ Name::Pink,
		{
			{ Shade::Shade50, { 0xFCE4EC, Luminance::Light } },
			{ Shade::Shade100, { 0xF8BBD0, Luminance::Light } },
			{ Shade::Shade200, { 0xF48FB1, Luminance::Light } },
			{ Shade::Shade300, { 0xF06292, Luminance::Light } },
			{ Shade::Shade400, { 0xEC407A, Luminance::Dark } },
			{ Shade::Shade500, { 0xE91E63, Luminance::Dark } },
			{ Shade::Shade600, { 0xD81B60, Luminance::Dark } },
			{ Shade::Shade700, { 0xC2185B, Luminance::Dark } },
			{ Shade::Shade800, { 0xAD1457, Luminance::Dark } },
			{ Shade::Shade900, { 0x880E4F, Luminance::Dark } },
			{ Shade::ShadeA100, { 0xFF80AB, Luminance::Light } },
			{ Shade::ShadeA200, { 0xFF4081, Luminance::Dark } },
			{ Shade::ShadeA400, { 0xF50057, Luminance::Dark } },
			{ Shade::ShadeA700, { 0xC51162, Luminance::Dark } },
		} },
	{ Name::Purple,
		{
			{ Shade::Shade50, { 0xF3E5F5, Luminance::Light } },
			{ Shade::Shade100, { 0xE1BEE7, Luminance::Light } },
			{ Shade::Shade200, { 0xCE93D8, Luminance::Light } },
			{ Shade::Shade300, { 0xBA68C8, Luminance::Dark } },
			{ Shade::Shade400, { 0xAB47BC, Luminance::Dark } },
			{ Shade::Shade500, { 0x9C27B0, Luminance::Dark } },
			{ Shade::Shade600, { 0x8E24AA, Luminance::Dark } },
			{ Shade::Shade700, { 0x7B1FA2, Luminance::Dark } },
			{ Shade::Shade800, { 0x6A1B9A, Luminance::Dark } },
			{ Shade::Shade900, { 0x4A148C, Luminance::Dark } },
			{ Shade::ShadeA100, { 0xEA80FC, Luminance::Light } },
			{ Shade::ShadeA200, { 0xE040FB, Luminance::Dark } },
			{ Shade::ShadeA400, { 0xD500F9, Luminance::Dark } },
			{ Shade::ShadeA700, { 0xAA00FF, Luminance::Dark } },
		} },
	{ Name::DeepPurple,
		{
			{ Shade::Shade50, { 0xEDE7F6, Luminance::Light } },
			{ Shade::Shade100, { 0xD1C4E9, Luminance::Light } },
			{ Shade::Shade200, { 0xB39DDB, Luminance::Light } },
			{ Shade::Shade300, { 0x9575CD, Luminance::Dark } },
			{ Shade::Shade400, { 0x7E57C2, Luminance::Dark } },
			{ Shade::Shade500, { 0x673AB7, Luminance::Dark } },
			{ Shade::Shade600, { 0x5E35B1, Luminance::Dark } },
			{ Shade::Shade700, { 0x512DA8, Luminance::Dark } },
			{ Shade::Shade800, { 0x4527A0, Luminance::Dark } },
			{ Shade::Shade900, { 0x311B92, Luminance::Dark } },
			{ Shade::ShadeA100, { 0xB388FF, Luminance::Light } },
			{ Shade::ShadeA200, { 0x7C4DFF, Luminance::Dark } },
			{ Shade::ShadeA400, { 0x651FFF, Luminance::Dark } },
			{ Shade::ShadeA700, { 0x6200EA, Luminance::Dark } },
		} },
	{ Name::Indigo,
		{
			{ Shade::Shade50, { 0xE8EAF6, Luminance::Light } },
			{ Shade::Shade100, { 0xC5CAE9, Luminance::Light } },
			{ Shade::Shade200, { 0x9FA8DA, Luminance::Light } },
			{ Shade::Shade300, { 0x7986CB, Luminance::Dark } },
			{ Shade::Shade400, { 0x5C6BC0, Luminance::Dark } },
			{ Shade::Shade500, { 0x3F51B5, Luminance::Dark } },
			{ Shade::Shade600, { 0x3949AB, Luminance::Dark } },
			{ Shade::Shade700, { 0x303F9F, Luminance::Dark } },
			{ Shade::Shade800, { 0x283593, Luminance::Dark } },
			{ Shade::Shade900, { 0x1A237E, Luminance::Dark } },
			{ Shade::ShadeA100, { 0x8C9EFF, Luminance::Light } },
			{ Shade::ShadeA200, { 0x536DFE, Luminance::Dark } },
			{ Shade::ShadeA400, { 0x3D5AFE, Luminance::Dark } },
			{ Shade::ShadeA700, { 0x304FFE, Luminance::Dark } },
		} },
	{ Name::Blue,
		{
			{ Shade::Shade50, { 0xE3F2FD, Luminance::Light } },
			{ Shade::Shade100, { 0xBBDEFB, Luminance::Light } },
			{ Shade::Shade200, { 0x90CAF9, Luminance::Light } },
			{ Shade::Shade300, { 0x64B5F6, Luminance::Light } },
			{ Shade::Shade400, { 0x42A5F5, Luminance::Light } },
			{ Shade::Shade500, { 0x2196F3, Luminance::Light } },
			{ Shade::Shade600, { 0x1E88E5, Luminance::Dark } },
			{ Shade::Shade700, { 0x1976D2, Luminance::Dark } },
			{ Shade::Shade800, { 0x1565C0, Luminance::Dark } },
			{ Shade::Shade900, { 0x0D47A1, Luminance::Dark } },
			{ Shade::ShadeA100, { 0x82B1FF, Luminance::Light } },
			{ Shade::ShadeA200, { 0x448AFF, Luminance::Dark } },
			{ Shade::ShadeA400, { 0x2979FF, Luminance::Dark } },
			{ Shade::ShadeA700, { 0x2962FF, Luminance::Dark } },
		} },
	{ Name::LightBlue,
		{
			{ Shade::Shade50, { 0xE1F5FE, Luminance::Light } },
			{ Shade::Shade100, { 0xB3E5FC, Luminance::Light } },
			{ Shade::Shade200, { 0x81D4FA, Luminance::Light } },
			{ Shade::Shade300, { 0x4FC3F7, Luminance::Light } },
			{ Shade::Shade400, { 0x29B6F6, Luminance::Light } },
			{ Shade::Shade500, { 0x03A9F4, Luminance::Light } },
			{ Shade::Shade600, { 0x039BE5, Luminance::Light } },
			{ Shade::Shade700, { 0x0288D1, Luminance::Dark } },
			{ Shade::Shade800, { 0x0277BD, Luminance::Dark } },
			{ Shade::Shade900, { 0x01579B, Luminance::Dark } },
			{ Shade::ShadeA100, { 0x80D8FF, Luminance::Light } },
			{ Shade::ShadeA200, { 0x40C4FF, Luminance::Light } },
			{ Shade::ShadeA400, { 0x00B0FF, Luminance::Light } },
			{ Shade::ShadeA700, { 0x0091EA, Luminance::Dark } },
		} },
	{ Name::Cyan,
		{
			{ Shade::Shade50, { 0xE0F7FA, Luminance::Light } },
			{ Shade::Shade100, { 0xB2EBF2, Luminance::Light } },
			{ Shade::Shade200, { 0x80DEEA, Luminance::Light } },
			{ Shade::Shade300, { 0x4DD0E1, Luminance::Light } },
			{ Shade::Shade400, { 0x26C6DA, Luminance::Light } },
			{ Shade::Shade500, { 0x00BCD4, Luminance::Light } },
			{ Shade::Shade600, { 0x00ACC1, Luminance::Light } },
			{ Shade::Shade700, { 0x0097A7, Luminance::Dark } },
			{ Shade::Shade800, { 0x00838F, Luminance::Dark } },
			{ Shade::Shade900, { 0x006064, Luminance::Dark } },
			{ Shade::ShadeA100, { 0x84FFFF, Luminance::Light } },
			{ Shade::ShadeA200, { 0x18FFFF, Luminance::Light } },
			{ Shade::ShadeA400, { 0x00E5FF, Luminance::Light } },
			{ Shade::ShadeA700, { 0x00B8D4, Luminance::Light } },
		} },
	{ Name::Teal,
		{
			{ Shade::Shade50, { 0xE0F2F1, Luminance::Light } },
			{ Shade::Shade100, { 0xB2DFDB, Luminance::Light } },
			{ Shade::Shade200, { 0x80CBC4, Luminance::Light } },
			{ Shade::Shade300, { 0x4DB6AC, Luminance::Light } },
			{ Shade::Shade400, { 0x26A69A, Luminance::Light } },
			{ Shade::Shade500, { 0x009688, Luminance::Dark } },
			{ Shade::Shade600, { 0x00897B, Luminance::Dark } },
			{ Shade::Shade700, { 0x00796B, Luminance::Dark } },
			{ Shade::Shade800, { 0x00695C, Luminance::Dark } },
			{ Shade::Shade900, { 0x004D40, Luminance::Dark } },
			{ Shade::ShadeA100, { 0xA7FFEB, Luminance::Light } },
			{ Shade::ShadeA200, { 0x64FFDA, Luminance::Light } },
			{ Shade::ShadeA400, { 0x1DE9B6, Luminance::Light } },
			{ Shade::ShadeA700, { 0x00BFA5, Luminance::Light } },
		} },
	{ Name::Green,
		{
			{ Shade::Shade50, { 0xE8F5E9, Luminance::Light } },
			{ Shade::Shade100, { 0xC8E6C9, Luminance::Light } },
			{ Shade::Shade200, { 0xA5D6A7, Luminance::Light } },
			{ Shade::Shade300, { 0x81C784, Luminance::Light } },
			{ Shade::Shade400, { 0x66BB6A, Luminance::Light } },
			{ Shade::Shade500, { 0x4CAF50, Luminance::Light } },
			{ Shade::Shade600, { 0x43A047, Luminance::Dark } },
			{ Shade::Shade700, { 0x388E3C, Luminance::Dark } },
			{ Shade::Shade800, { 0x2E7D32, Luminance::Dark } },
			{ Shade::Shade900, { 0x1B5E20, Luminance::Dark } },
			{ Shade::ShadeA100, { 0xB9F6CA, Luminance::Light } },
			{ Shade::ShadeA200, { 0x69F0AE, Luminance::Light } },
			{ Shade::ShadeA400, { 0x00E676, Luminance::Light } },
			{ Shade::ShadeA700, { 0x00C853, Luminance::Light } },
		} },
	{ Name::LightGreen,
		{
			{ Shade::Shade50, { 0xF1F8E9, Luminance::Light } },
			{ Shade::Shade100, { 0xDCEDC8, Luminance::Light } },
			{ Shade::Shade200, { 0xC5E1A5, Luminance::Light } },
			{ Shade::Shade300, { 0xAED581, Luminance::Light } },
			{ Shade::Shade400, { 0x9CCC65, Luminance::Light } },
			{ Shade::Shade500, { 0x8BC34A, Luminance::Light } },
			{ Shade::Shade600, { 0x7CB342, Luminance::Light } },
			{ Shade::Shade700, { 0x689F38, Luminance::Light } },
			{ Shade::Shade800, { 0x558B2F, Luminance::Dark } },
			{ Shade::Shade900, { 0xC33691E, Luminance::Dark } },
			{ Shade::ShadeA100, { 0xCCFF90, Luminance::Light } },
			{ Shade::ShadeA200, { 0xB2FF59, Luminance::Light } },
			{ Shade::ShadeA400, { 0x76FF03, Luminance::Light } },
			{ Shade::ShadeA700, { 0x64DD17, Luminance::Light } },
		} },
	{ Name::Lime,
		{
			{ Shade::Shade50, { 0xF9FBE7, Luminance::Light } },
			{ Shade::Shade100, { 0xF0F4C3, Luminance::Light } },
			{ Shade::Shade200, { 0xE6EE9C, Luminance::Light } },
			{ Shade::Shade300, { 0xDCE775, Luminance::Light } },
			{ Shade::Shade400, { 0xD4E157, Luminance::Light } },
			{ Shade::Shade500, { 0xCDDC39, Luminance::Light } },
			{ Shade::Shade600, { 0xC0CA33, Luminance::Light } },
			{ Shade::Shade700, { 0xAFB42B, Luminance::Light } },
			{ Shade::Shade800, { 0x9E9D24, Luminance::Light } },
			{ Shade::Shade900, { 0x827717, Luminance::Dark } },
			{ Shade::ShadeA100, { 0xF4FF81, Luminance::Light } },
			{ Shade::ShadeA200, { 0xEEFF41, Luminance::Light } },
			{ Shade::ShadeA400, { 0xC6FF00, Luminance::Light } },
			{ Shade::ShadeA700, { 0xAEEA00, Luminance::Light } },
		} },
	{ Name::Yellow,
		{
			{ Shade::Shade50, { 0xFFFDE7, Luminance::Light } },
			{ Shade::Shade100, { 0xFFF9C4, Luminance::Light } },
			{ Shade::Shade200, { 0xFFF59D, Luminance::Light } },
			{ Shade::Shade300, { 0xFFF176, Luminance::Light } },
			{ Shade::Shade400, { 0xFFEE58, Luminance::Light } },
			{ Shade::Shade500, { 0xFFEB3B, Luminance::Light } },
			{ Shade::Shade600, { 0xFDD835, Luminance::Light } },
			{ Shade::Shade700, { 0xFBC02D, Luminance::Light } },
			{ Shade::Shade800, { 0xF9A825, Luminance::Light } },
			{ Shade::Shade900, { 0xF57F17, Luminance::Light } },
			{ Shade::ShadeA100, { 0xFFFF8D, Luminance::Light } },
			{ Shade::ShadeA200, { 0xFFFF00, Luminance::Light } },
			{ Shade::ShadeA400, { 0xFFEA00, Luminance::Light } },
			{ Shade::ShadeA700, { 0xFFD600, Luminance::Light } },
		} },
	{ Name::Amber,
		{
			{ Shade::Shade50, { 0xFFF8E1, Luminance::Light } },
			{ Shade::Shade100, { 0xFFECB3, Luminance::Light } },
			{ Shade::Shade200, { 0xFFE082, Luminance::Light } },
			{ Shade::Shade300, { 0xFFD54F, Luminance::Light } },
			{ Shade::Shade400, { 0xFFCA28, Luminance::Light } },
			{ Shade::Shade500, { 0xFFC107, Luminance::Light } },
			{ Shade::Shade600, { 0xFFB300, Luminance::Light } },
			{ Shade::Shade700, { 0xFFA000, Luminance::Light } },
			{ Shade::Shade800, { 0xFF8F00, Luminance::Light } },
			{ Shade::Shade900, { 0xFF6F00, Luminance::Light } },
			{ Shade::ShadeA100, { 0xFFE57F, Luminance::Light } },
			{ Shade::ShadeA200, { 0xFFD740, Luminance::Light } },
			{ Shade::ShadeA400, { 0xFFC400, Luminance::Light } },
			{ Shade::ShadeA700, { 0xFFAB00, Luminance::Light } },
		} },
	{ Name::Orange,
		{
			{ Shade::Shade50, { 0xFFF3E0, Luminance::Light } },
			{ Shade::Shade100, { 0xFFE0B2, Luminance::Light } },
			{ Shade::Shade200, { 0xFFCC80, Luminance::Light } },
			{ Shade::Shade300, { 0xFFB74D, Luminance::Light } },
			{ Shade::Shade400, { 0xFFA726, Luminance::Light } },
			{ Shade::Shade500, { 0xFF9800, Luminance::Light } },
			{ Shade::Shade600, { 0xFB8C00, Luminance::Light } },
			{ Shade::Shade700, { 0xF57C00, Luminance::Light } },
			{ Shade::Shade800, { 0xEF6C00, Luminance::Light } },
			{ Shade::Shade900, { 0xE65100, Luminance::Dark } },
			{ Shade::ShadeA100, { 0xFFD180, Luminance::Light } },
			{ Shade::ShadeA200, { 0xFFAB40, Luminance::Light } },
			{ Shade::ShadeA400, { 0xFF9100, Luminance::Light } },
			{ Shade::ShadeA700, { 0xFF6D00, Luminance::Light } },
		} },
	{ Name::DeepOrange,
		{
			{ Shade::Shade50, { 0xFBE9E7, Luminance::Light } },
			{ Shade::Shade100, { 0xFFCCBC, Luminance::Light } },
			{ Shade::Shade200, { 0xFFAB91, Luminance::Light } },
			{ Shade::Shade300, { 0xFF8A65, Luminance::Light } },
			{ Shade::Shade400, { 0xFF7043, Luminance::Light } },
			{ Shade::Shade500, { 0xFF5722, Luminance::Light } },
			{ Shade::Shade600, { 0xF4511E, Luminance::Dark } },
			{ Shade::Shade700, { 0xE64A19, Luminance::Dark } },
			{ Shade::Shade800, { 0xD84315, Luminance::Dark } },
			{ Shade::Shade900, { 0xBF360C, Luminance::Dark } },
			{ Shade::ShadeA100, { 0xFF9E80, Luminance::Light } },
			{ Shade::ShadeA200, { 0xFF6E40, Luminance::Light } },
			{ Shade::ShadeA400, { 0xFF3D00, Luminance::Dark } },
			{ Shade::ShadeA700, { 0xDD2C00, Luminance::Dark } },
		} },
	{ Name::Brown,
		{
			{ Shade::Shade50, { 0xEFEBE9, Luminance::Light } },
			{ Shade::Shade100, { 0xD7CCC8, Luminance::Light } },
			{ Shade::Shade200, { 0xBCAAA4, Luminance::Light } },
			{ Shade::Shade300, { 0xA1887F, Luminance::Dark } },
			{ Shade::Shade400, { 0x8D6E63, Luminance::Dark } },
			{ Shade::Shade500, { 0x795548, Luminance::Dark } },
			{ Shade::Shade600, { 0x6D4C41, Luminance::Dark } },
			{ Shade::Shade700, { 0x5D4037, Luminance::Dark } },
			{ Shade::Shade800, { 0x4E342E, Luminance::Dark } },
			{ Shade::Shade900, { 0x3E2723, Luminance::Dark } },
			//
			{ Shade::ShadeA100, { 0xA1887F, Luminance::Dark } },
			{ Shade::ShadeA200, { 0x795548, Luminance::Dark } },
			{ Shade::ShadeA400, { 0x5D4037, Luminance::Dark } },
			{ Shade::ShadeA700, { 0x3E2723, Luminance::Dark } },
		} },
	{ Name::Gray,
		{
			{ Shade::Shade50, { 0xFAFAFA, Luminance::Light } },
			{ Shade::Shade100, { 0xF5F5F5, Luminance::Light } },
			{ Shade::Shade200, { 0xEEEEEE, Luminance::Light } },
			{ Shade::Shade300, { 0xE0E0E0, Luminance::Light } },
			{ Shade::Shade400, { 0xBDBDBD, Luminance::Light } },
			{ Shade::Shade500, { 0x9E9E9E, Luminance::Light } },
			{ Shade::Shade600, { 0x757575, Luminance::Dark } },
			{ Shade::Shade700, { 0x616161, Luminance::Dark } },
			{ Shade::Shade800, { 0x424242, Luminance::Dark } },
			{ Shade::Shade900, { 0x212121, Luminance::Dark } },
			//
			{ Shade::ShadeA100, { 0xE0E0E0, Luminance::Light } },
			{ Shade::ShadeA200, { 0x9E9E9E, Luminance::Light } },
			{ Shade::ShadeA400, { 0x616161, Luminance::Dark } },
			{ Shade::ShadeA700, { 0x212121, Luminance::Dark } },
		} },
	{ Name::BlueGray,
		{
			{ Shade::Shade50, { 0xECEFF1, Luminance::Light } },
			{ Shade::Shade100, { 0xCFD8DC, Luminance::Light } },
			{ Shade::Shade200, { 0xB0BEC5, Luminance::Light } },
			{ Shade::Shade300, { 0x90A4AE, Luminance::Light } },
			{ Shade::Shade400, { 0x78909C, Luminance::Dark } },
			{ Shade::Shade500, { 0x607D8B, Luminance::Dark } },
			{ Shade::Shade600, { 0x546E7A, Luminance::Dark } },
			{ Shade::Shade700, { 0x455A64, Luminance::Dark } },
			{ Shade::Shade800, { 0x37474F, Luminance::Dark } },
			{ Shade::Shade900, { 0x263238, Luminance::Dark } },
			//
			{ Shade::ShadeA100, { 0x90A4AE, Luminance::Light } },
			{ Shade::ShadeA200, { 0x607D8B, Luminance::Dark } },
			{ Shade::ShadeA400, { 0x455A64, Luminance::Dark } },
			{ Shade::ShadeA700, { 0x263238, Luminance::Dark } },
		} },
};

static const std::unordered_map<Name, std::unordered_map<Shade, QColor>> g_errorMap = {
	{ Name::Red,
		{
			{ Shade::Shade50, 0x000000 },
			{ Shade::Shade100, 0x000000 },
			{ Shade::Shade200, 0x000000 },
			{ Shade::Shade300, 0x000000 },
			{ Shade::Shade400, 0x000000 },
			{ Shade::Shade500, 0x000000 },
			{ Shade::Shade600, 0x000000 },
			{ Shade::Shade700, 0x000000 },
			{ Shade::Shade800, 0x000000 },
			{ Shade::Shade900, 0x000000 },
			{ Shade::ShadeA100, 0x000000 },
			{ Shade::ShadeA200, 0x000000 },
			{ Shade::ShadeA400, 0x000000 },
			{ Shade::ShadeA700, 0x000000 },
		} },
	{ Name::Pink,
		{
			{ Shade::Shade50, 0x000000 },
			{ Shade::Shade100, 0x000000 },
			{ Shade::Shade200, 0x000000 },
			{ Shade::Shade300, 0x000000 },
			{ Shade::Shade400, 0x000000 },
			{ Shade::Shade500, 0x000000 },
			{ Shade::Shade600, 0x000000 },
			{ Shade::Shade700, 0x000000 },
			{ Shade::Shade800, 0x000000 },
			{ Shade::Shade900, 0x000000 },
			{ Shade::ShadeA100, 0x000000 },
			{ Shade::ShadeA200, 0x000000 },
			{ Shade::ShadeA400, 0x000000 },
			{ Shade::ShadeA700, 0x000000 },
		} },
	{ Name::Purple,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade500, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade900, 0xEF5350 },
		} },
	{ Name::DeepPurple,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade500, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade900, 0xEF5350 },
		} },
	{ Name::Indigo,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade500, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade900, 0xEF5350 },
		} },
	//{ Name::Blue,
	//	{
	//		{ Shade::Shade300, 0xD50000 },
	//		{ Shade::Shade500, 0xD50000 },
	//		{ Shade::Shade600, 0xD50000 },
	//		{ Shade::Shade900, 0xD50000 },
	//	} },
	//{ Name::LightBlue,
	//	{
	//		{ Shade::Shade300, 0xD50000 },
	//		{ Shade::Shade500, 0xD50000 },
	//		{ Shade::Shade600, 0xD50000 },
	//		{ Shade::Shade900, 0xD50000 },
	//	} },
	//{ Name::Cyan,
	//	{
	//		{ Shade::Shade300, 0xD50000 },
	//		{ Shade::Shade500, 0xD50000 },
	//		{ Shade::Shade600, 0xD50000 },
	//		{ Shade::Shade900, 0xD50000 },
	//	} },
	{ Name::Teal,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade500, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade900, 0xEF5350 },
		} },
	//{ Name::Green,
	//	{
	//		{ Shade::Shade300, 0xD50000 },
	//		{ Shade::Shade500, 0xD50000 },
	//		{ Shade::Shade600, 0xD50000 },
	//		{ Shade::Shade900, 0xD50000 },
	//	} },
	//{ Name::LightGreen,
	//	{
	//		{ Shade::Shade300, 0xD50000 },
	//		{ Shade::Shade500, 0xD50000 },
	//		{ Shade::Shade600, 0xD50000 },
	//		{ Shade::Shade900, 0xD50000 },
	//	} },
	//{ Name::Lime,
	//	{
	//		{ Shade::Shade300, 0xD50000 },
	//		{ Shade::Shade500, 0xD50000 },
	//		{ Shade::Shade600, 0xD50000 },
	//		{ Shade::Shade900, 0xD50000 },
	//	} },
	//{ Name::Yellow,
	//	{
	//		{ Shade::Shade300, 0xD50000 },
	//		{ Shade::Shade500, 0xD50000 },
	//		{ Shade::Shade600, 0xD50000 },
	//		{ Shade::Shade900, 0xD50000 },
	//	} },
	//{ Name::Amber,
	//	{
	//		{ Shade::Shade300, 0xD50000 },
	//		{ Shade::Shade500, 0xD50000 },
	//		{ Shade::Shade600, 0xD50000 },
	//		{ Shade::Shade900, 0xD50000 },
	//	} },
	//{ Name::Orange,
	//	{
	//		{ Shade::Shade300, 0xD50000 },
	//		{ Shade::Shade500, 0xD50000 },
	//		{ Shade::Shade600, 0xD50000 },
	//		{ Shade::Shade900, 0xD50000 },
	//	} },
	{ Name::DeepOrange,
		{
			{ Shade::Shade50, 0x000000 },
			{ Shade::Shade100, 0x000000 },
			{ Shade::Shade200, 0x000000 },
			{ Shade::Shade300, 0x000000 },
			{ Shade::Shade400, 0x000000 },
			{ Shade::Shade500, 0x000000 },
			{ Shade::Shade600, 0x000000 },
			{ Shade::Shade700, 0x000000 },
			{ Shade::Shade800, 0x000000 },
			{ Shade::Shade900, 0x000000 },
			{ Shade::ShadeA100, 0x000000 },
			{ Shade::ShadeA200, 0x000000 },
			{ Shade::ShadeA400, 0x000000 },
			{ Shade::ShadeA700, 0x000000 },
		} },
	{ Name::Brown,
		{
			{ Shade::Shade300, 0xEF5350 },
			{ Shade::Shade500, 0xEF5350 },
			{ Shade::Shade600, 0xEF5350 },
			{ Shade::Shade900, 0xEF5350 },
		} },
	{ Name::Gray,
		{
			{ Shade::Shade300, 0xFF5252 },
			{ Shade::Shade500, 0xFF5252 },
			{ Shade::Shade600, 0xFF5252 },
			{ Shade::Shade900, 0xFF5252 },
		} },
	{ Name::BlueGray,
		{
			{ Shade::Shade300, 0xFF5252 },
			{ Shade::Shade500, 0xFF5252 },
			{ Shade::Shade600, 0xFF5252 },
			{ Shade::Shade900, 0xFF5252 },
		} },
};

static ShadeMap const& shadeMap(Name const name)
{
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

QColor complementary(QColor const& baseColor)
{
	auto const r = 255 - baseColor.red();
	auto const g = 255 - baseColor.green();
	auto const b = 255 - baseColor.blue();

	return QColor::fromRgb(r, g, b);
}

QColor value(Name const name, Shade const shade)
{
	return colorData(name, shade).value;
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
	auto errorColor = value(Name::Red, qtMate::material::color::Shade::ShadeA700);

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

} // namespace color
} // namespace material
} // namespace qtMate
