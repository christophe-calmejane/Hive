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

#include "QtMate/widgets/dynamicHeaderView.hpp"

#include <QMenu>

namespace qtMate
{
namespace widgets
{
DynamicHeaderView::DynamicHeaderView(Qt::Orientation orientation, QWidget* parent)
	: QHeaderView(orientation, parent)
{
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QHeaderView::customContextMenuRequested, this, &DynamicHeaderView::customContextMenuRequested);
	connect(this, &QHeaderView::sectionResized, this, &DynamicHeaderView::sectionChanged);
}

int DynamicHeaderView::mandatorySection() const
{
	return _mandatorySection;
}

void DynamicHeaderView::setMandatorySection(int mandatorySection)
{
	_mandatorySection = mandatorySection;
}

void DynamicHeaderView::customContextMenuRequested(QPoint const& pos)
{
	QMenu menu;

	auto const mandatorySection = qMax(0, qMin(_mandatorySection, count()));

	for (auto section = 0; section < count(); ++section)
	{
		auto const text = model()->headerData(section, orientation(), Qt::DisplayRole).toString();

		auto* action = menu.addAction(text);
		action->setData(section);
		action->setCheckable(true);
		action->setEnabled(section != mandatorySection);
		action->setChecked(!isSectionHidden(section));
	}
	menu.addSeparator();
	auto* closeAction = menu.addAction("Close");

	if (auto* action = menu.exec(viewport()->mapToGlobal(pos)))
	{
		if (action != closeAction)
		{
			auto const section = action->data().toInt();
			auto const isHidden = !action->isChecked();

			setSectionHidden(section, isHidden);

			emit sectionChanged();
		}
	}
}

} // namespace widgets
} // namespace qtMate
