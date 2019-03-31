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

#include "connectionMatrix/itemDelegate.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/node.hpp"
#include "connectionMatrix/paintHelper.hpp"

#include <QPainter>

#if ENABLE_CONNECTION_MATRIX_DEBUG
#include "toolkit/materialPalette.hpp"
#include <unordered_map>
#endif

namespace connectionMatrix
{

void ItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Sometimes when the model is transposed with hidden rows/columns, hidden items are asked to be drawn
	// This fixes the issue by filtering invalid parameters
	if (!option.rect.isValid())
	{
		return;
	}
	
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setPen(Qt::lightGray);

	// Background highlithing if selected
	if (option.state & QStyle::State_Selected)
	{
		painter->fillRect(option.rect, option.palette.highlight());
	}

#if ENABLE_CONNECTION_MATRIX_DEBUG
	auto const backgroundColorData = index.data(Qt::BackgroundRole);
	if (!backgroundColorData.isNull())
	{
		auto const backgroundColor = backgroundColorData.value<QColor>();
		painter->fillRect(option.rect, backgroundColor);
	}
#endif

	auto const& intersectionData = static_cast<Model const*>(index.model())->intersectionData(index);

	switch (intersectionData.type)
	{
		case Model::IntersectionData::Type::Entity_Entity:
		case Model::IntersectionData::Type::Entity_Redundant:
		case Model::IntersectionData::Type::Entity_RedundantStream:
		case Model::IntersectionData::Type::Entity_SingleStream:
			paintHelper::drawEntityConnectionSummary(painter, option.rect, intersectionData.capabilities);
			break;
		case Model::IntersectionData::Type::Redundant_RedundantStream:
		case Model::IntersectionData::Type::RedundantStream_RedundantStream:
			paintHelper::drawIndividualRedundantStreamStatus(painter, option.rect, intersectionData.capabilities);
			break;
		case Model::IntersectionData::Type::Redundant_Redundant:
		case Model::IntersectionData::Type::RedundantStream_SingleStream:
		case Model::IntersectionData::Type::Redundant_SingleStream:
		case Model::IntersectionData::Type::SingleStream_SingleStream:
			paintHelper::drawStreamConnectionStatus(painter, option.rect, intersectionData.capabilities);
			break;
			break;
		case Model::IntersectionData::Type::None:
		default:
			painter->fillRect(option.rect, QBrush{ 0xE1E1E1, Qt::BDiagPattern });
			break;
	}

#if ENABLE_CONNECTION_MATRIX_DEBUG
	static const std::unordered_map< Model::IntersectionData::Type, qt::toolkit::materialPalette::Name> debugColor =
	{
		{ Model::IntersectionData::Type::None, qt::toolkit::materialPalette::Name::Red },
		{ Model::IntersectionData::Type::Entity_Entity, qt::toolkit::materialPalette::Name::Purple },
		{ Model::IntersectionData::Type::Entity_Redundant, qt::toolkit::materialPalette::Name::Indigo },
		{ Model::IntersectionData::Type::Entity_RedundantStream, qt::toolkit::materialPalette::Name::Teal },
		{ Model::IntersectionData::Type::Entity_SingleStream, qt::toolkit::materialPalette::Name::Lime },
		{ Model::IntersectionData::Type::Redundant_Redundant, qt::toolkit::materialPalette::Name::Yellow },
		{ Model::IntersectionData::Type::Redundant_RedundantStream, qt::toolkit::materialPalette::Name::Orange },
		{ Model::IntersectionData::Type::Redundant_SingleStream, qt::toolkit::materialPalette::Name::Brown },
		{ Model::IntersectionData::Type::RedundantStream_RedundantStream, qt::toolkit::materialPalette::Name::Gray },
		{ Model::IntersectionData::Type::RedundantStream_SingleStream, qt::toolkit::materialPalette::Name::BlueGray },
		{ Model::IntersectionData::Type::SingleStream_SingleStream, qt::toolkit::materialPalette::Name::LightGreen },
	};

	auto color = qt::toolkit::materialPalette::color(debugColor.at(intersectionData.type), qt::toolkit::materialPalette::Shade::Shade500);
	color.setAlphaF(0.5f);
	painter->fillRect(option.rect, color);
#endif

	painter->restore();
}

} // namespace connectionMatrix
