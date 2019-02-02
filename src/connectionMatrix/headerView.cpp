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

#include "connectionMatrix/headerView.hpp"
#include "connectionMatrix/model.hpp"

#include "avdecc/helper.hpp"

#include <QPainter>
#include <QMouseEvent>

Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)

namespace connectionMatrix
{
HeaderView::HeaderView(Qt::Orientation orientation, QWidget* parent)
	: QHeaderView(orientation, parent)
{
	setSectionResizeMode(QHeaderView::Fixed);
	setSectionsClickable(true);

	int const size{ 20 };
	setMaximumSectionSize(size);
	setMinimumSectionSize(size);
	setDefaultSectionSize(size);

	setAttribute(Qt::WA_Hover);

	connect(this, &QHeaderView::sectionClicked, this, &HeaderView::handleSectionClicked);

	// Has our custom filter only hides filtered sections, we can use this has a "sectionVisibilityChanged" signal
	connect(this, &QHeaderView::sectionResized, this,
		[this](int logicalIndex, int oldSize, int newSize)
		{
			// Means the section is now visible
			if (oldSize == 0)
			{
				updateSectionVisibility(logicalIndex);
			}
		});
}

QVector<HeaderView::SectionState> HeaderView::saveSectionState() const
{
	return _sectionState;
}

void HeaderView::restoreSectionState(QVector<SectionState> const& sectionState)
{
	if (AVDECC_ASSERT_WITH_RET(sectionState.count() == count(), "Invalid state"))
	{
		_sectionState = sectionState;

		for (auto section = 0; section < count(); ++section)
		{
			updateSectionVisibility(section);
		}
	}
}

void HeaderView::setModel(QAbstractItemModel* model)
{
	if (this->model())
	{
		disconnect(this->model());
	}

	QHeaderView::setModel(model);

	if (model)
	{
		if (orientation() == Qt::Vertical)
		{
			connect(model, &QAbstractItemModel::rowsInserted, this, &HeaderView::handleSectionInserted);
			connect(model, &QAbstractItemModel::rowsRemoved, this, &HeaderView::handleSectionRemoved);
		}
		else
		{
			connect(model, &QAbstractItemModel::columnsInserted, this, &HeaderView::handleSectionInserted);
			connect(model, &QAbstractItemModel::columnsRemoved, this, &HeaderView::handleSectionRemoved);
		}

		connect(model, &QAbstractItemModel::modelReset, this, &HeaderView::handleModelReset);
		connect(model, &QAbstractItemModel::headerDataChanged, this, &HeaderView::handleHeaderDataChanged);
	}
}

void HeaderView::leaveEvent(QEvent* event)
{
	if (!rect().contains(mapFromGlobal(QCursor::pos())))
	{
		selectionModel()->clearSelection();
	}

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
		selectionModel()->select(model()->index(0, column), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Columns);
	}
	else
	{
		auto const row = logicalIndexAt(static_cast<QMouseEvent*>(event)->pos());
		selectionModel()->select(model()->index(row, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	}

	QHeaderView::mouseMoveEvent(event);
}

void HeaderView::paintSection(QPainter* painter, QRect const& rect, int logicalIndex) const
{
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);

	QBrush backgroundBrush{};

	auto const nodeType = model()->headerData(logicalIndex, orientation(), Model::NodeTypeRole).value<Model::NodeType>();
	auto nodeLevel{ 0 };

	switch (nodeType)
	{
		case Model::NodeType::Entity:
			backgroundBrush = QColor{ 0x4A148C };
			break;
		case Model::NodeType::InputStream:
		case Model::NodeType::OutputStream:
		case Model::NodeType::RedundantInput:
		case Model::NodeType::RedundantOutput:
			backgroundBrush = QColor{ 0x7B1FA2 };
			nodeLevel = 1;
			break;
		case Model::NodeType::RedundantInputStream:
		case Model::NodeType::RedundantOutputStream:
			backgroundBrush = QColor{ 0xBA68C8 };
			nodeLevel = 2;
			break;
		default:
			assert(false && "NodeType not handled");
			return;
	}

	auto const arrowSize{ 10 };
	auto const arrowOffset{ 20 * nodeLevel };

	auto isSelected{ false };

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
		return { defaultSectionSize(), 200 };
	}
	else
	{
		return { 200, defaultSectionSize() };
	}
}

void HeaderView::handleSectionInserted(QModelIndex const& parent, int first, int last)
{
	for (auto section = first; section <= last; ++section)
	{
		// Insert new section?
		if (section <= _sectionState.count())
		{
			_sectionState.push_back({});
		}
		else // Restore section state
		{
			updateSectionVisibility(section);
		}
	}
}

void HeaderView::handleSectionRemoved(QModelIndex const& parent, int first, int last)
{
	_sectionState.remove(first, last - first + 1);
}

void HeaderView::handleHeaderDataChanged(Qt::Orientation orientation, int first, int last)
{
	if (this->orientation() == orientation)
	{
		for (auto section = first; section <= last; ++section)
		{
			auto& state = _sectionState[section];
			if (!state.isInitialized)
			{
				auto const nodeType = model()->headerData(section, orientation, Model::NodeTypeRole).value<Model::NodeType>();

				switch (nodeType)
				{
					case Model::NodeType::RedundantOutput:
					case Model::NodeType::RedundantInput:
						state.isExpanded = false;
						break;
					case Model::NodeType::RedundantOutputStream:
					case Model::NodeType::RedundantInputStream:
						state.isVisible = false;
						setSectionHidden(section, true);
						break;
					default:
						break;
				}

				state.isInitialized = true;
			}
		}
	}
}

void HeaderView::handleSectionClicked(int logicalIndex)
{
	// Check if this node has children?
	auto const childrenCount = model()->headerData(logicalIndex, orientation(), Model::ChildrenCountRole).value<std::int32_t>();
	if (childrenCount == -1)
	{
		return;
	}

	// Toggle the section expand state
	auto const isExpanded = !_sectionState[logicalIndex].isExpanded;
	_sectionState[logicalIndex].isExpanded = isExpanded;

	// Update children
	for (auto childIndex = 0; childIndex < childrenCount; ++childIndex)
	{
		auto const index = logicalIndex + 1 + childIndex;

		_sectionState[index].isExpanded = isExpanded;
		_sectionState[index].isVisible = isExpanded;

		updateSectionVisibility(index);
	}
}

void HeaderView::handleModelReset()
{
	_sectionState.clear();
}

void HeaderView::updateSectionVisibility(int const logicalIndex)
{
	if (_sectionState[logicalIndex].isVisible)
	{
		showSection(logicalIndex);
	}
	else
	{
		hideSection(logicalIndex);
	}
}

} // namespace connectionMatrix
