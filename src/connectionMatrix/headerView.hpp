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

#pragma once

#include <QHeaderView>
#include <QVector>

namespace connectionMatrix
{
class HeaderView final : public QHeaderView
{
public:
	HeaderView(Qt::Orientation orientation, QWidget* parent = nullptr);

	struct SectionState
	{
		bool isInitialized{ false };
		bool isExpanded{ true };
		bool isVisible{ true };
	};

	QVector<SectionState> saveSectionState() const;
	void restoreSectionState(QVector<SectionState> const& sectionState);

private:
	virtual void setModel(QAbstractItemModel* model) override;

	virtual void leaveEvent(QEvent* event) override;
	virtual void mouseDoubleClickEvent(QMouseEvent* e) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void paintSection(QPainter* painter, QRect const& rect, int logicalIndex) const override;

	virtual QSize sizeHint() const override;

	Q_SLOT void handleSectionInserted(QModelIndex const& parent, int first, int last);
	Q_SLOT void handleSectionRemoved(QModelIndex const& parent, int first, int last);
	Q_SLOT void handleHeaderDataChanged(Qt::Orientation orientation, int first, int last);
	Q_SLOT void handleSectionClicked(int logicalIndex);

	QVector<SectionState> _sectionState;
};

} // namespace connectionMatrix
