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

#pragma once

#include "latencyComboBox.hpp"

#include <la/avdecc/internals/entityModelTypes.hpp>

#include <QStyledItemDelegate>
#include <QModelIndex>
#include <QWidget>
#include <QAbstractItemModel>

#include <string>
#include <chrono>

/** Helper struct. Holds all data needed to display a table row. */
struct LatencyTableRowEntry
{
	LatencyTableRowEntry() noexcept = default;
	LatencyTableRowEntry(la::avdecc::entity::model::StreamIndex const si, std::chrono::nanoseconds const& lat)
		: streamIndex{ si }
		, latency{ lat }
	{
	}
	constexpr friend bool operator==(LatencyTableRowEntry const& lhs, LatencyTableRowEntry const& rhs) noexcept
	{
		return lhs.streamIndex == rhs.streamIndex && lhs.latency == rhs.latency;
	}
	constexpr friend bool operator!=(LatencyTableRowEntry const& lhs, LatencyTableRowEntry const& rhs) noexcept
	{
		return !operator==(lhs, rhs);
	}

	la::avdecc::entity::model::StreamIndex streamIndex{ la::avdecc::entity::model::getInvalidDescriptorIndex() };
	std::chrono::nanoseconds latency{};
};
Q_DECLARE_METATYPE(LatencyTableRowEntry)


/** Implements a delegate to display the latency of an output stream. */
class LatencyItemDelegate final : public QStyledItemDelegate
{
	Q_OBJECT

public:
	LatencyItemDelegate(la::avdecc::UniqueIdentifier const entityID, QObject* parent = nullptr)
		: QStyledItemDelegate{ parent }
		, _entityID{ entityID }
	{
	}

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void setEditorData(QWidget* editor, const QModelIndex& index) const override;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

private:
	void updatePossibleLatencyValues(LatencyComboBox* const combobox, la::avdecc::entity::model::StreamFormat const& streamFormat) const noexcept;
	void updateCurrentLatencyValue(LatencyComboBox* const combobox, std::chrono::nanoseconds const& latency) const noexcept;
	std::string labelFromLatency(std::chrono::nanoseconds const& latency) const noexcept;

	la::avdecc::UniqueIdentifier _entityID{};
};
