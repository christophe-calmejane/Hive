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
	// Options to be passed to the base class
	auto basePainterOption = option;

	// Function to be called to fill the background, if not using the base delegate
	auto backgroundFill = std::function<void()>{};

	// Clear focus state if any
	if (basePainterOption.state & QStyle::State_HasFocus)
	{
		basePainterOption.state &= ~QStyle::State_HasFocus;
	}

	// Unsol not supported or not subscribed to
	auto const unsolSupported = index.data(la::avdecc::utils::to_integral(QtUserRoles::UnsolSupportedRole)).toBool();
	auto const unsolSubscribed = index.data(la::avdecc::utils::to_integral(QtUserRoles::SubscribedUnsolRole)).toBool();
	if (!unsolSupported)
	{
		auto const isSelected = basePainterOption.state & QStyle::StateFlag::State_Selected;
		// Change the text color
		if (!isSelected)
		{
			auto const brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::Shade::Shade500);
			basePainterOption.palette.setBrush(QPalette::Text, brush);
		}
		else
		{
			// If theme is dark, it means the selected color is light, so we want to use a light gray
			if (_isDark)
			{
				auto const brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::Shade::Shade300);
				basePainterOption.palette.setBrush(QPalette::HighlightedText, brush);
			}
			else
			{
				auto const brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::Shade::ShadeA400);
				basePainterOption.palette.setBrush(QPalette::HighlightedText, brush);
			}
		}
	}
	else if (!unsolSubscribed)
	{
		auto const isSelected = basePainterOption.state & QStyle::StateFlag::State_Selected;
		// Change the background
		if (!isSelected)
		{
			auto brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::Shade::Shade300);
			brush.setStyle(Qt::BrushStyle::BDiagPattern);
			basePainterOption.palette.setBrush(QPalette::NoRole, brush);
			// We need to draw right away as there is no background fill when there is no selection (ie. don't use the backgroundFill function)
			painter->fillRect(basePainterOption.rect, brush);
		}
		else
		{
			// If theme is dark, it means the text will be light, so we want to use black
			if (_isDark)
			{
				auto brush = QBrush{ Qt::black, Qt::BrushStyle::BDiagPattern };
				basePainterOption.palette.setBrush(QPalette::Highlight, brush);
				backgroundFill = [&painter, &basePainterOption, brush = std::move(brush)]()
				{
					painter->fillRect(basePainterOption.rect, brush);
				};
			}
			else
			{
				auto brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::Shade::Shade400);
				brush.setStyle(Qt::BrushStyle::BDiagPattern);
				basePainterOption.palette.setBrush(QPalette::Highlight, brush);
				backgroundFill = [&painter, &basePainterOption, brush = std::move(brush)]()
				{
					painter->fillRect(basePainterOption.rect, brush);
				};
			}
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
		else if (backgroundFill)
		{
			backgroundFill();
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
