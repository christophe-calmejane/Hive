/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

	auto const isSelected = basePainterOption.state & QStyle::StateFlag::State_Selected;
	auto const isVirtual = index.data(la::avdecc::utils::to_integral(QtUserRoles::IsVirtualRole)).toBool();
	auto const unsolSupported = index.data(la::avdecc::utils::to_integral(QtUserRoles::UnsolSupportedRole)).toBool();
	auto const unsolSubscribed = index.data(la::avdecc::utils::to_integral(QtUserRoles::SubscribedUnsolRole)).toBool();

	// Check for Virtual Entity
	if (isVirtual)
	{
		// Change the text color for all columns using the base painter
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

	// Unsol not supported or not subscribed to
	if (!unsolSupported) // Unsolicited notifications not supported
	{
		// Change the text font to italic for all columns using the base painter
		basePainterOption.font.setItalic(true);
	}
	else if (!unsolSubscribed) // Unsolicited notifications supported, but not subscribed to
	{
		// Change the background pattern for all columns
		if (!isSelected)
		{
			auto brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::isDarkColorScheme() ? qtMate::material::color::Shade::Shade800 : qtMate::material::color::Shade::Shade300);
			brush.setStyle(Qt::BrushStyle::BDiagPattern);
			// We need to draw right away as there is no background fill when there is no selection (ie. don't use the backgroundFill function)
			// (or just maybe I couldn't find the correct QPalette ColorRole to set in the brush, tried 'Window' but didn't work)
			painter->fillRect(basePainterOption.rect, brush);
		}
		else
		{
			// If theme is dark, it means the text will be light, so we want to use black
			if (_isDark)
			{
				auto brush = QBrush{ Qt::black, Qt::BrushStyle::BDiagPattern };
				basePainterOption.palette.setBrush(QPalette::Highlight, brush); // Change the base painter option (will be used by the base delegate)
				backgroundFill = [&painter, basePainterOption, brush = std::move(brush)]() // Set the background fill function
				{
					painter->fillRect(basePainterOption.rect, brush);
				};
			}
			else
			{
				auto brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::Shade::Shade400);
				brush.setStyle(Qt::BrushStyle::BDiagPattern);
				basePainterOption.palette.setBrush(QPalette::Highlight, brush); // Change the base painter option (will be used by the base delegate)
				backgroundFill = [&painter, basePainterOption, brush = std::move(brush)]() // Set the background fill function
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
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::EntityStatus):
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
			case DiscoveredEntitiesTableModel::EntityDataFlags::getPosition(DiscoveredEntitiesTableModel::EntityDataFlag::EntityStatus):
				static_cast<QStyledItemDelegate const&>(_errorIconItemDelegate).paint(painter, option, index);
				break;
			default:
				break;
		}
	}
}

} // namespace widgetModelsLibrary
} // namespace hive
