/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "connectionMatrix/headerView.hpp"
#include "connectionMatrix/model.hpp"

#include "avdecc/helper.hpp"

#include <QPainter>
#include <QMouseEvent>

namespace connectionMatrix
{

HeaderView::HeaderView(Qt::Orientation orientation, QWidget* parent)
	: QHeaderView(orientation, parent)
{
	setSectionResizeMode(QHeaderView::Fixed);
	setSectionsClickable(true);
	setDefaultSectionSize(20);
	setAttribute(Qt::WA_Hover);
	
	connect(this, &QHeaderView::sectionClicked, this, &HeaderView::handleSectionClicked);
}

void HeaderView::leaveEvent(QEvent* event)
{
	selectionModel()->clearSelection();
	QHeaderView::leaveEvent(event);
}

void HeaderView::mouseDoubleClickEvent(QMouseEvent* event)
{
	mousePressEvent(event);
}

void HeaderView::mouseMoveEvent(QMouseEvent* event)
{
	if (orientation() == Qt::Horizontal)
	{
		auto const column = logicalIndexAt(static_cast<QMouseEvent*>(event)->pos());
		selectionModel()->select(model()->index(0, column), QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Columns);
	}
	else
	{
		auto const row = logicalIndexAt(static_cast<QMouseEvent*>(event)->pos());
		selectionModel()->select(model()->index(row, 0), QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
	}

	QHeaderView::mouseMoveEvent(event);
}

void HeaderView::paintSection(QPainter* painter, QRect const& rect, int logicalIndex) const
{
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);
	
	QBrush backgroundBrush{};

	auto const nodeType = model()->headerData(logicalIndex, orientation(), Model::NodeTypeRole).value<Model::NodeType>();
	auto nodeLevel{0};
	
	switch (nodeType)
	{
		case Model::NodeType::Entity:
			backgroundBrush = QColor{ 0x4A148C };
			break;
		case Model::NodeType::InputStream:
		case Model::NodeType::OutputStream:
			backgroundBrush = QColor{ 0x7B1FA2 };
			nodeLevel = 1;
			break;
		default:
			assert(false && "NodeType not handled");
			backgroundBrush = QColor{ 0x808080 };
			break;
	}

	auto const arrowSize{ 10 };
	auto const arrowOffset{ 25 * nodeLevel };

	auto isSelected{false};

	QPainterPath path;
	if (orientation() == Qt::Horizontal)
	{
		path.moveTo(rect.topLeft());
		path.lineTo(rect.bottomLeft() - QPoint{ 0, arrowSize + arrowOffset });
		path.lineTo(rect.center() + QPoint{ 0, rect.height() / 2 - arrowOffset });
		path.lineTo(rect.bottomRight() - QPoint{ 0, arrowSize + arrowOffset });
		path.lineTo(rect.topRight());
		
		isSelected = selectionModel()->isColumnSelected(logicalIndex, {});
	}
	else
	{
		path.moveTo(rect.topLeft());
		path.lineTo(rect.topRight() - QPoint{ arrowSize + arrowOffset, 0 });
		path.lineTo(rect.center() + QPoint{ rect.width() / 2 - arrowOffset, 0 });
		path.lineTo(rect.bottomRight() - QPoint{ arrowSize + arrowOffset, 0 });
		path.lineTo(rect.bottomLeft());
		
		isSelected = selectionModel()->isRowSelected(logicalIndex, {});
	}

	if (isSelected)
	{
		backgroundBrush = QColor{ 0x007ACC };
	}

	painter->fillPath(path, backgroundBrush);
	painter->translate(rect.topLeft());

	auto r = QRect(0, 0, rect.width(), rect.height());
	if (orientation() == Qt::Horizontal)
	{
		r.setWidth(rect.height());
		r.setHeight(rect.width());

		painter->rotate(-90);
		painter->translate(-r.width(), 0);

		r.translate(arrowSize + arrowOffset, 0);
	}

	auto const padding{ 4 };
	auto textRect = r.adjusted(padding, 0, -(padding + arrowSize + arrowOffset), 0);

	auto const text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
	auto const elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, textRect.width());
	
	auto const isStreamingWait = model()->headerData(logicalIndex, orientation(), Model::StreamWaitingRole).toBool();
	if (isStreamingWait)
	{
		painter->setPen(Qt::red);
	}
	else
	{
		painter->setPen(Qt::white);
	}

	painter->drawText(textRect, Qt::AlignVCenter, elidedText);
	painter->restore();
}

QSize HeaderView::sizeHint() const
{
	if (orientation() == Qt::Horizontal)
	{
		return {defaultSectionSize(), 200};
	}
	else
	{
		return {200, defaultSectionSize()};
	}
}

void HeaderView::handleSectionClicked(int logicalIndex)
{
	auto const nodeType = model()->headerData(logicalIndex, orientation(), Model::NodeTypeRole).value<Model::NodeType>();
	
	if (nodeType == Model::NodeType::Entity)
	{
		auto const sectionEntityID = model()->headerData(logicalIndex, orientation(), Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>();
		
		for (auto index = logicalIndex + 1; index < count(); ++index)
		{
			auto const entityID = model()->headerData(index, orientation(), Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>();
			
			// We've reached another entity?
			if (entityID != sectionEntityID)
			{
				break;
			}
			
			setSectionHidden(index, !isSectionHidden(index));
		}
	}
}


} // namespace connectionMatrix
