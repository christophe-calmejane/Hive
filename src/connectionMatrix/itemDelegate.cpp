/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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
#include <QtMate/material/color.hpp>
#include <QPainter>

#if ENABLE_CONNECTION_MATRIX_DEBUG
#	include <unordered_map>
#endif

namespace connectionMatrix
{
ItemDelegate::ItemDelegate(bool const drawMediaLockedDot, QObject* parent)
	: QStyledItemDelegate{ parent }
	, _drawMediaLockedDot{ drawMediaLockedDot }
{
}

void ItemDelegate::setDrawMediaLockedDot(bool const drawMediaLockedDot) noexcept
{
	_drawMediaLockedDot = drawMediaLockedDot;
}

void ItemDelegate::setDrawCRFAudioConnections(bool const drawCRFAudioConnections) noexcept
{
	_drawCRFAudioConnections = drawCRFAudioConnections;
}

bool ItemDelegate::getDrawCRFAudioConnections() const noexcept
{
	return _drawCRFAudioConnections;
}

void ItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Sometimes when the model is transposed with hidden rows/columns, hidden items are asked to be drawn
	// This fixes the issue by filtering invalid parameters
	if (!option.rect.isValid())
	{
		return;
	}

	painter->setPen(qtMate::material::color::value(qtMate::material::color::Name::Gray));

	// Background highlithing if selected
	if (option.state & QStyle::State_Selected)
	{
		painter->fillRect(option.rect, option.palette.highlight());
	}

#if ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED
	auto const backgroundColorData = index.data(Qt::BackgroundRole);
	if (!backgroundColorData.isNull())
	{
		auto const backgroundColor = backgroundColorData.value<QColor>();
		painter->fillRect(option.rect, backgroundColor);
	}
#endif

	auto const& intersectionData = static_cast<Model const*>(index.model())->intersectionData(index);

	paintHelper::drawCapabilities(painter, option.rect, intersectionData.type, intersectionData.state, intersectionData.flags, _drawMediaLockedDot, _drawCRFAudioConnections);
}

} // namespace connectionMatrix
