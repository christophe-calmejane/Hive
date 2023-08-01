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

#include "hive/widgetModelsLibrary/discoveredEntitiesTableItemDelegate.hpp"
#include "hive/widgetModelsLibrary/discoveredEntitiesTableModel.hpp"
#include "hive/widgetModelsLibrary/qtUserRoles.hpp"

#include <QPainter>

namespace hive
{
namespace widgetModelsLibrary
{
DiscoveredEntitiesTableItemDelegate::DiscoveredEntitiesTableItemDelegate(qtMate::material::color::Name const themeColorName, QObject* parent) noexcept
	: QStyledItemDelegate(parent)
{
	setThemeColorName(themeColorName);
}

void DiscoveredEntitiesTableItemDelegate::setThemeColorName(qtMate::material::color::Name const themeColorName)
{
	_themeColorName = themeColorName;
	_isDark = qtMate::material::color::luminance(_themeColorName) == qtMate::material::color::Luminance::Dark;
	_errorIconItemDelegate.setThemeColorName(themeColorName);
	_imageItemDelegate.setThemeColorName(themeColorName);
}

void DiscoveredEntitiesTableItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Override default options according to the model current state
	auto basePainterOption = option;

	// Clear focus state if any
	if (basePainterOption.state & QStyle::State_HasFocus)
	{
		basePainterOption.state &= ~QStyle::State_HasFocus;
	}

	// Unsol supported
	auto const unsolSupported = index.data(la::avdecc::utils::to_integral(QtUserRoles::UnsolSupportedRole)).toBool();
	if (!unsolSupported)
	{
		auto const isSelected = basePainterOption.state & QStyle::StateFlag::State_Selected;
		// Only change the background color if not selected
		if (!isSelected)
		{
			auto const brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::DefaultShade);
			painter->fillRect(basePainterOption.rect, brush);
		}
	}

	// Base painter
	{
		auto ignoreBase = false;
		switch (index.column())
		{
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::EntityError):
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::EntityLogo):
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::Compatibility):
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::AcquireState):
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::LockState):
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::ClockDomainLockState):
				ignoreBase = true;
				break;
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::EntityID):
			{
				// Check for Identification
				auto const identifying = index.data(la::avdecc::utils::to_integral(QtUserRoles::IdentificationRole)).toBool();
				if (identifying)
				{
					basePainterOption.font.setBold(true);
				}
				break;
			}
			default:
				break;
		}

		// Check for Virtual Entity
		auto const isVirtual = index.data(la::avdecc::utils::to_integral(QtUserRoles::IsVirtualRole)).toBool();
		if (isVirtual)
		{
			basePainterOption.font.setItalic(true);
		}

		if (!ignoreBase)
		{
			QStyledItemDelegate::paint(painter, basePainterOption, index);
		}
	}

	// Image painter
	{
		switch (index.column())
		{
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::EntityLogo):
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::Compatibility):
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::AcquireState):
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::LockState):
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::ClockDomainLockState):
				static_cast<QStyledItemDelegate const&>(_imageItemDelegate).paint(painter, option, index);
				break;
			default:
				break;
		}
	}

	// Error painter
	{
		switch (index.column())
		{
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::EntityError):
				static_cast<QStyledItemDelegate const&>(_errorIconItemDelegate).paint(painter, option, index);
				break;
			default:
				break;
		}
	}
}

} // namespace widgetModelsLibrary
} // namespace hive
