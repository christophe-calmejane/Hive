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

#include "connectionMatrix/view.hpp"
#include "connectionMatrix/model.hpp"

#include <QHeaderView>
#include <QPainter>

namespace connectionMatrix
{

class HeaderView : public QHeaderView
{
public:
	HeaderView(Qt::Orientation orientation, QWidget* parent = nullptr)
		: QHeaderView(orientation, parent)
	{
		setDefaultSectionSize(20);
		setSectionResizeMode(QHeaderView::Fixed);
	}

	virtual void paintSection(QPainter* painter, QRect const& rect, int logicalIndex) const override
	{
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);

		auto const arrowSize{ 10 };
		auto const headerType = model()->headerData(logicalIndex, orientation(), Model::HeaderTypeRole).toInt();
		auto const arrowOffset{ 25 * headerType };

		QPainterPath path;
		if (orientation() == Qt::Horizontal)
		{
			path.moveTo(rect.topLeft());
			path.lineTo(rect.bottomLeft() - QPoint{ 0, arrowSize + arrowOffset });
			path.lineTo(rect.center() + QPoint{ 0, rect.height() / 2 - arrowOffset });
			path.lineTo(rect.bottomRight() - QPoint{ 0, arrowSize + arrowOffset });
			path.lineTo(rect.topRight());
		}
		else
		{
			path.moveTo(rect.topLeft());
			path.lineTo(rect.topRight() - QPoint{ arrowSize + arrowOffset, 0 });
			path.lineTo(rect.center() + QPoint{ rect.width() / 2 - arrowOffset, 0 });
			path.lineTo(rect.bottomRight() - QPoint{ arrowSize + arrowOffset, 0 });
			path.lineTo(rect.bottomLeft());
		}

		QBrush backgroundBrush{};
		switch (headerType)
		{
			case 0:
				backgroundBrush = QColor{ "#4A148C" };
				break;
			case 1:
				backgroundBrush = QColor{ "#7B1FA2" };
				break;
			case 2:
				backgroundBrush = QColor{ "#BA68C8" };
				break;
			default:
				backgroundBrush = QColor{ "#808080" };
				break;
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

		painter->setPen(Qt::white);
		painter->drawText(textRect, Qt::AlignVCenter, elidedText);
		painter->restore();
	}

	virtual QSize sizeHint() const override
	{
		if (orientation() == Qt::Horizontal)
		{
			return QSize(20, 200);
		}
		else
		{
			return QSize(200, 20);
		}
	}
};


View::View(QWidget* parent)
	: QTableView{parent}
	, _model{std::make_unique<Model>()} {
	setModel(_model.get());
	setVerticalHeader(new HeaderView{Qt::Vertical, this});
	setHorizontalHeader(new HeaderView{Qt::Horizontal, this});
	
}

} // namespace connectionMatrix
