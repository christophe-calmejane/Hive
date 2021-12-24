/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <la/avdecc/logger.hpp>

#include <QAbstractTableModel>
#include <QRegularExpression>

namespace avdecc
{
class LoggerModelPrivate;
class LoggerModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	LoggerModel(QObject* parent = nullptr);
	~LoggerModel();

	int rowCount(QModelIndex const& parent = QModelIndex()) const override;
	int columnCount(QModelIndex const& parent = QModelIndex()) const override;
	QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(QModelIndex const& index) const override;

	void clear();

	struct SaveConfiguration
	{
		QRegularExpression search{};
		QRegularExpression level{};
		QRegularExpression layer{};
	};

	void save(QString const& filename, SaveConfiguration const& saveConfiguration) const;

private:
	LoggerModelPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(LoggerModel)
};
} // namespace avdecc
