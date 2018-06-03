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

#include "textEntry.hpp"
#include "avdecc/stringValidator.hpp"
#include <QKeyEvent>

namespace qt
{
namespace toolkit
{

class TextEntryPrivate : public QObject
{
public:
	TextEntryPrivate(TextEntry* q)
	: q_ptr(q)
	{
		q->installEventFilter(this);
		q->setValidator(&_validator);
		connect(q, &QLineEdit::returnPressed, q, &QLineEdit::clearFocus);
	}

	virtual bool eventFilter(QObject* object, QEvent* event) override
	{
		Q_Q(TextEntry);

		auto abort{false};

		switch (event->type())
		{
			case QEvent::FocusIn:
				_focusInText = q->text();
				_validated = false;
				break;
			case QEvent::FocusOut:
				abort = true;
				break;
			case QEvent::KeyPress:
				switch (static_cast<QKeyEvent*>(event)->key())
				{
					case Qt::Key_Escape:
						abort = true;
						break;
					case Qt::Key_Return:
					case Qt::Key_Enter:
						_validated = true;
						break;
					case Qt::Key_Tab:
						// Swallow tab
						return true;
					default:
						break;
				}
				break;
			default:
				break;
		}

		if (!_validated && abort)
		{
			QSignalBlocker lock(q);
			q->setText(_focusInText);
			q->clearFocus();
		}

		return false;
	}

protected:
	TextEntry* const q_ptr{nullptr};
	Q_DECLARE_PUBLIC(TextEntry);

	QString _focusInText{};
	bool _validated{false};

	avdecc::StringValidator _validator;
};

TextEntry::TextEntry(QString const& text, QWidget* parent)
	: QLineEdit(text, parent)
	, d_ptr(new TextEntryPrivate(this))
{
	setFocusPolicy(Qt::ClickFocus);
}

TextEntry::TextEntry(QWidget* parent)
	: TextEntry(QString::null, parent)
{
}

TextEntry::~TextEntry()
{
	delete d_ptr;
}

void TextEntry::setText(QString const& text)
{
	if (hasFocus())
	{
		Q_D(TextEntry);
		d->_focusInText = text;
	}
	else
	{
		QLineEdit::setText(text);
	}
}

} // namespace toolkit
} // namespace qt
