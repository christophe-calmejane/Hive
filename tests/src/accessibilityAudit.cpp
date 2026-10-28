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
* @file accessibilityAudit.cpp
* @author Christophe Calmejane
*/

#include "accessibilityAudit.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QTabBar>
#include <QTextEdit>
#include <QWidget>

#include <algorithm>
#include <map>
#include <sstream>
#include <tuple>
#include <utility>

namespace accessibilityAudit
{
namespace
{
// The fonts Hive uses to display icons: the text of a widget using them is the name of a glyph
QStringList const IconFonts{ "Material Icons", "Hive" };

// Internal widgets of the Qt controls, represented by their parent control
QStringList const InternalClassNames{ "QTableCornerButton", "QLineEditIconButton", "QToolBarExtension", "QDockWidgetTitleButton", "QMenuBarExtension" };

bool isInternalWidget(QWidget const* const widget)
{
	if (widget->objectName().startsWith("qt_") || InternalClassNames.contains(widget->metaObject()->className()) || qobject_cast<QScrollBar const*>(widget) || qobject_cast<QHeaderView const*>(widget))
	{
		return true;
	}
	for (auto const* parent = widget->parentWidget(); parent != nullptr; parent = parent->parentWidget())
	{
		if (qobject_cast<QComboBox const*>(parent) || qobject_cast<QAbstractSpinBox const*>(parent) || qobject_cast<QLineEdit const*>(parent) || qobject_cast<QTabBar const*>(parent))
		{
			return true;
		}
		if (parent->isWindow())
		{
			break;
		}
	}
	return false;
}

bool isInteractive(QWidget* const widget)
{
	if (qobject_cast<QAbstractButton const*>(widget) || qobject_cast<QLineEdit const*>(widget) || qobject_cast<QTextEdit const*>(widget) || qobject_cast<QPlainTextEdit const*>(widget) || qobject_cast<QComboBox const*>(widget) || qobject_cast<QAbstractSpinBox const*>(widget) || qobject_cast<QAbstractSlider const*>(widget) || qobject_cast<QProgressBar const*>(widget) || qobject_cast<QAbstractItemView const*>(widget) || qobject_cast<QTabBar const*>(widget) || qobject_cast<QGraphicsView const*>(widget))
	{
		return true;
	}
	// A custom widget accepting the keyboard focus is an interactive control as well (not the Qt containers accepting it, like the tab widgets, they forward it to their content)
	if (widget->focusPolicy() != Qt::NoFocus && !QByteArray{ widget->metaObject()->className() }.startsWith('Q'))
	{
		return true;
	}
	// So is a custom widget whose accessible interface can be pressed
	auto* const iface = QAccessible::queryAccessibleInterface(widget);
	auto* const actions = iface ? iface->actionInterface() : nullptr;
	return actions != nullptr && actions->actionNames().contains(QAccessibleActionInterface::pressAction());
}

QString normalized(QString const& text)
{
	auto result = text.toLower();
	result.remove(QRegularExpression{ "[^a-z0-9]" });
	return result;
}

bool isExcepted(Exceptions const& exceptions, QWidget const* const widget, Check const check)
{
	for (auto const& exception : exceptions)
	{
		if (exception.check != check)
		{
			continue;
		}
		for (auto const* w = widget; w != nullptr; w = w->parentWidget())
		{
			if (w == exception.widget)
			{
				return true;
			}
		}
	}
	return false;
}

// Collects the objects reachable by navigating down the accessibility tree (the cells of the item views are not visited, they never are widgets)
void collectReachableObjects(QAccessibleInterface* const iface, QSet<QObject const*>& reachable, int const depth)
{
	if (iface == nullptr || depth > 64)
	{
		return;
	}
	if (auto const* const object = iface->object())
	{
		reachable.insert(object);
	}
	if (iface->tableInterface() != nullptr)
	{
		return;
	}
	auto const count = iface->childCount();
	for (auto i = 0; i < count; ++i)
	{
		collectReachableObjects(iface->child(i), reachable, depth + 1);
	}
}

class Auditor final
{
public:
	Auditor(QWidget* const topLevel, Exceptions const& exceptions)
		: _topLevel{ topLevel }
		, _exceptions{ exceptions }
	{
		collectReachableObjects(QAccessible::queryAccessibleInterface(topLevel), _reachable, 0);
	}

