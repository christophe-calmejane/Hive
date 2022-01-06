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

#pragma once

#include <QLineEdit>
#include <QValidator>

#include <optional>

namespace qtMate
{
namespace widgets
{
/** A TextEntry with rollback feature when receiving ESC key or FocusOut event */
class TextEntry final : public QLineEdit
{
public:
	TextEntry(QString const& text, std::optional<QValidator*> validator = std::nullopt, QWidget* parent = nullptr);
	TextEntry(QWidget* parent = nullptr);
	~TextEntry();

	void setText(QString const& text);

	using QLineEdit::setValidator;

protected:
	using QLineEdit::setFocusPolicy;

private:
	class TextEntryPrivate;
	TextEntryPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(TextEntry);
};

} // namespace widgets
} // namespace qtMate
