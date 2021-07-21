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

#pragma once

#include <QHeaderView>
#include <QEvent>
#include <QSet>

namespace qtMate
{
namespace widgets
{
// This class is intended to be used on a QHeaderView's viewport, in order to disable sorting on a list of sections.
// It swallows MouseButtonPress and MouseButtonRelease that may happen on these sections to disable the sorting mechanism.
// By default, all sections are disabled, call ::enable() to make a section clickable/sortable.
class HeaderViewSortSectionFilter : public QObject
{
public:
	HeaderViewSortSectionFilter(QHeaderView* headerView, QObject* parent = nullptr)
		: QObject{ parent }
		, _headerView{ headerView }
	{
		_headerView->viewport()->installEventFilter(this);
	}

	void disable(int logicalIndex)
	{
		_enabledSectionSet.remove(logicalIndex);
	}

	void enable(int logicalIndex)
	{
		_enabledSectionSet.insert(logicalIndex);
	}

private:
	virtual bool eventFilter(QObject* /*object*/, QEvent* event) override
	{
		if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)
		{
			auto* ev = static_cast<QMouseEvent*>(event);
			auto const logicalIndexUnderTheMouse = _headerView->logicalIndexAt(ev->pos());
			if (!_enabledSectionSet.contains(logicalIndexUnderTheMouse))
			{
				return true;
			}
		}

		return false;
	}

private:
	QHeaderView* _headerView{};
	QSet<int> _enabledSectionSet{};
};

} // namespace widgets
} // namespace qtMate