	Issues run()
	{
		auto widgets = _topLevel->findChildren<QWidget*>();
		widgets.prepend(_topLevel);

		// Controls sharing a name, by parent and role
		auto namesByScope = std::map<std::tuple<QWidget const*, int, QString>, std::vector<QWidget const*>>{};

		for (auto* const widget : widgets)
		{
			if ((widget != _topLevel && !widget->isVisibleTo(_topLevel)) || isInternalWidget(widget) || !isInteractive(widget))
			{
				continue;
			}
			auditWidget(widget);

			if (auto* const iface = QAccessible::queryAccessibleInterface(widget); iface != nullptr && iface->isValid())
			{
				auto const name = iface->text(QAccessible::Name);
				if (!name.isEmpty())
				{
					namesByScope[{ widget->parentWidget(), static_cast<int>(iface->role()), name }].push_back(widget);
				}
			}
		}

		for (auto const& [scope, scopeWidgets] : namesByScope)
		{
			if (scopeWidgets.size() > 1)
			{
				for (auto const* const widget : scopeWidgets)
				{
					report(widget, Check::DuplicateName, QString{ "%1 controls with this name and role share the same parent" }.arg(scopeWidgets.size()));
				}
			}
		}

		auditFormLayouts();

		return std::move(_issues);
	}

private:
	void report(QWidget const* const widget, Check const check, QString const& reason)
	{
		if (isExcepted(_exceptions, widget, check))
		{
			return;
		}
		auto* const iface = QAccessible::queryAccessibleInterface(const_cast<QWidget*>(widget));
		_issues.push_back(Issue{ check, widget->objectName(), widget->metaObject()->className(), widget->window()->windowTitle(), iface ? iface->role() : QAccessible::NoRole, iface ? iface->text(QAccessible::Name) : QString{}, reason });
	}

	void auditWidget(QWidget* const widget)
	{
		auto* const iface = QAccessible::queryAccessibleInterface(widget);
		if (iface == nullptr || !iface->isValid())
		{
			report(widget, Check::InvalidInterface, "No valid accessible interface");
			return;
		}

		auto const role = iface->role();
		auto const name = iface->text(QAccessible::Name);
		auto const description = iface->text(QAccessible::Description);

		// Name
		if (name.trimmed().isEmpty())
		{
			report(widget, Check::MissingName, "Empty accessible name");
		}
		else
		{
			if (widget->accessibleName().isEmpty() && IconFonts.contains(widget->font().family()))
			{
				report(widget, Check::IconFontGlyphName, "Displays an icons font glyph and has no accessible name (exposes the glyph name)");
			}
			else if (QRegularExpression{ "^[a-z0-9]+(_[a-z0-9]+)+$" }.match(name).hasMatch())
			{
				report(widget, Check::IconFontGlyphName, "The name looks like the name of an icons font glyph");
			}
			if (!widget->objectName().isEmpty() && name == widget->objectName())
			{
				report(widget, Check::ObjectNameAsName, "The accessible name is the object name");
			}
			if (!description.isEmpty() && normalized(description) == normalized(name))
			{
				report(widget, Check::DescriptionSameAsName, QString{ "The description repeats the name: '%1'" }.arg(description));
			}
		}

		// Role and interfaces, by kind of control (Qt lists no action for a disabled control)
		auto const isEnabled = !iface->state().disabled;
		auto const* const actions = iface->actionInterface();
		auto const actionNames = actions ? actions->actionNames() : QStringList{};
		auto const hasAnyAction = [&actionNames](QStringList const& expected)
		{
			for (auto const& action : expected)
			{
				if (actionNames.contains(action))
				{
					return true;
				}
			}
			return false;
		};

		if (qobject_cast<QAbstractButton*>(widget))
		{
			static auto const s_buttonRoles = std::vector<QAccessible::Role>{ QAccessible::PushButton, QAccessible::CheckBox, QAccessible::RadioButton, QAccessible::ButtonMenu, QAccessible::ButtonDropDown, QAccessible::ButtonDropGrid };
			if (std::find(s_buttonRoles.begin(), s_buttonRoles.end(), role) == s_buttonRoles.end())
			{
				report(widget, Check::UnexpectedRole, "Not a button role");
			}
			if (isEnabled && !hasAnyAction({ QAccessibleActionInterface::pressAction(), QAccessibleActionInterface::toggleAction(), QAccessibleActionInterface::showMenuAction() }))
			{
				report(widget, Check::MissingAction, "No press, toggle nor show menu action");
			}
		}
		else if (qobject_cast<QLineEdit*>(widget) || qobject_cast<QTextEdit*>(widget) || qobject_cast<QPlainTextEdit*>(widget))
		{
			if (role != QAccessible::EditableText)
			{
				report(widget, Check::UnexpectedRole, "Not an editable text role");
			}
			if (iface->textInterface() == nullptr)
			{
				report(widget, Check::MissingTextInterface, "No text interface");
			}
			else if (!iface->state().readOnly && iface->editableTextInterface() == nullptr)
			{
				report(widget, Check::MissingTextInterface, "Editable but no editable text interface");
			}
		}
		else if (qobject_cast<QComboBox*>(widget))
		{
			if (role != QAccessible::ComboBox)
			{
				report(widget, Check::UnexpectedRole, "Not a combo box role");
			}
			if (isEnabled && !hasAnyAction({ QAccessibleActionInterface::showMenuAction(), QAccessibleActionInterface::pressAction() }))
			{
				report(widget, Check::MissingAction, "No show menu nor press action");
			}
		}
		else if (qobject_cast<QAbstractSpinBox*>(widget) || qobject_cast<QAbstractSlider*>(widget) || qobject_cast<QProgressBar*>(widget))
		{
			if (iface->valueInterface() == nullptr)
			{
				report(widget, Check::MissingValueInterface, "No value interface");
			}
		}
		else if (qobject_cast<QAbstractItemView*>(widget))
		{
			if (role != QAccessible::Table && role != QAccessible::Tree && role != QAccessible::List)
			{
				report(widget, Check::UnexpectedRole, "Not a table, tree or list role");
			}
			if (iface->tableInterface() == nullptr)
			{
				report(widget, Check::MissingTableInterface, "No table interface");
			}
		}
		else if (qobject_cast<QTabBar*>(widget))
		{
			if (role != QAccessible::PageTabList)
			{
				report(widget, Check::UnexpectedRole, "Not a page tab list role");
			}
		}

		// Reachability from the window
		if (widget != _topLevel && !_reachable.contains(widget))
		{
			report(widget, Check::NotReachable, "Not reachable by navigating the accessibility tree from the window");
		}
	}

