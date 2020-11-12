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

#include "color.hpp"

#include <QAbstractTableModel>

namespace qtMate
{
namespace material
{
namespace color
{
// Standard Qt model that exposes material colors
class Palette : public QAbstractTableModel
{
public:
	using QAbstractTableModel::QAbstractTableModel;

	static int nameCount();
	static int shadeCount();

	static Name name(int const index);
	static Shade shade(int const index);

	static QString nameToString(Name const name);
	static QString shadeToString(Shade const shade);

	static int index(Name const name);
	static int index(Shade const shade);

	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual int columnCount(QModelIndex const& parent = {}) const override;

	virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
};

} // namespace color
} // namespace material
} // namespace qtMate
