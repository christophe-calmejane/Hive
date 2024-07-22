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

#include "QtMate/widgets/comboBox.hpp"

#include <QEvent>
#include <QStyledItemDelegate>
#include <QAbstractItemView>

namespace qtMate
{
namespace widgets
{
class ComboBoxPrivate : public QObject
{
public:
	ComboBoxPrivate(ComboBox* q)
		: q_ptr(q)
	{
		q->installEventFilter(this);
	}

	virtual bool eventFilter(QObject* /*object*/, QEvent* event) override
	{
		if (event->type() == QEvent::Wheel)
		{
			return true;
		}

		return false;
	}

protected:
	ComboBox* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(ComboBox)
};

class BoldCurrentIndexSelectionDelegate : public QStyledItemDelegate
{
	using QStyledItemDelegate::QStyledItemDelegate;

protected:
	virtual void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override
	{
		auto opt = option;

		if (auto* view = dynamic_cast<QAbstractItemView const*>(option.widget))
		{
			if (auto* parent = view->parent(); !qstrcmp("QComboBoxPrivateContainer", parent->metaObject()->className()))
			{
				if (auto* widget = dynamic_cast<QComboBox const*>(parent->parent()))
				{
					opt.font.setBold(index.row() == widget->currentIndex());
				}
			}
		}

		QStyledItemDelegate::paint(painter, opt, index);
	}
};

ComboBox::ComboBox(QWidget* parent)
	: QComboBox(parent)
	, d_ptr(new ComboBoxPrivate(this))
{
	setItemDelegate(new BoldCurrentIndexSelectionDelegate{ this });
}

ComboBox::~ComboBox()
{
	delete d_ptr;
}

} // namespace widgets
} // namespace qtMate
