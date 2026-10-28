/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

/**
* @file accessibilityAudit.hpp
* @author Christophe Calmejane
* @brief Generic audit of the accessibility information a widget hierarchy exposes through QAccessibleInterface.
*/

#pragma once

#include <QAccessible>
#include <QString>

#include <string>
#include <vector>

class QWidget;

namespace accessibilityAudit
{
/** The checks performed on every interactive widget of the audited hierarchy. */
enum class Check
{
	InvalidInterface, /**< No valid QAccessibleInterface */
	MissingName, /**< Empty accessible name */
	IconFontGlyphName, /**< The accessible name is the name of a glyph of an icons font (icon only button without accessible name) */
	ObjectNameAsName, /**< The accessible name is the (technical) object name of the widget */
	DescriptionSameAsName, /**< The accessible description repeats the accessible name */
	UnexpectedRole, /**< The role does not match the kind of widget */
	MissingAction, /**< An actionable control without the expected action */
	MissingTextInterface, /**< A text control without text interface (or without editable text interface when editable) */
	MissingValueInterface, /**< A value control without value interface */
	MissingTableInterface, /**< An item view without table interface */
	UnlabeledFormField, /**< A form layout field whose visible label is not its buddy */
	DuplicateName, /**< Several controls of the same role with the same name and parent */
	NotReachable, /**< The control can't be reached by navigating the accessibility tree from its window */
};

/** A deliberate, documented, exception to a check. */
struct Exception
{
	QWidget const* widget{ nullptr }; /**< The widget (and its descendants) the exception applies to */
	Check check{ Check::MissingName };
	QString reason{}; /**< Why it is acceptable (documentation only) */
};
using Exceptions = std::vector<Exception>;

/** A problem found by the audit, with the information a developer needs to locate the widget. */
struct Issue
{
	Check check{ Check::MissingName };
	QString objectName{};
	QString className{};
	QString windowTitle{};
	QAccessible::Role role{ QAccessible::NoRole };
	QString name{};
	QString reason{};
};
using Issues = std::vector<Issue>;

/**
* @brief Audits the accessibility information exposed by the interactive widgets of a hierarchy.
* @details Only the widgets visible in the hierarchy are audited (show it first, eg. with the Qt::WA_DontShowOnScreen
*          attribute). Layout containers, labels, separators, frames and the internal widgets of the Qt controls
*          (prefixed with qt_, children of combo boxes, spin boxes, line edits, tab bars and item views, scroll bars)
*          are not considered as interactive controls.
* @param[in] topLevel The root of the hierarchy (usually a window).
* @param[in] exceptions The deliberate exceptions to the checks.
* @return The issues found.
*/
Issues audit(QWidget* const topLevel, Exceptions const& exceptions = {});

/** Formats the issues as a developer readable report, one issue per line. */
std::string toString(Issues const& issues);

/** Gets the name of a check. */
char const* checkName(Check const check) noexcept;

} // namespace accessibilityAudit
