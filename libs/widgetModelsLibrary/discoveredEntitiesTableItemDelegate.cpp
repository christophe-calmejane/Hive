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

#include "hive/widgetModelsLibrary/discoveredEntitiesTableItemDelegate.hpp"
#include "hive/widgetModelsLibrary/qtUserRoles.hpp"

#include <QPainter>

namespace hive
{
namespace widgetModelsLibrary
{
void DiscoveredEntitiesTableItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Override default options according to the model current state
	auto basePainterOption = option;

	// Clear focus state if any
	if (basePainterOption.state & QStyle::State_HasFocus)
	{
		basePainterOption.state &= ~QStyle::State_HasFocus;
	}

	// Subscribed to unsol
	auto const subscribedToUnsol = index.data(la::avdecc::utils::to_integral(QtUserRoles::SubscribedUnsolRole)).toBool();
	if (!subscribedToUnsol)
	{
		if (basePainterOption.state & QStyle::StateFlag::State_Selected)
		{
			auto const brush = qtMate::material::color::brush(qtMate::material::color::Name::BlueGray, qtMate::material::color::DefaultShade);
			painter->fillRect(basePainterOption.rect, brush);
		}
		else
		{
			auto const brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::DefaultShade);
			painter->fillRect(basePainterOption.rect, brush);
		}
	}

	// Base painter
	switch (index.column())
	{
		case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::EntityID):
		{
			// Check for Identification
			auto const identifying = index.data(la::avdecc::utils::to_integral(QtUserRoles::IdentificationRole)).toBool();
			if (identifying)
			{
				basePainterOption.font.setBold(true);
			}

			// Check for Error
			auto const isError = index.data(la::avdecc::utils::to_integral(QtUserRoles::ErrorRole)).toBool();
			if (isError)
			{
				// Right now, always use default value, as we draw on white background
				auto const errorColorValue = qtMate::material::color::foregroundErrorColorValue(qtMate::material::color::DefaultColor, qtMate::material::color::DefaultShade);
				basePainterOption.palette.setColor(QPalette::ColorRole::Text, errorColorValue);
			}
			break;
		}
		default:
			break;
	}
	QStyledItemDelegate::paint(painter, basePainterOption, index);

	// Image painter
	switch (index.column())
	{
		case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::EntityLogo):
		case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::Compatibility):
		case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::AcquireState):
		case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::LockState):
			static_cast<QStyledItemDelegate const&>(_imageItemDelegate).paint(painter, option, index);
			break;
		default:
			break;
	}

	// Error painter
	switch (index.column())
	{
		case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::EntityID):
			static_cast<QStyledItemDelegate const&>(_errorItemDelegate).paint(painter, option, index);
			break;
		default:
			break;
	}
}

} // namespace widgetModelsLibrary
} // namespace hive