	void auditFormLayouts()
	{
		for (auto* const formLayout : _topLevel->findChildren<QFormLayout*>())
		{
			for (auto row = 0; row < formLayout->rowCount(); ++row)
			{
				auto* const labelItem = formLayout->itemAt(row, QFormLayout::LabelRole);
				auto* const fieldItem = formLayout->itemAt(row, QFormLayout::FieldRole);
				auto* const label = labelItem ? qobject_cast<QLabel*>(labelItem->widget()) : nullptr;
				if (label == nullptr || label->text().isEmpty() || fieldItem == nullptr)
				{
					continue;
				}
				// The field may be a layout grouping several controls: its first interactive control is the labeled one
				auto* field = fieldItem->widget();
				if (field == nullptr && fieldItem->layout() != nullptr)
				{
					for (auto i = 0; i < fieldItem->layout()->count() && field == nullptr; ++i)
					{
						auto* const widget = fieldItem->layout()->itemAt(i)->widget();
						if (widget != nullptr && isInteractive(widget))
						{
							field = widget;
						}
					}
				}
				if (field == nullptr || !field->isVisibleTo(_topLevel) || !isInteractive(field))
				{
					continue;
				}
				if (label->buddy() != field)
				{
					report(field, Check::UnlabeledFormField, QString{ "The visible label '%1' is not the buddy of the field" }.arg(label->text()));
				}
			}
		}
	}

	QWidget* const _topLevel{ nullptr };
	Exceptions const& _exceptions;
	QSet<QObject const*> _reachable{};
	Issues _issues{};
};
} // namespace

Issues audit(QWidget* const topLevel, Exceptions const& exceptions)
{
	return Auditor{ topLevel, exceptions }.run();
}

std::string toString(Issues const& issues)
{
	auto stream = std::stringstream{};
	for (auto const& issue : issues)
	{
		stream << "[" << checkName(issue.check) << "] " << issue.reason.toStdString() << " - class=" << issue.className.toStdString() << " objectName='" << issue.objectName.toStdString() << "' window='" << issue.windowTitle.toStdString() << "' role=0x" << std::hex << static_cast<int>(issue.role) << std::dec << " name='" << issue.name.toStdString() << "'\n";
	}
	return stream.str();
}

char const* checkName(Check const check) noexcept
{
	switch (check)
	{
		case Check::InvalidInterface:
			return "InvalidInterface";
		case Check::MissingName:
			return "MissingName";
		case Check::IconFontGlyphName:
			return "IconFontGlyphName";
		case Check::ObjectNameAsName:
			return "ObjectNameAsName";
		case Check::DescriptionSameAsName:
			return "DescriptionSameAsName";
		case Check::UnexpectedRole:
			return "UnexpectedRole";
		case Check::MissingAction:
			return "MissingAction";
		case Check::MissingTextInterface:
			return "MissingTextInterface";
		case Check::MissingValueInterface:
			return "MissingValueInterface";
		case Check::MissingTableInterface:
			return "MissingTableInterface";
		case Check::UnlabeledFormField:
			return "UnlabeledFormField";
		case Check::DuplicateName:
			return "DuplicateName";
		case Check::NotReachable:
			return "NotReachable";
		default:
			return "Unknown";
	}
}

} // namespace accessibilityAudit
